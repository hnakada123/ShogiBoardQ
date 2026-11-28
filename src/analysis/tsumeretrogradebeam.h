#ifndef TSUMERETROGRADEBEAM_H
#define TSUMERETROGRADEBEAM_H

/// @file tsumeretrogradebeam.h
/// @brief 逆算生成の探索の状態（手数の層ごとのビーム・待ち行列・既出の記録・統計）と、候補の供給・保存と復元

#include "tsumeedittypes.h"
#include "tsumelinemetrics.h"

#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <random>
#include <unordered_set>

/**
 * @brief 逆算生成（TsumeRetrogradeGenerator）の探索の状態
 *
 * エンジンを使わない部分をまとめる:
 * - 手数の層ごとに有望な局面を持つビーム（フロンティア）と、評価待ち・余詰の修復待ち・厳密証明待ちの行列
 * - 既に調べた局面・採択した局面・類似作の判定キーの記録と、統計
 * - 候補の供給（種局面・0手の詰み局面からの逆算・ビームの展開・1枚除去）
 * - 途中保存（チェックポイント）への書き出しと読み戻し
 *
 * エンジンへの問い合わせと、その答えで何をするか（評価・初手余詰の検査・厳密証明）は生成器が決める。
 */
class TsumeRetrogradeBeam
{
public:
    /// 探索の条件（生成器の設定のうち、探索の状態が使うもの）
    struct Config {
        int targetMoves = 101;
        int beamWidth = 12;                      ///< 各手数層に保持する最大ノード数
        int branchFactor = 24;                   ///< 1展開あたりに生成する逆算/編集候補数
        TsumeEditBoard::Limits limits;           ///< 作る局面の駒数の上限
        bool requireNoSurplus = true;
        bool allowFinalMoveAlternatives = false;
        bool excludeSimilar = true;
        bool hasSeedLines = false;               ///< 種局面の指定がある（停滞しても0手の詰み局面を足さない）
    };

    /// ビームの1局面
    struct Node {
        QString sfen;
        QStringList pv;
        int plies = 0;
        int surplusCount = 0;
        double score = 0.0;
        int expansions = 0;
        bool firstMoveUnique = false;
        QString signature;
    };

    /// 余詰の修復待ち
    struct RepairItem {
        QString sfen;
        QString mainMove;
        QString cookMove;
        int plies = 0;
        int attempts = 0;
    };

    /// 評価待ちの候補
    struct EvalItem {
        QString sfen;
        QString tag;
        int minDesiredPlies = 1;
        bool fromRepair = false;
    };

    /// 厳密証明待ちの候補
    struct PendingCertifyItem {
        QString sfen;
        bool allowTrimOnReject = true;
    };

    /// 採択した局面
    struct FoundPosition {
        QString sfen;
        QStringList pv;
        int removedPieces = 0;
        int verifyQueries = 0;
        qint64 verifyMs = 0;
    };

    /// 統計（途中保存にも書く）
    struct Stats {
        qint64 accumulatedElapsedMs = 0;   ///< 再開前までの経過時間
        int maxPliesReached = 0;
        qint64 generatedCandidates = 0;
        qint64 engineQueries = 0;
        int certifiedRejected = 0;
        int certifiedInconclusive = 0;
        int repairSucceeded = 0;
        int engineRestarts = 0;
        int batchesSinceProgress = 0;
    };

    /// すべて空にして、条件を入れる
    void reset(const Config& config);
    const Config& config() const { return m_config; }
    Stats& stats() { return m_stats; }
    const Stats& stats() const { return m_stats; }

    /// 乱数の列を種から始める（新しい生成）
    void seedRng(std::uint64_t seed);
    std::mt19937_64& rng() { return m_rng; }
    /// 乱数を使った回数を数える（途中保存から再開するとき、同じ種から別の列へ進めるために保存する）
    void countDraw() { ++m_rngDraws; }

