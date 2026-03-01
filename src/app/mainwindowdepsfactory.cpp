/// @file mainwindowdepsfactory.cpp
/// @brief 各 Wiring/Controller 用の Deps 構造体を生成する純粋ファクトリの実装

#include "mainwindowdepsfactory.h"

DialogCoordinatorWiring::Deps MainWindowDepsFactory::createDialogCoordinatorDeps(
    const MainWindowRuntimeRefs& refs,
    const DialogCoordinatorCallbacks& callbacks)
{
    DialogCoordinatorWiring::Deps deps;

    // 生成に必要な依存
    deps.parentWidget = refs.ui.parentWidget;
    deps.match = refs.match;
    deps.gameController = refs.gameController;

    // 複数コンテキストで共有
    deps.sfenRecord = refs.kifu.sfenRecord;
    deps.currentMoveIndex = refs.state.currentMoveIndex;
    deps.gameUsiMoves = refs.kifu.gameUsiMoves;
    deps.kifuLoadCoordinator = refs.kifuLoadCoordinator;
    deps.startSfenStr = refs.state.startSfenStr;

    // ConsiderationContext 固有
    deps.gameMoves = refs.kifu.gameMoves;
    deps.kifuRecordModel = refs.models.kifuRecordModel;
    deps.currentSfenStr = refs.state.currentSfenStr;
    deps.branchTree = refs.branchNav.branchTree;
    deps.navState = refs.branchNav.navState;
    deps.considerationModel = refs.models.considerationModel;

    // TsumeSearchContext 固有
    deps.positionStrList = refs.kifu.positionStrList;

    // KifuAnalysisContext 固有
    deps.moveRecords = refs.kifu.moveRecords;
    deps.activePly = refs.kifu.activePly;
    deps.gameInfoController = refs.gameInfoController;
    deps.evalChart = refs.ui.evalChart;
    deps.presenter = refs.analysisPresenter;
    deps.getBoardFlipped = callbacks.getBoardFlipped;

    // 遅延初期化コールバック
    deps.getConsiderationWiring = callbacks.getConsiderationWiring;
    deps.getUiStatePolicyManager = callbacks.getUiStatePolicyManager;

    // 解析イベントハンドラ用
    deps.evalChartWidget = refs.ui.evalChart;
    deps.analysisTab = refs.ui.analysisTab;
    deps.playMode = refs.state.playMode;
    deps.navigateKifuViewToRow = callbacks.navigateKifuViewToRow;

    return deps;
}

KifuFileController::Deps MainWindowDepsFactory::createKifuFileControllerDeps(
    const MainWindowRuntimeRefs& refs,
    const KifuFileCallbacks& callbacks)
{
    KifuFileController::Deps deps;

    deps.parentWidget = refs.ui.parentWidget;
    deps.statusBar = refs.ui.statusBar;
    deps.saveFileName = refs.kifu.saveFileName;

    // 準備コールバック
    deps.clearUiBeforeKifuLoad = callbacks.clearUiBeforeKifuLoad;
    deps.setReplayMode = callbacks.setReplayMode;
    deps.ensurePlayerInfoAndGameInfo = callbacks.ensurePlayerInfoAndGameInfo;
    deps.ensureGameRecordModel = callbacks.ensureGameRecordModel;
    deps.ensureKifuExportController = callbacks.ensureKifuExportController;
    deps.updateKifuExportDependencies = callbacks.updateKifuExportDependencies;
    deps.createAndWireKifuLoadCoordinator = callbacks.createAndWireKifuLoadCoordinator;
    deps.ensureKifuLoadCoordinatorForLive = callbacks.ensureKifuLoadCoordinatorForLive;

    // ゲッター
    deps.getKifuExportController = callbacks.getKifuExportController;
    deps.getKifuLoadCoordinator = callbacks.getKifuLoadCoordinator;

    return deps;
}

RecordNavigationWiring::Deps MainWindowDepsFactory::createRecordNavigationDeps(
    const MainWindowRuntimeRefs& refs)
{
    RecordNavigationWiring::Deps deps;

    deps.mainWindow = refs.ui.mainWindow;
    deps.navState = refs.branchNav.navState;
    deps.branchTree = refs.branchNav.branchTree;
    deps.displayCoordinator = refs.branchNav.displayCoordinator;
    deps.kifuRecordModel = refs.models.kifuRecordModel;
    deps.shogiView = refs.shogiView;
    deps.evalGraphController = refs.evalGraphController;
    deps.sfenRecord = refs.kifu.sfenRecord;
    deps.activePly = refs.kifu.activePly;
    deps.currentSelectedPly = refs.kifu.currentSelectedPly;
    deps.currentMoveIndex = refs.state.currentMoveIndex;
    deps.currentSfenStr = refs.state.currentSfenStr;
    deps.skipBoardSyncForBranchNav = refs.state.skipBoardSyncForBranchNav;
    deps.csaGameCoordinator = refs.csaGameCoordinator;
    deps.playMode = refs.state.playMode;
    deps.match = refs.match;

    return deps;
}
