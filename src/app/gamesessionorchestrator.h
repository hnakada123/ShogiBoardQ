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
class KifuLoadCoordinator;
class KifuNavigationState;
class KifuRecordListModel;
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
        // === Controllers（ダブルポインタ：遅延生成/再生成反映のため） ===
        GameStateController** gameStateController = nullptr;
        GameStartCoordinator** gameStart = nullptr;
        DialogCoordinator** dialogCoordinator = nullptr;
        CsaGameCoordinator** csaGameCoordinator = nullptr;
        MatchCoordinator** match = nullptr;

        // === Core objects（安定ポインタ） ===
        ReplayController* replayController = nullptr;
        ShogiGameController* gameController = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        KifuRecordListModel* kifuModel = nullptr;

        // === State pointers（MainWindow の状態フィールドへの直接ポインタ） ===
        PlayMode* playMode = nullptr;
        QString* currentSfenStr = nullptr;
        QString* startSfenStr = nullptr;
        int* currentSelectedPly = nullptr;
        bool* bottomIsP1 = nullptr;

        // === Branch navigation ===
        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;

        // === Lazy-init callbacks ===
        std::function<void()> ensureGameStateController;
        std::function<void()> ensureGameStartCoordinator;
        std::function<void()> ensureDialogCoordinator;
    };

    explicit GameSessionOrchestrator(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

public slots:
    void initializeGame();
    void handleResignation();
    void handleBreakOffGame();
    void movePieceImmediately();
    void stopTsumeSearch();
    void openWebsiteInExternalBrowser();
    void onResignationTriggered();

private:
    Deps m_deps;
};

#endif // GAMESESSIONORCHESTRATOR_H
