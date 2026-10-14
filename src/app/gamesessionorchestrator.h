#ifndef GAMESESSIONORCHESTRATOR_H
#define GAMESESSIONORCHESTRATOR_H

/// @file gamesessionorchestrator.h
/// @brief UI 起点の対局操作を集約するオーケストレータ

#include <QObject>

#include "gamestartcoordinator.h"
#include "matchcoordinator.h"

class CsaGameWiring;
class GameRecordModel;
class KifuFileController;
class PlayerInfoWiring;
class SessionLifecycleCoordinator;
class ShogiView;
class TimeControlController;
class UiStatePolicyManager;
class DialogCoordinator;
class GameStateController;
class KifuBranchTree;
class KifuNavigationState;
class KifuRecordListModel;
class LiveGameSession;
class ReplayController;
class ShogiGameController;

/**
 * @brief 開始ダイアログや対局操作など UI 起点の処理を束ねる
 *
 * 責務は開始ダイアログ、投了、中断、即時指し手、詰み探索停止、
 * 外部ブラウザ起動に限定する。対局ライフサイクルの状態遷移は
 * SessionLifecycleCoordinator / GameStateController / ConsecutiveGamesController
 * などの責務クラスへ分離済み。
 */
class GameSessionOrchestrator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        // === 部品（MainWindowComposer が起動時に作って渡す。非所有） ===
        GameStateController* gameStateController = nullptr;
        GameStartCoordinator* gameStart = nullptr;
        DialogCoordinator* dialogCoordinator = nullptr;
        CsaGameWiring* csaGameWiring = nullptr;          ///< 通信対局の司令塔は最初の CSA 対局で作られる
        MatchCoordinator* match = nullptr;
        ReplayController* replayController = nullptr;
        ShogiGameController* gameController = nullptr;
        KifuRecordListModel* kifuModel = nullptr;
        UiStatePolicyManager* uiStatePolicy = nullptr;   ///< 中断した対局の再開ボタン
        SessionLifecycleCoordinator* sessionLifecycle = nullptr;
        KifuFileController* kifuFileController = nullptr; ///< 未保存の棋譜の確認
        PlayerInfoWiring* playerInfoWiring = nullptr;    ///< 再開時に終局時刻を消す
        TimeControlController* timeController = nullptr;
        ShogiView* shogiView = nullptr;
        GameRecordModel* gameRecord = nullptr;

        // === 状態（AppState のフィールド） ===
        PlayMode* playMode = nullptr;
        QString* currentSfenStr = nullptr;
        QString* startSfenStr = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;   ///< 対局の指し手列（開始時に表示中の手順から作り直す）
        Turn* bottomSide = nullptr;              ///< 盤の手前に表示している側

        // === 分岐ナビゲーション ===
        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;
        LiveGameSession* liveSession = nullptr;
    };

    explicit GameSessionOrchestrator(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

public slots:
    void initializeGame();
    void handleResignation();
    void handleBreakOffGame();
    void handleResumeGame();
    void onGameInterrupted();
    void discardInterruptedGame();
    void movePieceImmediately();
    void stopTsumeSearch();
    void openWebsiteInExternalBrowser();

private:
    Deps m_deps;
    int m_interruptedNodeId = -1;
};

#endif // GAMESESSIONORCHESTRATOR_H
