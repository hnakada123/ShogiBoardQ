/// @file tsumeshogigenerator.cpp
/// @brief 詰将棋局面の自動生成の実装

#include "tsumeshogigenerator.h"

#include <QCoreApplication>
#include <QtConcurrentRun>

#include "mateoracle.h"
#include "logcategories.h"
#include "matequeryloop.h"

namespace {
/// プログレス更新間隔(ms)
constexpr int kProgressIntervalMs = 500;
/// 1回のバッチで集める候補局面数（候補はエンジン探索に比べ十分軽く作れるので少量でよい）
constexpr int kBatchSize = 8;
/// 1回のバッチで生成する局面数の上限（候補が集まらなくても進捗を返す）
constexpr int kBatchMaxGenerated = 2000;
/// ワーカースレッドでの事前選別の時間上限。2026-09-25 の計測では 100ms で時間切れは 4000 局面中 0
constexpr TsumeshogiCandidateScreener::Limits kScreenLimits{100, 100, 2};
}

TsumeshogiGenerator::TsumeshogiGenerator(QObject* parent)
    : QObject(parent)
{
    connect(&m_progressTimer, &QTimer::timeout, this, &TsumeshogiGenerator::onProgressTimerTimeout);
    connect(&m_batchWatcher, &QFutureWatcher<TsumeshogiCandidateScreener::Batch>::finished,
            this, &TsumeshogiGenerator::onBatchReady);
}

TsumeshogiGenerator::~TsumeshogiGenerator()
{
    stop();
}

void TsumeshogiGenerator::start(const Settings& settings)
{
    if (m_running) return;

    m_settings = settings;
    m_running = true;
    m_generatedCount = 0;
    m_foundCount = 0;
    m_foundSfens.clear();
    m_verificationRejected = 0;
    m_verificationInconclusive = 0;
    m_screenUnknownCount = 0;
    emit verificationStatsUpdated(0, 0);

    createOracle();
    // 起動できなかったときは、戻った後に onEngineError で届く
    m_oracle->start(settings.enginePath, settings.engineName);
}

void TsumeshogiGenerator::createOracle()
{
    // エンジンとの接続を作る（思考情報の表示は使わない）。所有は parent（this）
    m_oracle = new MateOracle(this);
    m_loop = new MateQueryLoop(m_oracle, m_oracle);
    connect(m_oracle, &MateOracle::ready, this, &TsumeshogiGenerator::onEngineReady);
    connect(m_oracle, &MateOracle::errorOccurred, this, &TsumeshogiGenerator::onEngineError);
    connect(m_oracle, &MateOracle::timedOut, this, &TsumeshogiGenerator::onNoResponse);
    connect(m_loop, &MateQueryLoop::queryStarted, this, &TsumeshogiGenerator::onQueryStarted);
    connect(m_loop, &MateQueryLoop::finished, this, &TsumeshogiGenerator::onCandidateFinished);
    connect(m_loop, &MateQueryLoop::unsupported, this, &TsumeshogiGenerator::onUnsupported);
}

void TsumeshogiGenerator::onEngineReady()
{
    if (!m_running) return;
    m_cancelFlag = makeCancelFlag();
    m_positionQueue.clear();
    m_waitingForPositions = false;

    m_elapsedTimer.start();
    m_progressTimer.start(kProgressIntervalMs);
    emit searchPhaseStarted();

    // 空のキューを生成待ちにしてから、最初のバッチ生成を始める（完了で最初の候補を調べる）
    startNextCandidate();
}

void TsumeshogiGenerator::stop()
{
    if (!m_running) return;
    finish();
}

void TsumeshogiGenerator::finish()
{
    cleanup();
    emit finished();
}

// ============================================================
// 候補
// ============================================================

void TsumeshogiGenerator::startNextCandidate()
{
    if (!m_running) return;

    // キューが空ならバッチ生成の完了を待つ
    if (m_positionQueue.isEmpty()) {
        m_waitingForPositions = true;
        if (!m_batchWatcher.isRunning()) startBatchGeneration();
        return;
    }

    const QString sfen = m_positionQueue.takeFirst();
    // キューが少なくなったら次のバッチ生成を先に始める
    if (m_positionQueue.size() < 2 && !m_batchWatcher.isRunning()) startBatchGeneration();

    m_job = std::make_unique<TsumeCandidateJob>(
        sfen, TsumeCandidateJob::Settings{m_settings.targetMoves, m_settings.timeoutMs,
                                          m_settings.allowFinalMoveAlternatives});
    m_jobRejectedReported = 0;
    m_jobInconclusiveReported = 0;
    m_loop->run(m_job.get());
}

void TsumeshogiGenerator::onQueryStarted()
{
    if (!m_job) return;
    reportCandidateStats();
    switch (m_job->stage()) {
    case TsumeCandidateJob::Stage::Trim:
        emit trimmingProgress(m_job->trimCandidate(), m_job->trimCandidateCount());
        break;
    case TsumeCandidateJob::Stage::Verify:
    case TsumeCandidateJob::Stage::VerifyTrimmed:
        emit verificationProgress(m_job->verificationQueries());
        break;
    case TsumeCandidateJob::Stage::CheckLength:
    case TsumeCandidateJob::Stage::Accepted:
    case TsumeCandidateJob::Stage::Rejected:
        break;
    }
}

