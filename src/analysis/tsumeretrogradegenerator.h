#ifndef TSUMERETROGRADEGENERATOR_H
#define TSUMERETROGRADEGENERATOR_H

/// @file tsumeretrogradegenerator.h
/// @brief 詰み局面からの逆算・盤面編集・余詰リペア・多層ビームサーチによる長手数詰将棋自動生成器

#include "mateoracle.h"
#include "tsumefirstmovecheck.h"
#include "tsumeretrogradebeam.h"
#include "tsumeshogiverifier.h"
#include "tsumestrictcertifier.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

class TsumeMateQueryLink;

/**
 * @brief 詰み局面（0手）または種局面から逆算・駒追加移動・余詰リペアで長手数詰将棋を育成・厳密証明する生成器
 *
 * GUI・CLI から共通利用できる QObject として設計されている。エンジンへの問い合わせと、その答えで
 * 何をするか（候補の評価 → 初手余詰の検査 → 厳密証明）の順番を受け持つ。エンジンを使わない部分は別の部品:
 * 探索の状態と候補の供給は TsumeRetrogradeBeam、エンジンの起動・問い合わせ・再起動は TsumeMateQueryLink、
 * 内蔵探索による事前ふるい分けは TsumeLocalScreen、初手余詰の軽い検査は TsumeFirstMoveCheck、
 * 厳密証明は TsumeStrictCertifier、途中保存ファイルの形式は TsumeRetrogradeCheckpoint。
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
        int engineHashMb = 0;                     ///< USI_Hash の上書き(MiB、0 は登録設定のまま)
        int engineThreads = 0;                    ///< Threads の上書き(0 は登録設定のまま)
        bool addRemainingToDefenderHand = true;   ///< 未使用駒をすべて玉方持駒へ入れる
        bool allowFinalMoveAlternatives = false;  ///< 既定 false: 最終手余詰も禁止（完全一意）
        bool requireNoSurplus = true;             ///< 既定 true: 駒余りを完全禁止
        bool strictMinLength = true;              ///< 既定 true: 厳密検証では go mate infinite 自然終了のみ確定
        bool excludeSimilar = true;               ///< 既定 true: 採択済み・除外済みと類似手順の候補を厳密証明前に除外
        QStringList seedSfens;                    ///< 初期種局面（"SFEN" または "SFEN moves ..."。空なら0手詰み局面を自動生成）
        QStringList excludeLines;                 ///< 既出の除外対象（"SFEN" または "SFEN moves ..."）
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

    /// 採択した局面
    using FoundPosition = TsumeRetrogradeBeam::FoundPosition;

    explicit TsumeRetrogradeGenerator(QObject* parent = nullptr);
    ~TsumeRetrogradeGenerator() override;

    /// 生成を開始する（resumeFromCheckpoint=true のときはチェックポイントを読み込んで再開）
    /// 読込エラー等の同期失敗は errorOccurred を出して false を返す。
    bool start(const Settings& settings);
    /// 安全に停止する（チェックポイント指定時は最新状態を保存してから finished を出す）
    void stop();
    bool isRunning() const { return m_running; }

    /// 実行中に外部（並列オーケストレータ等）から除外対象（"SFEN" または "SFEN moves ..."）を追加する
    void addExcludeLine(const QString& line);
    /// 実行中に外部から新しい種局面（"SFEN moves ..." 等）を追加する
    void addSeedLine(const QString& line);

    /// 現在の進捗スナップショット
    Progress progress() const;
    /// これまでに採択した局面一覧（チェックポイントから復元した分も含む）
    const QList<FoundPosition>& foundPositions() const { return m_beam.foundPositions(); }
    /// 現在のチェックポイント JSON（テスト・永続化用）
    QJsonObject buildCheckpointJson() const;
    /// 種局面・除外のファイルを読み、空行と # で始まる行を除いた行を返す。開けなければ nullopt（理由は errorString）
    static std::optional<QStringList> readInputLines(const QString& path, QString* errorString = nullptr);
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
    void onQuerySent();
    void onOracleAnswered(const MateAnswer& answer, qint64 elapsedMs);
    void onEngineUnsupported();
    void onEngineUnresponsive();
    void onOracleTimedOut();
    void onOracleError(const QString& message);
    void onStepTimer();
    void onProgressTimer();
    void onCheckpointTimer();

private:
    enum class ActiveMode {
        Idle,
        EvalCandidate,
        FirstMoveCheck,
        Certifying,
    };

    bool loadCheckpointFile(const QString& path, QString* errorMessage);
    void initializeFreshSeeds();
    void restartEngineForRecovery();
    void scheduleStep();
    void driveNext();
    void beginCertification(const TsumeRetrogradeBeam::PendingCertifyItem& item);
    void evaluateQueuedCandidates();
    void submitEvalAnswer(const MateAnswer& answer);
    void submitTargetCandidate(const MateAnswer& answer, int plies);
    void startFirstMoveCheck(const QString& sfen, const QStringList& pv,
                             const TsumeLineMetrics::Metrics& metrics, bool fromRepair);
    void stepFirstMoveCheck();
    void submitFirstMoveCheckAnswer(const MateAnswer& answer);
    void finishFirstMoveCheck();
    void stepCertifier();
    void submitCertifierAnswer(const MateAnswer& answer, qint64 elapsedMs);
    void finishCertifier();
    void askEngine(const QString& sfen, int limitMs, bool strictInfinite);
    void handleQueryUnknownOrTimeout(bool dueToTimeout);
    void finishRun(bool stopped);
    void failRun(const QString& message);
    void pruneFastCacheIfNeeded();
    /// 成長フェーズで分かった答え（無ければ nullptr）
    const MateAnswer* cachedAnswer(const QString& sfen) const;
    /// 再開前を含む経過時間
    qint64 elapsedMs() const;
    TsumeRetrogradeBeam::Config beamConfig() const;
    TsumeStrictCertifier::Settings certifierSettings() const;

    Settings m_settings;
    bool m_running = false;
    bool m_stopping = false;
    ActiveMode m_mode = ActiveMode::Idle;

    /// エンジンへの問い合わせの窓口（QObject parent 所有）
    TsumeMateQueryLink* m_engine = nullptr;
    QTimer m_stepTimer;
    QTimer m_progressTimer;
    QTimer m_checkpointTimer;
    QElapsedTimer m_sessionTimer;

    /// 探索の状態（ビーム・待ち行列・既出の記録・統計・乱数）
    TsumeRetrogradeBeam m_beam;
    // 進行中の処理（どれも無ければ、評価待ちの候補を評価する）
    bool m_activeCertifierAllowTrim = true;
    std::optional<TsumeFirstMoveCheck> m_firstCheck;
    std::unique_ptr<TsumeStrictCertifier> m_activeCertifier;
    TsumeRetrogradeBeam::EvalItem m_activeEval;

    // 成長フェーズ用キャッシュと厳密証明フェーズ用キャッシュは分離する
    std::map<std::string, MateAnswer> m_fastCache;
    std::map<std::string, TsumeshogiVerifier::CacheEntry> m_strictCache;
};

Q_DECLARE_METATYPE(TsumeRetrogradeGenerator::Progress)

#endif // TSUMERETROGRADEGENERATOR_H
