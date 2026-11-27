#ifndef TSUMERETROGRADEGENERATOR_H
#define TSUMERETROGRADEGENERATOR_H

/// @file tsumeretrogradegenerator.h
/// @brief 詰み局面からの逆算・盤面編集・余詰リペア・多層ビームサーチによる長手数詰将棋自動生成器

#include "mateoracle.h"
#include "tsumeretrogradeops.h"
#include "tsumeshogiverifier.h"
#include "tsumestrictcertifier.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>

/**
 * @brief 詰み局面（0手）または種局面から逆算・駒追加移動・余詰リペアで長手数詰将棋を育成・厳密証明する生成器
 *
 * GUI・CLI から共通利用できる QObject として設計されている。
 * - 多層ビームサーチ（手数層ごとに有望・多様な局面を保持）
 * - 案B 長手数ヒューリスティック（駒余りゼロ優先・玉移動距離・捨駒・合駒・展開回数制御）
 * - 初手余詰検出と標的型リペアキュー
 * - 目標手数到達後の完全厳密証明（TsumeStrictCertifier: 全主手順一意・駒余りなし・全1枚除去）
 * - KomoringHeights 向け MinLength セッション設定と askWithin（自然終了のみ確定）＋タイムアウト時自動再起動
 * - チェックポイントの原子的保存（QSaveFile）と完全再開（厳密検証キャッシュ含む）
 */
class TsumeRetrogradeGenerator : public QObject
{
    Q_OBJECT

public:
    struct Settings {
        QString enginePath;
        QString engineName;
        int targetMoves = 101;                    ///< 目標手数（奇数、1..301）
        int maxPositionsToFind = 1;               ///< 発見上限（0 = 無制限）
        int beamWidth = 12;                       ///< 各手数層に保持する最大ノード数
        int branchFactor = 24;                    ///< 1展開あたりに生成する逆算/編集候補数
        int maxAttackPieces = 20;                 ///< 攻め駒上限（盤上+持駒、玉除く）
        int maxDefendPieces = 14;                 ///< 玉以外の玉方盤上駒上限
        int maxTotalBoardPieces = 30;             ///< 盤上総駒数上限（玉含む）
        int stepTimeoutMs = 5000;                 ///< 成長フェーズの1問い合わせ上限(ms)
        int verifyQueryTimeoutMs = 15000;         ///< 厳密検証・トリミングの1問い合わせ上限(ms)
        qint64 verifyBudgetMs = 1800000;          ///< 1候補の厳密検証・トリミング総予算(ms)
        bool addRemainingToDefenderHand = true;   ///< 未使用駒をすべて玉方持駒へ入れる
        bool allowFinalMoveAlternatives = false;  ///< 既定 false: 最終手余詰も禁止（完全一意）
        bool requireNoSurplus = true;             ///< 既定 true: 駒余りを完全禁止
        bool strictMinLength = true;              ///< 既定 true: 厳密検証では go mate infinite 自然終了のみ確定
        QStringList seedSfens;                    ///< 初期種局面（空なら0手詰み局面を自動生成）
        QString checkpointPath;                   ///< 途中保存ファイルパス（空なら保存しない）
        bool resumeFromCheckpoint = false;        ///< checkpointPath から再開するか
        int checkpointIntervalSec = 30;           ///< 定期保存間隔（秒）
        std::uint64_t rngSeed = 0;                ///< 乱数シード（0 なら自動シード）

        Settings normalized() const;
    };

    struct Progress {
        QString phase;               ///< "growing" / "repairing" / "checking_first" / "certifying"
        int maxPliesReached = 0;     ///< これまでに到達した最大手数
        int frontierNodes = 0;       ///< ビーム上の保持ノード総数
        int repairQueueSize = 0;     ///< 余詰リペア待ち数
        int pendingCertifySize = 0;  ///< 厳密証明待ち候補数
        qint64 generatedCandidates = 0;
        qint64 engineQueries = 0;
        int found = 0;
        int certifiedRejected = 0;
        int certifiedInconclusive = 0;
        int repairSucceeded = 0;
        int engineRestarts = 0;
        QString certifierStage;      ///< 厳密証明中の段階名
        int certifierQueries = 0;
        int trimIndex = 0;
        int trimTotal = 0;
        qint64 elapsedMs = 0;
    };

    struct FoundPosition {
        QString sfen;
        QStringList pv;
        int removedPieces = 0;
        int verifyQueries = 0;
        qint64 verifyMs = 0;
    };

    explicit TsumeRetrogradeGenerator(QObject* parent = nullptr);
    ~TsumeRetrogradeGenerator() override;

    /// 生成を開始する（resumeFromCheckpoint=true のときはチェックポイントを読み込んで再開）
    /// 読込エラー等の同期失敗は errorOccurred を出して false を返す。
    bool start(const Settings& settings);
    /// 安全に停止する（チェックポイント指定時は最新状態を保存してから finished を出す）
    void stop();
    bool isRunning() const { return m_running; }

