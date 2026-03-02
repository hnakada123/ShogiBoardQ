#ifndef MATCHCOORDINATOR_H
#define MATCHCOORDINATOR_H

/// @file matchcoordinator.h
/// @brief 対局進行コーディネータ（司令塔）クラスの定義

#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <QStringList>
#include <QList>
#include <QDateTime>

#include "shogigamecontroller.h"

#include "shogimove.h"
#include "playmode.h"
#include "matchundohandler.h"
#include "enginelifecyclemanager.h"
#include "startgamedatabridge.h"

#include "matchtypes.h"
#include "timecontrol.h"
#include "startoptions.h"
#include "analysisoptions.h"
#include "matchcoordinatorhooks.h"

class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiGameController;
class ShogiClock;
class GameModeStrategy;
class EngineVsEngineStrategy;
class ShogiView;
class Usi;
class AnalysisSessionHandler;
class GameEndHandler;
class GameStartOrchestrator;
class MatchTimekeeper;
class MatchTurnHandler;

/**
 * @brief 対局進行/終局/時計/USI送受のハブとなる司令塔クラス
 *
 * 対局全体のライフサイクルを管理し、エンジン通信・時計管理・
 * 棋譜記録・終局判定を統括する。寿命はMainWindow側で管理する。
 *
 */
class MatchCoordinator : public QObject {
    Q_OBJECT

public:
    class StrategyContext;
    StrategyContext& strategyCtx();
    // --- 型定義 ---

    /// 対局者を表す列挙値（matchtypes.h の MatchPlayer の using エイリアス）
    using Player = MatchPlayer;
    static constexpr Player P1 = Player::P1;  ///< 先手
    static constexpr Player P2 = Player::P2;  ///< 後手

    /// 終局原因（matchtypes.h の MatchCause の using エイリアス）
    using Cause = MatchCause;

    /// USI goコマンドに渡す時間パラメータ（matchtypes.h の MatchGoTimes の using エイリアス）
    using GoTimes = MatchGoTimes;

    /// 終局情報（matchtypes.h の MatchGameEndInfo の using エイリアス）
    using GameEndInfo = MatchGameEndInfo;

    /// 直近の終局状態（matchtypes.h の MatchGameOverState の using エイリアス）
    using GameOverState = MatchGameOverState;

    /// 時間制御の設定（timecontrol.h の MatchTimeControl の using エイリアス）
    using TimeControl = MatchTimeControl;

    /// 対局開始のための設定パラメータ（startoptions.h の MatchStartOptions の using エイリアス）
    using StartOptions = MatchStartOptions;

    /// 検討オプション（analysisoptions.h の MatchAnalysisOptions の using エイリアス）
    using AnalysisOptions = MatchAnalysisOptions;

    /// UI/Engine/Game コールバック群（matchcoordinatorhooks.h の MatchCoordinatorHooks の using エイリアス）
    using Hooks = MatchCoordinatorHooks;

    /// 依存オブジェクト
    struct Deps {
        ShogiGameController* gc = nullptr;         ///< ゲームコントローラ（非所有）
        ShogiClock*          clock = nullptr;       ///< 将棋時計（非所有）
        ShogiView*           view = nullptr;        ///< 盤面ビュー（非所有）
        Usi*                 usi1 = nullptr;         ///< エンジン1（非所有）
        Usi*                 usi2 = nullptr;         ///< エンジン2（非所有）
        UsiCommLogModel*           comm1 = nullptr;  ///< エンジン1通信ログ（非所有）
        ShogiEngineThinkingModel*  think1 = nullptr; ///< エンジン1思考情報（非所有）
        UsiCommLogModel*           comm2 = nullptr;  ///< エンジン2通信ログ（非所有）
        ShogiEngineThinkingModel*  think2 = nullptr; ///< エンジン2思考情報（非所有）
        Hooks                hooks;                  ///< UIコールバック群
        QStringList* sfenRecord = nullptr;           ///< SFEN履歴（非所有）
        QList<ShogiMove>* gameMoves = nullptr;     ///< 指し手リスト（非所有、外部共有用）
    };

