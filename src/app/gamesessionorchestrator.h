#ifndef GAMESESSIONORCHESTRATOR_H
#define GAMESESSIONORCHESTRATOR_H

/// @file gamesessionorchestrator.h
/// @brief 対局ライフサイクル（開始・終了・連続対局）のイベントオーケストレータ

#include <QObject>
#include <functional>

#include "gamestartcoordinator.h"
#include "matchcoordinator.h"

class ConsecutiveGamesController;
class CsaGameCoordinator;
class DialogCoordinator;
class GameStateController;
class KifuBranchTree;
class KifuLoadCoordinator;
class KifuNavigationState;
class KifuRecordListModel;
class LiveGameSession;
class PreStartCleanupHandler;
class ReplayController;
class SessionLifecycleCoordinator;
class ShogiGameController;
class ShogiView;
class TimeControlController;

/**
 * @brief 対局ライフサイクルのイベントオーケストレーション
 *
 * MainWindow から対局の開始・終了・連続対局遷移のイベントハンドリングを分離し、
 * 既存コントローラ（GameStateController, SessionLifecycleCoordinator, ConsecutiveGamesController,
 * GameStartCoordinator 等）へ適切にルーティングする。
 *
 * SessionLifecycleCoordinator が状態遷移の純粋処理を担当するのに対し、
 * このクラスはシグナル受信時のオーケストレーション（遅延初期化・ルーティング・副作用）を担当する。
 */
class GameSessionOrchestrator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // === Controllers（ダブルポインタ：遅延生成のため） ===
        GameStateController** gameStateController = nullptr;
        SessionLifecycleCoordinator** sessionLifecycle = nullptr;
        ConsecutiveGamesController** consecutiveGamesController = nullptr;
        GameStartCoordinator** gameStart = nullptr;            ///< メニュー操作用
        PreStartCleanupHandler** preStartCleanupHandler = nullptr;
        DialogCoordinator** dialogCoordinator = nullptr;
        ReplayController** replayController = nullptr;
        TimeControlController** timeController = nullptr;
        CsaGameCoordinator** csaGameCoordinator = nullptr;
        MatchCoordinator** match = nullptr;

        // === Core objects（安定ポインタ） ===
        ShogiView* shogiView = nullptr;
        ShogiGameController* gameController = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        KifuRecordListModel* kifuModel = nullptr;

        // === State pointers（MainWindow の状態フィールドへの直接ポインタ） ===
        PlayMode* playMode = nullptr;
        QString* currentSfenStr = nullptr;
        QString* startSfenStr = nullptr;
        int* currentSelectedPly = nullptr;
        bool* bottomIsP1 = nullptr;
        GameStartCoordinator::TimeControl* lastTimeControl = nullptr;

        // === Branch navigation ===
        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;
        LiveGameSession** liveGameSession = nullptr;

        // === Lazy-init callbacks ===
        /// @note 各 ensure* は MainWindowServiceRegistry 内の対応メソッドに束縛される
        std::function<void()> ensureGameStateController;         ///< GameStateController 遅延初期化
        std::function<void()> ensureSessionLifecycleCoordinator; ///< SessionLifecycleCoordinator 遅延初期化
        std::function<void()> ensureConsecutiveGamesController;  ///< ConsecutiveGamesController 遅延初期化
        std::function<void()> ensureGameStartCoordinator;        ///< GameStartCoordinator 遅延初期化
        std::function<void()> ensurePreStartCleanupHandler;      ///< PreStartCleanupHandler 遅延初期化
        std::function<void()> ensureDialogCoordinator;           ///< DialogCoordinator 遅延初期化
        std::function<void()> ensureReplayController;            ///< ReplayController 遅延初期化

        // === Action callbacks ===
        std::function<void()> initMatchCoordinator;        ///< MatchCoordinator の遅延初期化
        std::function<void()> clearSessionDependentUi;     ///< セッション依存UIのクリア
        std::function<void()> updateJosekiWindow;          ///< 定跡ウィンドウ更新
        std::function<QStringList*()> sfenRecord;          ///< SFEN履歴取得
    };

    explicit GameSessionOrchestrator(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

public slots:
    // --- 対局開始（QAction から接続） ---
    void initializeGame();

    // --- 対局操作（QAction から接続） ---
    void handleResignation();
    void handleBreakOffGame();
    void movePieceImmediately();
    void stopTsumeSearch();
    void openWebsiteInExternalBrowser();

    // --- 対局ライフサイクルイベント（MatchCoordinatorWiring から接続） ---
    void onMatchGameEnded(const MatchCoordinator::GameEndInfo& info);
    void onGameOverStateChanged(const MatchCoordinator::GameOverState& st);
    void onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);
    void onResignationTriggered();
    void onPreStartCleanupRequested();
    void onApplyTimeControlRequested(const GameStartCoordinator::TimeControl& tc);
    void onGameStarted(const MatchCoordinator::StartOptions& opt);

    // --- 連続対局 ---
    void onConsecutiveStartRequested(const GameStartCoordinator::StartParams& params);
    void onConsecutiveGamesConfigured(int totalGames, bool switchTurn);
    void startNextConsecutiveGame();

    // --- SessionLifecycleCoordinator コールバック用 ---
    void startNewShogiGame();
    void invokeStartGame();

private:
    Deps m_deps;
};

#endif // GAMESESSIONORCHESTRATOR_H
