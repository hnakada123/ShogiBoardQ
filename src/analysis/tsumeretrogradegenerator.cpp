/// @file tsumeretrogradegenerator.cpp
/// @brief 詰み局面からの逆算・盤面編集・余詰リペア・多層ビームサーチによる長手数詰将棋自動生成器の実装

#include "tsumeretrogradegenerator.h"

#include "logcategories.h"
#include "tsumecandidatejob.h"
#include "tsumecollection.h"
#include "tsumemateengine.h"
#include "tsumeshogisettings.h"
#include "usienginesession.h"
#include "usiprotocolhandler.h"

#include <position.h>
#include <tsume.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>
#include <atomic>
#include <chrono>

namespace {
constexpr int kCheckpointVersion = 1;
constexpr int kProgressIntervalMs = 1000;
constexpr int kOracleInitTimeoutMs = 30000;
constexpr int kMaxConsecutiveRecoveries = 5;
constexpr std::size_t kMaxVisitedHashes = 400000;
constexpr std::size_t kMaxFastCacheEntries = 100000;
constexpr std::size_t kMaxStrictCacheEntries = 200000;
constexpr std::size_t kMaxRepairQueue = 256;

/// 成長時の軽量初手チェック用：応手の反駁優先度（小さいほど先に調べる）
int defensePriority(const shogi::Position& beforeDefense, const shogi::Move& defense)
{
    if (defense.drop) return 400 + static_cast<int>(defense.piece);
    const bool isCapture = !shogi::is_empty(beforeDefense.piece_at(defense.to));
    const bool isKing = defense.piece == shogi::PieceType::King;
    if (isCapture && !isKing) return 100;
    if (isCapture && isKing) return 150;
    if (isKing) return 200;
    return 300;
}

bool hasLocalImmediateMate(const shogi::Position& position)
{
    shogi::Position probe = position;
    std::vector<shogi::Move> checks;
    probe.generate_checking_moves(checks);
    for (const auto& mv : checks) {
        shogi::MoveUndo undo;
        probe.make_move(mv, undo);
        const bool mated = !probe.has_legal_move();
        probe.unmake_move(mv, undo);
        if (mated) return true;
    }
    return false;
}

/// 63手以下のとき、内蔵 Hayanagi で超短時間に別初手余詰を検出する（検出できたらその初手USIを返す）
QString quickLocalCookCheck(const QString& sfen, const QString& mainMove, int plies, int perMoveBudgetMs = 4)
{
    if (plies > 61) return {};
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return {};
    shogi::TsumeSearch search;
    const std::atomic_bool stop{false};
    const int depth = std::min(63, plies + 1);
    for (const auto& move : pos.generate_checking_moves()) {
        const QString usi = QString::fromStdString(pos.move_to_usi(move));
        if (usi == mainMove) continue;
        shogi::Position child = pos;
        child.do_move(move);
        if (!child.has_legal_move()) return usi;
        const auto res = search.solve(child, shogi::Color::Black, depth, perMoveBudgetMs, stop, 1);
        if (res.status == shogi::TsumeStatus::Mate) return usi;
    }
    return {};
}

struct QuickMainLineCook {
    int ply = -1;
    QString mainMove;
    QString cookMove;
};

/// 61手以下のとき、主手順の各攻方手番（1, 3, ..., N-2手目）について内蔵 Hayanagi で
/// 明白な余詰を数ミリ秒で事前検出し、厳密証明の無駄撃ちを防ぐ。
QuickMainLineCook quickLocalMainLineCookCheck(const QString& sfen, const QStringList& pv, int perMoveBudgetMs = 2)
{
    const int n = static_cast<int>(pv.size());
    if (n < 3 || n > 61 || n % 2 == 0) return {};
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return {};
    const std::atomic_bool stop{false};

    for (int i = 0; i < n - 1; ++i) {
        if (i % 2 == 0) {
            const int remaining = n - i;
            const QString& mainMove = pv.at(i);
            shogi::TsumeSearch search;
            const int depth = std::min(63, remaining);
            for (const auto& move : pos.generate_checking_moves()) {
                const QString usi = QString::fromStdString(pos.move_to_usi(move));
                if (usi == mainMove) continue;
                shogi::Position child = pos;
                child.do_move(move);
                if (!child.has_legal_move()) return {i, mainMove, usi};
                const auto res = search.solve(child, shogi::Color::Black, depth, perMoveBudgetMs, stop, 1);
                if (res.status == shogi::TsumeStatus::Mate) return {i, mainMove, usi};
            }
        }
        if (!pos.apply_usi_move(pv.at(i).toStdString())) break;
    }
    return {};
}

enum class LocalScreenOutcome {
    Reject,
    Solved,
    DeferToEngine,
};

/// 外部エンジン（KomoringHeights）へ IPC 送信する前に、内蔵 Hayanagi で超高速プレスクリーニングを行う。
/// - 確定不詰（NoMate）や高層展開からの浅すぎる早詰・泥沼タイムアウトを数ミリ秒で棄却し、IPC の無駄撃ちを防ぐ。
/// - 19手以下では目標手数までの詰み・PV を直接抽出し、本命候補だけを KomoringHeights の厳密証明へ回す。
LocalScreenOutcome localScreenCandidate(const QString& sfen, int targetMoves, int minDesiredPlies,
                                        int maxPliesReached, bool fromRepair, int stepTimeoutMs,
                                        std::mt19937_64& rng, MateAnswer* solvedOut)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return LocalScreenOutcome::Reject;
    if (pos.side_to_move() != shogi::Color::Black || pos.is_in_check(shogi::Color::White)) {
        return LocalScreenOutcome::Reject;
    }

    // 親が高手数（例: 13手以上から15手狙い）の展開由来のときだけ、浅すぎる早詰ゴミを落とす
    const bool hasHighFrontier = (maxPliesReached >= std::max(3, targetMoves - 2));
    const bool dropShallow = (minDesiredPlies >= 9 && minDesiredPlies >= targetMoves - 2);
    const std::atomic_bool stop{false};
    shogi::TsumeSearch search;

    if (targetMoves <= 19) {
        const int baseBudget = (targetMoves <= 9) ? 5 : (targetMoves <= 15 ? 5 : (targetMoves <= 17 ? 9 : 15));
        const int budgetMs = std::clamp(baseBudget, 2, std::max(2, stepTimeoutMs));
        auto res = search.solve(pos, shogi::Color::Black, targetMoves, budgetMs, stop, 1);

        if (res.status == shogi::TsumeStatus::NoMate) {
            return LocalScreenOutcome::Reject;
        }
        if (res.status == shogi::TsumeStatus::Limit) {
            // targetMoves 以内では確実に不詰。15〜17手は既に親種があるなら外部へ送らず即次候補へ
            if (hasHighFrontier && targetMoves <= 17) return LocalScreenOutcome::Reject;
            const int deferPct = (dropShallow ? 4 : 15);
            std::uniform_int_distribution<int> pct(0, 99);
            return pct(rng) < deferPct ? LocalScreenOutcome::DeferToEngine : LocalScreenOutcome::Reject;
        }
        if (res.status == shogi::TsumeStatus::Mate) {
            const int plies = res.plies;
            if (dropShallow && plies < targetMoves - 2) {
                return LocalScreenOutcome::Reject;
            }
            // PV を抽出（置換表がすでに埋まっているのでほぼ0ms）
            QStringList pv;
            pv.reserve(plies);
            shogi::Position cur = pos;
            int rem = plies;
            while (rem > 0) {
                if (res.status != shogi::TsumeStatus::Mate || !res.move.is_valid()) break;
                pv.append(QString::fromStdString(cur.move_to_usi(res.move)));
                if (!cur.do_move(res.move)) break;
                --rem;
                if (rem > 0) {
                    res = search.solve(cur, shogi::Color::Black, rem, std::max(10, budgetMs), stop, 1);
                }
            }
            if (pv.size() == plies) {
                if (solvedOut) *solvedOut = MateAnswer{MateAnswer::Kind::Mate, pv};
                return LocalScreenOutcome::Solved;
            }
            return LocalScreenOutcome::Reject;
        }
        // Timeout：15〜17手で既に親種があるときは重い泥沼候補を外部エンジンに送らず即捨てる
        if (hasHighFrontier && targetMoves <= 17) {
            return LocalScreenOutcome::Reject;
        }
        if (!hasHighFrontier && fromRepair) return LocalScreenOutcome::DeferToEngine;
        const int deferPct = (targetMoves <= 17) ? 10 : 20;
        std::uniform_int_distribution<int> pct(0, 99);
        return pct(rng) < deferPct ? LocalScreenOutcome::DeferToEngine : LocalScreenOutcome::Reject;
    }

