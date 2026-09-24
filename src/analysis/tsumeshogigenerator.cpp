/// @file tsumeshogigenerator.cpp
/// @brief 詰将棋局面自動生成オーケストレータの実装

#include "tsumeshogigenerator.h"
#include "usi.h"
#include "boardconstants.h"

#include <QtConcurrentRun>

namespace {
/// エンジン無応答ガードの余裕時間(ms)
constexpr int kSafetyMarginMs = 5000;
/// プログレス更新間隔(ms)
constexpr int kProgressIntervalMs = 500;
/// 安全タイマー発火後に送る stop への応答待ち時間(ms)
constexpr int kStopResponseTimeoutMs = 3000;
/// 1回のバッチで生成する候補局面数（生成はエンジン探索に比べ十分軽いので少量でよい）
constexpr int kBatchSize = 8;
}

TsumeshogiGenerator::TsumeshogiGenerator(QObject* parent)
    : QObject(parent)
{
    m_safetyTimer.setSingleShot(true);
    connect(&m_safetyTimer, &QTimer::timeout, this, &TsumeshogiGenerator::onSafetyTimeout);

    connect(&m_progressTimer, &QTimer::timeout, this, &TsumeshogiGenerator::onProgressTimerTimeout);
    connect(&m_batchWatcher, &QFutureWatcher<QStringList>::finished,
            this, &TsumeshogiGenerator::onBatchReady);
    m_verificationStepTimer.setSingleShot(true);
    connect(&m_verificationStepTimer, &QTimer::timeout,
            this, &TsumeshogiGenerator::continueVerification);
}

TsumeshogiGenerator::~TsumeshogiGenerator()
{
    stop();
}

void TsumeshogiGenerator::start(const Settings& settings)
{
    if (m_phase != Phase::Idle) return;

    m_settings = settings;
    m_triedCount = 0;
    m_foundCount = 0;
    m_phase = Phase::Searching;
    m_currentSfen.clear();
    m_foundSfens.clear();
    m_verificationRejected = 0;
    m_verificationInconclusive = 0;
    emit verificationStatsUpdated(0, 0);

    // Usi インスタンスを作成（モデル不要、ゲームコントローラ不要）。所有は parent（this）
    m_usi = new Usi(nullptr, nullptr, nullptr, this);

    // checkmate シグナルを接続
    connect(m_usi, &Usi::checkmateSolved,
            this, &TsumeshogiGenerator::onCheckmateSolved);
    connect(m_usi, &Usi::checkmateNoMate,
            this, &TsumeshogiGenerator::onCheckmateNoMate);
    connect(m_usi, &Usi::checkmateNotImplemented,
            this, &TsumeshogiGenerator::onCheckmateNotImplemented);
    connect(m_usi, &Usi::checkmateUnknown,
            this, &TsumeshogiGenerator::onCheckmateUnknown);
    connect(m_usi, &Usi::errorOccurred,
            this, &TsumeshogiGenerator::onEngineError);

    // エンジン起動。usiok/readyok 待ちでイベントループが回るため、
    // この間の stop() は m_stopRequestedDuringStart に記録して復帰後に処理する
    m_starting = true;
    m_stopRequestedDuringStart = false;
    const bool started = m_usi->startAndInitializeEngine(settings.enginePath, settings.engineName);
    m_starting = false;

    if (!started || m_stopRequestedDuringStart) {
        m_stopRequestedDuringStart = false;
        m_phase = Phase::Idle;
        cleanup();
        emit finished();
        return;
    }

    // ThinkingInfoPresenter が info 行を処理する際に盤面データが必要。
    // 局面は position コマンドで直接指定するため、ダミーの盤面（81マス空欄）を一度だけ設定する
    m_usi->setClonedBoardData(QList<QChar>(BoardConstants::kNumBoardSquares, QChar(' ')));

    // キャンセルフラグを初期化
    m_cancelFlag = makeCancelFlag();
    m_positionQueue.clear();
    m_waitingForPositions = false;

    // タイマー開始
    m_elapsedTimer.start();
    m_progressTimer.start(kProgressIntervalMs);
    emit searchPhaseStarted();

    // 空のキューを生成待ちにしてから、最初のバッチ生成を開始する。
    // 完了通知で generateAndSendNext() が再開され、最初の局面を送信する。
    generateAndSendNext();
}

