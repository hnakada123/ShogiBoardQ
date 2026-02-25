/// @file mainwindowcompositionroot.cpp
/// @brief ensure* の生成ロジックを集約する CompositionRoot の実装

#include "mainwindowcompositionroot.h"
#include "dialogcoordinatorwiring.h"
#include "kifufilecontroller.h"
#include "recordnavigationwiring.h"

#include "gamestatecontroller.h"
#include "boardsetupcontroller.h"
#include "positioneditcoordinator.h"
#include "considerationwiring.h"
#include "commentcoordinator.h"
#include "pvclickcontroller.h"
#include "uistatepolicymanager.h"
#include "playerinfocontroller.h"
#include "playerinfowiring.h"
#include "engineanalysistab.h"

MainWindowCompositionRoot::MainWindowCompositionRoot(QObject* parent)
    : QObject(parent)
{
}

void MainWindowCompositionRoot::ensureDialogCoordinator(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::DialogCoordinatorCallbacks& callbacks,
    QObject* parent,
    DialogCoordinatorWiring*& wiring,
    DialogCoordinator*& coordinator)
{
    if (coordinator) return;

    if (!wiring) {
        wiring = new DialogCoordinatorWiring(parent);
    }

    auto deps = MainWindowDepsFactory::createDialogCoordinatorDeps(refs, callbacks);
    wiring->ensure(deps);
    coordinator = wiring->coordinator();
}

void MainWindowCompositionRoot::ensureKifuFileController(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::KifuFileCallbacks& callbacks,
    QObject* parent,
    KifuFileController*& controller)
{
    if (!controller) {
        controller = new KifuFileController(parent);
    }

    auto deps = MainWindowDepsFactory::createKifuFileControllerDeps(refs, callbacks);
    controller->updateDeps(deps);
}

void MainWindowCompositionRoot::ensureRecordNavigationWiring(
    const MainWindowRuntimeRefs& refs,
    QObject* parent,
    RecordNavigationWiring*& wiring)
{
    if (!wiring) {
        wiring = new RecordNavigationWiring(parent);
    }

    auto deps = MainWindowDepsFactory::createRecordNavigationDeps(refs);
    wiring->ensure(deps);
}

void MainWindowCompositionRoot::ensureGameStateController(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::GameStateControllerCallbacks& cbs,
    QObject* parent,
    GameStateController*& controller)
{
    if (controller) return;

    controller = new GameStateController(parent);
    controller->setMatchCoordinator(refs.match);
    controller->setShogiView(refs.shogiView);
    controller->setRecordPane(refs.recordPane);
    controller->setReplayController(refs.replayController);
    controller->setTimeController(refs.timeController);
    controller->setKifuLoadCoordinator(refs.kifuLoadCoordinator);
    controller->setKifuRecordModel(refs.kifuRecordModel);
    controller->setPlayMode(*refs.playMode);

    controller->setEnableArrowButtonsCallback(cbs.enableArrowButtons);
    controller->setSetReplayModeCallback(cbs.setReplayMode);
    controller->setRefreshBranchTreeCallback(cbs.refreshBranchTree);
    controller->setUpdatePlyStateCallback(cbs.updatePlyState);
}

void MainWindowCompositionRoot::ensureBoardSetupController(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::BoardSetupControllerCallbacks& cbs,
    QObject* parent,
    BoardSetupController*& controller)
{
    if (controller) return;

    controller = new BoardSetupController(parent);
    controller->setShogiView(refs.shogiView);
    controller->setGameController(refs.gameController);
    controller->setMatchCoordinator(refs.match);
    controller->setTimeController(refs.timeController);
    controller->setSfenRecord(refs.sfenRecord);
    controller->setGameMoves(refs.gameMoves);
    controller->setCurrentMoveIndex(refs.currentMoveIndex);

    controller->setEnsurePositionEditCallback(cbs.ensurePositionEdit);
    controller->setEnsureTimeControllerCallback(cbs.ensureTimeController);
    controller->setUpdateGameRecordCallback(cbs.updateGameRecord);
    controller->setRedrawEngine1GraphCallback(cbs.redrawEngine1Graph);
    controller->setRedrawEngine2GraphCallback(cbs.redrawEngine2Graph);
    controller->setRefreshBranchTreeCallback([]() {
        // LiveGameSession + KifuDisplayCoordinator が自動更新するため no-op
    });
}