    // 21手以上：15手以下の早詰と確定不詰を 4ms で弾き、真の深手数候補だけを外部エンジンへ送る
    const int earlyDepth = std::min(15, std::max(3, targetMoves - 4));
    const int budgetMs = std::clamp(4, 1, std::max(1, stepTimeoutMs));
    auto res = search.solve(pos, shogi::Color::Black, earlyDepth, budgetMs, stop, 1);
    if (res.status == shogi::TsumeStatus::NoMate) {
        return LocalScreenOutcome::Reject;
    }
    if (res.status == shogi::TsumeStatus::Mate) {
        if (dropShallow && res.plies < targetMoves - 2) {
            return LocalScreenOutcome::Reject;
        }
        // ボトムアップ成長中の足場なら PV を抽出してそのまま使う
        QStringList pv;
        pv.reserve(res.plies);
        shogi::Position cur = pos;
        int rem = res.plies;
        while (rem > 0) {
            if (res.status != shogi::TsumeStatus::Mate || !res.move.is_valid()) break;
            pv.append(QString::fromStdString(cur.move_to_usi(res.move)));
            if (!cur.do_move(res.move)) break;
            --rem;
            if (rem > 0) {
                res = search.solve(cur, shogi::Color::Black, rem, 10, stop, 1);
            }
        }
        if (pv.size() == res.plies) {
            if (solvedOut) *solvedOut = MateAnswer{MateAnswer::Kind::Mate, pv};
            return LocalScreenOutcome::Solved;
        }
        return LocalScreenOutcome::Reject;
    }
    if (res.status == shogi::TsumeStatus::Timeout) {
        // 15手すら 4ms で読み切れない過密泥沼局面は間引いて外部エンジンの詰まりを防ぐ
        std::uniform_int_distribution<int> pct(0, 99);
        return pct(rng) < 35 ? LocalScreenOutcome::DeferToEngine : LocalScreenOutcome::Reject;
    }
    // Limit（earlyDepth以内では確実に詰まない）＝真の長手数候補！外部エンジンで判定する
    return LocalScreenOutcome::DeferToEngine;
}
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
{
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
        teardownOracle();
    }
}

TsumeRetrogradeOps::Limits TsumeRetrogradeGenerator::opLimits() const
{
    TsumeRetrogradeOps::Limits lim;
    lim.maxAttackPieces = m_settings.maxAttackPieces;
    lim.maxDefendPieces = m_settings.maxDefendPieces;
    lim.maxTotalBoardPieces = m_settings.maxTotalBoardPieces;
    lim.addRemainingToDefenderHand = m_settings.addRemainingToDefenderHand;
    return lim;
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

std::uint64_t TsumeRetrogradeGenerator::hashSfen(const QString& sfen)
{
    const QByteArray digest = QCryptographicHash::hash(sfen.toUtf8(), QCryptographicHash::Sha256);
    std::uint64_t h = 0;
    for (int i = 0; i < 8; ++i) {
        h = (h << 8) | static_cast<std::uint8_t>(digest.at(i));
    }
    return h;
}

bool TsumeRetrogradeGenerator::markVisited(const QString& sfen)
{
    const std::uint64_t h = hashSfen(sfen);
    if (m_visitedHashes.find(h) != m_visitedHashes.end()) return false;
    m_visitedHashes.insert(h);
    m_visitedOrder.push_back(h);
    while (m_visitedOrder.size() > kMaxVisitedHashes) {
        m_visitedHashes.erase(m_visitedOrder.front());
        m_visitedOrder.pop_front();
    }
    return true;
}

bool TsumeRetrogradeGenerator::start(const Settings& settings)
{
    if (m_running) return false;
    m_settings = settings.normalized();
    m_stopping = false;
    m_engineReady = false;
    m_awaitingAnswer = false;
    m_hashDirty = false;
    m_consecutiveRecoveries = 0;
    m_mode = ActiveMode::Idle;
    m_activeQuerySfen.clear();

    m_frontier.clear();
    m_repairQueue.clear();
    m_evalQueue.clear();
    m_pendingCertify.clear();
    m_firstCheck.reset();
    m_activeCertifier.reset();
    m_visitedHashes.clear();
    m_visitedOrder.clear();
    m_foundSfens.clear();
    m_certifiedQueuedOrDone.clear();
    m_diversityKeys.clear();
    m_foundPositions.clear();
    m_fastCache.clear();
    m_strictCache.clear();

    m_accumulatedElapsedMs = 0;
    m_maxPliesReached = 0;
    m_generatedCandidates = 0;
    m_engineQueries = 0;
    m_certifiedRejected = 0;
    m_certifiedInconclusive = 0;
    m_repairSucceeded = 0;
    m_engineRestarts = 0;
    m_batchesSinceProgress = 0;

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
        m_rngSeedUsed = m_settings.rngSeed != 0
            ? m_settings.rngSeed
            : static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        m_rng.seed(m_rngSeedUsed);
        m_rngDraws = 0;
        initializeFreshSeeds();
    }

    // 既出の除外対象（SFEN / SFEN moves PV）を登録する（種局面・再開フロンティア構築後に適用し、既消化親種も外す）
    for (const QString& raw : std::as_const(m_settings.excludeLines)) {
        addExcludeLine(raw);
    }

    m_running = true;
    m_sessionTimer.start();
    m_progressTimer.start(kProgressIntervalMs);
    if (!m_settings.checkpointPath.isEmpty()) {
        m_checkpointTimer.start(m_settings.checkpointIntervalSec * 1000);
    }

    startOracle();
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
    const QString line = raw.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) return;
    const qsizetype movesIdx = line.indexOf(QLatin1String(" moves "));
    const QString sfenPart = movesIdx >= 0 ? line.left(movesIdx).trimmed() : line;
    const QString norm = TsumeRetrogradeOps::normalizeProblemSfen(sfenPart, m_settings.addRemainingToDefenderHand);
    if (!norm.isEmpty()) {
        m_foundSfens.insert(norm);
        m_certifiedQueuedOrDone.insert(norm);
    }
    if (movesIdx >= 0 && m_settings.excludeSimilar) {
        const QStringList pv = line.mid(movesIdx + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!norm.isEmpty() && !pv.isEmpty()) {
            registerDiversityKeys(norm, pv);
            // 採択済みの直前親種（2手進めた N-2 手局面）がフロンティアに居れば即座に外す
            if (pv.size() >= 9) {
                const auto parentSlice = TsumeRetrogradeOps::sliceToTargetLength(
                    norm, pv, static_cast<int>(pv.size()) - 2);
                if (parentSlice) {
                    auto it = m_frontier.find(static_cast<int>(pv.size()) - 2);
                    if (it != m_frontier.end()) {
                        auto& list = it->second;
                        list.erase(std::remove_if(list.begin(), list.end(),
                                                  [&](const BeamNode& n) { return n.sfen == parentSlice->sfen; }),
                                   list.end());
                        if (list.isEmpty()) m_frontier.erase(it);
                    }
                }
            }
        }
    }
}

void TsumeRetrogradeGenerator::addSeedLine(const QString& raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) return;
    const auto lim = opLimits();
    const qsizetype movesIdx = trimmed.indexOf(QLatin1String(" moves "));
    if (movesIdx >= 0) {
        const QString sfenPart = trimmed.left(movesIdx).trimmed();
        const QStringList pv = trimmed.mid(movesIdx + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString normalized = TsumeRetrogradeOps::normalizeProblemSfen(
            sfenPart, m_settings.addRemainingToDefenderHand);
        if (!normalized.isEmpty() && !pv.isEmpty()) {
            const auto metrics = TsumeRetrogradeOps::analyzeLine(normalized, pv);
            if (metrics && metrics->plies <= m_settings.targetMoves) {
                markVisited(normalized);
                BeamNode node;
                node.sfen = normalized;
                node.pv = pv;
                node.plies = metrics->plies;
                node.surplusCount = metrics->surplusCount;
                node.firstMoveUnique = true;
                node.signature = TsumeRetrogradeOps::structuralSignature(normalized, pv);
                node.score = TsumeRetrogradeOps::scoreNode(
                    *metrics, m_settings.targetMoves, 0, true, m_settings.requireNoSurplus,
                    !m_settings.allowFinalMoveAlternatives);
                addToFrontier(node);
                if (metrics->plies > m_maxPliesReached) m_maxPliesReached = metrics->plies;
                return;
            }
        }
    }
    if (TsumeRetrogradeOps::isMateEnding(trimmed)) {
        const auto reversed = TsumeRetrogradeOps::reverseFromMateEnding(trimmed, lim, 8, m_rng);
        ++m_rngDraws;
        for (const auto& cand : reversed) {
            if (markVisited(cand.sfen)) {
                m_evalQueue.push_back({cand.sfen, cand.tag, 1, false});
                ++m_generatedCandidates;
            }
        }
    } else {
        const QString sfenOnly = movesIdx >= 0 ? trimmed.left(movesIdx).trimmed() : trimmed;
        const QString normalized = TsumeRetrogradeOps::normalizeProblemSfen(
            sfenOnly, m_settings.addRemainingToDefenderHand);
        if (!normalized.isEmpty() && markVisited(normalized)) {
            m_evalQueue.push_back({normalized, QStringLiteral("seed"), 1, false});
            ++m_generatedCandidates;
        }
    }
}

