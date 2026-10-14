#ifndef GAMESESSIONORCHESTRATOR_H
#define GAMESESSIONORCHESTRATOR_H

/// @file gamesessionorchestrator.h
/// @brief UI 起点の対局操作を集約するオーケストレータ

#include <QObject>
#include <functional>

#include "gamestartcoordinator.h"
#include "matchcoordinator.h"

class CsaGameCoordinator;
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
        // === Controllers（使うときに取得する。遅延生成のものは取得時に作る） ===
        std::function<GameStateController*()> gameStateController;
        std::function<GameStartCoordinator*()> gameStart;
        std::function<DialogCoordinator*()> dialogCoordinator;
        std::function<CsaGameCoordinator*()> csaGameCoordinator;   ///< 通信対局をしていなければ nullptr
        std::function<MatchCoordinator*()> match;                 ///< 起動の途中で作られる（以後は変わらない）

        // === Core objects（安定ポインタ） ===
        ReplayController* replayController = nullptr;
        ShogiGameController* gameController = nullptr;
        KifuRecordListModel* kifuModel = nullptr;

        // === State pointers（MainWindow の状態フィールドへの直接ポインタ） ===
        PlayMode* playMode = nullptr;
        QString* currentSfenStr = nullptr;
        QString* startSfenStr = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;   ///< 対局の指し手列（開始時に表示中の手順から作り直す）
        Turn* bottomSide = nullptr;      ///< 盤の手前に表示している側

        // === Branch navigation ===
        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;
        LiveGameSession* liveSession = nullptr;
        std::function<void(bool)> setResumeAvailable;
        std::function<void()> prepareResumeUi;

        std::function<bool()> confirmDiscardUnsavedKifu;
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
    void onResignationTriggered();

private:
    Deps m_deps;
    int m_interruptedNodeId = -1;
};

#endif // GAMESESSIONORCHESTRATOR_H
