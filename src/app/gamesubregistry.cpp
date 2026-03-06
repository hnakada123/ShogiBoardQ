/// @file gamesubregistry.cpp
/// @brief Game系 状態・コントローラ管理の ensure* 実装
///
/// MainWindowServiceRegistry の Game 系メソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowfoundationregistry.h"

#include "consecutivegamescontroller.h"
#include "gamestatecontroller.h"
#include "kifunavigationcoordinator.h"
#include "matchcoordinatorwiring.h"
#include "recordpane.h"
#include "replaycontroller.h"
#include "sessionlifecyclecoordinator.h"
#include "shogiclock.h"
#include "shogiview.h"

#include <QTableView>
#include "timecontrolcontroller.h"
#include "turnmanager.h"
#include "turnstatesyncservice.h"
#include "uistatepolicymanager.h"
#include "undoflowservice.h"
#include "matchruntimequeryservice.h"
#include "logcategories.h"

#include <functional>

// ---------------------------------------------------------------------------
// 時間制御
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureTimeController()
{
    if (m_mw.m_timeController) return;

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_timeController = new TimeControlController(&m_mw);
    m_mw.m_timeController->setTimeDisplayPresenter(m_mw.m_timePresenter);
    m_mw.m_timeController->ensureClock();
}

// ---------------------------------------------------------------------------
// リプレイ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureReplayController()
{
    if (m_mw.m_replayController) return;

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_replayController = new ReplayController(&m_mw);
    m_mw.m_replayController->setClock(m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr);
    m_mw.m_replayController->setShogiView(m_mw.m_shogiView);
    m_mw.m_replayController->setGameController(m_mw.m_gameController);
    m_mw.m_replayController->setMatchCoordinator(m_mw.m_match);
    m_mw.m_replayController->setRecordPane(m_mw.m_recordPane);
}

// ---------------------------------------------------------------------------
// ゲーム状態コントローラ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureGameStateController()
{
    m_foundation->ensureUiStatePolicyManager();

    MainWindowDepsFactory::GameStateControllerCallbacks cbs;
    cbs.enableArrowButtons = [this]() {
        m_mw.m_uiStatePolicy->transitionToIdle();
    };
    cbs.setReplayMode = [this](bool replayMode) {
        m_mw.setReplayMode(replayMode);
    };
    cbs.refreshBranchTree = [this]() { m_mw.m_kifu.currentSelectedPly = 0; };
    cbs.updatePlyState = [this](int ap, int sp, int mi) {
        m_mw.m_kifu.activePly = ap;
        m_mw.m_kifu.currentSelectedPly = sp;
        m_mw.m_state.currentMoveIndex = mi;
        m_foundation->ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->syncNavStateToPly(sp);
    };
    cbs.updateBoardView = [this]() {
        if (m_mw.m_shogiView) m_mw.m_shogiView->update();
    };
    cbs.setGameOverStyleLock = [this](bool locked) {
        if (m_mw.m_shogiView) m_mw.m_shogiView->setGameOverStyleLock(locked);
    };
    cbs.setMouseClickMode = [this](bool mode) {
        if (m_mw.m_shogiView) m_mw.m_shogiView->setMouseClickMode(mode);
    };
    cbs.removeHighlightAllData = [this]() {
        if (m_mw.m_shogiView) m_mw.m_shogiView->removeHighlightAllData();
    };
    cbs.setKifuViewSingleSelection = [this]() {
        if (m_mw.m_recordPane && m_mw.m_recordPane->kifuView())
            m_mw.m_recordPane->kifuView()->setSelectionMode(QAbstractItemView::SingleSelection);
    };

    const MainWindowRuntimeRefs refs = m_mw.buildRuntimeRefs();
    if (!m_mw.m_gameStateController) {
        m_mw.m_compositionRoot->ensureGameStateController(refs, cbs, &m_mw, m_mw.m_gameStateController);
        return;
    }

    m_mw.m_compositionRoot->refreshGameStateControllerDeps(m_mw.m_gameStateController, refs, cbs);
}

// ---------------------------------------------------------------------------
// 対局開始コーディネーター
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureGameStartCoordinator()
{
    if (m_mw.m_gameStart) return;

    ensureMatchCoordinatorWiring();
    m_mw.m_matchWiring->ensureMenuGameStartCoordinator();
    m_mw.m_gameStart = m_mw.m_matchWiring->menuGameStartCoordinator();
    if (m_mw.m_consecutiveGamesController) {
        m_mw.m_consecutiveGamesController->setGameStartCoordinator(m_mw.m_gameStart);
    }
}

// ---------------------------------------------------------------------------
// 手番同期サービス
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::prepareTurnStateSyncService()
{
    if (!m_mw.m_turnStateSync) {
        createTurnStateSyncService();
    }
    refreshTurnStateSyncDeps();
}

void MainWindowServiceRegistry::createTurnStateSyncService()
{
    m_mw.m_turnStateSync = std::make_unique<TurnStateSyncService>();
}

void MainWindowServiceRegistry::refreshTurnStateSyncDeps()
{
    if (!m_mw.m_turnStateSync) return;

    TurnStateSyncService::Deps deps;
    deps.gameController = m_mw.m_gameController;
    deps.shogiView = m_mw.m_shogiView;
    deps.timeController = m_mw.m_timeController;
    deps.timePresenter = m_mw.m_timePresenter;
    deps.playMode = &m_mw.m_state.playMode;
    deps.turnManagerParent = &m_mw;
    deps.updateTurnStatus = [this](int currentPlayer) {
        updateTurnStatus(currentPlayer);
    };
    deps.onTurnManagerCreated = [this](TurnManager* tm) {
        connect(tm, &TurnManager::changed,
                &m_mw, &MainWindow::onTurnManagerChanged,
                Qt::UniqueConnection);
    };
    m_mw.m_turnStateSync->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 手戻しフロー
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::prepareUndoFlowService()
{
    if (m_mw.m_isShuttingDown) return;
    if (!m_mw.m_undoFlowService) {
        createUndoFlowService();
    }
    refreshUndoFlowDeps();
}

void MainWindowServiceRegistry::createUndoFlowService()
{
    m_mw.m_undoFlowService = std::make_unique<UndoFlowService>();
}

void MainWindowServiceRegistry::refreshUndoFlowDeps()
{
    if (!m_mw.m_undoFlowService) return;

    UndoFlowService::Deps deps;
    deps.match = m_mw.m_match;
    deps.evalGraphController = m_mw.m_evalGraphController.get();
    deps.playMode = &m_mw.m_state.playMode;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    m_mw.m_undoFlowService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 手番表示更新
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::updateTurnStatus(int currentPlayer)
{
    if (!m_mw.m_shogiView) return;

    ensureTimeController();
    ShogiClock* clock = m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr;
    if (!clock) {
        qCWarning(lcApp) << "ShogiClock not ready yet";
        return;
    }

    clock->setCurrentPlayer(currentPlayer);
    m_mw.m_shogiView->setActiveSide(currentPlayer == 1);
}

// ---------------------------------------------------------------------------
// MainWindow スロットからの転送
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::resetToInitialState()
{
    ensureSessionLifecycleCoordinator();
    m_mw.m_sessionLifecycle->resetToInitialState();
}

void MainWindowServiceRegistry::resetGameState()
{
    ensureSessionLifecycleCoordinator();
    m_mw.m_sessionLifecycle->resetGameState();
}

void MainWindowServiceRegistry::setReplayMode(bool on)
{
    ensureReplayController();
    if (m_mw.m_replayController) {
        m_mw.m_replayController->setReplayMode(on);
    }
}