    // --- 種・除外 ---
    /// 種局面（"SFEN" または "SFEN moves ..."）を足す。手順つきならビームへ、局面だけなら評価待ちへ
    void addSeedLine(const QString& raw);
    /// 除外対象（"SFEN" または "SFEN moves ..."）を足す
    void addExcludeLine(const QString& raw);
    /// 0手の詰み局面を作り、1手戻した候補を評価待ちへ足す
    void enqueueEndingSeeds(int count);
    /// 評価待ちもビームも空か
    bool isEmpty() const { return m_evalQueue.empty() && m_frontier.empty(); }

    // --- 評価待ち ---
    bool hasEvalItems() const { return !m_evalQueue.empty(); }
    EvalItem takeEvalItem();
    /// 余詰の修復・ビームの展開・0手の詰み局面の注入のどれかで、評価待ちを補充する
    void expandNextBatch();
    /// sfen から1枚除いた局面を、評価待ちの先頭へ maxCount 件まで足す（足した件数を返す）
    int enqueueTrims(const QString& sfen, const QString& tag, int maxCount);

    // --- ビーム ---
    /// ビームに入れる局面を作る（優先度は条件と手順の評価から。penalty は優先度から引く）
    Node makeNode(const QString& sfen, const QStringList& pv, const TsumeLineMetrics::Metrics& metrics,
                  bool firstMoveUnique, double penalty = 0.0) const;
    void addToFrontier(const Node& node);
    /// 初手が一意だった場合の優先度で、この手数の層に入る見込みがあるか
    bool layerAccepts(const TsumeLineMetrics::Metrics& metrics);
    int frontierNodeCount() const;
    /// 採択作・除外作の直前の親（2手進めた局面）がビームにあれば外す
    void removeParentOf(const QString& sfen, const QStringList& pv);

    // --- 余詰の修復待ち ---
    void addToRepairQueue(const QString& sfen, const QString& mainMove, const QString& cookMove, int plies);
    int repairQueueSize() const { return static_cast<int>(m_repairQueue.size()); }

    // --- 厳密証明待ち・採択 ---
    /// 採択済みか、厳密証明に回したことがあるか
    bool isKnown(const QString& sfen) const;
    bool isFound(const QString& sfen) const { return m_foundSfens.contains(sfen); }
    bool isSimilarToAccepted(const QString& sfen, const QStringList& pv) const;
    void enqueueForCertification(const QString& sfen, const QStringList& pv, bool allowTrimOnReject = true);
    /// 次に厳密証明する候補（採択済みのものは飛ばす）
    std::optional<PendingCertifyItem> takePendingCertify();
    int pendingCertifySize() const { return static_cast<int>(m_pendingCertify.size()); }
    void addFound(const FoundPosition& found);
    const QList<FoundPosition>& foundPositions() const { return m_foundPositions; }

    /// 初めて見る局面なら記録して true
    bool markVisited(const QString& sfen);

    // --- 途中保存 ---
    /// 状態を root に書く。elapsedMs は今までの経過時間、inFlight は評価中だった候補（評価待ちの先頭に書く）
    void writeJson(QJsonObject& root, qint64 elapsedMs, const std::optional<EvalItem>& inFlight) const;
    /// root から状態を読む（reset() の後に呼ぶ）
    void readJson(const QJsonObject& root);

private:
    void expandFromRepairQueue();
    Node* chooseParent(bool focusTop, int* chosenLayer);
    void expandNode(Node& chosen, int chosenLayer);
    void registerDiversityKeys(const QString& sfen, const QStringList& pv);
    static std::uint64_t hashSfen(const QString& sfen);

    Config m_config;
    Stats m_stats;

    std::mt19937_64 m_rng;
    std::uint64_t m_rngSeedUsed = 0;
    qint64 m_rngDraws = 0;

    std::map<int, QList<Node>> m_frontier;
    std::deque<RepairItem> m_repairQueue;
    std::deque<EvalItem> m_evalQueue;
    std::deque<PendingCertifyItem> m_pendingCertify;

    std::unordered_set<std::uint64_t> m_visitedHashes;
    std::deque<std::uint64_t> m_visitedOrder;
    QSet<QString> m_foundSfens;
    QSet<QString> m_certifiedQueuedOrDone;
    QSet<QString> m_diversityKeys;
    QList<FoundPosition> m_foundPositions;
};

#endif // TSUMERETROGRADEBEAM_H
