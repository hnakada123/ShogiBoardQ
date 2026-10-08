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
#include "matchcoordinator.h"

#include <QPointer>

/// ConsiderationWiring::Deps を refs/cbs から組み立てるヘルパー
static ConsiderationWiring::Deps buildConsiderationDeps(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::ConsiderationWiringCallbacks& cbs,
    QWidget* parentWidget)
{
    ConsiderationWiring::Deps deps;
    deps.parentWidget = parentWidget;
    deps.considerationTabManager = refs.ui.analysisTab ? refs.ui.analysisTab->considerationTabManager() : nullptr;
    deps.thinkingInfo1 = refs.ui.analysisTab ? refs.ui.analysisTab->info1() : nullptr;
    deps.shogiView = refs.gameService.shogiView;
    deps.match = refs.gameService.match;
    deps.dialogCoordinator = refs.uiController.dialogCoordinator;
    deps.considerationModel = refs.models.consideration;
    deps.commLogModel = refs.models.commLog1;
    deps.playMode = refs.state.playMode;
    deps.currentSfenStr = refs.state.currentSfenStr;
    deps.ensureDialogCoordinator = cbs.ensureDialogCoordinator;
    return deps;
}

MainWindowCompositionRoot::MainWindowCompositionRoot(QObject* parent)
    : QObject(parent)
{
}

// ============================================================
// ensure* (orchestration: create if needed + refresh deps)
// ============================================================

void MainWindowCompositionRoot::ensureDialogCoordinator(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::DialogCoordinatorCallbacks& callbacks,
    QObject* parent,
    DialogCoordinatorWiring*& wiring,
    DialogCoordinator*& coordinator)
{
    if (!wiring) {
        wiring = createDialogCoordinatorWiring(parent);
    }

    refreshDialogCoordinatorDeps(wiring, refs, callbacks);
    coordinator = wiring->coordinator();
}

void MainWindowCompositionRoot::ensureKifuFileController(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::KifuFileCallbacks& callbacks,
    QObject* parent,
    KifuFileController*& controller)
{
    if (!controller) {
        controller = createKifuFileController(parent);
    }

    refreshKifuFileControllerDeps(controller, refs, callbacks);
}

void MainWindowCompositionRoot::ensureRecordNavigationWiring(
    const MainWindowRuntimeRefs& refs,
    const RecordNavigationWiring::WiringTargets& targets,
    QObject* parent,
    RecordNavigationWiring*& wiring)
{
    if (!wiring) {
        wiring = createRecordNavigationWiring(parent);
    }

    refreshRecordNavigationWiringDeps(wiring, refs, targets);
}

void MainWindowCompositionRoot::ensureGameStateController(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::GameStateControllerCallbacks& cbs,
    QObject* parent,
    GameStateController*& controller)
{
    if (!controller) {
        controller = createGameStateController(parent);
    }
    refreshGameStateControllerDeps(controller, refs, cbs);
}

void MainWindowCompositionRoot::ensureBoardSetupController(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::BoardSetupControllerCallbacks& cbs,
    QObject* parent,
    BoardSetupController*& controller)
{
    if (!controller) {
        controller = createBoardSetupController(parent);
    }
    refreshBoardSetupControllerDeps(controller, refs, cbs);
}

void MainWindowCompositionRoot::ensurePositionEditCoordinator(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::PositionEditCoordinatorCallbacks& cbs,
    QObject* parent,
    PositionEditCoordinator*& coordinator)
{
    if (!coordinator) {
        coordinator = createPositionEditCoordinator(parent);
    }
    refreshPositionEditCoordinatorDeps(coordinator, refs, cbs);
}

void MainWindowCompositionRoot::ensureConsiderationWiring(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::ConsiderationWiringCallbacks& cbs,
    QObject* parent,
    ConsiderationWiring*& wiring)
{
    if (!wiring) {
        wiring = createConsiderationWiring(refs, cbs, parent);
    } else {
        wiring->updateDeps(buildConsiderationDeps(refs, cbs, qobject_cast<QWidget*>(parent)));
    }
}

void MainWindowCompositionRoot::ensureCommentCoordinator(
    const MainWindowRuntimeRefs& refs,
    QObject* parent,
    CommentCoordinator*& coordinator)
{
    if (!coordinator) {
        coordinator = createCommentCoordinator(parent);
    }
    refreshCommentCoordinatorDeps(coordinator, refs);
}

void MainWindowCompositionRoot::ensurePvClickController(
    const MainWindowRuntimeRefs& refs,
    QObject* parent,
    PvClickController*& controller)
{
    if (!controller) {
        controller = createPvClickController(parent);
    }
    refreshPvClickControllerDeps(controller, refs);
}

void MainWindowCompositionRoot::ensureUiStatePolicyManager(
    const MainWindowRuntimeRefs& refs,
    QObject* parent,
    UiStatePolicyManager*& controller)
{
    if (!controller) {
        controller = createUiStatePolicyManager(parent);
    }

    refreshUiStatePolicyManagerDeps(controller, refs);
}

