/// @file tsumeretrogradegenerator.cpp
/// @brief 詰み局面からの逆算・盤面編集・余詰リペア・多層ビームサーチによる長手数詰将棋自動生成器の実装

#include "tsumeretrogradegenerator.h"

#include "tsumelocalscreen.h"
#include "tsumematequerylink.h"
#include "tsumeretrogradecheckpoint.h"
#include "tsumeretrogradeops.h"
#include "tsumeshogisettings.h"

#include <QFile>

#include <algorithm>
#include <chrono>
#include <functional>
#include <utility>

namespace {
constexpr int kProgressIntervalMs = 1000;
constexpr std::size_t kMaxFastCacheEntries = 100000;
constexpr std::size_t kMaxStrictCacheEntries = 200000;
/// エンジンに問い合わせずに続けて評価する候補の数と時間の上限（超えたらイベントループへ戻る）
constexpr int kMaxLocalEvalsPerSlice = 48;
constexpr int kMaxLocalEvalSliceMs = 40;
} // namespace

TsumeRetrogradeGenerator::Settings TsumeRetrogradeGenerator::Settings::normalized() const
{
    using Limits = TsumeshogiSettings::GeneratorLimits;
    Settings out = *this;
    out.targetMoves = Limits::oddTargetMoves(targetMoves);
    out.maxPositionsToFind = qBound(0, maxPositionsToFind, 10000);
    out.beamWidth = qBound(1, beamWidth, 2000);
    out.branchFactor = qBound(4, branchFactor, 200);
    out.maxAttackPieces = qBound(1, maxAttackPieces, 38);
    out.maxDefendPieces = qBound(0, maxDefendPieces, 38);
    out.maxTotalBoardPieces = qBound(2, maxTotalBoardPieces, 39);
    out.stepTimeoutMs = qBound(100, stepTimeoutMs, 600000);
    out.verifyQueryTimeoutMs = qBound(500, verifyQueryTimeoutMs, 600000);
    out.verifyBudgetMs = qBound<qint64>(5000, verifyBudgetMs, 86400000LL);
    out.engineHashMb = qBound(0, engineHashMb, 65536);
    out.engineThreads = qBound(0, engineThreads, 256);
    out.checkpointIntervalSec = qBound(5, checkpointIntervalSec, 3600);
    return out;
}

TsumeRetrogradeGenerator::TsumeRetrogradeGenerator(QObject* parent)
    : QObject(parent)
    , m_engine(new TsumeMateQueryLink(this))
{
    connect(m_engine, &TsumeMateQueryLink::ready, this, &TsumeRetrogradeGenerator::onEngineReady);
    connect(m_engine, &TsumeMateQueryLink::aboutToAsk, this, &TsumeRetrogradeGenerator::onQuerySent);
    connect(m_engine, &TsumeMateQueryLink::answered, this, &TsumeRetrogradeGenerator::onOracleAnswered);
    connect(m_engine, &TsumeMateQueryLink::unsupported, this, &TsumeRetrogradeGenerator::onEngineUnsupported);
    connect(m_engine, &TsumeMateQueryLink::timedOut, this, &TsumeRetrogradeGenerator::onOracleTimedOut);
    connect(m_engine, &TsumeMateQueryLink::errorOccurred, this, &TsumeRetrogradeGenerator::onOracleError);
    m_stepTimer.setSingleShot(true);
    connect(&m_stepTimer, &QTimer::timeout, this, &TsumeRetrogradeGenerator::onStepTimer);
    connect(&m_progressTimer, &QTimer::timeout, this, &TsumeRetrogradeGenerator::onProgressTimer);
    connect(&m_checkpointTimer, &QTimer::timeout, this, &TsumeRetrogradeGenerator::onCheckpointTimer);
}

TsumeRetrogradeGenerator::~TsumeRetrogradeGenerator()
{
    if (m_running) {
        m_running = false;
        m_stepTimer.stop();
        m_progressTimer.stop();
        m_checkpointTimer.stop();
        m_engine->shutdown();
    }
}

