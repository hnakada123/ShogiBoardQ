/// @file kifunavigationdepsfactory.cpp
/// @brief KifuNavigationCoordinator 用 Deps を生成するファクトリの実装

#include "kifunavigationdepsfactory.h"

KifuNavigationCoordinator::Deps KifuNavigationDepsFactory::createDeps(
    const MainWindowRuntimeRefs& refs,
    const Callbacks& callbacks)
{
    KifuNavigationCoordinator::Deps deps;

    // UI
    deps.recordPane = refs.recordPane;
    deps.kifuRecordModel = refs.kifuRecordModel;

    // Board sync
    deps.boardSync = refs.boardSync;
    deps.shogiView = refs.shogiView;

    // Navigation state
    deps.navState = refs.navState;
    deps.branchTree = refs.branchTree;

    // External state pointers
    deps.activePly = refs.activePly;
    deps.currentSelectedPly = refs.currentSelectedPly;
    deps.currentMoveIndex = refs.currentMoveIndex;
    deps.currentSfenStr = refs.currentSfenStr;
    deps.skipBoardSyncForBranchNav = refs.skipBoardSyncForBranchNav;
    deps.playMode = refs.playMode;

    // Coordinators
    deps.match = refs.match;
    deps.analysisTab = refs.analysisTab;
    deps.uiStatePolicy = refs.uiStatePolicy;

    // Callbacks
    deps.setCurrentTurn = callbacks.setCurrentTurn;
    deps.updateJosekiWindow = callbacks.updateJosekiWindow;
    deps.ensureBoardSyncPresenter = callbacks.ensureBoardSyncPresenter;
    deps.isGameActivelyInProgress = callbacks.isGameActivelyInProgress;
    deps.getSfenRecord = callbacks.getSfenRecord;

    return deps;
}