void TsumeRetrogradeGenerator::initializeFreshSeeds()
{
    if (!m_settings.seedSfens.isEmpty()) {
        for (const QString& raw : std::as_const(m_settings.seedSfens)) {
            addSeedLine(raw);
        }
    }
    if (m_evalQueue.empty() && m_frontier.empty()) {
        enqueueEndingSeeds(std::max(6, m_settings.beamWidth));
    }
}

void TsumeRetrogradeGenerator::enqueueEndingSeeds(int count)
{
    const auto lim = opLimits();
    const QStringList endings = TsumeRetrogradeOps::generateMateEndings(lim, count, m_rng);
    ++m_rngDraws;
    const bool strictFinal = !m_settings.allowFinalMoveAlternatives;
    for (const QString& ending : endings) {
        const auto candidates = TsumeRetrogradeOps::reverseFromMateEnding(ending, lim, 6, m_rng);
        ++m_rngDraws;
        int added = 0;
        for (const auto& cand : candidates) {
            if (strictFinal && TsumeRetrogradeOps::countImmediateMates(cand.sfen) != 1) continue;
            if (!markVisited(cand.sfen)) continue;
            m_evalQueue.push_back({cand.sfen, cand.tag, 1, false});
            ++m_generatedCandidates;
            if (++added >= 3) break;
        }
        if (added == 0) {
            for (const auto& cand : candidates) {
                if (!markVisited(cand.sfen)) continue;
                m_evalQueue.push_back({cand.sfen, cand.tag, 1, false});
                ++m_generatedCandidates;
                if (++added >= 2) break;
            }
        }
    }
}

void TsumeRetrogradeGenerator::startOracle()
{
    teardownOracle();
    m_engineReady = false;
    m_awaitingAnswer = false;
    m_oracle = new MateOracle(this);
    m_oracle->session()->setInitializationTimeoutMs(kOracleInitTimeoutMs);
    // KomoringHeights 等の最短手数確認に必要なセッションオプションを自動適用
    auto sessionOptions = TsumeMateEngine::requiredOptions();
    if (m_settings.engineHashMb > 0) {
        sessionOptions.append({QStringLiteral("USI_Hash"), QString::number(m_settings.engineHashMb)});
    }
    if (m_settings.engineThreads > 0) {
        sessionOptions.append({QStringLiteral("Threads"), QString::number(m_settings.engineThreads)});
    }
    m_oracle->session()->handler()->setSessionOptions(sessionOptions);
    connect(m_oracle, &MateOracle::ready, this, &TsumeRetrogradeGenerator::onEngineReady);
    connect(m_oracle, &MateOracle::answered, this, &TsumeRetrogradeGenerator::onOracleAnswered);
    connect(m_oracle, &MateOracle::timedOut, this, &TsumeRetrogradeGenerator::onOracleTimedOut);
    connect(m_oracle, &MateOracle::errorOccurred, this, &TsumeRetrogradeGenerator::onOracleError);
    m_oracle->start(m_settings.enginePath, m_settings.engineName);
}

void TsumeRetrogradeGenerator::teardownOracle()
{
    if (!m_oracle) return;
    m_oracle->quit();
    disconnect(m_oracle, nullptr, this, nullptr);
    m_oracle->deleteLater();
    m_oracle = nullptr;
    m_engineReady = false;
    m_awaitingAnswer = false;
}

void TsumeRetrogradeGenerator::restartOracleForRecovery()
{
    ++m_engineRestarts;
    ++m_consecutiveRecoveries;
    if (m_consecutiveRecoveries > kMaxConsecutiveRecoveries) {
        failRun(tr("エンジンが連続して応答しないため生成を中止しました。"));
        return;
    }
    qCInfo(lcAnalysis).noquote() << "Restarting mate engine after timeout (count=" << m_engineRestarts << ")";
    startOracle();
}

void TsumeRetrogradeGenerator::onEngineReady()
{
    if (!m_running) return;
    m_engineReady = true;
    m_consecutiveRecoveries = 0;
    scheduleStep();
}

void TsumeRetrogradeGenerator::scheduleStep()
{
    if (!m_running || !m_engineReady || m_awaitingAnswer) return;
    if (!m_stepTimer.isActive()) m_stepTimer.start(0);
}

void TsumeRetrogradeGenerator::onStepTimer()
{
    driveNext();
}

