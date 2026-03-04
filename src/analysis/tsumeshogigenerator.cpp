/// @file tsumeshogigenerator.cpp
/// @brief 詰将棋局面自動生成オーケストレータの実装

#include "tsumeshogigenerator.h"
#include "usi.h"
#include "playmode.h"
#include "boardconstants.h"

#include <QThread>
#include <QtConcurrent>

namespace {
/// エンジン無応答ガードの余裕時間(ms)
constexpr int kSafetyMarginMs = 5000;
/// プログレス更新間隔(ms)
constexpr int kProgressIntervalMs = 500;
}

TsumeshogiGenerator::TsumeshogiGenerator(QObject* parent)
    : QObject(parent)
{
    m_safetyTimer.setSingleShot(true);
    connect(&m_safetyTimer, &QTimer::timeout, this, &TsumeshogiGenerator::onSafetyTimeout);

    connect(&m_progressTimer, &QTimer::timeout, this, &TsumeshogiGenerator::onProgressTimerTimeout);
    connect(&m_batchWatcher, &QFutureWatcher<QStringList>::finished,
            this, &TsumeshogiGenerator::onBatchReady);
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

    m_positionGenerator.setSettings(settings.posGenSettings);

    // PlayMode を作成（Usiコンストラクタが参照を要求するため）
    m_playMode = std::make_unique<PlayMode>(PlayMode::TsumiSearchMode);

    // Usi インスタンスを作成（モデル不要、ゲームコントローラ不要）
    m_usi = std::make_unique<Usi>(nullptr, nullptr, nullptr, *m_playMode, this);

    // checkmate シグナルを接続
    connect(m_usi.get(), &Usi::checkmateSolved,
            this, &TsumeshogiGenerator::onCheckmateSolved);
    connect(m_usi.get(), &Usi::checkmateNoMate,
            this, &TsumeshogiGenerator::onCheckmateNoMate);
    connect(m_usi.get(), &Usi::checkmateNotImplemented,
            this, &TsumeshogiGenerator::onCheckmateNotImplemented);
    connect(m_usi.get(), &Usi::checkmateUnknown,
            this, &TsumeshogiGenerator::onCheckmateUnknown);
    connect(m_usi.get(), &Usi::errorOccurred,
            this, &TsumeshogiGenerator::errorOccurred);

    // エンジン起動
    if (!m_usi->startAndInitializeEngine(settings.enginePath, settings.engineName)) {
        m_phase = Phase::Idle;
        cleanup();
        emit finished();
        return;
    }

    // キャンセルフラグを初期化
    m_cancelFlag = makeCancelFlag();
    m_positionQueue.clear();
    m_waitingForPositions = false;

    // タイマー開始
    m_elapsedTimer.start();
    m_progressTimer.start(kProgressIntervalMs);

    // バッチ生成を開始し、完了後に最初の局面を送信する
    startBatchGeneration();
}

void TsumeshogiGenerator::stop()
{
    if (m_phase == Phase::Idle) return;
    m_phase = Phase::Idle;

    // バッチ生成をキャンセル
    if (m_cancelFlag) m_cancelFlag->store(true);
    m_batchWatcher.cancel();
    m_batchWatcher.waitForFinished();

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
    m_safetyTimer.stop();

    if (m_phase == Phase::Searching) {
        // 「ちょうどN手」のフィルタリング
        if (pv.size() == m_settings.targetMoves) {
            startTrimmingPhase(m_currentSfen, pv);
        } else {
            processResult(false);
        }
    } else { // Trimming
        if (pv.size() == m_settings.targetMoves) {
            // 除去しても同じ手数で詰む → 除去確定
            m_trimBaseSfen = m_trimTestSfen;
            m_trimBasePv = pv;
            // 候補リストを再構築し先頭から再開
            m_trimCandidates = enumerateRemovablePieces(m_trimBaseSfen);
            m_trimCandidateIndex = 0;
            tryNextTrimCandidate();
        } else {
            // 手数が変わった → 除去却下、次の候補へ
            m_trimCandidateIndex++;
            tryNextTrimCandidate();
        }
    }
}

void TsumeshogiGenerator::onCheckmateNoMate()
{
    if (m_phase == Phase::Idle) return;
    m_safetyTimer.stop();

    if (m_phase == Phase::Searching) {
        processResult(false);
    } else { // Trimming
        // 詰みなし → 除去却下、次の候補へ
        m_trimCandidateIndex++;
        tryNextTrimCandidate();
    }
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
    m_safetyTimer.stop();

    if (m_phase == Phase::Searching) {
        processResult(false);
    } else { // Trimming
        // 不明 → 除去却下、次の候補へ
        m_trimCandidateIndex++;
        tryNextTrimCandidate();
    }
}

void TsumeshogiGenerator::onSafetyTimeout()
{
    if (m_phase == Phase::Idle) return;

    if (m_phase == Phase::Searching) {
        // エンジン無応答 → 次の局面に進む
        processResult(false);
    } else { // Trimming
        // タイムアウト → 除去却下、次の候補へ
        m_trimCandidateIndex++;
        tryNextTrimCandidate();
    }
}

void TsumeshogiGenerator::onProgressTimerTimeout()
{
    emit progressUpdated(m_triedCount, m_foundCount, m_elapsedTimer.elapsed());
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

    // キューが少なくなったら次のバッチ生成を先行開始
    if (m_positionQueue.size() < 2 && !m_batchWatcher.isRunning()) {
        startBatchGeneration();
    }

    // ThinkingInfoPresenter が info 行を処理する際に盤面データが必要
    // ダミーの盤面データ（81マス空欄）を設定してクラッシュを防ぐ
    QList<QChar> dummyBoard(BoardConstants::kNumBoardSquares, QChar(' '));
    m_usi->setClonedBoardData(dummyBoard);

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

    if (found) {
        m_foundCount++;
        emit positionFound(m_currentSfen, pv);

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

void TsumeshogiGenerator::cleanup()
{
    m_safetyTimer.stop();
    m_progressTimer.stop();

    if (m_usi) {
        m_usi->cleanupEngineProcessAndThread();
        m_usi.reset();
    }
    m_playMode.reset();

    // バッチ生成状態をクリア
    m_positionQueue.clear();
    m_waitingForPositions = false;
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
    const int batchSize = qMax(QThread::idealThreadCount(), 4);

    auto future = QtConcurrent::run(
        &TsumeshogiPositionGenerator::generateBatch, settings, batchSize, cancelFlag);
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

    // 全候補を試し終わった → トリミング完了
    if (m_trimCandidateIndex >= m_trimCandidates.size()) {
        finishTrimmingPhase();
        return;
    }

    const TrimCandidate& candidate = m_trimCandidates.at(m_trimCandidateIndex);
    m_trimTestSfen = removePieceFromSfen(m_trimBaseSfen, candidate);
    sendTrimmingCheck(m_trimTestSfen);
}

void TsumeshogiGenerator::sendTrimmingCheck(const QString& modifiedSfen)
{
    QList<QChar> dummyBoard(BoardConstants::kNumBoardSquares, QChar(' '));
    m_usi->setClonedBoardData(dummyBoard);

    QString positionStr = QStringLiteral("position sfen ") + modifiedSfen;
    m_usi->sendPositionAndGoMateCommands(m_settings.timeoutMs, positionStr);

    m_safetyTimer.start(m_settings.timeoutMs + kSafetyMarginMs);
}

void TsumeshogiGenerator::finishTrimmingPhase()
{
    // トリミング結果を出力
    m_currentSfen = m_trimBaseSfen;
    m_phase = Phase::Searching;
    processResult(true, m_trimBasePv);
}
