/// @file tsumeretrogradebeam.cpp
/// @brief 逆算生成の探索の状態と、候補の供給・保存と復元の実装

#include "tsumeretrogradebeam.h"

#include "tsumecandidatejob.h"
#include "tsumepositionedits.h"
#include "tsumeretrogradeops.h"

#include <QCryptographicHash>
#include <QJsonArray>

#include <algorithm>
#include <chrono>
#include <vector>

namespace {
constexpr std::size_t kMaxVisitedHashes = 400000;
constexpr std::size_t kMaxRepairQueue = 256;
/// 途中保存に書く評価待ち・既訪問の記録の上限（チェックポイントの大きさを抑える）
constexpr std::size_t kMaxSavedEvalItems = 256;
constexpr std::size_t kMaxSavedVisitedHashes = 50000;

QJsonObject evalItemJson(const TsumeRetrogradeBeam::EvalItem& item)
{
    QJsonObject eo;
    eo[QStringLiteral("sfen")] = item.sfen;
    eo[QStringLiteral("tag")] = item.tag;
    eo[QStringLiteral("min_plies")] = item.minDesiredPlies;
    eo[QStringLiteral("from_repair")] = item.fromRepair;
    return eo;
}

QJsonObject nodeJson(const TsumeRetrogradeBeam::Node& node)
{
    QJsonObject n;
    n[QStringLiteral("sfen")] = node.sfen;
    n[QStringLiteral("pv")] = QJsonArray::fromStringList(node.pv);
    n[QStringLiteral("plies")] = node.plies;
    n[QStringLiteral("surplus")] = node.surplusCount;
    n[QStringLiteral("score")] = node.score;
    n[QStringLiteral("expansions")] = node.expansions;
    n[QStringLiteral("first_unique")] = node.firstMoveUnique;
    n[QStringLiteral("sig")] = node.signature;
    return n;
}

TsumeRetrogradeBeam::Node nodeFromJson(const QJsonObject& o)
{
    TsumeRetrogradeBeam::Node node;
    node.sfen = o.value(QStringLiteral("sfen")).toString();
    for (const auto& mv : o.value(QStringLiteral("pv")).toArray()) node.pv.append(mv.toString());
    node.plies = o.value(QStringLiteral("plies")).toInt();
    node.surplusCount = o.value(QStringLiteral("surplus")).toInt();
    node.score = o.value(QStringLiteral("score")).toDouble();
    node.expansions = o.value(QStringLiteral("expansions")).toInt();
    node.firstMoveUnique = o.value(QStringLiteral("first_unique")).toBool();
    node.signature = o.value(QStringLiteral("sig")).toString();
    return node;
}

QJsonObject repairItemJson(const TsumeRetrogradeBeam::RepairItem& item)
{
    QJsonObject r;
    r[QStringLiteral("sfen")] = item.sfen;
    r[QStringLiteral("main_move")] = item.mainMove;
    r[QStringLiteral("cook_move")] = item.cookMove;
    r[QStringLiteral("plies")] = item.plies;
    r[QStringLiteral("attempts")] = item.attempts;
    return r;
}

QJsonObject foundPositionJson(const TsumeRetrogradeBeam::FoundPosition& fp)
{
    QJsonObject fo;
    fo[QStringLiteral("sfen")] = fp.sfen;
    fo[QStringLiteral("pv")] = QJsonArray::fromStringList(fp.pv);
    fo[QStringLiteral("removed_pieces")] = fp.removedPieces;
    fo[QStringLiteral("verify_queries")] = fp.verifyQueries;
    fo[QStringLiteral("verify_ms")] = static_cast<double>(fp.verifyMs);
    return fo;
}

TsumeRetrogradeBeam::FoundPosition foundPositionFromJson(const QJsonObject& o)
{
    TsumeRetrogradeBeam::FoundPosition fp;
    fp.sfen = o.value(QStringLiteral("sfen")).toString();
    for (const auto& mv : o.value(QStringLiteral("pv")).toArray()) fp.pv.append(mv.toString());
    fp.removedPieces = o.value(QStringLiteral("removed_pieces")).toInt();
    fp.verifyQueries = o.value(QStringLiteral("verify_queries")).toInt();
    fp.verifyMs = static_cast<qint64>(o.value(QStringLiteral("verify_ms")).toDouble());
    return fp;
}

QJsonObject statsJson(const TsumeRetrogradeBeam::Stats& stats, qint64 elapsedMs)
{
    QJsonObject st;
    st[QStringLiteral("elapsed_ms")] = static_cast<double>(elapsedMs);
    st[QStringLiteral("max_plies_reached")] = stats.maxPliesReached;
    st[QStringLiteral("generated_candidates")] = static_cast<double>(stats.generatedCandidates);
    st[QStringLiteral("engine_queries")] = static_cast<double>(stats.engineQueries);
    st[QStringLiteral("certified_rejected")] = stats.certifiedRejected;
    st[QStringLiteral("certified_inconclusive")] = stats.certifiedInconclusive;
    st[QStringLiteral("repair_succeeded")] = stats.repairSucceeded;
    st[QStringLiteral("engine_restarts")] = stats.engineRestarts;
    st[QStringLiteral("batches_since_progress")] = stats.batchesSinceProgress;
    return st;
}

TsumeRetrogradeBeam::Stats statsFromJson(const QJsonObject& st)
{
    TsumeRetrogradeBeam::Stats stats;
    stats.accumulatedElapsedMs = std::max<qint64>(0, static_cast<qint64>(st.value(QStringLiteral("elapsed_ms")).toDouble()));
    stats.maxPliesReached = std::max(0, st.value(QStringLiteral("max_plies_reached")).toInt());
    stats.generatedCandidates = std::max<qint64>(0, static_cast<qint64>(st.value(QStringLiteral("generated_candidates")).toDouble()));
    stats.engineQueries = std::max<qint64>(0, static_cast<qint64>(st.value(QStringLiteral("engine_queries")).toDouble()));
    stats.certifiedRejected = std::max(0, st.value(QStringLiteral("certified_rejected")).toInt());
    stats.certifiedInconclusive = std::max(0, st.value(QStringLiteral("certified_inconclusive")).toInt());
    stats.repairSucceeded = std::max(0, st.value(QStringLiteral("repair_succeeded")).toInt());
    stats.engineRestarts = std::max(0, st.value(QStringLiteral("engine_restarts")).toInt());
    stats.batchesSinceProgress = std::max(0, st.value(QStringLiteral("batches_since_progress")).toInt());
    return stats;
}
} // namespace