    explicit MatchCoordinator(const Deps& deps, QObject* parent=nullptr);
    ~MatchCoordinator() override;

    // --- 外部API ---

    /// 人間の投了を処理する
    void handleResign();

    /// エンジン投了通知を処理する（idx: 1 or 2）
    void handleEngineResign(int idx);

    /// エンジン入玉宣言勝ち通知を処理する（idx: 1 or 2）
    void handleEngineWin(int idx);

    void flipBoard();

    /// エンジンポインタを差し替える（再生成時など）
    void updateUsiPtrs(Usi* e1, Usi* e2);

    /// 中断を処理する（UI→司令塔）
    void handleBreakOff();

    /// 入玉宣言を処理する
    void handleNyugyokuDeclaration(Player declarer, bool success, bool isDraw);

    /// 対局中の指し手リストを取得する（CSA出力等で使用）
    const QList<ShogiMove>& gameMoves() const { return m_gameMoves; }
    /// SFEN履歴への参照を取得する（UI共有用）
    QStringList* sfenRecordPtr() { return m_sfenHistory; }
    /// SFEN履歴への参照を取得する（const版）
    const QStringList* sfenRecordPtr() const { return m_sfenHistory; }

    // --- 時間管理 ---

    void setTimeControlConfig(bool useByoyomi,
                              int byoyomiMs1, int byoyomiMs2,
                              int incMs1,     int incMs2,
                              bool loseOnTimeout);
    const TimeControl& timeControl() const;

    // --- ターン計測（KIF表示用） ---

    void   markTurnEpochNowFor(Player side, qint64 nowMs = -1);
    qint64 turnEpochFor(Player side) const;

    // --- HvH: ターン計測 ---

    void armTurnTimerIfNeeded();
    void finishTurnTimerAndSetConsiderationFor(Player mover);

    // --- HvE: 人間タイマー解除（Strategy委譲） ---

    void disarmHumanTimerIfNeeded();

    // --- USI時間計算 ---

    void refreshGoTimes();

    // --- 対局開始フロー ---

    /// 初手がエンジン手番なら go を発行する（Strategy委譲）
    void startInitialEngineMoveIfNeeded();

    // --- UNDO（MatchUndoHandler へ委譲） ---

    using UndoRefs  = MatchUndoHandler::UndoRefs;
    using UndoHooks = MatchUndoHandler::UndoHooks;