void TsumeRetrogradeGenerator::driveNext()
{
    if (!m_running || !m_engineReady || m_awaitingAnswer) return;

    // 目標件数に達していれば完了
    if (m_settings.maxPositionsToFind > 0
        && static_cast<int>(m_foundPositions.size()) >= m_settings.maxPositionsToFind) {
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
    while (!m_pendingCertify.empty()) {
        const PendingCertifyItem item = m_pendingCertify.front();
        m_pendingCertify.pop_front();
        if (m_foundSfens.contains(item.sfen)) continue;
        m_activeCertifier = std::make_unique<TsumeStrictCertifier>(item.sfen, certifierSettings());
        m_activeCertifierAllowTrim = item.allowTrimOnReject;
        m_activeCertifier->seedVerifiedCache(m_strictCache);
        m_mode = ActiveMode::Certifying;
        if (m_hashDirty && m_oracle && m_oracle->session() && m_oracle->session()->handler()) {
            // 成長フェーズの時間付き探索の置換表残骸を持ち越さないようリセットする
            m_oracle->session()->handler()->sendUsiNewGame();
            m_hashDirty = false;
        }
        if (!m_settings.checkpointPath.isEmpty()) saveCheckpointNow();
        stepCertifier();
        return;
    }

    // 3. 進行中の軽量初手余詰チェックがあれば続行
    if (m_firstCheck) {
        m_mode = ActiveMode::FirstMoveCheck;
        stepFirstMoveCheck();
        return;
    }

    // 4. 評価待ち候補がなければ次のバッチを展開
    if (m_evalQueue.empty()) {
        expandNextBatch();
        if (m_evalQueue.empty()) {
            enqueueEndingSeeds(std::max(4, m_settings.beamWidth));
        }
        if (m_evalQueue.empty()) {
            failRun(tr("候補局面を生成できませんでした。駒数上限などの設定を確認してください。"));
            return;
        }
    }

    // 5. 評価キューから候補を取り出して評価（キャッシュ／内蔵 Hayanagi プレスクリーニング優先）
    QElapsedTimer sliceTimer;
    sliceTimer.start();
    int localProcessed = 0;
    while (!m_evalQueue.empty()) {
        m_activeEval = m_evalQueue.front();
        m_evalQueue.pop_front();
        const auto cached = m_fastCache.find(m_activeEval.sfen.toStdString());
        if (cached != m_fastCache.end()) {
            m_mode = ActiveMode::EvalCandidate;
            submitEvalAnswer(cached->second);
            if (!m_running || m_awaitingAnswer) return;
            if (m_activeCertifier || !m_pendingCertify.empty() || m_firstCheck) {
                scheduleStep();
                return;
            }
            if (++localProcessed >= 48 || sliceTimer.elapsed() >= 40) {
                scheduleStep();
                return;
            }
            continue;
        }

        MateAnswer localAnswer;
        const LocalScreenOutcome screen = localScreenCandidate(
            m_activeEval.sfen, m_settings.targetMoves, m_activeEval.minDesiredPlies,
            m_maxPliesReached, m_activeEval.fromRepair, m_settings.stepTimeoutMs, m_rng, &localAnswer);
        ++m_rngDraws;
        if (screen == LocalScreenOutcome::Reject) {
            if (++localProcessed >= 48 || sliceTimer.elapsed() >= 40) {
                scheduleStep();
                return;
            }
            continue;
        }
        if (screen == LocalScreenOutcome::Solved) {
            pruneFastCacheIfNeeded();
            m_fastCache.emplace(m_activeEval.sfen.toStdString(), localAnswer);
            m_mode = ActiveMode::EvalCandidate;
            submitEvalAnswer(localAnswer);
            if (!m_running || m_awaitingAnswer) return;
            if (m_activeCertifier || !m_pendingCertify.empty() || m_firstCheck) {
                scheduleStep();
                return;
            }
            if (++localProcessed >= 48 || sliceTimer.elapsed() >= 40) {
                scheduleStep();
                return;
            }
            continue;
        }

        m_mode = ActiveMode::EvalCandidate;
        askEngine(m_activeEval.sfen, m_settings.stepTimeoutMs, false);
        return;
    }
    scheduleStep();
}

void TsumeRetrogradeGenerator::askEngine(const QString& sfen, int limitMs, bool strictInfinite)
{
    const std::optional<UsiPosition> pos = UsiPosition::parse(sfen);
    if (!pos || !m_oracle || !m_engineReady) {
        handleQueryUnknownOrTimeout(false);
        return;
    }
    m_activeQuerySfen = sfen;
    m_lastQueryWasStrict = strictInfinite;
    if (!strictInfinite) m_hashDirty = true;
    m_awaitingAnswer = true;
    ++m_engineQueries;
    m_queryTimer.start();

    const bool ok = strictInfinite
        ? m_oracle->askWithin(*pos, std::max(1, limitMs))
        : m_oracle->ask(*pos, std::max(1, limitMs));
    if (!ok) {
        m_awaitingAnswer = false;
        handleQueryUnknownOrTimeout(false);
    }
}

void TsumeRetrogradeGenerator::onOracleAnswered(const MateAnswer& answer)
{
    if (!m_running || !m_awaitingAnswer) return;
    m_awaitingAnswer = false;
    m_consecutiveRecoveries = 0;
    const qint64 elapsedMs = m_queryTimer.elapsed();

    if (answer.kind == MateAnswer::Kind::Unsupported) {
        failRun(tr("選択されたエンジンは詰み探索（go mate）に対応していません。"));
        return;
    }

    // 確定応答を適切なキャッシュへ保存
    if (answer.kind == MateAnswer::Kind::Mate || answer.kind == MateAnswer::Kind::NoMate) {
        if (m_lastQueryWasStrict) {
            if (m_strictCache.size() < kMaxStrictCacheEntries) {
                const auto reply = answer.kind == MateAnswer::Kind::Mate
                    ? TsumeshogiVerifier::Reply::Mate : TsumeshogiVerifier::Reply::NoMate;
                m_strictCache.emplace(m_activeQuerySfen.toStdString(),
                                      TsumeshogiVerifier::CacheEntry{reply, static_cast<int>(answer.pv.size())});
            }
        }
        pruneFastCacheIfNeeded();
        m_fastCache.emplace(m_activeQuerySfen.toStdString(), answer);
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

void TsumeRetrogradeGenerator::onOracleTimedOut()
{
    if (!m_running) return;
    m_awaitingAnswer = false;
    handleQueryUnknownOrTimeout(true);
}

void TsumeRetrogradeGenerator::handleQueryUnknownOrTimeout(bool dueToTimeout)
{
    const qint64 elapsedMs = m_queryTimer.isValid() ? m_queryTimer.elapsed() : 0;
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
        restartOracleForRecovery();
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

// ============================================================
// 展開・ビームサーチ（案A/B）
// ============================================================

void TsumeRetrogradeGenerator::expandNextBatch()
{
    const auto lim = opLimits();
    std::uniform_int_distribution<int> pct(0, 99);
    const int roll = pct(m_rng);
    ++m_rngDraws;

    // 1. 余詰リペアキューからの展開（キューがあれば約15%の確率）
    if (!m_repairQueue.empty() && (roll < 15 || m_frontier.empty())) {
        // 手数が最も長く試行回数が少ないリペア候補を優先
        auto bestIt = m_repairQueue.begin();
        for (auto it = m_repairQueue.begin(); it != m_repairQueue.end(); ++it) {
            const int scoreIt = it->plies * 10 - it->attempts * 15;
            const int scoreBest = bestIt->plies * 10 - bestIt->attempts * 15;
            if (scoreIt > scoreBest) bestIt = it;
        }
        RepairItem item = *bestIt;
        m_repairQueue.erase(bestIt);
        ++item.attempts;
        if (item.attempts < 2) m_repairQueue.push_back(item);

        const int repairBranch = std::min(m_settings.branchFactor, 16);
        const auto repairs = TsumeRetrogradeOps::generateCookRepairs(
            item.sfen, item.mainMove, item.cookMove, lim, repairBranch, m_rng);
        ++m_rngDraws;
        for (const auto& cand : repairs) {
            if (!markVisited(cand.sfen)) continue;
            m_evalQueue.push_back({cand.sfen, cand.tag, item.plies, true});
            ++m_generatedCandidates;
        }
        return;
    }

    // 2. フロンティアが空、またはシード未指定のボトムアップ探索で停滞したら新しい0手詰み局面を注入
    if (m_frontier.empty()
        || (m_settings.seedSfens.isEmpty() && (roll >= 92 || m_batchesSinceProgress >= 25))) {
        m_batchesSinceProgress = 0;
        enqueueEndingSeeds(std::max(4, m_settings.beamWidth / 2));
        if (!m_evalQueue.empty()) return;
    }

    // 3. フロンティアから親ノードを選択
    // 約65%は上位手数帯（最大手数から4手以内）のベストスコア、約35%は全層からバックトラック選択
    BeamNode* chosen = nullptr;
    int chosenLayer = 0;
    const bool focusTop = (roll < 75);

    if (focusTop) {
        double bestScore = -1e18;
        int inspectedLayers = 0;
        std::uniform_real_distribution<double> jitter(0.0, 28.0);
        for (auto rit = m_frontier.rbegin(); rit != m_frontier.rend() && inspectedLayers < 3; ++rit, ++inspectedLayers) {
            for (BeamNode& node : rit->second) {
                // 1つの親種・同じ上位種に複数ワーカーが集中しないよう、展開ペナルティ＋乱数ジッターで分散巡回する
                const double effective = node.score - static_cast<double>(node.expansions) * 45.0 + jitter(m_rng);
                ++m_rngDraws;
                if (effective > bestScore) {
                    bestScore = effective;
                    chosen = &node;
                    chosenLayer = rit->first;
                }
            }
        }
    } else {
        std::vector<std::pair<int, int>> allRefs;
        for (const auto& [plies, list] : m_frontier) {
            for (int i = 0; i < list.size(); ++i) allRefs.push_back({plies, i});
        }
        if (!allRefs.empty()) {
            // 複数ランダムサンプルから最高スコアを選ぶトーナメント選択
            double bestScore = -1e18;
            std::uniform_int_distribution<std::size_t> pick(0, allRefs.size() - 1);
            for (int t = 0; t < 6; ++t) {
                const auto [plies, idx] = allRefs[pick(m_rng)];
                ++m_rngDraws;
                BeamNode& cand = m_frontier[plies][idx];
                const double effective = cand.score - static_cast<double>(cand.expansions) * 40.0;
                if (effective > bestScore) {
                    bestScore = effective;
                    chosen = &cand;
                    chosenLayer = plies;
                }
            }
        }
    }

    if (!chosen) return;
    ++chosen->expansions;
    // 展開回数が増えたのでノード自体のスコアを再計算して過集中を防ぐ
    if (const auto metrics = TsumeRetrogradeOps::analyzeLine(chosen->sfen, chosen->pv)) {
        chosen->score = TsumeRetrogradeOps::scoreNode(*metrics, m_settings.targetMoves, chosen->expansions,
                                                      chosen->firstMoveUnique, m_settings.requireNoSurplus,
                                                      !m_settings.allowFinalMoveAlternatives);
    }

    // 同手数シードの展開時は逆算して切り出しても元手順に戻るため 100% 盤面変異に集中し、短いシードは逆算を主軸にする
    const int retroBudget = (chosenLayer >= m_settings.targetMoves)
        ? 0
        : std::max(4, (m_settings.branchFactor * 2) / 3);
    const int mutateBudget = std::max(2, m_settings.branchFactor - retroBudget);

    // 逆算候補（2手伸ばしを狙う）
    if (retroBudget > 0) {
        const auto predecessors = TsumeRetrogradeOps::generatePredecessors(chosen->sfen, lim, retroBudget, m_rng);
        ++m_rngDraws;
        for (const auto& cand : predecessors) {
            if (!markVisited(cand.sfen)) continue;
            m_evalQueue.push_back({cand.sfen, cand.tag, chosenLayer + 2, false});
            ++m_generatedCandidates;
        }
    }

    // 盤面編集候補（駒の移動・追加・変更で新しい手筋や延長を生む）
    const auto mutations = TsumeRetrogradeOps::generateMutations(chosen->sfen, lim, mutateBudget, m_rng);
    ++m_rngDraws;
    for (const auto& cand : mutations) {
        if (!markVisited(cand.sfen)) continue;
        m_evalQueue.push_back({cand.sfen, cand.tag, chosenLayer, false});
        ++m_generatedCandidates;
    }

    ++m_batchesSinceProgress;
}

void TsumeRetrogradeGenerator::submitEvalAnswer(const MateAnswer& answer)
{
    if (answer.kind != MateAnswer::Kind::Mate || answer.pv.isEmpty()) return;

    const auto metrics = TsumeRetrogradeOps::analyzeLine(m_activeEval.sfen, answer.pv);
    if (!metrics) return;

    const int plies = metrics->plies;
    if (plies > m_maxPliesReached) {
        m_maxPliesReached = std::min(plies, m_settings.targetMoves);
        m_batchesSinceProgress = 0;
        if (!m_settings.checkpointPath.isEmpty()) saveCheckpointNow();
    }

    // 目標手数以上に到達した場合
    if (plies >= m_settings.targetMoves) {
        QString targetSfen = m_activeEval.sfen;
        QStringList targetPv = answer.pv;
        if (plies > m_settings.targetMoves) {
            const auto sliced = TsumeRetrogradeOps::sliceToTargetLength(
                m_activeEval.sfen, answer.pv, m_settings.targetMoves);
            if (!sliced) return;
            targetSfen = sliced->sfen;
            targetPv = sliced->pv;
        }
        const auto targetMetrics = TsumeRetrogradeOps::analyzeLine(targetSfen, targetPv);
        if (!targetMetrics) return;

        // 駒余りや最終手複数解の有無を先に判定
        const bool failsSurplus = m_settings.requireNoSurplus && targetMetrics->surplusCount > 0;
        const bool failsFinalAlt = !m_settings.allowFinalMoveAlternatives && targetMetrics->finalImmediateMates > 1;

        // 既出・類似作は採択・厳密証明しないが、類似作で駒余りなし・最終手一意なら
        // 次の1箇所変異で別系統へ飛ぶための探索足場（ステッピングストーン）としてフロンティアへ残す
        if (m_foundSfens.contains(targetSfen) || m_certifiedQueuedOrDone.contains(targetSfen)) {
            return;
        }
        if (isSimilarToAccepted(targetSfen, targetPv)) {
            if (!failsSurplus && !failsFinalAlt) {
                BeamNode stepping;
                stepping.sfen = targetSfen;
                stepping.pv = targetPv;
                stepping.plies = targetMetrics->plies;
                stepping.surplusCount = targetMetrics->surplusCount;
                stepping.firstMoveUnique = true;
                stepping.signature = TsumeRetrogradeOps::structuralSignature(targetSfen, targetPv);
                stepping.score = TsumeRetrogradeOps::scoreNode(
                    *targetMetrics, m_settings.targetMoves, 0, true, m_settings.requireNoSurplus,
                    !m_settings.allowFinalMoveAlternatives) - 15.0;
                addToFrontier(stepping);
            }
            return;
        }

        // 駒余りや最終手複数解が既に判明している場合 → 1段目のみ1枚除去で不要駒落としを試みる
        if (failsSurplus || failsFinalAlt) {
            if (!m_activeEval.fromRepair) {
                int added = 0;
                for (const QString& trimmed : TsumeCandidateJob::onePieceRemovedPositions(targetSfen)) {
                    if (markVisited(trimmed)) {
                        m_evalQueue.push_front({trimmed, QStringLiteral("preflight_trim"), m_settings.targetMoves, true});
                        ++m_generatedCandidates;
                        if (++added >= 4) break;
                    }
                }
            }
            return;
        }

        // 61手以下なら、外部エンジンの厳密証明へ送る前に内蔵 Hayanagi で主手順上の明白な余詰を事前弾き
        const QuickMainLineCook preCook = quickLocalMainLineCookCheck(targetSfen, targetPv, 2);
        if (!preCook.cookMove.isEmpty()) {
            if (preCook.ply == 0) {
                addToRepairQueue(targetSfen, preCook.mainMove, preCook.cookMove, m_settings.targetMoves);
            }
            if (!m_activeEval.fromRepair) {
                int added = 0;
                for (const QString& trimmed : TsumeCandidateJob::onePieceRemovedPositions(targetSfen)) {
                    if (markVisited(trimmed)) {
                        m_evalQueue.push_front({trimmed, QStringLiteral("precook_trim"), m_settings.targetMoves, true});
                        ++m_generatedCandidates;
                        if (++added >= 2) break;
                    }
                }
            }
            return;
        }

        enqueueForCertification(targetSfen, targetPv, !m_activeEval.fromRepair);
        return;
    }

    // 目標手数未満：この手数層のビームに入る見込みがあれば軽量初手余詰チェックを行う
    const auto& layer = m_frontier[plies];
    const double optimisticScore = TsumeRetrogradeOps::scoreNode(
        *metrics, m_settings.targetMoves, 0, true, m_settings.requireNoSurplus,
        !m_settings.allowFinalMoveAlternatives);
    if (layer.size() >= m_settings.beamWidth && optimisticScore <= layer.last().score) {
        return;
    }

    // 駒余りが多すぎる局面は厳密モードでは優先度を大きく落とす（ただし1枚除去で解消を試せる）
    if (m_settings.requireNoSurplus && metrics->surplusCount >= 3) return;

    startFirstMoveCheck(m_activeEval.sfen, answer.pv, *metrics, m_activeEval.fromRepair);
}

// ============================================================
// 成長中の軽量初手余詰チェック
// ============================================================

void TsumeRetrogradeGenerator::startFirstMoveCheck(const QString& sfen, const QStringList& pv,
                                                   const TsumeRetrogradeOps::Metrics& metrics,
                                                   bool fromRepair)
{
    // 63手以下ならまずローカル Hayanagi で瞬時に明白な初手余詰を検出
    const QString quickCook = quickLocalCookCheck(sfen, pv.first(), metrics.plies,
                                                  metrics.plies <= 15 ? 6 : 4);
    if (!quickCook.isEmpty()) {
        m_firstCheck = FirstMoveCheckState{sfen, pv, metrics, fromRepair, {}, 0, {}, 0, false, 0, 0, {}};
        finishFirstMoveCheck(false, quickCook);
        return;
    }

    // 15手以下の成長足場はローカル Hayanagi の初手余詰検査だけで十分（外部エンジン問い合わせを温存）
    if (metrics.plies <= 15) {
        m_firstCheck = FirstMoveCheckState{sfen, pv, metrics, fromRepair, {}, 0, {}, 0, false, 0, 0, {}};
        finishFirstMoveCheck(true, QString());
        return;
    }

    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return;

    FirstMoveCheckState st;
    st.sfen = sfen;
    st.mainPv = pv;
    st.metrics = metrics;
    st.fromRepair = fromRepair;
    st.maxQueries = std::clamp(24 + metrics.plies / 2, 24, 80);

    for (const auto& move : pos.generate_checking_moves()) {
        const QString usi = QString::fromStdString(pos.move_to_usi(move));
        if (usi == pv.first()) continue;
        shogi::Position child = pos;
        child.do_move(move);
        if (!child.has_legal_move()) {
            // 別の即詰がある＝即座に初手余詰確定
            m_firstCheck = std::move(st);
            finishFirstMoveCheck(false, usi);
            return;
        }
        st.altMoves.append(usi);
    }

    m_firstCheck = std::move(st);
    m_mode = ActiveMode::FirstMoveCheck;
    scheduleStep();
}

void TsumeRetrogradeGenerator::stepFirstMoveCheck()
{
    while (m_firstCheck) {
        auto& st = *m_firstCheck;
        if (st.altMoveIndex >= st.altMoves.size()) {
            finishFirstMoveCheck(!st.altUnresolved, QString());
            return;
        }

        // 現在の別攻手の応手リストがまだ無ければ展開する
        if (st.defenses.isEmpty() && st.defenseIndex == 0) {
            shogi::Position pos;
            if (!pos.set_sfen(st.sfen.toStdString(), true)
                || !pos.apply_usi_move(st.altMoves.at(st.altMoveIndex).toStdString())) {
                ++st.altMoveIndex;
                continue;
            }
            const auto legals = pos.generate_legal_moves();
            std::vector<std::pair<int, QString>> ordered;
            ordered.reserve(legals.size());
            for (const auto& def : legals) {
                shogi::Position after = pos;
                after.do_move(def);
                // 1手詰の応手は反駁にならないので外部に聞く必要がない
                if (hasLocalImmediateMate(after)) continue;
                ordered.push_back({defensePriority(pos, def), QString::fromStdString(after.to_sfen())});
            }
            std::stable_sort(ordered.begin(), ordered.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& item : ordered) st.defenses.append(item.second);
            if (st.defenses.isEmpty()) {
                // 全応手が1手詰＝確実に詰む別初手（初手余詰！）
                const QString cook = st.altMoves.at(st.altMoveIndex);
                finishFirstMoveCheck(false, cook);
                return;
            }
        }

        if (st.defenseIndex >= st.defenses.size()) {
            if (!st.altUnresolved) {
                // 全応手が詰むことが確認できた＝初手余詰確定
                const QString cook = st.altMoves.at(st.altMoveIndex);
                finishFirstMoveCheck(false, cook);
                return;
            }
            // 未解決が残ったので次へ
            st.defenses.clear();
            st.defenseIndex = 0;
            ++st.altMoveIndex;
            continue;
        }

        const QString defSfen = st.defenses.at(st.defenseIndex);
        const auto cached = m_fastCache.find(defSfen.toStdString());
        if (cached != m_fastCache.end()) {
            submitFirstMoveCheckAnswer(cached->second);
            continue;
        }

        if (st.queriesUsed >= st.maxQueries) {
            // 成長段階の軽量チェック上限に達したら未確定としてビームに委ねる
            finishFirstMoveCheck(false, QString());
            return;
        }

        ++st.queriesUsed;
        st.activeQuerySfen = defSfen;
        // 反駁判定なので短めの予算で十分（最大 stepTimeoutMs / 2, 下限 200ms）
        const int limit = std::clamp(m_settings.stepTimeoutMs / 2, 200, m_settings.stepTimeoutMs);
        askEngine(defSfen, limit, false);
        return;
    }
}

void TsumeRetrogradeGenerator::submitFirstMoveCheckAnswer(const MateAnswer& answer)
{
    if (!m_firstCheck) return;
    auto& st = *m_firstCheck;
    if (answer.kind == MateAnswer::Kind::NoMate) {
        // この別攻手には逃れがある（不詰確定）→ 次の別攻手へ
        st.defenses.clear();
        st.defenseIndex = 0;
        ++st.altMoveIndex;
        return;
    }
    if (answer.kind == MateAnswer::Kind::Unknown) {
        st.altUnresolved = true;
    }
    ++st.defenseIndex;
}

void TsumeRetrogradeGenerator::finishFirstMoveCheck(bool unique, const QString& cookMove)
{
    if (!m_firstCheck) return;
    const FirstMoveCheckState st = std::move(*m_firstCheck);
    m_firstCheck.reset();

    if (!cookMove.isEmpty()) {
        // 余詰が見つかった → 目標付近（target-2 以上）または最大到達層なら標的型リペアキューに登録
        if (st.metrics.plies >= std::max(1, m_settings.targetMoves - 2)
            || st.metrics.plies >= m_maxPliesReached) {
            addToRepairQueue(st.sfen, st.mainPv.first(), cookMove, st.metrics.plies);
        }
        // 盤面の足場として低めのスコアで残すことは許容する（特にまだビームが空いているとき）
        BeamNode node;
        node.sfen = st.sfen;
        node.pv = st.mainPv;
        node.plies = st.metrics.plies;
        node.surplusCount = st.metrics.surplusCount;
        node.firstMoveUnique = false;
        node.signature = TsumeRetrogradeOps::structuralSignature(st.sfen, st.mainPv);
        node.score = TsumeRetrogradeOps::scoreNode(st.metrics, m_settings.targetMoves, 0, false,
                                                   m_settings.requireNoSurplus,
                                                   !m_settings.allowFinalMoveAlternatives) - 18.0;
        addToFrontier(node);
        scheduleStep();
        return;
    }

    if (unique && st.fromRepair) ++m_repairSucceeded;

    BeamNode node;
    node.sfen = st.sfen;
    node.pv = st.mainPv;
    node.plies = st.metrics.plies;
    node.surplusCount = st.metrics.surplusCount;
    node.firstMoveUnique = unique;
    node.signature = TsumeRetrogradeOps::structuralSignature(st.sfen, st.mainPv);
    node.score = TsumeRetrogradeOps::scoreNode(st.metrics, m_settings.targetMoves, 0, unique,
                                               m_settings.requireNoSurplus,
                                               !m_settings.allowFinalMoveAlternatives);
    addToFrontier(node);
    scheduleStep();
}

void TsumeRetrogradeGenerator::addToFrontier(const BeamNode& node)
{
    auto& layer = m_frontier[node.plies];
    for (const BeamNode& existing : layer) {
        if (existing.sfen == node.sfen) return;
    }

    // 同一系統（signature）の過集中を防ぐ（1系統最大2個）
    int sameSigCount = 0;
    int worstSameSigIdx = -1;
    for (int i = 0; i < layer.size(); ++i) {
        if (layer.at(i).signature == node.signature) {
            ++sameSigCount;
            if (worstSameSigIdx < 0 || layer.at(i).score < layer.at(worstSameSigIdx).score) {
                worstSameSigIdx = i;
            }
        }
    }
    if (sameSigCount >= 2 && worstSameSigIdx >= 0) {
        if (node.score <= layer.at(worstSameSigIdx).score) return;
        layer[worstSameSigIdx] = node;
    } else {
        layer.append(node);
    }

    std::sort(layer.begin(), layer.end(),
              [](const BeamNode& a, const BeamNode& b) { return a.score > b.score; });
    while (layer.size() > m_settings.beamWidth) layer.removeLast();
}

void TsumeRetrogradeGenerator::addToRepairQueue(const QString& sfen, const QString& mainMove,
                                                const QString& cookMove, int plies)
{
    if (cookMove.isEmpty()) return;
    for (const RepairItem& existing : m_repairQueue) {
        if (existing.sfen == sfen && existing.cookMove == cookMove) return;
    }
    m_repairQueue.push_back(RepairItem{sfen, mainMove, cookMove, plies, 0});
    while (m_repairQueue.size() > kMaxRepairQueue) {
        // 最も手数が短いものから落とす
        auto worst = std::min_element(m_repairQueue.begin(), m_repairQueue.end(),
                                      [](const RepairItem& a, const RepairItem& b) {
                                          return a.plies < b.plies;
                                      });
        m_repairQueue.erase(worst);
    }
}

bool TsumeRetrogradeGenerator::isSimilarToAccepted(const QString& sfen, const QStringList& pv) const
{
    if (!m_settings.excludeSimilar || pv.isEmpty()) return false;
    for (const QString& key : TsumeRetrogradeOps::diversityKeys(sfen, pv)) {
        if (m_diversityKeys.contains(key)) return true;
    }
    return false;
}

void TsumeRetrogradeGenerator::registerDiversityKeys(const QString& sfen, const QStringList& pv)
{
    if (!m_settings.excludeSimilar || pv.isEmpty()) return;
    for (const QString& key : TsumeRetrogradeOps::diversityKeys(sfen, pv)) {
        m_diversityKeys.insert(key);
    }
}

void TsumeRetrogradeGenerator::enqueueForCertification(const QString& sfen, const QStringList& pv,
                                                       bool allowTrimOnReject)
{
    if (m_foundSfens.contains(sfen) || m_certifiedQueuedOrDone.contains(sfen)) return;
    if (isSimilarToAccepted(sfen, pv)) return;
    m_certifiedQueuedOrDone.insert(sfen);
    m_pendingCertify.push_back(PendingCertifyItem{sfen, allowTrimOnReject});
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
        if (!m_foundSfens.contains(out.sfen) && !isSimilarToAccepted(out.sfen, out.pv)) {
            m_foundSfens.insert(out.sfen);
            registerDiversityKeys(out.sfen, out.pv);
            m_foundPositions.append(FoundPosition{out.sfen, out.pv, out.removedPieces, out.queries, out.spentMs});
            emit positionFound(out.sfen, out.pv, out.removedPieces, out.queries);

            // 類似作除外が有効なとき、採択作の直前親種（2手進めた N-2 手局面）は
            // 再び2手逆算しても同一 core で類似除外されるため、フロンティアから外して次の別系統種へ移る
            if (m_settings.excludeSimilar && out.pv.size() >= 9) {
                const auto parentSlice = TsumeRetrogradeOps::sliceToTargetLength(
                    out.sfen, out.pv, static_cast<int>(out.pv.size()) - 2);
                if (parentSlice) {
                    auto it = m_frontier.find(static_cast<int>(out.pv.size()) - 2);
                    if (it != m_frontier.end()) {
                        auto& list = it->second;
                        list.erase(std::remove_if(list.begin(), list.end(),
                                                  [&](const BeamNode& n) { return n.sfen == parentSlice->sfen; }),
                                   list.end());
                        if (list.isEmpty()) m_frontier.erase(it);
                    }
                }
            }
            if (!m_settings.checkpointPath.isEmpty()) saveCheckpointNow();
        }
    } else {
        ++m_certifiedRejected;
        if (cert->rejectionReason() == TsumeStrictCertifier::RejectionReason::Inconclusive) {
            ++m_certifiedInconclusive;
        }
        // 初手余詰で棄却された場合はリペアキューに戻して修復を試みる
        if (cert->rejectionReason() == TsumeStrictCertifier::RejectionReason::Multiple
            && cert->cookInfo() && cert->cookInfo()->ply == 0) {
            addToRepairQueue(cert->candidateSfen(), cert->cookInfo()->mainMove,
                             cert->cookInfo()->cookMove, m_settings.targetMoves);
        }
        // 余詰・駒余りで棄却された目標手数候補は、1段目に限り不要な1枚を落として再試行する（再帰増殖は防止）
        if (m_activeCertifierAllowTrim
            && (cert->rejectionReason() == TsumeStrictCertifier::RejectionReason::Multiple
                || cert->rejectionReason() == TsumeStrictCertifier::RejectionReason::Surplus)) {
            int added = 0;
            for (const QString& trimmed : TsumeCandidateJob::onePieceRemovedPositions(cert->candidateSfen())) {
                if (markVisited(trimmed)) {
                    m_evalQueue.push_front({trimmed, QStringLiteral("cert_trim_repair"), m_settings.targetMoves, true});
                    ++m_generatedCandidates;
                    if (++added >= 2) break;
                }
            }
        }
    }
    scheduleStep();
}

// ============================================================
// 進捗・チェックポイント保存／復元・終了
// ============================================================

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
    p.maxPliesReached = m_maxPliesReached;
    int fCount = 0;
    for (const auto& [plies, list] : m_frontier) fCount += static_cast<int>(list.size());
    p.frontierNodes = fCount;
    p.repairQueueSize = static_cast<int>(m_repairQueue.size());
    p.pendingCertifySize = static_cast<int>(m_pendingCertify.size());
    p.generatedCandidates = m_generatedCandidates;
    p.engineQueries = m_engineQueries;
    p.found = static_cast<int>(m_foundPositions.size());
    p.certifiedRejected = m_certifiedRejected;
    p.certifiedInconclusive = m_certifiedInconclusive;
    p.repairSucceeded = m_repairSucceeded;
    p.engineRestarts = m_engineRestarts;
    p.elapsedMs = m_accumulatedElapsedMs + (m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0);
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
    QJsonObject root;
    root[QStringLiteral("checkpoint_version")] = kCheckpointVersion;

    QJsonObject cfg;
    cfg[QStringLiteral("target_moves")] = m_settings.targetMoves;
    cfg[QStringLiteral("max_positions")] = m_settings.maxPositionsToFind;
    cfg[QStringLiteral("beam_width")] = m_settings.beamWidth;
    cfg[QStringLiteral("branch_factor")] = m_settings.branchFactor;
    cfg[QStringLiteral("max_attack_pieces")] = m_settings.maxAttackPieces;
    cfg[QStringLiteral("max_defend_pieces")] = m_settings.maxDefendPieces;
    cfg[QStringLiteral("max_total_board_pieces")] = m_settings.maxTotalBoardPieces;
    cfg[QStringLiteral("add_remaining_to_defender_hand")] = m_settings.addRemainingToDefenderHand;
    cfg[QStringLiteral("allow_final_move_alternatives")] = m_settings.allowFinalMoveAlternatives;
    cfg[QStringLiteral("require_no_surplus")] = m_settings.requireNoSurplus;
    cfg[QStringLiteral("strict_min_length")] = m_settings.strictMinLength;
    root[QStringLiteral("settings")] = cfg;

    QJsonObject rngObj;
    rngObj[QStringLiteral("seed")] = QString::number(m_rngSeedUsed);
    rngObj[QStringLiteral("draws")] = static_cast<double>(m_rngDraws);
    root[QStringLiteral("rng")] = rngObj;

    QJsonObject st;
    st[QStringLiteral("elapsed_ms")] = static_cast<double>(
        m_accumulatedElapsedMs + (m_sessionTimer.isValid() ? m_sessionTimer.elapsed() : 0));
    st[QStringLiteral("max_plies_reached")] = m_maxPliesReached;
    st[QStringLiteral("generated_candidates")] = static_cast<double>(m_generatedCandidates);
    st[QStringLiteral("engine_queries")] = static_cast<double>(m_engineQueries);
    st[QStringLiteral("certified_rejected")] = m_certifiedRejected;
    st[QStringLiteral("certified_inconclusive")] = m_certifiedInconclusive;
    st[QStringLiteral("repair_succeeded")] = m_repairSucceeded;
    st[QStringLiteral("engine_restarts")] = m_engineRestarts;
    st[QStringLiteral("batches_since_progress")] = m_batchesSinceProgress;
    root[QStringLiteral("stats")] = st;

    QJsonArray frontierArr;
    for (const auto& [plies, list] : m_frontier) {
        for (const BeamNode& node : list) {
            QJsonObject n;
            n[QStringLiteral("sfen")] = node.sfen;
            n[QStringLiteral("pv")] = QJsonArray::fromStringList(node.pv);
            n[QStringLiteral("plies")] = node.plies;
            n[QStringLiteral("surplus")] = node.surplusCount;
            n[QStringLiteral("score")] = node.score;
            n[QStringLiteral("expansions")] = node.expansions;
            n[QStringLiteral("first_unique")] = node.firstMoveUnique;
            n[QStringLiteral("sig")] = node.signature;
            frontierArr.append(n);
        }
    }
    root[QStringLiteral("frontier")] = frontierArr;

    QJsonArray repairArr;
    for (const RepairItem& item : m_repairQueue) {
        QJsonObject r;
        r[QStringLiteral("sfen")] = item.sfen;
        r[QStringLiteral("main_move")] = item.mainMove;
        r[QStringLiteral("cook_move")] = item.cookMove;
        r[QStringLiteral("plies")] = item.plies;
        r[QStringLiteral("attempts")] = item.attempts;
        repairArr.append(r);
    }
    root[QStringLiteral("repair_queue")] = repairArr;

    QJsonArray pendingArr;
    for (const PendingCertifyItem& item : m_pendingCertify) pendingArr.append(item.sfen);
    root[QStringLiteral("pending_certify")] = pendingArr;

    QJsonArray evalArr;
    // 中断時に評価中・初手検査中だった候補を取りこぼさないよう先頭に含める
    if (m_firstCheck && !m_firstCheck->sfen.isEmpty()) {
        QJsonObject eo;
        eo[QStringLiteral("sfen")] = m_firstCheck->sfen;
        eo[QStringLiteral("tag")] = QStringLiteral("resumed_first_check");
        eo[QStringLiteral("min_plies")] = m_firstCheck->metrics.plies;
        eo[QStringLiteral("from_repair")] = m_firstCheck->fromRepair;
        evalArr.append(eo);
    } else if (m_awaitingAnswer && m_mode == ActiveMode::EvalCandidate && !m_activeEval.sfen.isEmpty()) {
        QJsonObject eo;
        eo[QStringLiteral("sfen")] = m_activeEval.sfen;
        eo[QStringLiteral("tag")] = m_activeEval.tag;
        eo[QStringLiteral("min_plies")] = m_activeEval.minDesiredPlies;
        eo[QStringLiteral("from_repair")] = m_activeEval.fromRepair;
        evalArr.append(eo);
    }
    const std::size_t evalCap = std::min<std::size_t>(m_evalQueue.size(), 256);
    for (std::size_t i = 0; i < evalCap; ++i) {
        const EvalItem& e = m_evalQueue[i];
        QJsonObject eo;
        eo[QStringLiteral("sfen")] = e.sfen;
        eo[QStringLiteral("tag")] = e.tag;
        eo[QStringLiteral("min_plies")] = e.minDesiredPlies;
        eo[QStringLiteral("from_repair")] = e.fromRepair;
        evalArr.append(eo);
    }
    root[QStringLiteral("eval_queue")] = evalArr;

    if (m_activeCertifier && !m_activeCertifier->isDone()) {
        root[QStringLiteral("active_certifier")] = m_activeCertifier->toJson();
    }

    QJsonArray foundArr;
    for (const FoundPosition& fp : m_foundPositions) {
        QJsonObject fo;
        fo[QStringLiteral("sfen")] = fp.sfen;
        fo[QStringLiteral("pv")] = QJsonArray::fromStringList(fp.pv);
        fo[QStringLiteral("removed_pieces")] = fp.removedPieces;
        fo[QStringLiteral("verify_queries")] = fp.verifyQueries;
        fo[QStringLiteral("verify_ms")] = static_cast<double>(fp.verifyMs);
        foundArr.append(fo);
    }
    root[QStringLiteral("found_positions")] = foundArr;

    QJsonArray certDoneArr;
    for (const QString& sfen : m_certifiedQueuedOrDone) certDoneArr.append(sfen);
    root[QStringLiteral("certified_seen")] = certDoneArr;

    // 直近の既訪問ハッシュを保存（チェックポイントサイズを抑えつつ再探索を防止）
    QJsonArray visitedArr;
    const std::size_t visitKeep = std::min<std::size_t>(m_visitedOrder.size(), 50000);
    const std::size_t startIdx = m_visitedOrder.size() - visitKeep;
    for (std::size_t i = startIdx; i < m_visitedOrder.size(); ++i) {
        visitedArr.append(QString::number(m_visitedOrder[i], 16));
    }
    root[QStringLiteral("visited_hashes")] = visitedArr;

    return root;
}

bool TsumeRetrogradeGenerator::saveCheckpointNow(QString* errorMessage)
{
    if (m_settings.checkpointPath.isEmpty()) return false;
    const QFileInfo info(m_settings.checkpointPath);
    if (!info.dir().exists() && !QDir().mkpath(info.dir().absolutePath())) {
        if (errorMessage) *errorMessage = tr("チェックポイント保存先ディレクトリを作成できませんでした。");
        return false;
    }

    QSaveFile file(m_settings.checkpointPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    const QByteArray payload = QJsonDocument(buildCheckpointJson()).toJson(QJsonDocument::Compact);
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    emit checkpointSaved(m_settings.checkpointPath);
    return true;
}

bool TsumeRetrogradeGenerator::loadCheckpointFile(const QString& path, QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = tr("チェックポイントファイルを開けません: %1").arg(file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) *errorMessage = tr("チェックポイントファイルの JSON が不正です: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("checkpoint_version")).toInt() != kCheckpointVersion) {
        if (errorMessage) *errorMessage = tr("未対応のチェックポイントバージョンです。");
        return false;
    }

    const QJsonObject cfg = root.value(QStringLiteral("settings")).toObject();
    if (cfg.value(QStringLiteral("target_moves")).toInt() != m_settings.targetMoves) {
        if (errorMessage) {
            *errorMessage = tr("チェックポイントの目標手数 (%1) が指定 (%2) と一致しません。")
                                .arg(cfg.value(QStringLiteral("target_moves")).toInt())
                                .arg(m_settings.targetMoves);
        }
        return false;
    }
    if (cfg.value(QStringLiteral("allow_final_move_alternatives")).toBool() != m_settings.allowFinalMoveAlternatives
        || cfg.value(QStringLiteral("require_no_surplus")).toBool() != m_settings.requireNoSurplus) {
        if (errorMessage) {
            *errorMessage = tr("チェックポイントの厳密検証条件（最終手許容・駒余り禁止）が指定と一致しません。");
        }
        return false;
    }

    const QJsonObject rngObj = root.value(QStringLiteral("rng")).toObject();
    bool seedOk = false;
    m_rngSeedUsed = rngObj.value(QStringLiteral("seed")).toString().toULongLong(&seedOk);
    if (!seedOk || m_rngSeedUsed == 0) {
        m_rngSeedUsed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    m_rngDraws = std::max<qint64>(0, static_cast<qint64>(rngObj.value(QStringLiteral("draws")).toDouble()));
    // 同じシード＋経過ステップから決定論的に異なる列へ進める
    m_rng.seed(m_rngSeedUsed ^ (static_cast<std::uint64_t>(m_rngDraws) * 0x9e3779b97f4a7c15ULL));

    const QJsonObject st = root.value(QStringLiteral("stats")).toObject();
    m_accumulatedElapsedMs = std::max<qint64>(0, static_cast<qint64>(st.value(QStringLiteral("elapsed_ms")).toDouble()));
    m_maxPliesReached = std::max(0, st.value(QStringLiteral("max_plies_reached")).toInt());
    m_generatedCandidates = std::max<qint64>(0, static_cast<qint64>(st.value(QStringLiteral("generated_candidates")).toDouble()));
    m_engineQueries = std::max<qint64>(0, static_cast<qint64>(st.value(QStringLiteral("engine_queries")).toDouble()));
    m_certifiedRejected = std::max(0, st.value(QStringLiteral("certified_rejected")).toInt());
    m_certifiedInconclusive = std::max(0, st.value(QStringLiteral("certified_inconclusive")).toInt());
    m_repairSucceeded = std::max(0, st.value(QStringLiteral("repair_succeeded")).toInt());
    m_engineRestarts = std::max(0, st.value(QStringLiteral("engine_restarts")).toInt());
    m_batchesSinceProgress = std::max(0, st.value(QStringLiteral("batches_since_progress")).toInt());

    for (const auto& item : root.value(QStringLiteral("frontier")).toArray()) {
        const QJsonObject o = item.toObject();
        BeamNode node;
        node.sfen = o.value(QStringLiteral("sfen")).toString();
        for (const auto& mv : o.value(QStringLiteral("pv")).toArray()) node.pv.append(mv.toString());
        node.plies = o.value(QStringLiteral("plies")).toInt();
        node.surplusCount = o.value(QStringLiteral("surplus")).toInt();
        node.score = o.value(QStringLiteral("score")).toDouble();
        node.expansions = o.value(QStringLiteral("expansions")).toInt();
        node.firstMoveUnique = o.value(QStringLiteral("first_unique")).toBool();
        node.signature = o.value(QStringLiteral("sig")).toString();
        if (!node.sfen.isEmpty() && node.plies > 0) addToFrontier(node);
    }

    for (const auto& item : root.value(QStringLiteral("repair_queue")).toArray()) {
        const QJsonObject o = item.toObject();
        const QString sfen = o.value(QStringLiteral("sfen")).toString();
        const QString cook = o.value(QStringLiteral("cook_move")).toString();
        if (sfen.isEmpty() || cook.isEmpty()) continue;
        m_repairQueue.push_back(RepairItem{
            sfen, o.value(QStringLiteral("main_move")).toString(), cook,
            o.value(QStringLiteral("plies")).toInt(), o.value(QStringLiteral("attempts")).toInt()});
    }

    for (const auto& item : root.value(QStringLiteral("pending_certify")).toArray()) {
        const QString sfen = item.toString();
        if (!sfen.isEmpty()) m_pendingCertify.push_back(PendingCertifyItem{sfen, false});
    }

    for (const auto& item : root.value(QStringLiteral("eval_queue")).toArray()) {
        const QJsonObject o = item.toObject();
        const QString sfen = o.value(QStringLiteral("sfen")).toString();
        if (sfen.isEmpty()) continue;
        m_evalQueue.push_back(EvalItem{sfen, o.value(QStringLiteral("tag")).toString(),
                                       o.value(QStringLiteral("min_plies")).toInt(1),
                                       o.value(QStringLiteral("from_repair")).toBool()});
    }

    if (root.contains(QStringLiteral("active_certifier"))) {
        m_activeCertifier = TsumeStrictCertifier::fromJson(
            root.value(QStringLiteral("active_certifier")).toObject(), certifierSettings());
        if (m_activeCertifier) {
            for (const auto& [k, v] : m_activeCertifier->verifiedCache()) {
                if (m_strictCache.size() >= kMaxStrictCacheEntries) break;
                m_strictCache.emplace(k, v);
            }
        }
    }

    for (const auto& item : root.value(QStringLiteral("found_positions")).toArray()) {
        const QJsonObject o = item.toObject();
        FoundPosition fp;
        fp.sfen = o.value(QStringLiteral("sfen")).toString();
        for (const auto& mv : o.value(QStringLiteral("pv")).toArray()) fp.pv.append(mv.toString());
        fp.removedPieces = o.value(QStringLiteral("removed_pieces")).toInt();
        fp.verifyQueries = o.value(QStringLiteral("verify_queries")).toInt();
        fp.verifyMs = static_cast<qint64>(o.value(QStringLiteral("verify_ms")).toDouble());
        if (!fp.sfen.isEmpty()) {
            m_foundSfens.insert(fp.sfen);
            registerDiversityKeys(fp.sfen, fp.pv);
            m_foundPositions.append(fp);
        }
    }

    for (const auto& item : root.value(QStringLiteral("certified_seen")).toArray()) {
        const QString sfen = item.toString();
        if (!sfen.isEmpty()) m_certifiedQueuedOrDone.insert(sfen);
    }

    for (const auto& item : root.value(QStringLiteral("visited_hashes")).toArray()) {
        bool ok = false;
        const std::uint64_t h = item.toString().toULongLong(&ok, 16);
        if (ok && m_visitedHashes.insert(h).second) m_visitedOrder.push_back(h);
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
    teardownOracle();
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
    teardownOracle();
    emit errorOccurred(message);
    emit finished(false);
}