void TsumeRetrogradeBeam::reset(const Config& config)
{
    m_config = config;
    m_stats = Stats{};
    m_frontier.clear();
    m_repairQueue.clear();
    m_evalQueue.clear();
    m_pendingCertify.clear();
    m_visitedHashes.clear();
    m_visitedOrder.clear();
    m_foundSfens.clear();
    m_certifiedQueuedOrDone.clear();
    m_diversityKeys.clear();
    m_foundPositions.clear();
}

void TsumeRetrogradeBeam::seedRng(std::uint64_t seed)
{
    m_rngSeedUsed = seed;
    m_rng.seed(m_rngSeedUsed);
    m_rngDraws = 0;
}

std::uint64_t TsumeRetrogradeBeam::hashSfen(const QString& sfen)
{
    const QByteArray digest = QCryptographicHash::hash(sfen.toUtf8(), QCryptographicHash::Sha256);
    std::uint64_t h = 0;
    for (int i = 0; i < 8; ++i) {
        h = (h << 8) | static_cast<std::uint8_t>(digest.at(i));
    }
    return h;
}

bool TsumeRetrogradeBeam::markVisited(const QString& sfen)
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

// ============================================================
// 種・除外
// ============================================================

void TsumeRetrogradeBeam::addExcludeLine(const QString& raw)
{
    const QString line = raw.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) return;
    const qsizetype movesIdx = line.indexOf(QLatin1String(" moves "));
    const QString sfenPart = movesIdx >= 0 ? line.left(movesIdx).trimmed() : line;
    const QString norm = TsumeRetrogradeOps::normalizeProblemSfen(sfenPart, m_config.limits.addRemainingToDefenderHand);
    if (!norm.isEmpty()) {
        m_foundSfens.insert(norm);
        m_certifiedQueuedOrDone.insert(norm);
    }
    if (movesIdx >= 0 && m_config.excludeSimilar) {
        const QStringList pv = line.mid(movesIdx + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!norm.isEmpty() && !pv.isEmpty()) {
            registerDiversityKeys(norm, pv);
            // 採択済みの直前親種（2手進めた N-2 手局面）がフロンティアに居れば即座に外す
            if (pv.size() >= 9) removeParentOf(norm, pv);
        }
    }
}