void MainWindowCompositionRoot::ensurePositionEditCoordinator(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::PositionEditCoordinatorCallbacks& cbs,
    QObject* parent,
    PositionEditCoordinator*& coordinator)
{
    if (coordinator) return;

    coordinator = new PositionEditCoordinator(parent);
    coordinator->setShogiView(refs.shogiView);
    coordinator->setGameController(refs.gameController);
    coordinator->setBoardController(refs.boardController);
    coordinator->setPositionEditController(refs.positionEditController);
    coordinator->setMatchCoordinator(refs.match);
    coordinator->setReplayController(refs.replayController);
    coordinator->setSfenRecord(refs.sfenRecord);

    coordinator->setCurrentSelectedPly(refs.currentSelectedPly);
    coordinator->setActivePly(refs.activePly);
    coordinator->setStartSfenStr(refs.startSfenStr);
    coordinator->setCurrentSfenStr(refs.currentSfenStr);
    coordinator->setResumeSfenStr(refs.resumeSfenStr);
    coordinator->setOnMainRowGuard(refs.onMainRowGuard);

    coordinator->setApplyEditMenuStateCallback(cbs.applyEditMenuState);
    coordinator->setEnsurePositionEditCallback(cbs.ensurePositionEdit);

    PositionEditCoordinator::EditActions actions;
    actions.actionReturnAllPiecesToStand = cbs.actionReturnAllPiecesToStand;
    actions.actionSetHiratePosition = cbs.actionSetHiratePosition;
    actions.actionSetTsumePosition = cbs.actionSetTsumePosition;
    actions.actionChangeTurn = cbs.actionChangeTurn;
    coordinator->setEditActions(actions);
}

void MainWindowCompositionRoot::ensureConsiderationWiring(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::ConsiderationWiringCallbacks& cbs,
    QObject* parent,
    ConsiderationWiring*& wiring)
{
    if (wiring) return;

    ConsiderationWiring::Deps deps;
    deps.parentWidget = qobject_cast<QWidget*>(parent);
    deps.considerationTabManager = refs.analysisTab ? refs.analysisTab->considerationTabManager() : nullptr;
    deps.thinkingInfo1 = refs.analysisTab ? refs.analysisTab->info1() : nullptr;
    deps.shogiView = refs.shogiView;
    deps.match = refs.match;
    deps.dialogCoordinator = refs.dialogCoordinator;
    deps.considerationModel = refs.consideration;
    deps.commLogModel = refs.commLog1;
    deps.playMode = refs.playMode;
    deps.currentSfenStr = refs.currentSfenStr;
    deps.ensureDialogCoordinator = cbs.ensureDialogCoordinator;

    wiring = new ConsiderationWiring(deps, parent);
}

void MainWindowCompositionRoot::ensureCommentCoordinator(
    const MainWindowRuntimeRefs& refs,
    QObject* parent,
    CommentCoordinator*& coordinator)
{
    if (coordinator) return;

    coordinator = new CommentCoordinator(parent);
    if (refs.analysisTab) {
        coordinator->setCommentEditor(refs.analysisTab->commentEditor());
    }
    coordinator->setRecordPane(refs.recordPane);
    coordinator->setRecordPresenter(refs.recordPresenter);
    coordinator->setStatusBar(refs.statusBar);
    coordinator->setCurrentMoveIndex(refs.currentMoveIndex);
    coordinator->setCommentsByRow(refs.commentsByRow);
    coordinator->setGameRecordModel(refs.gameRecordModel);
    coordinator->setKifuRecordListModel(refs.kifuRecordModel);
}

void MainWindowCompositionRoot::ensurePvClickController(
    const MainWindowRuntimeRefs& refs,
    QObject* parent,
    PvClickController*& controller)
{
    if (controller) return;

    controller = new PvClickController(parent);
    controller->setThinkingModels(refs.thinking1, refs.thinking2);
    controller->setConsiderationModel(refs.consideration);
    controller->setLogModels(refs.commLog1, refs.commLog2);
    controller->setSfenRecord(refs.sfenRecord);
    controller->setGameMoves(refs.gameMoves);
    controller->setUsiMoves(refs.gameUsiMoves);
}

void MainWindowCompositionRoot::ensureUiStatePolicyManager(
    const MainWindowRuntimeRefs& refs,
    QObject* parent,
    UiStatePolicyManager*& controller)
{
    if (!controller) {
        controller = new UiStatePolicyManager(parent);
    }

    UiStatePolicyManager::Deps deps;
    deps.ui = refs.uiForm;
    deps.recordPane = refs.recordPane;
    deps.analysisTab = refs.analysisTab;
    deps.boardController = refs.boardController;
    controller->updateDeps(deps);
}

void MainWindowCompositionRoot::ensurePlayerInfoController(
    const MainWindowRuntimeRefs& refs,
    QObject* /*parent*/,
    PlayerInfoController*& controller)
{
    if (controller) return;

    if (refs.playerInfoWiring) {
        controller = refs.playerInfoWiring->playerInfoController();
    }
    if (controller) {
        controller->setEvalGraphController(refs.evalGraphController);
        controller->setLogModels(refs.commLog1, refs.commLog2);
        controller->setAnalysisTab(refs.analysisTab);
    }
}
