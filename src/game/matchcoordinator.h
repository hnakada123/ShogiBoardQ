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
#include "interruptedgamestate.h"
#include "startoptions.h"
#include "analysisoptions.h"
#include "matchcoordinatorhooks.h"

class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiGameController;
class ShogiClock;
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

    /// 終局原因（matchtypes.h の MatchCause の using エイリアス）
    using Cause = MatchCause;

    /// 終局情報（matchtypes.h の MatchGameEndInfo の using エイリアス）
    using GameEndInfo = MatchGameEndInfo;

    /// 直近の終局状態（matchtypes.h の MatchGameOverState の using エイリアス）
    using GameOverState = MatchGameOverState;

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

    /// エンジン投了通知を処理する（side: 投了したエンジンの手番）
    void handleEngineResign(Turn side);

    /// エンジン入玉宣言勝ち通知を処理する（side: 宣言したエンジンの手番）
    void handleEngineWin(Turn side);

    /// エンジンが反則手を返したときに、そのエンジンの反則負けとする（side: 反則したエンジンの手番）
    void handleEngineIllegalMove(Turn side);

    void flipBoard();

    /// エンジンポインタを差し替える（再生成時など）
    void updateUsiPtrs(Usi* e1, Usi* e2);

    /// 思考情報・通信ログを表示するモデルを設定する（エンジンを作る前に呼ぶ）
    void setEngineModels(UsiCommLogModel* comm1, ShogiEngineThinkingModel* think1,
                         UsiCommLogModel* comm2, ShogiEngineThinkingModel* think2);


    /// 中断を処理する（UI→司令塔）
    void handleBreakOff();

    const InterruptedGameState* interruptedGame() const { return m_interruptedGame.get(); }
    void discardInterruptedGame();
    bool resumeInterruptedGame();

    /// 入玉宣言を処理する
    void handleNyugyokuDeclaration(Turn declarer, bool success, bool isDraw);
    /// 終局・入玉宣言の知らせで対局者を下手・上手と呼ぶか（駒落ちで、対局情報の見出しが下手・上手のとき）。
    /// CSA 通信対局の入玉宣言の確認でも使う
    bool usesHandicapNames() const { return m_hooks.ui.usesHandicapNames && m_hooks.ui.usesHandicapNames(); }

    /// 対局中の指し手リストを取得する（CSA出力等で使用）
    const QList<ShogiMove>& gameMoves() const { return gameMovesRef(); }
    /// SFEN履歴への参照を取得する（UI共有用）
    QStringList* sfenRecordPtr() { return m_sfenHistory; }
    /// SFEN履歴への参照を取得する（const版）
    const QStringList* sfenRecordPtr() const { return m_sfenHistory; }

    // --- HvH: ターン計測 ---

    void armTurnTimerIfNeeded();
    void finishTurnTimerAndSetConsiderationFor(Turn mover);

    // --- HvE: 人間タイマー解除（Strategy委譲） ---

    void disarmHumanTimerIfNeeded();

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

    /// エンジンの初期化と開始（side: エンジンの手番）
    void initializeAndStartEngineFor(Turn side,
                                     const QString& enginePathIn,
                                     const QString& engineNameIn);

    /// 全エンジンを破棄する
    void destroyEngines(bool clearModels = true);

    // --- 終局状態管理 ---

    const GameOverState& gameOverState() const { return m_gameOver; }

    /// 終局状態をクリアする（対局開始/局面編集開始時に呼ぶ）
    void clearGameOverState();

    /// 終局状態を設定する
    void setGameOver(const GameEndInfo& info, bool appendMoveOnce = true);

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
    void ensureHumanAtBottomIfApplicable(const StartGameDialogData* dlg, Turn bottomSide);


    // --- 時間/手番・終局の処理 ---

    /// 時間切れを処理する（player: 時間が切れた側）
    void handlePlayerTimeOut(Turn player);

    /// 開始直後のタイマー起動や初手go判定を1本化する
    void startMatchTimingAndMaybeInitialGo();

    // --- PlayMode ---

    PlayMode playMode() const { return m_playMode; }

    /// 終局1回だけの棋譜追記と重複防止を一括処理する
    void appendGameOverLineAndMark(Cause cause, Turn loser);

    /// 人間着手後の後処理（Strategy へ委譲）
    void onHumanMove(const QPoint& from, const QPoint& to,
                     const QString& prettyMove);

    /// 現在の状況に応じて適切なエンジンへstopを送り、即時bestmoveを促す
    void forceImmediateMove();

    // --- USI送受 ---

    void sendStopToEngine(Usi* which);

    /// 持将棋（最大手数到達）時の棋譜1行追記と終局処理
    void handleMaxMovesJishogi();

    int maxMoves() const { return m_maxMoves; }

    // --- ShogiClockアクセサ ---

    ShogiClock*       clock();
    const ShogiClock* clock() const;

    /// 時計を差し替える（後からの配線用）
    void setClock(ShogiClock* clock);

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
    void gameInterrupted();
    void playModeChanged();
    /// 盤面反転の通知（→ MainWindowAppearanceController::onBoardFlipped）
    void boardFlipped(bool nowFlipped);

    /// 終局の通知（→ SessionLifecycleCoordinator::handleGameEnded、続いて UiStatePolicyManager::transitionToIdle）
    void gameEnded(const MatchCoordinator::GameEndInfo& info);
    void gameEndProcessed(const MatchCoordinator::GameEndInfo& info);

    /**
     * @brief 時計更新通知
     * @param blackMs 先手残りms
     * @param whiteMs 後手残りms
     * @param sideToMove 現在の手番
     * @param urgencyMs 盤の緊急カラー用ms（非緊急ならINT64_MAX）
     */
    void timeUpdated(qint64 blackMs, qint64 whiteMs, Turn sideToMove, qint64 urgencyMs);

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

    using EngineModelPair = EngineLifecycleManager::EngineModelPair;

    void createEngineManager();
    void createTimekeeper();
    void createMatchTurnHandler();
    EngineLifecycleManager::Refs buildEngineLifecycleRefs();
    EngineLifecycleManager::Hooks buildEngineLifecycleHooks();
    void configureMatchTurnHandler();
    void connectAnalysisSessionSignals();
    void configureAnalysisSession();