void TsumeRetrogradeBeam::addSeedLine(const QString& raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) return;
    const qsizetype movesIdx = trimmed.indexOf(QLatin1String(" moves "));
    if (movesIdx >= 0) {
        const QString sfenPart = trimmed.left(movesIdx).trimmed();
        const QStringList pv = trimmed.mid(movesIdx + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString normalized = TsumeRetrogradeOps::normalizeProblemSfen(
            sfenPart, m_config.limits.addRemainingToDefenderHand);
        if (!normalized.isEmpty() && !pv.isEmpty()) {
            const auto metrics = TsumeLineMetrics::analyzeLine(normalized, pv);
            if (metrics && metrics->plies <= m_config.targetMoves) {
                markVisited(normalized);
                addToFrontier(makeNode(normalized, pv, *metrics, true));
                if (metrics->plies > m_stats.maxPliesReached) m_stats.maxPliesReached = metrics->plies;
                return;
            }
        }
    }
    if (TsumeRetrogradeOps::isMateEnding(trimmed)) {
        const auto reversed = TsumeRetrogradeOps::reverseFromMateEnding(trimmed, m_config.limits, 8, m_rng);
        ++m_rngDraws;
        for (const auto& cand : reversed) {
            if (markVisited(cand.sfen)) {
                m_evalQueue.push_back({cand.sfen, cand.tag, 1, false});
                ++m_stats.generatedCandidates;
            }
        }
    } else {
        const QString sfenOnly = movesIdx >= 0 ? trimmed.left(movesIdx).trimmed() : trimmed;
        const QString normalized = TsumeRetrogradeOps::normalizeProblemSfen(
            sfenOnly, m_config.limits.addRemainingToDefenderHand);
        if (!normalized.isEmpty() && markVisited(normalized)) {
            m_evalQueue.push_back({normalized, QStringLiteral("seed"), 1, false});
            ++m_stats.generatedCandidates;
        }
    }
}

void TsumeRetrogradeBeam::enqueueEndingSeeds(int count)
{
    const QStringList endings = TsumeRetrogradeOps::generateMateEndings(m_config.limits, count, m_rng);
    ++m_rngDraws;
    const bool strictFinal = !m_config.allowFinalMoveAlternatives;
    for (const QString& ending : endings) {
        const auto candidates = TsumeRetrogradeOps::reverseFromMateEnding(ending, m_config.limits, 6, m_rng);
        ++m_rngDraws;
        int added = 0;
        for (const auto& cand : candidates) {
            if (strictFinal && TsumeLineMetrics::countImmediateMates(cand.sfen) != 1) continue;
            if (!markVisited(cand.sfen)) continue;
            m_evalQueue.push_back({cand.sfen, cand.tag, 1, false});
            ++m_stats.generatedCandidates;
            if (++added >= 3) break;
        }
        if (added == 0) {
            for (const auto& cand : candidates) {
                if (!markVisited(cand.sfen)) continue;
                m_evalQueue.push_back({cand.sfen, cand.tag, 1, false});
                ++m_stats.generatedCandidates;
                if (++added >= 2) break;
            }
        }
    }
}

// ============================================================
// 評価待ち・展開（案A/B）
// ============================================================

TsumeRetrogradeBeam::EvalItem TsumeRetrogradeBeam::takeEvalItem()
{
    EvalItem item = m_evalQueue.front();
    m_evalQueue.pop_front();
    return item;
}

int TsumeRetrogradeBeam::enqueueTrims(const QString& sfen, const QString& tag, int maxCount)
{
    int added = 0;
    for (const QString& trimmed : TsumeCandidateJob::onePieceRemovedPositions(sfen)) {
        if (markVisited(trimmed)) {
            m_evalQueue.push_front({trimmed, tag, m_config.targetMoves, true});
            ++m_stats.generatedCandidates;
            if (++added >= maxCount) break;
        }
    }
    return added;
}

void TsumeRetrogradeBeam::expandNextBatch()
{
    std::uniform_int_distribution<int> pct(0, 99);
    const int roll = pct(m_rng);
    ++m_rngDraws;

    // 1. 余詰リペアキューからの展開（キューがあれば約15%の確率）
    if (!m_repairQueue.empty() && (roll < 15 || m_frontier.empty())) {
        expandFromRepairQueue();
        return;
    }

    // 2. フロンティアが空、またはシード未指定のボトムアップ探索で停滞したら新しい0手詰み局面を注入
    if (m_frontier.empty()
        || (!m_config.hasSeedLines && (roll >= 92 || m_stats.batchesSinceProgress >= 25))) {
        m_stats.batchesSinceProgress = 0;
        enqueueEndingSeeds(std::max(4, m_config.beamWidth / 2));
        if (!m_evalQueue.empty()) return;
    }

    // 3. フロンティアから親ノードを選択
    // 約65%は上位手数帯（最大手数から4手以内）のベストスコア、約35%は全層からバックトラック選択
    int chosenLayer = 0;
    Node* chosen = chooseParent(roll < 75, &chosenLayer);
    if (!chosen) return;
    expandNode(*chosen, chosenLayer);
}