void TsumeshogiGenerator::stop()
{
    if (m_phase == Phase::Idle) return;

    if (m_starting) {
        // エンジン初期化の待機ループ中。ここで Usi を破棄すると初期化処理が
        // 解放済みオブジェクトに触れるため、待機を中断させて start() 側で後始末する
        m_stopRequestedDuringStart = true;
        if (m_usi) m_usi->cancelCurrentOperation();
        return;
    }

    flushTrimmingResult();
    m_phase = Phase::Idle;

    // バッチ生成をキャンセル
    if (m_cancelFlag) m_cancelFlag->store(true);
    if (m_batchWatcher.isRunning()) {
        // UIブロックを避けるため同期待機せず、完了通知は onBatchReady 側で無視する。
        m_batchWatcher.cancel();
    }

    cleanup();
    emit finished();
}

bool TsumeshogiGenerator::isRunning() const
{
    return m_phase != Phase::Idle;
}

// ======================================================================
// エンジン応答スロット
// ======================================================================

void TsumeshogiGenerator::onCheckmateSolved(const QStringList& pv)
{
    if (m_phase == Phase::Idle) return;
    if (consumeStaleResponse()) return;
    m_safetyTimer.stop();

    if (m_phase == Phase::Verifying) {
        submitVerification(TsumeshogiVerifier::Reply::Mate, pv);
        return;
    }

    // 「ちょうどN手」のフィルタリング
    if (pv.size() != m_settings.targetMoves) {
        // 探索中: 次の局面へ / トリミング中: 手数が変わったので除去却下
        advanceAfterFailure();
        return;
    }

    startVerification(m_phase == Phase::Searching ? m_currentSfen : m_trimTestSfen, m_phase);
}

void TsumeshogiGenerator::onCheckmateNoMate()
{
    if (m_phase == Phase::Idle) return;
    if (consumeStaleResponse()) return;
    m_safetyTimer.stop();
    if (m_phase == Phase::Verifying) {
        submitVerification(TsumeshogiVerifier::Reply::NoMate);
        return;
    }
    // 詰みなし → 探索中: 次の局面へ / トリミング中: 除去却下
    advanceAfterFailure();
}

void TsumeshogiGenerator::onCheckmateNotImplemented()
{
    if (m_phase == Phase::Idle) return;
    m_safetyTimer.stop();
    m_phase = Phase::Idle;
    cleanup();
    emit errorOccurred(tr("エンジンが詰み探索に対応していません。"));
    emit finished();
}

void TsumeshogiGenerator::onCheckmateUnknown()
{
    if (m_phase == Phase::Idle) return;
    if (consumeStaleResponse()) return;
    m_safetyTimer.stop();
    // 不明・時間切れ → 探索中: 次の局面へ / トリミング中: 除去却下
    advanceAfterFailure();
}

void TsumeshogiGenerator::onSafetyTimeout()
{
    if (m_phase == Phase::Idle) return;

    if (m_awaitingStopResponse) {
        // stop にも応答しない → エンジンが固まっているとみなして生成を中止する
        m_awaitingStopResponse = false;
        flushTrimmingResult();
        m_phase = Phase::Idle;
        cleanup();
        emit errorOccurred(tr("エンジンが応答しないため生成を中止しました。"));
        emit finished();
        return;
    }

    // エンジン無応答 → 探索を止めて応答を待つ。次の局面は応答を受け取ってから送る。
    // 先に次を送ると、遅れて届いた前局面の応答を新しい局面の結果として誤採択してしまう
    m_awaitingStopResponse = true;
    m_usi->sendStopCommand();
    m_safetyTimer.start(kStopResponseTimeoutMs);
}

bool TsumeshogiGenerator::consumeStaleResponse()
{
    // 安全タイマー発火後に届いた応答（stop への応答または遅延した応答）は結果として扱わない
    if (!m_awaitingStopResponse) return false;
    m_awaitingStopResponse = false;
    m_safetyTimer.stop();
    advanceAfterFailure();
    return true;
}