private slots:
    void onUsiError(const QString& msg);

private:
    // --- コアオブジェクト（非所有、寿命はMainWindow側で管理） ---

    ShogiGameController* m_gc   = nullptr;   ///< ゲームコントローラ
    Hooks                m_hooks;             ///< UIコールバック群

    // --- エンジン管理 ---
    EngineLifecycleManager* m_engineManager = nullptr; ///< エンジンライフサイクルマネージャ（Qt parent 管理）

    // --- 時計管理 ---
    MatchTimekeeper* m_timekeeper = nullptr; ///< 時計マネージャ（Qt parent 管理）

    // --- ターン進行ハンドラ ---
    std::unique_ptr<MatchTurnHandler> m_turnHandler; ///< ターン進行・ストラテジー管理

    // --- モデル ---

    PlayMode                 m_playMode = PlayMode::NotStarted; ///< 対局モード

    // --- USI position文字列 ---

    QString m_positionStr1, m_positionPonder1;  ///< エンジン1用
    QString m_positionStr2, m_positionPonder2;  ///< エンジン2用

    // --- 棋譜/SFEN記録 ---

    int m_currentMoveIndex = 0;              ///< 現在の手数インデックス
    StartOptions m_lastStartOptions;
    std::unique_ptr<InterruptedGameState> m_interruptedGame;
    void captureInterruptedGame();
    QStringList* m_sfenHistory = nullptr;     ///< SFEN履歴（共有ポインタ、非所有）
    QStringList m_sharedSfenRecord;          ///< 共有ポインタ未指定時の内部SFEN履歴
    QList<ShogiMove> m_gameMoves;          ///< 対局中の指し手リスト
    QList<ShogiMove>* m_externalGameMoves = nullptr; ///< 外部共有の指し手リスト（非所有、Deps経由）

    /// 指し手列を局面列の長さ（手数）までに切り詰める（対局開始時）
    void alignGameMovesWithSfenHistory();

    // 外部共有ポインタが設定されていればそれを使う（Deps経由で注入）
    QList<ShogiMove>& gameMovesRef() {
        return m_externalGameMoves ? *m_externalGameMoves : m_gameMoves;
    }
    const QList<ShogiMove>& gameMovesRef() const {
        return m_externalGameMoves ? *m_externalGameMoves : m_gameMoves;
    }

    int m_maxMoves = 0;                       ///< 最大手数（0=無制限）

    // --- 棋譜自動保存 ---

    bool m_autoSaveKifu = false;              ///< 自動保存フラグ
    QString m_kifuSaveDir;                    ///< 保存ディレクトリ

    // --- 終局状態 ---

    GameOverState m_gameOver;                 ///< 終局状態


    // --- 検討・詰み探索セッション管理 ---

    std::unique_ptr<AnalysisSessionHandler> m_analysisSession; ///< 検討・詰み探索ハンドラ
    void createAnalysisSession();

    // --- 終局処理ハンドラ ---

    GameEndHandler* m_gameEndHandler = nullptr; ///< 終局処理ハンドラ（Qt parent 管理）
    void createGameEndHandler();

    // --- 対局開始オーケストレータ ---

    std::unique_ptr<GameStartOrchestrator> m_gameStartOrchestrator; ///< 対局開始オーケストレータ
    void createGameStartOrchestrator();

    // --- UNDO ---

    std::unique_ptr<MatchUndoHandler> m_undoHandler; ///< UNDO処理ハンドラ
    void createUndoHandler();

    // --- USI position履歴 ---

    QStringList m_positionStrHistory;          ///< position文字列の履歴

    // --- 対局履歴 ---

    QList<QStringList> m_allGameHistories;  ///< 過去の対局履歴
};

#endif // MATCHCOORDINATOR_H