void TsumeRetrogradeBeam::expandFromRepairQueue()
{
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

    const int repairBranch = std::min(m_config.branchFactor, 16);
    const auto repairs = TsumePositionEdits::generateCookRepairs(
        item.sfen, item.mainMove, item.cookMove, m_config.limits, repairBranch, m_rng);
    ++m_rngDraws;
    for (const auto& cand : repairs) {
        if (!markVisited(cand.sfen)) continue;
        m_evalQueue.push_back({cand.sfen, cand.tag, item.plies, true});
        ++m_stats.generatedCandidates;
    }
}

TsumeRetrogradeBeam::Node* TsumeRetrogradeBeam::chooseParent(bool focusTop, int* chosenLayer)
{
    Node* chosen = nullptr;
    if (focusTop) {
        double bestScore = -1e18;
        int inspectedLayers = 0;
        std::uniform_real_distribution<double> jitter(0.0, 28.0);
        for (auto rit = m_frontier.rbegin(); rit != m_frontier.rend() && inspectedLayers < 3; ++rit, ++inspectedLayers) {
            for (Node& node : rit->second) {
                // 1つの親種・同じ上位種に複数ワーカーが集中しないよう、展開ペナルティ＋乱数ジッターで分散巡回する
                const double effective = node.score - static_cast<double>(node.expansions) * 45.0 + jitter(m_rng);
                ++m_rngDraws;
                if (effective > bestScore) {
                    bestScore = effective;
                    chosen = &node;
                    *chosenLayer = rit->first;
                }
            }
        }
        return chosen;
    }

    std::vector<std::pair<int, int>> allRefs;
    for (const auto& [plies, list] : m_frontier) {
        for (int i = 0; i < list.size(); ++i) allRefs.push_back({plies, i});
    }
    if (allRefs.empty()) return nullptr;
    // 複数ランダムサンプルから最高スコアを選ぶトーナメント選択
    double bestScore = -1e18;
    std::uniform_int_distribution<std::size_t> pick(0, allRefs.size() - 1);
    for (int t = 0; t < 6; ++t) {
        const auto [plies, idx] = allRefs[pick(m_rng)];
        ++m_rngDraws;
        Node& cand = m_frontier[plies][idx];
        const double effective = cand.score - static_cast<double>(cand.expansions) * 40.0;
        if (effective > bestScore) {
            bestScore = effective;
            chosen = &cand;
            *chosenLayer = plies;
        }
    }
    return chosen;
}

void TsumeRetrogradeBeam::expandNode(Node& chosen, int chosenLayer)
{
    ++chosen.expansions;
    // 展開回数が増えたのでノード自体のスコアを再計算して過集中を防ぐ
    if (const auto metrics = TsumeLineMetrics::analyzeLine(chosen.sfen, chosen.pv)) {
        chosen.score = TsumeLineMetrics::scoreNode(*metrics, m_config.targetMoves, chosen.expansions,
                                                   chosen.firstMoveUnique, m_config.requireNoSurplus,
                                                   !m_config.allowFinalMoveAlternatives);
    }

    // 同手数シードの展開時は逆算して切り出しても元手順に戻るため 100% 盤面変異に集中し、短いシードは逆算を主軸にする
    const int retroBudget = (chosenLayer >= m_config.targetMoves)
        ? 0
        : std::max(4, (m_config.branchFactor * 2) / 3);
    const int mutateBudget = std::max(2, m_config.branchFactor - retroBudget);

    // 逆算候補（2手伸ばしを狙う）
    if (retroBudget > 0) {
        const auto predecessors = TsumeRetrogradeOps::generatePredecessors(chosen.sfen, m_config.limits, retroBudget, m_rng);
        ++m_rngDraws;
        for (const auto& cand : predecessors) {
            if (!markVisited(cand.sfen)) continue;
            m_evalQueue.push_back({cand.sfen, cand.tag, chosenLayer + 2, false});
            ++m_stats.generatedCandidates;
        }
    }

    // 盤面編集候補（駒の移動・追加・変更で新しい手筋や延長を生む）
    const auto mutations = TsumePositionEdits::generateMutations(chosen.sfen, m_config.limits, mutateBudget, m_rng);
    ++m_rngDraws;
    for (const auto& cand : mutations) {
        if (!markVisited(cand.sfen)) continue;
        m_evalQueue.push_back({cand.sfen, cand.tag, chosenLayer, false});
        ++m_stats.generatedCandidates;
    }

    ++m_stats.batchesSinceProgress;
}

