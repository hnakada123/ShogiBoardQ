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
    deps.match = refs.gameService.match;
    deps.gameController = refs.gameService.gameController;

    // 検討・詰み探索・棋譜解析で共有（手順は分岐ツリーから取る）
    deps.startSfenStr = refs.state.startSfenStr;
    deps.kifuRecordModel = refs.models.kifuRecordModel;
    deps.currentSfenStr = refs.state.currentSfenStr;
    deps.navState = refs.branchNav.navState;
    deps.considerationModel = refs.models.considerationModel;

    // KifuAnalysisContext 固有
    deps.gameInfoController = refs.analysis.gameInfoController;
    deps.evalChart = refs.ui.evalChart;
    deps.presenter = refs.analysis.analysisPresenter;
    deps.getBoardFlipped = callbacks.getBoardFlipped;

    // 遅延初期化コールバック
    deps.getConsiderationWiring = callbacks.getConsiderationWiring;
    deps.getUiStatePolicyManager = callbacks.getUiStatePolicyManager;

    // 解析イベントハンドラ用
    deps.evalChartWidget = refs.ui.evalChart;
    deps.analysisTab = refs.ui.analysisTab;
    deps.playMode = refs.state.playMode;
    deps.navigateKifuViewToLine = callbacks.navigateKifuViewToLine;

    return deps;
}

KifuFileController::Deps MainWindowDepsFactory::createKifuFileControllerDeps(
    const MainWindowRuntimeRefs& refs,
    const KifuFileCallbacks& callbacks)
{
    KifuFileController::Deps deps;

    deps.parentWidget = refs.ui.parentWidget;
    deps.statusBar = refs.ui.statusBar;
    deps.gameInfo = refs.analysis.gameInfoController;
    deps.saveFileName = refs.kifu.saveFileName;

    // 準備コールバック
    deps.clearUiBeforeKifuLoad = callbacks.clearUiBeforeKifuLoad;
    deps.setReplayMode = callbacks.setReplayMode;
    deps.ensurePlayerInfoAndGameInfo = callbacks.ensurePlayerInfoAndGameInfo;
    deps.ensureGameRecordModel = callbacks.ensureGameRecordModel;
    deps.ensureKifuExportController = callbacks.ensureKifuExportController;
    deps.updateKifuExportDependencies = callbacks.updateKifuExportDependencies;
    deps.createAndWireKifuLoadCoordinator = callbacks.createAndWireKifuLoadCoordinator;
    deps.prepareKifuLoadCoordinatorForLive = callbacks.prepareKifuLoadCoordinatorForLive;

    // ゲッター
    deps.getGameRecordModel = callbacks.getGameRecordModel;
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
    deps.shogiView = refs.gameService.shogiView;
    deps.evalGraphController = refs.analysis.evalGraphController;
    deps.currentSfenStr = refs.state.currentSfenStr;
    deps.skipBoardSyncForBranchNav = refs.state.skipBoardSyncForBranchNav;
    deps.csaGameCoordinator = refs.gameService.csaGameCoordinator;
    deps.playMode = refs.state.playMode;
    deps.match = refs.gameService.match;

    return deps;
}