void TsumeshogiGenerator::advanceAfterFailure()
{
    if (m_phase == Phase::Verifying) {
        submitVerification(TsumeshogiVerifier::Reply::Unknown);
    } else if (m_phase == Phase::Searching) {
        processResult(false);
    } else if (m_phase == Phase::Trimming) {
        m_trimCandidateIndex++;
        tryNextTrimCandidate();
    }
}

void TsumeshogiGenerator::onProgressTimerTimeout()
{
    emit progressUpdated(m_triedCount, m_foundCount, m_elapsedTimer.elapsed());
}

void TsumeshogiGenerator::onEngineError(const QString& message)
{
    if (m_phase == Phase::Idle) return;

    if (m_starting) {
        // start() 内のエンジン初期化失敗。後始末は start() が行う。
        // ユーザーの停止要求で待機を中断した場合はエラーとして通知しない
        if (!m_stopRequestedDuringStart) {
            emit errorOccurred(message);
        }
        return;
    }

    // 探索中のエンジン異常（プロセス異常終了など）→ 生成ループを終了する
    flushTrimmingResult();
    m_phase = Phase::Idle;
    cleanup();
    emit errorOccurred(message);
    emit finished();
}

// ======================================================================
// 検索フェーズ
// ======================================================================

void TsumeshogiGenerator::generateAndSendNext()
{
    if (m_phase != Phase::Searching) return;

    // キューから局面を取得（空ならバッチ生成完了を待つ）
    if (m_positionQueue.isEmpty()) {
        m_waitingForPositions = true;
        if (!m_batchWatcher.isRunning()) {
            startBatchGeneration();
        }
        return;
    }

    m_currentSfen = m_positionQueue.takeFirst();
    m_verifiedSfen.clear();
    m_verifiedPv.clear();
    m_trimBaseSfen.clear();
    m_trimBasePv.clear();

    // キューが少なくなったら次のバッチ生成を先行開始
    if (m_positionQueue.size() < 2 && !m_batchWatcher.isRunning()) {
        startBatchGeneration();
    }

    // position コマンド用の文字列を構築
    QString positionStr = QStringLiteral("position sfen ") + m_currentSfen;

    // エンジンに送信
    m_usi->sendPositionAndGoMateCommands(m_settings.timeoutMs, positionStr);

    // 安全タイマーを設定
    m_safetyTimer.start(m_settings.timeoutMs + kSafetyMarginMs);
}

void TsumeshogiGenerator::processResult(bool found, const QStringList& pv)
{
    m_triedCount++;

    if (found && registerFoundPosition(m_currentSfen, pv)) {
        // 上限チェック
        if (m_settings.maxPositionsToFind > 0 && m_foundCount >= m_settings.maxPositionsToFind) {
            m_phase = Phase::Idle;
            emit progressUpdated(m_triedCount, m_foundCount, m_elapsedTimer.elapsed());
            cleanup();
            emit finished();
            return;
        }
    }

    emit progressUpdated(m_triedCount, m_foundCount, m_elapsedTimer.elapsed());

    // 次の局面を送信
    generateAndSendNext();
}

bool TsumeshogiGenerator::registerFoundPosition(const QString& sfen, const QStringList& pv)
{
    // 通常終了・停止・エラーのすべての出力経路で同じ条件を要求する。
    if (sfen.isEmpty() || sfen != m_verifiedSfen || pv != m_verifiedPv) return false;
    // トリミングにより異なる候補が同じ最小局面に収束することがあるため重複を除外する
    if (m_foundSfens.contains(sfen)) return false;
    m_foundSfens.insert(sfen);
    m_foundCount++;
    emit positionFound(sfen, pv);
    return true;
}

void TsumeshogiGenerator::flushTrimmingResult()
{
    // 除去局面の検査途中なら、最後に全検査を通ったベースだけを出力する。
    const bool trimming = m_phase == Phase::Trimming
        || (m_phase == Phase::Verifying && m_verificationOrigin == Phase::Trimming);
    if (!trimming || m_trimBaseSfen.isEmpty() || m_trimBaseSfen != m_verifiedSfen
        || m_trimBasePv != m_verifiedPv) return;
    m_triedCount++;
    registerFoundPosition(m_trimBaseSfen, m_trimBasePv);
    emit progressUpdated(m_triedCount, m_foundCount, m_elapsedTimer.elapsed());
}