    /// 現在の進捗スナップショット
    Progress progress() const;
    /// 現在のチェックポイント JSON（テスト・永続化用）
    QJsonObject buildCheckpointJson() const;
    /// チェックポイントファイルへ原子的に保存する
    bool saveCheckpointNow(QString* errorMessage = nullptr);

signals:
    void positionFound(const QString& sfen, const QStringList& pv, int removedPieces, int verifyQueries);
    void progressUpdated(const TsumeRetrogradeGenerator::Progress& progress);
    void checkpointSaved(const QString& path);
    void finished(bool stopped);
    void errorOccurred(const QString& message);

private slots:
    void onEngineReady();
    void onOracleAnswered(const MateAnswer& answer);
    void onOracleTimedOut();
    void onOracleError(const QString& message);
    void onStepTimer();
    void onProgressTimer();
    void onCheckpointTimer();

private:
    struct BeamNode {
        QString sfen;
        QStringList pv;
        int plies = 0;
        int surplusCount = 0;
        double score = 0.0;
        int expansions = 0;
        bool firstMoveUnique = false;
        QString signature;
    };

    struct RepairItem {
        QString sfen;
        QString mainMove;
        QString cookMove;
        int plies = 0;
        int attempts = 0;
    };

    struct EvalItem {
        QString sfen;
        QString tag;
        int minDesiredPlies = 1;
        bool fromRepair = false;
    };

    /// 成長中の軽量初手余詰チェック状態
    struct FirstMoveCheckState {
        QString sfen;
        QStringList mainPv;
        TsumeRetrogradeOps::Metrics metrics;
        bool fromRepair = false;
        QStringList altMoves;
        int altMoveIndex = 0;
        QStringList defenses;
        int defenseIndex = 0;
        bool altUnresolved = false;
        int queriesUsed = 0;
        int maxQueries = 48;
        QString activeQuerySfen;
    };

    enum class ActiveMode {
        Idle,
        EvalCandidate,
        FirstMoveCheck,
        Certifying,
    };

    bool loadCheckpointFile(const QString& path, QString* errorMessage);
    void initializeFreshSeeds();
    void enqueueEndingSeeds(int count);
    void startOracle();
    void restartOracleForRecovery();
    void teardownOracle();
    void scheduleStep();
    void driveNext();
    void expandNextBatch();
    void submitEvalAnswer(const MateAnswer& answer);
    void startFirstMoveCheck(const QString& sfen, const QStringList& pv,
                             const TsumeRetrogradeOps::Metrics& metrics, bool fromRepair);
    void stepFirstMoveCheck();
    void submitFirstMoveCheckAnswer(const MateAnswer& answer);
    void finishFirstMoveCheck(bool unique, const QString& cookMove);
    void addToFrontier(const BeamNode& node);
    void addToRepairQueue(const QString& sfen, const QString& mainMove, const QString& cookMove, int plies);
    void enqueueForCertification(const QString& sfen, const QStringList& pv);
    void stepCertifier();
    void submitCertifierAnswer(const MateAnswer& answer, qint64 elapsedMs);
    void finishCertifier();
    void askEngine(const QString& sfen, int limitMs, bool strictInfinite);
    void handleQueryUnknownOrTimeout(bool dueToTimeout);
    void finishRun(bool stopped);
    void failRun(const QString& message);
    void pruneFastCacheIfNeeded();
    bool markVisited(const QString& sfen);
    static std::uint64_t hashSfen(const QString& sfen);
    TsumeRetrogradeOps::Limits opLimits() const;
    TsumeStrictCertifier::Settings certifierSettings() const;

    Settings m_settings;
    bool m_running = false;
    bool m_stopping = false;
    bool m_engineReady = false;
    bool m_awaitingAnswer = false;
    bool m_lastQueryWasStrict = false;
    int m_consecutiveRecoveries = 0;
    QString m_activeQuerySfen;
    ActiveMode m_mode = ActiveMode::Idle;

    MateOracle* m_oracle = nullptr;
    QTimer m_stepTimer;
    QTimer m_progressTimer;
    QTimer m_checkpointTimer;
    QElapsedTimer m_sessionTimer;
    QElapsedTimer m_queryTimer;
    qint64 m_accumulatedElapsedMs = 0;

    std::mt19937_64 m_rng;
    std::uint64_t m_rngSeedUsed = 0;
    qint64 m_rngDraws = 0;

    // 探索状態
    std::map<int, QList<BeamNode>> m_frontier;
    std::deque<RepairItem> m_repairQueue;
    std::deque<EvalItem> m_evalQueue;
    std::deque<QString> m_pendingCertify;
    std::optional<FirstMoveCheckState> m_firstCheck;
    std::unique_ptr<TsumeStrictCertifier> m_activeCertifier;
    EvalItem m_activeEval;

    std::unordered_set<std::uint64_t> m_visitedHashes;
    std::deque<std::uint64_t> m_visitedOrder;
    QSet<QString> m_foundSfens;
    QSet<QString> m_certifiedQueuedOrDone;
    QList<FoundPosition> m_foundPositions;

    // 成長フェーズ用キャッシュと厳密証明フェーズ用キャッシュは分離する
    std::map<std::string, MateAnswer> m_fastCache;
    std::map<std::string, TsumeshogiVerifier::CacheEntry> m_strictCache;

    // 統計
    int m_maxPliesReached = 0;
    qint64 m_generatedCandidates = 0;
    qint64 m_engineQueries = 0;
    int m_certifiedRejected = 0;
    int m_certifiedInconclusive = 0;
    int m_repairSucceeded = 0;
    int m_engineRestarts = 0;
    int m_batchesSinceProgress = 0;
};

Q_DECLARE_METATYPE(TsumeRetrogradeGenerator::Progress)

#endif // TSUMERETROGRADEGENERATOR_H