    void setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks);

    /// 2手分のUNDOを実行する
    bool undoTwoPlies();

    // --- エンジン管理 ---

    /// エンジンの初期化と開始（side: P1 or P2）
    void initializeAndStartEngineFor(Player side,
                                     const QString& enginePathIn,
                                     const QString& engineNameIn);

    /// エンジンを破棄する（idx: 1 or 2）
    void destroyEngine(int idx, bool clearThinking = true);

    /// 全エンジンを破棄する
    void destroyEngines(bool clearModels = true);

    // --- 終局状態管理 ---

    const GameOverState& gameOverState() const { return m_gameOver; }

    /// 終局状態をクリアする（対局開始/局面編集開始時に呼ぶ）
    void clearGameOverState();

    /// 終局状態を設定する
    void setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce = true);

    /// 棋譜に終局手を一意追記後に通知する
    void markGameOverMoveAppended();

    // --- 検討API ---

    /// 検討を開始する
    void startAnalysis(const AnalysisOptions& opt);

    /// 詰み探索・検討エンジンを終了する
    void stopAnalysisEngine();

    /// 検討中にMultiPVを変更する
    void updateConsiderationMultiPV(int multiPV);

    /**
     * @brief 検討中にポジションを変更する（棋譜欄の別の手を選択したとき）
     * @return true: 再開処理を開始した, false: 検討モードでないか同じポジションのため無視
     */
    bool updateConsiderationPosition(const QString& newPositionStr,
                                     int previousFileTo = 0, int previousRankTo = 0,
                                     const QString& lastUsiMove = QString());

    // --- エンジンアクセサ ---

    Usi* primaryEngine() const;
    Usi* secondaryEngine() const;

    // --- 対局開始一括API ---

    /// 対局開始フローを一元化する
    void configureAndStart(const StartOptions& opt);

    /// StartOptionsを構築する
    StartOptions buildStartOptions(PlayMode mode,
                                   const QString& startSfenStr,
                                   const QStringList* sfenRecord,
                                   const StartGameDialogData* dlg) const;

    /// 人間を手前に配置する
    void ensureHumanAtBottomIfApplicable(const StartGameDialogData* dlg, bool bottomIsP1);

    /// 準備→開始→必要なら初手goまでを一括実行する
    void prepareAndStartGame(PlayMode mode,
                             const QString& startSfenStr,
                             const QStringList* sfenRecord,
                             const StartGameDialogData* dlg,
                             bool bottomIsP1);

    // --- 時間/手番・終局の処理 ---

    void handlePlayerTimeOut(int player);

    /// 開始直後のタイマー起動や初手go判定を1本化する
    void startMatchTimingAndMaybeInitialGo();

    // --- PlayMode ---

    PlayMode playMode() const { return m_playMode; }

    /// 終局1回だけの棋譜追記と重複防止を一括処理する
    void appendGameOverLineAndMark(Cause cause, Player loser);

    /// 人間着手後の後処理（Strategy へ委譲）
    void onHumanMove(const QPoint& from, const QPoint& to,
                     const QString& prettyMove);

    /// 現在の状況に応じて適切なエンジンへstopを送り、即時bestmoveを促す
    void forceImmediateMove();

    // --- USI送受 ---

    void sendGoToEngine(Usi* which, const GoTimes& t);
    void sendStopToEngine(Usi* which);
    void sendRawToEngine(Usi* which, const QString& cmd);

    /// 持将棋（最大手数到達）時の棋譜1行追記と終局処理
    void handleMaxMovesJishogi();

    int maxMoves() const { return m_maxMoves; }

    // --- ShogiClockアクセサ ---

    ShogiClock*       clock();
    const ShogiClock* clock() const;

    /// 時計を差し替える（後からの配線用）
    void setClock(ShogiClock* clock);

    /// 時計スナップショットを再計算する（手番テキスト・残り時間文字列）
    void recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const;

    void setPlayMode(PlayMode m);

    /// EvE用のエンジン生成・配線・初期化
    void initEnginesForEvE(const QString& engineName1,
                           const QString& engineName2);

    /// エンジン破棄中かどうかを返す
    bool isEngineShutdownInProgress() const;

    /// エンジンライフサイクルマネージャを取得する
    EngineLifecycleManager* engineManager();

public slots:
    /// 即時に現在値でtimeUpdated(...)を発火する（UIをすぐ同期させたい時に使う）
    void pokeTimeUpdateNow();

signals:
    /// 盤面反転の通知（→ GameStartCoordinator::boardFlipped へ再送出）
    void boardFlipped(bool nowFlipped);

    /// 終局の通知（→ GameStartCoordinator::matchGameEnded へ再送出）
    void gameEnded(const MatchCoordinator::GameEndInfo& info);

    /**
     * @brief 時計更新通知
     * @param p1ms 先手残りms
     * @param p2ms 後手残りms
     * @param p1turn 現在手番が先手ならtrue
     * @param urgencyMs 盤の緊急カラー用ms（非緊急ならINT64_MAX）
     */
    void timeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 urgencyMs);

    void gameOverStateChanged(const MatchCoordinator::GameOverState& st);
    void requestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

    /// 検討モード終了時に発火する
    void considerationModeEnded();

    /// 詰み探索モード終了時に発火する
    void tsumeSearchModeEnded();

    /// 検討待機開始時に発火する（経過タイマー停止用）
    void considerationWaitingStarted();

private:
    // --- 内部ヘルパ ---

    GoTimes computeGoTimes() const;

    using EngineModelPair = EngineLifecycleManager::EngineModelPair;

    void ensureEngineManager();  ///< エンジンマネージャの遅延生成
    void ensureTimekeeper();     ///< 時計マネージャの遅延生成
    void ensureMatchTurnHandler(); ///< ターン進行ハンドラの遅延生成