void TsumeshogiGenerator::cleanup()
{
    m_safetyTimer.stop();
    m_progressTimer.stop();
    m_verificationStepTimer.stop();
    m_verifier.abort();
    m_verificationAwaiting = false;
    m_verificationSfen.clear();
    m_verifiedSfen.clear();
    m_verifiedPv.clear();

    if (m_usi) {
        m_usi->cleanupEngineProcessAndThread();
        // Usi のシグナル処理中（エンジン応答・エラー通知）に呼ばれることがあるため
        // 同期削除は行わず、切断してから遅延削除する（this が親なので取りこぼしはない）
        disconnect(m_usi, nullptr, this, nullptr);
        m_usi->deleteLater();
        m_usi = nullptr;
    }

    // バッチ生成状態をクリア
    m_positionQueue.clear();
    m_waitingForPositions = false;
    m_awaitingStopResponse = false;
    m_cancelFlag.reset();

    // トリミング状態をクリア
    m_trimBaseSfen.clear();
    m_trimBasePv.clear();
    m_trimCandidates.clear();
    m_trimCandidateIndex = 0;
    m_trimTestSfen.clear();
}

// ======================================================================
// バッチ生成
// ======================================================================

void TsumeshogiGenerator::startBatchGeneration()
{
    if (m_batchWatcher.isRunning()) return;

    const auto settings = m_settings.posGenSettings;
    const auto cancelFlag = m_cancelFlag;

    // UI をブロックしないようワーカースレッドで生成する
    auto future = QtConcurrent::run([settings, cancelFlag]() {
        return TsumeshogiPositionGenerator::generateBatch(settings, kBatchSize, cancelFlag);
    });
    m_batchWatcher.setFuture(future);
}

void TsumeshogiGenerator::onBatchReady()
{
    if (m_phase == Phase::Idle) return;

    if (!m_batchWatcher.isCanceled()) {
        m_positionQueue.append(m_batchWatcher.result());
    }

    // キュー空待ちだった場合は再開
    if (m_waitingForPositions && m_phase == Phase::Searching) {
        m_waitingForPositions = false;
        generateAndSendNext();
    }
}

// ======================================================================
// トリミングフェーズ
// ======================================================================

void TsumeshogiGenerator::startTrimmingPhase(const QString& sfen, const QStringList& pv)
{
    m_phase = Phase::Trimming;
    m_trimBaseSfen = sfen;
    m_trimBasePv = pv;
    m_trimCandidates = enumerateRemovablePieces(sfen);
    m_trimCandidateIndex = 0;
    tryNextTrimCandidate();
}

void TsumeshogiGenerator::tryNextTrimCandidate()
{
    if (m_phase != Phase::Trimming) return;

    while (m_trimCandidateIndex < m_trimCandidates.size()) {
        const TrimCandidate& candidate = m_trimCandidates.at(m_trimCandidateIndex);
        const QString testSfen = removePieceFromSfen(m_trimBaseSfen, candidate);

        // 除去によって開始局面で玉に王手がかかる（詰将棋として不正）候補は
        // エンジンに送らず除外する（例: 飛車と玉の間の守り駒を除去した場合）
        if (TsumeshogiPositionGenerator::isDefenderKingInCheck(testSfen)) {
            m_trimCandidateIndex++;
            continue;
        }

        m_trimTestSfen = testSfen;
        emit trimmingProgress(m_trimCandidateIndex + 1, static_cast<int>(m_trimCandidates.size()));
        sendTrimmingCheck(m_trimTestSfen);
        return;
    }

    // 全候補を試し終わった → トリミング完了
    finishTrimmingPhase();
}

void TsumeshogiGenerator::sendTrimmingCheck(const QString& modifiedSfen)
{
    QString positionStr = QStringLiteral("position sfen ") + modifiedSfen;
    m_usi->sendPositionAndGoMateCommands(m_settings.timeoutMs, positionStr);

    m_safetyTimer.start(m_settings.timeoutMs + kSafetyMarginMs);
}

void TsumeshogiGenerator::finishTrimmingPhase()
{
    // トリミング結果を出力
    m_currentSfen = m_trimBaseSfen;
    m_phase = Phase::Searching;
    emit searchPhaseStarted();
    processResult(true, m_trimBasePv);
}
