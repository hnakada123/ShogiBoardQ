/// @file mainwindowdepsfactory.cpp
/// @brief 各 Wiring/Controller 用の Deps 構造体を生成する純粋ファクトリの実装

#include "mainwindowdepsfactory.h"

DialogCoordinatorWiring::Deps MainWindowDepsFactory::createDialogCoordinatorDeps(
    const MainWindowRuntimeRefs& refs,
    const DialogCoordinatorCallbacks& callbacks)
{
    DialogCoordinatorWiring::Deps deps;

    // 生成に必要な依存
    deps.parentWidget = refs.parentWidget;
    deps.match = refs.match;
    deps.gameController = refs.gameController;

    // 複数コンテキストで共有
    deps.sfenRecord = refs.sfenRecord;
    deps.currentMoveIndex = refs.currentMoveIndex;
    deps.gameUsiMoves = refs.gameUsiMoves;
    deps.kifuLoadCoordinator = refs.kifuLoadCoordinator;
    deps.startSfenStr = refs.startSfenStr;

    // ConsiderationContext 固有
    deps.gameMoves = refs.gameMoves;
    deps.kifuRecordModel = refs.kifuRecordModel;
    deps.currentSfenStr = refs.currentSfenStr;
    deps.branchTree = refs.branchTree;
    deps.navState = refs.navState;
    deps.considerationModel = refs.considerationModel;

    // TsumeSearchContext 固有
    deps.positionStrList = refs.positionStrList;

    // KifuAnalysisContext 固有
    deps.moveRecords = refs.moveRecords;
    deps.activePly = refs.activePly;
    deps.gameInfoController = refs.gameInfoController;
    deps.evalChart = refs.evalChart;
    deps.presenter = refs.analysisPresenter;
    deps.getBoardFlipped = callbacks.getBoardFlipped;

    // 遅延初期化コールバック
    deps.getConsiderationWiring = callbacks.getConsiderationWiring;
    deps.getUiStatePolicyManager = callbacks.getUiStatePolicyManager;

    // 解析イベントハンドラ用
    deps.evalChartWidget = refs.evalChart;
    deps.analysisTab = refs.analysisTab;
    deps.playMode = refs.playMode;
    deps.navigateKifuViewToRow = callbacks.navigateKifuViewToRow;

    return deps;
}

KifuFileController::Deps MainWindowDepsFactory::createKifuFileControllerDeps(
    const MainWindowRuntimeRefs& refs,
    const KifuFileCallbacks& callbacks)
{
    KifuFileController::Deps deps;

    deps.parentWidget = refs.parentWidget;
    deps.statusBar = refs.statusBar;
    deps.saveFileName = refs.saveFileName;

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

    deps.mainWindow = refs.mainWindow;
    deps.navState = refs.navState;
    deps.branchTree = refs.branchTree;
    deps.displayCoordinator = refs.displayCoordinator;
    deps.kifuRecordModel = refs.kifuRecordModel;
    deps.shogiView = refs.shogiView;
    deps.evalGraphController = refs.evalGraphController;
    deps.sfenRecord = refs.sfenRecord;
    deps.activePly = refs.activePly;
    deps.currentSelectedPly = refs.currentSelectedPly;
    deps.currentMoveIndex = refs.currentMoveIndex;
    deps.currentSfenStr = refs.currentSfenStr;
    deps.skipBoardSyncForBranchNav = refs.skipBoardSyncForBranchNav;
    deps.csaGameCoordinator = refs.csaGameCoordinator;
    deps.playMode = refs.playMode;
    deps.match = refs.match;

    return deps;
}