// ============================================================
// ビーム・余詰の修復待ち
// ============================================================

TsumeRetrogradeBeam::Node TsumeRetrogradeBeam::makeNode(const QString& sfen, const QStringList& pv,
                                                      const TsumeLineMetrics::Metrics& metrics,
                                                      bool firstMoveUnique, double penalty) const
{
    Node node;
    node.sfen = sfen;
    node.pv = pv;
    node.plies = metrics.plies;
    node.surplusCount = metrics.surplusCount;
    node.firstMoveUnique = firstMoveUnique;
    node.signature = TsumeLineMetrics::structuralSignature(sfen, pv);
    node.score = TsumeLineMetrics::scoreNode(metrics, m_config.targetMoves, 0, firstMoveUnique,
                                             m_config.requireNoSurplus,
                                             !m_config.allowFinalMoveAlternatives) - penalty;
    return node;
}

void TsumeRetrogradeBeam::addToFrontier(const Node& node)
{
    auto& layer = m_frontier[node.plies];
    for (const Node& existing : layer) {
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
              [](const Node& a, const Node& b) { return a.score > b.score; });
    while (layer.size() > m_config.beamWidth) layer.removeLast();
}

bool TsumeRetrogradeBeam::layerAccepts(const TsumeLineMetrics::Metrics& metrics)
{
    const auto& layer = m_frontier[metrics.plies];
    const double optimisticScore = TsumeLineMetrics::scoreNode(
        metrics, m_config.targetMoves, 0, true, m_config.requireNoSurplus,
        !m_config.allowFinalMoveAlternatives);
    return !(layer.size() >= m_config.beamWidth && optimisticScore <= layer.last().score);
}

int TsumeRetrogradeBeam::frontierNodeCount() const
{
    int count = 0;
    for (const auto& [plies, list] : m_frontier) count += static_cast<int>(list.size());
    return count;
}

void TsumeRetrogradeBeam::removeParentOf(const QString& sfen, const QStringList& pv)
{
    const auto parentSlice = TsumeRetrogradeOps::sliceToTargetLength(sfen, pv, static_cast<int>(pv.size()) - 2);
    if (!parentSlice) return;
    auto it = m_frontier.find(static_cast<int>(pv.size()) - 2);
    if (it == m_frontier.end()) return;
    auto& list = it->second;
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const Node& n) { return n.sfen == parentSlice->sfen; }),
               list.end());
    if (list.isEmpty()) m_frontier.erase(it);
}