TsumeRetrogradeBeam::Config TsumeRetrogradeGenerator::beamConfig() const
{
    TsumeRetrogradeBeam::Config config;
    config.targetMoves = m_settings.targetMoves;
    config.beamWidth = m_settings.beamWidth;
    config.branchFactor = m_settings.branchFactor;
    config.limits.maxAttackPieces = m_settings.maxAttackPieces;
    config.limits.maxDefendPieces = m_settings.maxDefendPieces;
    config.limits.maxTotalBoardPieces = m_settings.maxTotalBoardPieces;
    config.limits.addRemainingToDefenderHand = m_settings.addRemainingToDefenderHand;
    config.requireNoSurplus = m_settings.requireNoSurplus;
    config.allowFinalMoveAlternatives = m_settings.allowFinalMoveAlternatives;
    config.excludeSimilar = m_settings.excludeSimilar;
    config.hasSeedLines = !m_settings.seedSfens.isEmpty();
    return config;
}

TsumeStrictCertifier::Settings TsumeRetrogradeGenerator::certifierSettings() const
{
    TsumeStrictCertifier::Settings cs;
    cs.targetMoves = m_settings.targetMoves;
    cs.totalBudgetMs = m_settings.verifyBudgetMs;
    cs.queryMaxMs = m_settings.verifyQueryTimeoutMs;
    cs.allowFinalMoveAlternatives = m_settings.allowFinalMoveAlternatives;
    cs.requireNoSurplus = m_settings.requireNoSurplus;
    cs.strictMinLength = m_settings.strictMinLength;
    cs.maxVisitedNodes = 200000;
    cs.maxCacheEntries = static_cast<int>(kMaxStrictCacheEntries);
    return cs;
}

std::optional<QStringList> TsumeRetrogradeGenerator::readInputLines(const QString& path, QString* errorString)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorString) *errorString = file.errorString();
        return std::nullopt;
    }
    QStringList lines;
    const QStringList rawLines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& raw : rawLines) {
        // "SFEN" と "SFEN moves ..." のどちらの行もそのまま返す
        const QString line = raw.trimmed();
        if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) lines.append(line);
    }
    return lines;
}