void MainWindowCompositionRoot::ensurePlayerInfoController(
    const MainWindowRuntimeRefs& refs,
    QObject* /*parent*/,
    PlayerInfoController*& controller)
{
    if (refs.uiController.playerInfoWiring) {
        controller = refs.uiController.playerInfoWiring->playerInfoController();
    }
    refreshPlayerInfoControllerDeps(controller, refs);
}

// ============================================================
// create* (生成のみ)
// ============================================================

DialogCoordinatorWiring* MainWindowCompositionRoot::createDialogCoordinatorWiring(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new DialogCoordinatorWiring(parent);
}

KifuFileController* MainWindowCompositionRoot::createKifuFileController(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new KifuFileController(parent);
}

RecordNavigationWiring* MainWindowCompositionRoot::createRecordNavigationWiring(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new RecordNavigationWiring(parent);
}

GameStateController* MainWindowCompositionRoot::createGameStateController(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new GameStateController(parent);
}

BoardSetupController* MainWindowCompositionRoot::createBoardSetupController(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new BoardSetupController(parent);
}

PositionEditCoordinator* MainWindowCompositionRoot::createPositionEditCoordinator(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new PositionEditCoordinator(parent);
}

ConsiderationWiring* MainWindowCompositionRoot::createConsiderationWiring(
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::ConsiderationWiringCallbacks& cbs,
    QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    auto deps = buildConsiderationDeps(refs, cbs, qobject_cast<QWidget*>(parent));
    return new ConsiderationWiring(deps, parent);
}

CommentCoordinator* MainWindowCompositionRoot::createCommentCoordinator(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new CommentCoordinator(parent);
}

PvClickController* MainWindowCompositionRoot::createPvClickController(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new PvClickController(parent);
}

UiStatePolicyManager* MainWindowCompositionRoot::createUiStatePolicyManager(QObject* parent)
{
    // Lifetime: owned by parent (QObject parent)
    // Created: once on first use, never recreated
    return new UiStatePolicyManager(parent);
}

// ============================================================
// refresh*Deps (依存更新のみ)
// ============================================================

void MainWindowCompositionRoot::refreshDialogCoordinatorDeps(
    DialogCoordinatorWiring* wiring,
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::DialogCoordinatorCallbacks& callbacks)
{
    auto deps = MainWindowDepsFactory::createDialogCoordinatorDeps(refs, callbacks);
    wiring->ensure(deps);
}

void MainWindowCompositionRoot::refreshKifuFileControllerDeps(
    KifuFileController* controller,
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::KifuFileCallbacks& callbacks)
{
    auto deps = MainWindowDepsFactory::createKifuFileControllerDeps(refs, callbacks);
    controller->updateDeps(deps);
}

void MainWindowCompositionRoot::refreshRecordNavigationWiringDeps(
    RecordNavigationWiring* wiring,
    const MainWindowRuntimeRefs& refs,
    const RecordNavigationWiring::WiringTargets& targets)
{
    auto deps = MainWindowDepsFactory::createRecordNavigationDeps(refs);
    wiring->ensure(deps, targets);
}

void MainWindowCompositionRoot::refreshGameStateControllerDeps(
    GameStateController* controller,
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::GameStateControllerCallbacks& cbs)
{
    controller->setMatchCoordinator(refs.gameService.match);
    controller->setTimeController(refs.uiController.timeController);
    controller->setKifuRecordModel(refs.models.kifuRecordModel);
    controller->setPlayMode(refs.state.playMode ? *refs.state.playMode : PlayMode::NotStarted);

    GameStateController::Hooks hooks;
    hooks.enableArrowButtons = cbs.enableArrowButtons;
    hooks.setReplayMode = cbs.setReplayMode;
    hooks.refreshBranchTree = cbs.refreshBranchTree;
    hooks.updatePlyState = cbs.updatePlyState;
    hooks.updateBoardView = cbs.updateBoardView;
    hooks.setGameOverStyleLock = cbs.setGameOverStyleLock;
    hooks.setMouseClickMode = cbs.setMouseClickMode;
    hooks.removeHighlightAllData = cbs.removeHighlightAllData;
    hooks.setKifuViewSingleSelection = cbs.setKifuViewSingleSelection;
    controller->setHooks(hooks);
}