private slots:
    void onUsiError(const QString& msg);

private:
    // --- コアオブジェクト（非所有、寿命はMainWindow側で管理） ---

    ShogiGameController* m_gc   = nullptr;   ///< ゲームコントローラ
    ShogiView*           m_view = nullptr;   ///< 盤面ビュー
    Hooks                m_hooks;             ///< UIコールバック群

    // --- エンジン管理 ---
    EngineLifecycleManager* m_engineManager = nullptr; ///< エンジンライフサイクルマネージャ

    // --- 時計管理 ---
    MatchTimekeeper* m_timekeeper = nullptr; ///< 時計マネージャ（Qt parent 管理）

    // --- ターン進行ハンドラ ---
    std::unique_ptr<MatchTurnHandler> m_turnHandler; ///< ターン進行・ストラテジー管理

    Player m_cur = P1;                        ///< 現在手番

    // --- モデル ---

    PlayMode                 m_playMode = PlayMode::NotStarted; ///< 対局モード

    // --- USI position文字列 ---

    QString m_positionStr1, m_positionPonder1;  ///< エンジン1用
    QString m_positionStr2, m_positionPonder2;  ///< エンジン2用

    // --- 棋譜/SFEN記録 ---

    int m_currentMoveIndex = 0;              ///< 現在の手数インデックス
    QStringList* m_sfenHistory = nullptr;     ///< SFEN履歴（共有ポインタ、非所有）
    QStringList m_sharedSfenRecord;          ///< 共有ポインタ未指定時の内部SFEN履歴
    QList<ShogiMove> m_gameMoves;          ///< 対局中の指し手リスト
    QList<ShogiMove>* m_externalGameMoves = nullptr; ///< 外部共有の指し手リスト（非所有、Deps経由）

    // 外部共有ポインタが設定されていればそれを使う（Deps経由で注入）
    QList<ShogiMove>& gameMovesRef() {
        return m_externalGameMoves ? *m_externalGameMoves : m_gameMoves;
    }

    int m_maxMoves = 0;                       ///< 最大手数（0=無制限）

    // --- 棋譜自動保存 ---

    bool m_autoSaveKifu = false;              ///< 自動保存フラグ
    QString m_kifuSaveDir;                    ///< 保存ディレクトリ

    // --- 対局者名 ---

    QString m_humanName1;                     ///< 先手人間名
    QString m_humanName2;                     ///< 後手人間名
    QString m_engineNameForSave1;             ///< 先手エンジン名（保存用）
    QString m_engineNameForSave2;             ///< 後手エンジン名（保存用）

    // --- 終局状態 ---

    GameOverState m_gameOver;                 ///< 終局状態


    // --- 検討・詰み探索セッション管理 ---

    std::unique_ptr<AnalysisSessionHandler> m_analysisSession; ///< 検討・詰み探索ハンドラ
    void ensureAnalysisSession();             ///< ハンドラの遅延生成

    // --- 終局処理ハンドラ ---

    GameEndHandler* m_gameEndHandler = nullptr; ///< 終局処理ハンドラ（Qt parent 管理）
    void ensureGameEndHandler();               ///< ハンドラの遅延生成

    // --- 対局開始オーケストレータ ---

    std::unique_ptr<GameStartOrchestrator> m_gameStartOrchestrator; ///< 対局開始オーケストレータ
    void ensureGameStartOrchestrator();        ///< オーケストレータの遅延生成

    // --- UNDO ---

    std::unique_ptr<MatchUndoHandler> m_undoHandler; ///< UNDO処理ハンドラ
    void ensureUndoHandler();                        ///< ハンドラの遅延生成

    // --- USI position履歴 ---

    QStringList m_positionStrHistory;          ///< position文字列の履歴

    // --- 対局履歴 ---

    static constexpr int kMaxGameHistories = 10;  ///< 最大対局履歴数
    QList<QStringList> m_allGameHistories;  ///< 過去の対局履歴
};

#endif // MATCHCOORDINATOR_H