bool TsumeRetrogradeGenerator::start(const Settings& settings)
{
    if (m_running) return false;
    m_settings = settings.normalized();
    m_stopping = false;
    m_mode = ActiveMode::Idle;

    m_beam.reset(beamConfig());
    m_firstCheck.reset();
    m_activeCertifier.reset();
    m_fastCache.clear();
    m_strictCache.clear();

    if (m_settings.resumeFromCheckpoint) {
        if (m_settings.checkpointPath.isEmpty()) {
            emit errorOccurred(tr("再開するにはチェックポイントファイルを指定してください。"));
            return false;
        }
        QString loadErr;
        if (!loadCheckpointFile(m_settings.checkpointPath, &loadErr)) {
            emit errorOccurred(loadErr);
            return false;
        }
    } else {
        m_beam.seedRng(m_settings.rngSeed != 0
            ? m_settings.rngSeed
            : static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
        initializeFreshSeeds();
    }

    // 既出の除外対象（SFEN / SFEN moves PV）を登録する（種局面・再開フロンティア構築後に適用し、既消化親種も外す）
    for (const QString& raw : std::as_const(m_settings.excludeLines)) {
        m_beam.addExcludeLine(raw);
    }

    m_running = true;
    m_sessionTimer.start();
    m_progressTimer.start(kProgressIntervalMs);
    if (!m_settings.checkpointPath.isEmpty()) {
        m_checkpointTimer.start(m_settings.checkpointIntervalSec * 1000);
    }

    TsumeMateQueryLink::Options engineOptions;
    engineOptions.enginePath = m_settings.enginePath;
    engineOptions.engineName = m_settings.engineName;
    engineOptions.hashMb = m_settings.engineHashMb;
    engineOptions.threads = m_settings.engineThreads;
    m_engine->start(engineOptions);
    return true;
}

void TsumeRetrogradeGenerator::stop()
{
    if (!m_running || m_stopping) return;
    m_stopping = true;
    finishRun(true);
}

void TsumeRetrogradeGenerator::addExcludeLine(const QString& raw)
{
    m_beam.addExcludeLine(raw);
}

void TsumeRetrogradeGenerator::addSeedLine(const QString& raw)
{
    m_beam.addSeedLine(raw);
}

void TsumeRetrogradeGenerator::initializeFreshSeeds()
{
    for (const QString& raw : std::as_const(m_settings.seedSfens)) {
        m_beam.addSeedLine(raw);
    }
    if (m_beam.isEmpty()) {
        m_beam.enqueueEndingSeeds(std::max(6, m_settings.beamWidth));
    }
}

void TsumeRetrogradeGenerator::onEngineReady()
{
    if (!m_running) return;
    scheduleStep();
}

void TsumeRetrogradeGenerator::restartEngineForRecovery()
{
    ++m_beam.stats().engineRestarts;
    if (!m_engine->restartForRecovery(m_beam.stats().engineRestarts)) {
        failRun(tr("エンジンが連続して応答しないため生成を中止しました。"));
    }
}

void TsumeRetrogradeGenerator::scheduleStep()
{
    if (!m_running || !m_engine->isReady() || m_engine->isAwaitingAnswer()) return;
    if (!m_stepTimer.isActive()) m_stepTimer.start(0);
}

void TsumeRetrogradeGenerator::onStepTimer()
{
    driveNext();
}

void TsumeRetrogradeGenerator::driveNext()
{
    if (!m_running || !m_engine->isReady() || m_engine->isAwaitingAnswer()) return;

    // 目標件数に達していれば完了
    if (m_settings.maxPositionsToFind > 0
        && static_cast<int>(m_beam.foundPositions().size()) >= m_settings.maxPositionsToFind) {
        finishRun(false);
        return;
    }

    // 1. 進行中の厳密証明ジョブがあれば最優先
    if (m_activeCertifier) {
        m_mode = ActiveMode::Certifying;
        stepCertifier();
        return;
    }

    // 2. 厳密証明待ち候補があれば開始
    if (const auto pending = m_beam.takePendingCertify()) {
        beginCertification(*pending);
        return;
    }

    // 3. 進行中の軽量初手余詰チェックがあれば続行
    if (m_firstCheck) {
        m_mode = ActiveMode::FirstMoveCheck;
        stepFirstMoveCheck();
        return;
    }

    // 4. 評価待ち候補がなければ次のバッチを展開
    if (!m_beam.hasEvalItems()) {
        m_beam.expandNextBatch();
        if (!m_beam.hasEvalItems()) {
            m_beam.enqueueEndingSeeds(std::max(4, m_settings.beamWidth));
        }
        if (!m_beam.hasEvalItems()) {
            failRun(tr("候補局面を生成できませんでした。駒数上限などの設定を確認してください。"));
            return;
        }
    }

    // 5. 評価キューから候補を取り出して評価（キャッシュ／内蔵 Hayanagi プレスクリーニング優先）
    evaluateQueuedCandidates();
}

void TsumeRetrogradeGenerator::beginCertification(const TsumeRetrogradeBeam::PendingCertifyItem& item)
{
    m_activeCertifier = std::make_unique<TsumeStrictCertifier>(item.sfen, certifierSettings());
    m_activeCertifierAllowTrim = item.allowTrimOnReject;
    m_activeCertifier->seedVerifiedCache(m_strictCache);
    m_mode = ActiveMode::Certifying;
    // 成長フェーズの時間付き探索の置換表残骸を持ち越さないようリセットする
    m_engine->resetHashIfDirty();
    if (!m_settings.checkpointPath.isEmpty()) saveCheckpointNow();
    stepCertifier();
}

void TsumeRetrogradeGenerator::evaluateQueuedCandidates()
{
    QElapsedTimer sliceTimer;
    sliceTimer.start();
    int localProcessed = 0;
    while (m_beam.hasEvalItems()) {
        m_activeEval = m_beam.takeEvalItem();

        // エンジンに問い合わせずに分かる答え（判定済み、または内蔵探索で求めた詰み手順）
        std::optional<MateAnswer> answer;
        if (const MateAnswer* cached = cachedAnswer(m_activeEval.sfen)) {
            answer = *cached;
        } else {
            MateAnswer localAnswer;
            const TsumeLocalScreen::Outcome screen = TsumeLocalScreen::screenCandidate(
                m_activeEval.sfen, m_settings.targetMoves, m_activeEval.minDesiredPlies,
                m_beam.stats().maxPliesReached, m_activeEval.fromRepair, m_settings.stepTimeoutMs,
                m_beam.rng(), &localAnswer);
            m_beam.countDraw();
            if (screen == TsumeLocalScreen::Outcome::DeferToEngine) {
                m_mode = ActiveMode::EvalCandidate;
                askEngine(m_activeEval.sfen, m_settings.stepTimeoutMs, false);
                return;
            }
            if (screen == TsumeLocalScreen::Outcome::Solved) {
                pruneFastCacheIfNeeded();
                m_fastCache.emplace(m_activeEval.sfen.toStdString(), localAnswer);
                answer = localAnswer;
            }
        }

        if (answer) {
            m_mode = ActiveMode::EvalCandidate;
            submitEvalAnswer(*answer);
            if (!m_running || m_engine->isAwaitingAnswer()) return;
            if (m_activeCertifier || m_beam.pendingCertifySize() > 0 || m_firstCheck) {
                scheduleStep();
                return;
            }
        }
        if (++localProcessed >= kMaxLocalEvalsPerSlice || sliceTimer.elapsed() >= kMaxLocalEvalSliceMs) {
            scheduleStep();
            return;
        }
    }
    scheduleStep();
}

void TsumeRetrogradeGenerator::askEngine(const QString& sfen, int limitMs, bool strictInfinite)
{
    // 送れなかった問い合わせは、答えが「不明」だったものとして進める
    if (m_engine->ask(sfen, limitMs, strictInfinite) != TsumeMateQueryLink::AskResult::Sent) {
        handleQueryUnknownOrTimeout(false);
    }
}

void TsumeRetrogradeGenerator::onQuerySent()
{
    ++m_beam.stats().engineQueries;
}

void TsumeRetrogradeGenerator::onOracleAnswered(const MateAnswer& answer, qint64 elapsedMs)
{
    if (!m_running) return;

    // 確定応答を適切なキャッシュへ保存
    if (answer.kind == MateAnswer::Kind::Mate || answer.kind == MateAnswer::Kind::NoMate) {
        const std::string key = m_engine->lastQuerySfen().toStdString();
        if (m_engine->lastQueryWasStrict()) {
            if (m_strictCache.size() < kMaxStrictCacheEntries) {
                const auto reply = answer.kind == MateAnswer::Kind::Mate
                    ? TsumeshogiVerifier::Reply::Mate : TsumeshogiVerifier::Reply::NoMate;
                m_strictCache.emplace(key, TsumeshogiVerifier::CacheEntry{reply, static_cast<int>(answer.pv.size())});
            }
        }
        pruneFastCacheIfNeeded();
        m_fastCache.emplace(key, answer);
    }

    switch (m_mode) {
    case ActiveMode::EvalCandidate:
        submitEvalAnswer(answer);
        break;
    case ActiveMode::FirstMoveCheck:
        submitFirstMoveCheckAnswer(answer);
        break;
    case ActiveMode::Certifying:
        submitCertifierAnswer(answer, elapsedMs);
        break;
    case ActiveMode::Idle:
        break;
    }
    scheduleStep();
}

void TsumeRetrogradeGenerator::onEngineUnsupported()
{
    if (!m_running) return;
    failRun(tr("選択されたエンジンは詰み探索（go mate）に対応していません。"));
}

void TsumeRetrogradeGenerator::onOracleTimedOut()
{
    if (!m_running) return;
    handleQueryUnknownOrTimeout(true);
}

void TsumeRetrogradeGenerator::handleQueryUnknownOrTimeout(bool dueToTimeout)
{
    const qint64 elapsedMs = m_engine->lastQueryElapsedMs();
    const MateAnswer unknown{MateAnswer::Kind::Unknown, {}};

    switch (m_mode) {
    case ActiveMode::EvalCandidate:
        submitEvalAnswer(unknown);
        break;
    case ActiveMode::FirstMoveCheck:
        submitFirstMoveCheckAnswer(unknown);
        break;
    case ActiveMode::Certifying:
        submitCertifierAnswer(unknown, elapsedMs);
        break;
    case ActiveMode::Idle:
        break;
    }

    if (!m_running) return;
    if (dueToTimeout) {
        // askWithin の期限切れ、またはエンジン無応答 → プロセスを再起動して安全に続行
        restartEngineForRecovery();
    } else {
        scheduleStep();
    }
}

void TsumeRetrogradeGenerator::onOracleError(const QString& message)
{
    if (!m_running) return;
    failRun(message.isEmpty() ? tr("エンジンでエラーが発生しました。") : message);
}

void TsumeRetrogradeGenerator::pruneFastCacheIfNeeded()
{
    if (m_fastCache.size() < kMaxFastCacheEntries) return;
    // 半分を破棄してメモリを一定に保つ
    auto it = m_fastCache.begin();
    for (std::size_t i = 0; i < kMaxFastCacheEntries / 2 && it != m_fastCache.end(); ++i) {
        it = m_fastCache.erase(it);
    }
}

const MateAnswer* TsumeRetrogradeGenerator::cachedAnswer(const QString& sfen) const
{
    const auto cached = m_fastCache.find(sfen.toStdString());
    return cached != m_fastCache.end() ? &cached->second : nullptr;
}

// ============================================================
// 候補の評価
// ============================================================

void TsumeRetrogradeGenerator::submitEvalAnswer(const MateAnswer& answer)
{
    if (answer.kind != MateAnswer::Kind::Mate || answer.pv.isEmpty()) return;

    const auto metrics = TsumeLineMetrics::analyzeLine(m_activeEval.sfen, answer.pv);
    if (!metrics) return;

    const int plies = metrics->plies;
    TsumeRetrogradeBeam::Stats& stats = m_beam.stats();
    if (plies > stats.maxPliesReached) {
        stats.maxPliesReached = std::min(plies, m_settings.targetMoves);
        stats.batchesSinceProgress = 0;
        if (!m_settings.checkpointPath.isEmpty()) saveCheckpointNow();
    }

    // 目標手数以上に到達した場合
    if (plies >= m_settings.targetMoves) {
        submitTargetCandidate(answer, plies);
        return;
    }

    // 目標手数未満：この手数層のビームに入る見込みがあれば軽量初手余詰チェックを行う
    if (!m_beam.layerAccepts(*metrics)) return;

    // 駒余りが多すぎる局面は厳密モードでは優先度を大きく落とす（ただし1枚除去で解消を試せる）
    if (m_settings.requireNoSurplus && metrics->surplusCount >= 3) return;

    startFirstMoveCheck(m_activeEval.sfen, answer.pv, *metrics, m_activeEval.fromRepair);
}

void TsumeRetrogradeGenerator::submitTargetCandidate(const MateAnswer& answer, int plies)
{
    QString targetSfen = m_activeEval.sfen;
    QStringList targetPv = answer.pv;
    if (plies > m_settings.targetMoves) {
        const auto sliced = TsumeRetrogradeOps::sliceToTargetLength(
            m_activeEval.sfen, answer.pv, m_settings.targetMoves);
        if (!sliced) return;
        targetSfen = sliced->sfen;
        targetPv = sliced->pv;
    }
    const auto targetMetrics = TsumeLineMetrics::analyzeLine(targetSfen, targetPv);
    if (!targetMetrics) return;

    // 駒余りや最終手複数解の有無を先に判定
    const bool failsSurplus = m_settings.requireNoSurplus && targetMetrics->surplusCount > 0;
    const bool failsFinalAlt = !m_settings.allowFinalMoveAlternatives && targetMetrics->finalImmediateMates > 1;

    // 既出・類似作は採択・厳密証明しないが、類似作で駒余りなし・最終手一意なら
    // 次の1箇所変異で別系統へ飛ぶための探索足場（ステッピングストーン）としてフロンティアへ残す
    if (m_beam.isKnown(targetSfen)) return;
    if (m_beam.isSimilarToAccepted(targetSfen, targetPv)) {
        if (!failsSurplus && !failsFinalAlt) {
            m_beam.addToFrontier(m_beam.makeNode(targetSfen, targetPv, *targetMetrics, true, 15.0));
        }
        return;
    }

    // 駒余りや最終手複数解が既に判明している場合 → 1段目のみ1枚除去で不要駒落としを試みる
    if (failsSurplus || failsFinalAlt) {
        if (!m_activeEval.fromRepair) m_beam.enqueueTrims(targetSfen, QStringLiteral("preflight_trim"), 4);
        return;
    }

    // 61手以下なら、外部エンジンの厳密証明へ送る前に内蔵 Hayanagi で主手順上の明白な余詰を事前弾き
    const TsumeLocalScreen::MainLineCook preCook = TsumeLocalScreen::quickMainLineCookCheck(targetSfen, targetPv, 2);
    if (!preCook.cookMove.isEmpty()) {
        if (preCook.ply == 0) {
            m_beam.addToRepairQueue(targetSfen, preCook.mainMove, preCook.cookMove, m_settings.targetMoves);
        }
        if (!m_activeEval.fromRepair) m_beam.enqueueTrims(targetSfen, QStringLiteral("precook_trim"), 2);
        return;
    }

    m_beam.enqueueForCertification(targetSfen, targetPv, !m_activeEval.fromRepair);
}

// ============================================================
// 成長中の軽量初手余詰チェック
// ============================================================

void TsumeRetrogradeGenerator::startFirstMoveCheck(const QString& sfen, const QStringList& pv,
                                                   const TsumeLineMetrics::Metrics& metrics,
                                                   bool fromRepair)
{
    m_firstCheck = TsumeFirstMoveCheck::begin(sfen, pv, metrics, fromRepair);
    if (!m_firstCheck) return;
    if (m_firstCheck->verdict()) {
        // 内蔵探索だけで結論が出た
        finishFirstMoveCheck();
        return;
    }
    m_mode = ActiveMode::FirstMoveCheck;
    scheduleStep();
}

void TsumeRetrogradeGenerator::stepFirstMoveCheck()
{
    if (!m_firstCheck) return;
    const std::optional<QString> query = m_firstCheck->nextQuery(
        std::bind(&TsumeRetrogradeGenerator::cachedAnswer, this, std::placeholders::_1));
    if (!query) {
        finishFirstMoveCheck();
        return;
    }
    // 反駁判定なので短めの予算で十分（最大 stepTimeoutMs / 2, 下限 200ms）
    const int limit = std::clamp(m_settings.stepTimeoutMs / 2, 200, m_settings.stepTimeoutMs);
    askEngine(*query, limit, false);
}

void TsumeRetrogradeGenerator::submitFirstMoveCheckAnswer(const MateAnswer& answer)
{
    if (!m_firstCheck) return;
    m_firstCheck->submit(answer);
}

void TsumeRetrogradeGenerator::finishFirstMoveCheck()
{
    if (!m_firstCheck || !m_firstCheck->verdict()) return;
    const TsumeFirstMoveCheck check = std::move(*m_firstCheck);
    m_firstCheck.reset();
    const TsumeFirstMoveCheck::Verdict& verdict = *check.verdict();
    const TsumeLineMetrics::Metrics& metrics = check.metrics();

    if (!verdict.cookMove.isEmpty()) {
        // 余詰が見つかった → 目標付近（target-2 以上）または最大到達層なら標的型リペアキューに登録
        if (metrics.plies >= std::max(1, m_settings.targetMoves - 2)
            || metrics.plies >= m_beam.stats().maxPliesReached) {
            m_beam.addToRepairQueue(check.sfen(), check.mainPv().first(), verdict.cookMove, metrics.plies);
        }
        // 盤面の足場として低めのスコアで残すことは許容する（特にまだビームが空いているとき）
        m_beam.addToFrontier(m_beam.makeNode(check.sfen(), check.mainPv(), metrics, false, 18.0));
        scheduleStep();
        return;
    }

    if (verdict.unique && check.fromRepair()) ++m_beam.stats().repairSucceeded;

    m_beam.addToFrontier(m_beam.makeNode(check.sfen(), check.mainPv(), metrics, verdict.unique));
    scheduleStep();
}

// ============================================================
// 厳密証明・不要駒トリミングフェーズ
// ============================================================

void TsumeRetrogradeGenerator::stepCertifier()
{
    while (m_activeCertifier) {
        const std::optional<MateQuery> query = m_activeCertifier->nextQuery();
        if (m_activeCertifier->isDone()) {
            finishCertifier();
            return;
        }
        if (!query) {
            scheduleStep();
            return;
        }
        askEngine(query->sfen, query->limitMs, m_settings.strictMinLength);
        return;
    }
}

void TsumeRetrogradeGenerator::submitCertifierAnswer(const MateAnswer& answer, qint64 elapsedMs)
{
    if (!m_activeCertifier) return;
    m_activeCertifier->submit(answer, elapsedMs);
    if (m_activeCertifier->isDone()) finishCertifier();
}

void TsumeRetrogradeGenerator::finishCertifier()
{
    if (!m_activeCertifier) return;
    const std::unique_ptr<TsumeStrictCertifier> cert = std::move(m_activeCertifier);

    // 厳密証明で新たに得られた確定キャッシュをマージ
    for (const auto& [k, v] : cert->verifiedCache()) {
        if (m_strictCache.size() >= kMaxStrictCacheEntries) break;
        if (v.reply != TsumeshogiVerifier::Reply::Unknown) m_strictCache.emplace(k, v);
    }

    if (cert->stage() == TsumeStrictCertifier::Stage::Certified && cert->outcome()) {
        const auto& out = *cert->outcome();
        if (!m_beam.isFound(out.sfen) && !m_beam.isSimilarToAccepted(out.sfen, out.pv)) {
            m_beam.addFound(FoundPosition{out.sfen, out.pv, out.removedPieces, out.queries, out.spentMs});
            emit positionFound(out.sfen, out.pv, out.removedPieces, out.queries);

            // 類似作除外が有効なとき、採択作の直前親種（2手進めた N-2 手局面）は
            // 再び2手逆算しても同一 core で類似除外されるため、フロンティアから外して次の別系統種へ移る
            if (m_settings.excludeSimilar && out.pv.size() >= 9) m_beam.removeParentOf(out.sfen, out.pv);
            if (!m_settings.checkpointPath.isEmpty()) saveCheckpointNow();
        }
    } else {
        ++m_beam.stats().certifiedRejected;
        if (cert->rejectionReason() == TsumeStrictCertifier::RejectionReason::Inconclusive) {
            ++m_beam.stats().certifiedInconclusive;
        }
        // 初手余詰で棄却された場合はリペアキューに戻して修復を試みる
        if (cert->rejectionReason() == TsumeStrictCertifier::RejectionReason::Multiple
            && cert->cookInfo() && cert->cookInfo()->ply == 0) {
            m_beam.addToRepairQueue(cert->candidateSfen(), cert->cookInfo()->mainMove,
                                    cert->cookInfo()->cookMove, m_settings.targetMoves);
        }
        // 余詰・駒余りで棄却された目標手数候補は、1段目に限り不要な1枚を落として再試行する（再帰増殖は防止）
        if (m_activeCertifierAllowTrim
            && (cert->rejectionReason() == TsumeStrictCertifier::RejectionReason::Multiple
                || cert->rejectionReason() == TsumeStrictCertifier::RejectionReason::Surplus)) {
            m_beam.enqueueTrims(cert->candidateSfen(), QStringLiteral("cert_trim_repair"), 2);
        }
    }
    scheduleStep();
}

// ============================================================
// 進捗・チェックポイント保存／復元・終了
// ============================================================

qint64 TsumeRetrogradeGenerator::elapsedMs() const
{
    return m_beam.stats().accumulatedElapsedMs + (m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0);
}

TsumeRetrogradeGenerator::Progress TsumeRetrogradeGenerator::progress() const
{
    Progress p;
    if (m_activeCertifier) {
        p.phase = QStringLiteral("certifying");
        p.certifierStage = TsumeStrictCertifier::stageName(m_activeCertifier->stage());
        p.certifierQueries = m_activeCertifier->queries();
        p.trimIndex = m_activeCertifier->trimIndex();
        p.trimTotal = m_activeCertifier->trimTotal();
    } else if (m_firstCheck) {
        p.phase = QStringLiteral("checking_first");
    } else if (m_mode == ActiveMode::EvalCandidate && m_activeEval.fromRepair) {
        p.phase = QStringLiteral("repairing");
    } else {
        p.phase = QStringLiteral("growing");
    }
    const TsumeRetrogradeBeam::Stats& stats = m_beam.stats();
    p.maxPliesReached = stats.maxPliesReached;
    p.frontierNodes = m_beam.frontierNodeCount();
    p.repairQueueSize = m_beam.repairQueueSize();
    p.pendingCertifySize = m_beam.pendingCertifySize();
    p.generatedCandidates = stats.generatedCandidates;
    p.engineQueries = stats.engineQueries;
    p.found = static_cast<int>(m_beam.foundPositions().size());
    p.certifiedRejected = stats.certifiedRejected;
    p.certifiedInconclusive = stats.certifiedInconclusive;
    p.repairSucceeded = stats.repairSucceeded;
    p.engineRestarts = stats.engineRestarts;
    p.elapsedMs = elapsedMs();
    return p;
}

void TsumeRetrogradeGenerator::onProgressTimer()
{
    if (!m_running) return;
    emit progressUpdated(progress());
}

void TsumeRetrogradeGenerator::onCheckpointTimer()
{
    if (!m_running || m_settings.checkpointPath.isEmpty()) return;
    saveCheckpointNow();
}

QJsonObject TsumeRetrogradeGenerator::buildCheckpointJson() const
{
    QJsonObject root = TsumeRetrogradeCheckpoint::begin(m_settings);

    // 中断時に評価中・初手検査中だった候補を取りこぼさないよう、評価待ちの先頭に書く
    std::optional<TsumeRetrogradeBeam::EvalItem> inFlight;
    if (m_firstCheck && !m_firstCheck->sfen().isEmpty()) {
        inFlight = TsumeRetrogradeBeam::EvalItem{m_firstCheck->sfen(), QStringLiteral("resumed_first_check"),
                                                 m_firstCheck->metrics().plies, m_firstCheck->fromRepair()};
    } else if (m_engine->isAwaitingAnswer() && m_mode == ActiveMode::EvalCandidate && !m_activeEval.sfen.isEmpty()) {
        inFlight = m_activeEval;
    }
    m_beam.writeJson(root, elapsedMs(), inFlight);

    if (m_activeCertifier && !m_activeCertifier->isDone()) {
        root[QStringLiteral("active_certifier")] = m_activeCertifier->toJson();
    }
    return root;
}

bool TsumeRetrogradeGenerator::saveCheckpointNow(QString* errorMessage)
{
    if (m_settings.checkpointPath.isEmpty()) return false;
    if (!TsumeRetrogradeCheckpoint::save(m_settings.checkpointPath, buildCheckpointJson(), errorMessage)) return false;
    emit checkpointSaved(m_settings.checkpointPath);
    return true;
}

bool TsumeRetrogradeGenerator::loadCheckpointFile(const QString& path, QString* errorMessage)
{
    const std::optional<QJsonObject> root = TsumeRetrogradeCheckpoint::load(path, m_settings, errorMessage);
    if (!root) return false;

    m_beam.readJson(*root);

    if (root->contains(QStringLiteral("active_certifier"))) {
        m_activeCertifier = TsumeStrictCertifier::fromJson(
            root->value(QStringLiteral("active_certifier")).toObject(), certifierSettings());
        if (m_activeCertifier) {
            for (const auto& [k, v] : m_activeCertifier->verifiedCache()) {
                if (m_strictCache.size() >= kMaxStrictCacheEntries) break;
                m_strictCache.emplace(k, v);
            }
        }
    }

    return true;
}

void TsumeRetrogradeGenerator::finishRun(bool stopped)
{
    if (!m_running) return;
    // 途中保存が設定されていれば、最終状態を保存してから終了する
    if (!m_settings.checkpointPath.isEmpty()) saveCheckpointNow();
    m_running = false;
    m_stepTimer.stop();
    m_progressTimer.stop();
    m_checkpointTimer.stop();
    m_engine->shutdown();
    emit progressUpdated(progress());
    emit finished(stopped);
}

void TsumeRetrogradeGenerator::failRun(const QString& message)
{
    if (!m_running) return;
    if (!m_settings.checkpointPath.isEmpty()) saveCheckpointNow();
    m_running = false;
    m_stepTimer.stop();
    m_progressTimer.stop();
    m_checkpointTimer.stop();
    m_engine->shutdown();
    emit errorOccurred(message);
    emit finished(false);
}