void TsumeRetrogradeBeam::addToRepairQueue(const QString& sfen, const QString& mainMove,
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

// ============================================================
// 厳密証明待ち・採択
// ============================================================

bool TsumeRetrogradeBeam::isKnown(const QString& sfen) const
{
    return m_foundSfens.contains(sfen) || m_certifiedQueuedOrDone.contains(sfen);
}

bool TsumeRetrogradeBeam::isSimilarToAccepted(const QString& sfen, const QStringList& pv) const
{
    if (!m_config.excludeSimilar || pv.isEmpty()) return false;
    for (const QString& key : TsumeLineMetrics::diversityKeys(sfen, pv)) {
        if (m_diversityKeys.contains(key)) return true;
    }
    return false;
}

void TsumeRetrogradeBeam::registerDiversityKeys(const QString& sfen, const QStringList& pv)
{
    if (!m_config.excludeSimilar || pv.isEmpty()) return;
    for (const QString& key : TsumeLineMetrics::diversityKeys(sfen, pv)) {
        m_diversityKeys.insert(key);
    }
}

void TsumeRetrogradeBeam::enqueueForCertification(const QString& sfen, const QStringList& pv,
                                                  bool allowTrimOnReject)
{
    if (isKnown(sfen)) return;
    if (isSimilarToAccepted(sfen, pv)) return;
    m_certifiedQueuedOrDone.insert(sfen);
    m_pendingCertify.push_back(PendingCertifyItem{sfen, allowTrimOnReject});
}

std::optional<TsumeRetrogradeBeam::PendingCertifyItem> TsumeRetrogradeBeam::takePendingCertify()
{
    while (!m_pendingCertify.empty()) {
        PendingCertifyItem item = m_pendingCertify.front();
        m_pendingCertify.pop_front();
        if (m_foundSfens.contains(item.sfen)) continue;
        return item;
    }
    return std::nullopt;
}

void TsumeRetrogradeBeam::addFound(const FoundPosition& found)
{
    m_foundSfens.insert(found.sfen);
    registerDiversityKeys(found.sfen, found.pv);
    m_foundPositions.append(found);
}

// ============================================================
// 途中保存
// ============================================================

void TsumeRetrogradeBeam::writeJson(QJsonObject& root, qint64 elapsedMs,
                                    const std::optional<EvalItem>& inFlight) const
{
    QJsonObject rngObj;
    rngObj[QStringLiteral("seed")] = QString::number(m_rngSeedUsed);
    rngObj[QStringLiteral("draws")] = static_cast<double>(m_rngDraws);
    root[QStringLiteral("rng")] = rngObj;
    root[QStringLiteral("stats")] = statsJson(m_stats, elapsedMs);

    QJsonArray frontierArr;
    for (const auto& [plies, list] : m_frontier) {
        for (const Node& node : list) frontierArr.append(nodeJson(node));
    }
    root[QStringLiteral("frontier")] = frontierArr;

    QJsonArray repairArr;
    for (const RepairItem& item : m_repairQueue) repairArr.append(repairItemJson(item));
    root[QStringLiteral("repair_queue")] = repairArr;

    QJsonArray pendingArr;
    for (const PendingCertifyItem& item : m_pendingCertify) pendingArr.append(item.sfen);
    root[QStringLiteral("pending_certify")] = pendingArr;

    QJsonArray evalArr;
    // 中断時に評価中・初手検査中だった候補を取りこぼさないよう先頭に含める
    if (inFlight) evalArr.append(evalItemJson(*inFlight));
    const std::size_t evalCap = std::min<std::size_t>(m_evalQueue.size(), kMaxSavedEvalItems);
    for (std::size_t i = 0; i < evalCap; ++i) evalArr.append(evalItemJson(m_evalQueue[i]));
    root[QStringLiteral("eval_queue")] = evalArr;

    QJsonArray foundArr;
    for (const FoundPosition& fp : m_foundPositions) foundArr.append(foundPositionJson(fp));
    root[QStringLiteral("found_positions")] = foundArr;

    QJsonArray certDoneArr;
    for (const QString& sfen : m_certifiedQueuedOrDone) certDoneArr.append(sfen);
    root[QStringLiteral("certified_seen")] = certDoneArr;

    // 直近の既訪問ハッシュを保存（チェックポイントサイズを抑えつつ再探索を防止）
    QJsonArray visitedArr;
    const std::size_t visitKeep = std::min<std::size_t>(m_visitedOrder.size(), kMaxSavedVisitedHashes);
    const std::size_t startIdx = m_visitedOrder.size() - visitKeep;
    for (std::size_t i = startIdx; i < m_visitedOrder.size(); ++i) {
        visitedArr.append(QString::number(m_visitedOrder[i], 16));
    }
    root[QStringLiteral("visited_hashes")] = visitedArr;
}

void TsumeRetrogradeBeam::readJson(const QJsonObject& root)
{
    const QJsonObject rngObj = root.value(QStringLiteral("rng")).toObject();
    bool seedOk = false;
    m_rngSeedUsed = rngObj.value(QStringLiteral("seed")).toString().toULongLong(&seedOk);
    if (!seedOk || m_rngSeedUsed == 0) {
        m_rngSeedUsed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    m_rngDraws = std::max<qint64>(0, static_cast<qint64>(rngObj.value(QStringLiteral("draws")).toDouble()));
    // 同じシード＋経過ステップから決定論的に異なる列へ進める
    m_rng.seed(m_rngSeedUsed ^ (static_cast<std::uint64_t>(m_rngDraws) * 0x9e3779b97f4a7c15ULL));

    m_stats = statsFromJson(root.value(QStringLiteral("stats")).toObject());

    for (const auto& item : root.value(QStringLiteral("frontier")).toArray()) {
        const Node node = nodeFromJson(item.toObject());
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

    for (const auto& item : root.value(QStringLiteral("found_positions")).toArray()) {
        const FoundPosition fp = foundPositionFromJson(item.toObject());
        if (!fp.sfen.isEmpty()) addFound(fp);
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
}