void TsumeshogiGenerator::onCandidateFinished()
{
    if (!m_job) return;
    reportCandidateStats();
    const std::unique_ptr<TsumeCandidateJob> job = std::move(m_job);
    if (job->outcome()) publish(*job->outcome());
    if (!m_running) return;
    // 検査・除去まで進んだ候補を終えたら、表示を探索に戻す
    if (job->passedLengthCheck()) emit searchPhaseStarted();

    if (m_settings.maxPositionsToFind > 0 && m_foundCount >= m_settings.maxPositionsToFind) {
        emit progressUpdated(m_generatedCount, m_foundCount, m_elapsedTimer.elapsed());
        finish();
        return;
    }
    emit progressUpdated(m_generatedCount, m_foundCount, m_elapsedTimer.elapsed());
    startNextCandidate();
}

void TsumeshogiGenerator::reportCandidateStats()
{
    const int rejected = m_job->rejectedCount() - m_jobRejectedReported;
    const int inconclusive = m_job->inconclusiveCount() - m_jobInconclusiveReported;
    if (rejected == 0 && inconclusive == 0) return;
    m_jobRejectedReported += rejected;
    m_jobInconclusiveReported += inconclusive;
    m_verificationRejected += rejected;
    m_verificationInconclusive += inconclusive;
    emit verificationStatsUpdated(m_verificationRejected, m_verificationInconclusive);
}

void TsumeshogiGenerator::publish(const TsumeCandidateJob::Outcome& outcome)
{
    // 除去により異なる候補が同じ最小局面に収束することがあるため重複を除く
    if (m_foundSfens.contains(outcome.sfen)) return;
    m_foundSfens.insert(outcome.sfen);
    ++m_foundCount;
    emit positionFound(outcome.sfen, outcome.pv);
}

// ============================================================
// エンジンの異常
// ============================================================

void TsumeshogiGenerator::onUnsupported()
{
    if (!m_running) return;
    cleanup();
    emit errorOccurred(tr("エンジンが詰み探索に対応していません。"));
    emit finished();
}

void TsumeshogiGenerator::onNoResponse()
{
    // stop にも応答しない → エンジンが固まっているとみなして生成を中止する
    if (!m_running) return;
    cleanup();
    emit errorOccurred(tr("エンジンが応答しないため生成を中止しました。"));
    emit finished();
}

void TsumeshogiGenerator::onEngineError(const QString& message)
{
    // 探索中のエンジン異常（プロセス異常終了など）→ 生成を終える
    if (!m_running) return;
    cleanup();
    emit errorOccurred(message);
    emit finished();
}

void TsumeshogiGenerator::onProgressTimerTimeout()
{
    emit progressUpdated(m_generatedCount, m_foundCount, m_elapsedTimer.elapsed());
}

void TsumeshogiGenerator::cleanup()
{
    // 生成の結果のまとめ（事前選別が時間切れで判定できずに通した局面の数は、選別の時間の調整の目安）
    if (m_running) {
        qCInfo(lcAnalysis).noquote() << "tsume generation ended: generated" << m_generatedCount << "found" << m_foundCount
                                     << "screening unknown" << m_screenUnknownCount << "rejected" << m_verificationRejected
                                     << "inconclusive" << m_verificationInconclusive;
    }
    m_running = false;
    // 上限到達・エラーでも先行生成を中止する。フラグを手放す前にワーカーへ通知する
    if (m_cancelFlag) m_cancelFlag->store(true);
    if (m_batchWatcher.isRunning()) m_batchWatcher.cancel();
    else m_batchWatcher.setFuture({});
    m_progressTimer.stop();

    if (m_loop) m_loop->cancel();
    m_job.reset();
    if (m_oracle) {
        m_oracle->quit();
        // 接続のシグナル処理中（エンジン応答・エラー通知）に呼ばれることがあるため
        // 同期削除は行わず、切断してから遅延削除する（this が親なので取りこぼしはない）
        disconnect(m_oracle, nullptr, this, nullptr);
        disconnect(m_loop, nullptr, this, nullptr);
        m_oracle->deleteLater();
        m_oracle = nullptr;
        m_loop = nullptr;
    }

    m_positionQueue.clear();
    m_foundSfens.clear();
    m_waitingForPositions = false;
    m_cancelFlag.reset();
}

// ============================================================
// バッチ生成
// ============================================================

void TsumeshogiGenerator::startBatchGeneration()
{
    if (m_batchWatcher.isRunning()) return;

    const auto settings = m_settings.posGenSettings;
    const int targetMoves = m_settings.targetMoves;
    const auto cancelFlag = m_cancelFlag;

    // UI をブロックしないようワーカースレッドで生成し、内蔵探索で候補だけを残す
    auto future = QtConcurrent::run([settings, targetMoves, cancelFlag]() {
        return TsumeshogiCandidateScreener::generateBatch(settings, targetMoves, kBatchSize,
                                                          kBatchMaxGenerated, kScreenLimits, cancelFlag);
    });
    m_batchWatcher.setFuture(future);
}

void TsumeshogiGenerator::onBatchReady()
{
    if (!m_running) {
        m_batchWatcher.setFuture({});
        return;
    }

    if (!m_batchWatcher.isCanceled()) {
        const TsumeshogiCandidateScreener::Batch batch = m_batchWatcher.result();
        m_batchWatcher.setFuture({});
        m_generatedCount += batch.generated;
        m_positionQueue.append(batch.candidates);
        m_screenUnknownCount += batch.unknown;
        if (batch.multipleFirstMoves > 0) {
            // 内蔵探索で2つ以上の詰む初手が見つかった局面は余詰として除外済み
            m_verificationRejected += batch.multipleFirstMoves;
            emit verificationStatsUpdated(m_verificationRejected, m_verificationInconclusive);
        }
        emit progressUpdated(m_generatedCount, m_foundCount, m_elapsedTimer.elapsed());
    }

    // キュー空待ちだった場合は再開
    if (m_waitingForPositions && !m_job) {
        m_waitingForPositions = false;
        startNextCandidate();
    }
}