void MainWindowCompositionRoot::refreshBoardSetupControllerDeps(
    BoardSetupController* controller,
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::BoardSetupControllerCallbacks& cbs)
{
    controller->setShogiView(refs.gameService.shogiView);
    controller->setGameController(refs.gameService.gameController);
    controller->setMatchCoordinator(refs.gameService.match);
    controller->setPositionEditController(refs.uiController.positionEditController);
    controller->setPlayMode(refs.state.playMode ? *refs.state.playMode : PlayMode::NotStarted);
    controller->setSfenRecord(refs.kifu.sfenRecord);
    controller->setGameMoves(refs.kifu.gameMoves);
    controller->setCurrentMoveIndex(refs.state.currentMoveIndex);

    controller->setEnsurePositionEditCallback(cbs.ensurePositionEdit);
    controller->setRedrawEngine1GraphCallback(cbs.redrawEngine1Graph);
    controller->setRedrawEngine2GraphCallback(cbs.redrawEngine2Graph);
    controller->setVariationInputCallbacks(cbs.isVariationInput, cbs.recordVariationMove);
    controller->setRefreshBranchTreeCallback([]() {
        // LiveGameSession + KifuDisplayCoordinator が自動更新するため no-op
    });
}

void MainWindowCompositionRoot::refreshPositionEditCoordinatorDeps(
    PositionEditCoordinator* coordinator,
    const MainWindowRuntimeRefs& refs,
    const MainWindowDepsFactory::PositionEditCoordinatorCallbacks& cbs)
{
    coordinator->setShogiView(refs.gameService.shogiView);
    coordinator->setGameController(refs.gameService.gameController);
    coordinator->setBoardController(refs.uiController.boardController);
    coordinator->setPositionEditController(refs.uiController.positionEditController);
    coordinator->setMatchCoordinator(refs.gameService.match);
    coordinator->setReplayController(refs.kifuService.replayController);
    coordinator->setSfenRecord(refs.kifu.sfenRecord);

    coordinator->setCurrentSelectedPly(refs.kifu.currentSelectedPly);
    coordinator->setActivePly(refs.kifu.activePly);
    coordinator->setStartSfenStr(refs.state.startSfenStr);
    coordinator->setCurrentSfenStr(refs.state.currentSfenStr);
    coordinator->setResumeSfenStr(refs.state.resumeSfenStr);
    coordinator->setOnMainRowGuard(refs.kifu.onMainRowGuard);

    coordinator->setApplyEditMenuStateCallback(cbs.applyEditMenuState);
    coordinator->setEnsurePositionEditCallback(cbs.ensurePositionEdit);

    PositionEditCoordinator::EditActions actions;
    actions.actionReturnAllPiecesToStand = cbs.actionReturnAllPiecesToStand;
    actions.actionSetHiratePosition = cbs.actionSetHiratePosition;
    actions.actionSetTsumePosition = cbs.actionSetTsumePosition;
    actions.actionChangeTurn = cbs.actionChangeTurn;
    coordinator->setEditActions(actions);
}

void MainWindowCompositionRoot::refreshCommentCoordinatorDeps(
    CommentCoordinator* coordinator,
    const MainWindowRuntimeRefs& refs)
{
    coordinator->setCommentEditor(refs.ui.analysisTab ? refs.ui.analysisTab->commentEditor() : nullptr);
    coordinator->setRecordPane(refs.ui.recordPane);
    coordinator->setRecordPresenter(refs.kifuService.recordPresenter);
    coordinator->setStatusBar(refs.ui.statusBar);
    coordinator->setCurrentMoveIndex(refs.state.currentMoveIndex);
    coordinator->setGameRecordModel(refs.models.gameRecordModel);
    coordinator->setKifuRecordListModel(refs.models.kifuRecordModel);
}

void MainWindowCompositionRoot::refreshPvClickControllerDeps(
    PvClickController* controller,
    const MainWindowRuntimeRefs& refs)
{
    controller->setThinkingModels(refs.models.thinking1, refs.models.thinking2);
    controller->setConsiderationModel(refs.models.consideration);
    controller->setLogModels(refs.models.commLog1, refs.models.commLog2);
    controller->setSfenRecord(refs.kifu.sfenRecord);
    controller->setGameMoves(refs.kifu.gameMoves);
    controller->setUsiMoves(refs.kifu.gameUsiMoves);
}

void MainWindowCompositionRoot::refreshUiStatePolicyManagerDeps(
    UiStatePolicyManager* controller,
    const MainWindowRuntimeRefs& refs)
{
    UiStatePolicyManager::Deps deps;
    deps.ui = refs.ui.uiForm;
    deps.recordPane = refs.ui.recordPane;
    deps.analysisTab = refs.ui.analysisTab;
    deps.boardController = refs.uiController.boardController;
    // started 通知では MainWindow 側のモード同期より先に UI ポリシーが更新される。
    deps.getPlayMode = [match = QPointer<MatchCoordinator>(refs.gameService.match)]() {
        return match ? match->playMode() : PlayMode::NotStarted;
    };
    controller->updateDeps(deps);
}

void MainWindowCompositionRoot::refreshPlayerInfoControllerDeps(
    PlayerInfoController* controller,
    const MainWindowRuntimeRefs& refs)
{
    if (!controller) return;
    controller->setEvalGraphController(refs.analysis.evalGraphController);
    controller->setLogModels(refs.models.commLog1, refs.models.commLog2);
    controller->setAnalysisTab(refs.ui.analysisTab);
}
