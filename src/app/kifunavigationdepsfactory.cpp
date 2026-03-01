/// @file kifunavigationdepsfactory.cpp
/// @brief KifuNavigationCoordinator 用 Deps を生成するファクトリの実装

#include "kifunavigationdepsfactory.h"

KifuNavigationCoordinator::Deps KifuNavigationDepsFactory::createDeps(
    const MainWindowRuntimeRefs& refs,
    const Callbacks& callbacks)
{
    KifuNavigationCoordinator::Deps deps;

    // UI
    deps.recordPane = refs.ui.recordPane;
    deps.kifuRecordModel = refs.models.kifuRecordModel;

    // Board sync
    deps.boardSync = refs.boardSync;
    deps.shogiView = refs.shogiView;

    // Navigation state
    deps.navState = refs.branchNav.navState;
    deps.branchTree = refs.branchNav.branchTree;

    // External state pointers
    deps.activePly = refs.kifu.activePly;
    deps.currentSelectedPly = refs.kifu.currentSelectedPly;
    deps.currentMoveIndex = refs.state.currentMoveIndex;
    deps.currentSfenStr = refs.state.currentSfenStr;
    deps.skipBoardSyncForBranchNav = refs.state.skipBoardSyncForBranchNav;
    deps.playMode = refs.state.playMode;

    // Coordinators
    deps.match = refs.match;
    deps.analysisTab = refs.ui.analysisTab;
    deps.uiStatePolicy = refs.uiStatePolicy;

    // Callbacks
    deps.setCurrentTurn = callbacks.setCurrentTurn;
    deps.updateJosekiWindow = callbacks.updateJosekiWindow;
    deps.ensureBoardSyncPresenter = callbacks.ensureBoardSyncPresenter;
    deps.isGameActivelyInProgress = callbacks.isGameActivelyInProgress;
    deps.getSfenRecord = callbacks.getSfenRecord;

    return deps;
}
