/// @file gamesubregistry.cpp
/// @brief Game系 状態・コントローラ管理の ensure* 実装
///
/// セッション管理は GameSessionSubRegistry、配線管理は GameWiringSubRegistry に委譲。

#include "gamesubregistry.h"
#include "gamesessionsubregistry.h"
#include "gamewiringsubregistry.h"
#include "mainwindow.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowserviceregistry.h"

#include "gamestatecontroller.h"
#include "kifunavigationcoordinator.h"
#include "matchcoordinatorwiring.h"
#include "recordpane.h"
#include "replaycontroller.h"
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

GameSubRegistry::GameSubRegistry(MainWindow& mw,
                                 MainWindowServiceRegistry* registry,
                                 MainWindowFoundationRegistry* foundation,
                                 QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_registry(registry)
    , m_foundation(foundation)
    , m_session(new GameSessionSubRegistry(mw, this, registry, foundation, this))
    , m_wiring(new GameWiringSubRegistry(mw, this, registry, foundation, this))
{
}

// ---------------------------------------------------------------------------
// 時間制御
// ---------------------------------------------------------------------------

void GameSubRegistry::ensureTimeController()
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

void GameSubRegistry::ensureReplayController()
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

void GameSubRegistry::ensureGameStateController()
{
    if (m_mw.m_gameStateController) return;

    m_foundation->ensureUiStatePolicyManager();

    MainWindowDepsFactory::GameStateControllerCallbacks cbs;
    cbs.enableArrowButtons = std::bind(&UiStatePolicyManager::transitionToIdle, m_mw.m_uiStatePolicy);
    cbs.setReplayMode = std::bind(&MainWindow::setReplayMode, &m_mw, std::placeholders::_1);
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

    m_mw.m_compositionRoot->ensureGameStateController(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_gameStateController);
}

// ---------------------------------------------------------------------------
// 対局開始コーディネーター
// ---------------------------------------------------------------------------

void GameSubRegistry::ensureGameStartCoordinator()
{
    if (m_mw.m_gameStart) return;

    m_wiring->ensureMatchCoordinatorWiring();
    m_mw.m_matchWiring->ensureMenuGameStartCoordinator();
    m_mw.m_gameStart = m_mw.m_matchWiring->menuGameStartCoordinator();
}

// ---------------------------------------------------------------------------
// 手番同期サービス
// ---------------------------------------------------------------------------

void GameSubRegistry::ensureTurnStateSyncService()
{
    if (!m_mw.m_turnStateSync) {
        createTurnStateSyncService();
    }
    refreshTurnStateSyncDeps();
}

void GameSubRegistry::createTurnStateSyncService()
{
    m_mw.m_turnStateSync = std::make_unique<TurnStateSyncService>();
}

void GameSubRegistry::refreshTurnStateSyncDeps()
{
    if (!m_mw.m_turnStateSync) return;

    TurnStateSyncService::Deps deps;
    deps.gameController = m_mw.m_gameController;
    deps.shogiView = m_mw.m_shogiView;
    deps.timeController = m_mw.m_timeController;
    deps.timePresenter = m_mw.m_timePresenter;
    deps.playMode = &m_mw.m_state.playMode;
    deps.turnManagerParent = &m_mw;
    deps.updateTurnStatus = std::bind(&GameSubRegistry::updateTurnStatus, this, std::placeholders::_1);
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

void GameSubRegistry::ensureUndoFlowService()
{
    if (!m_mw.m_undoFlowService) {
        createUndoFlowService();
    }
    refreshUndoFlowDeps();
}

void GameSubRegistry::createUndoFlowService()
{
    m_mw.m_undoFlowService = std::make_unique<UndoFlowService>();
}

void GameSubRegistry::refreshUndoFlowDeps()
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

void GameSubRegistry::updateTurnStatus(int currentPlayer)
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

void GameSubRegistry::setReplayMode(bool on)
{
    ensureReplayController();
    if (m_mw.m_replayController) {
        m_mw.m_replayController->setReplayMode(on);
    }
}
