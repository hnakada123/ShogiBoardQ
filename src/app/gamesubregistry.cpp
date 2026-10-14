/// @file gamesubregistry.cpp
/// @brief Game系 状態・コントローラ管理の ensure* 実装
///
/// MainWindowServiceRegistry の Game 系メソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"

#include "consecutivegamescontroller.h"
#include "csagamecoordinator.h"
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
#include "gamerecordmodel.h"
#include "playmodepolicyservice.h"
#include "logcategories.h"

#include <functional>

// ---------------------------------------------------------------------------
// 時間制御
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureTimeController()
{
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    if (!m_mw.m_timeController) {
        m_mw.m_timeController = new TimeControlController(&m_mw);
    }
    m_mw.m_timeController->setTimeDisplayPresenter(m_mw.m_timePresenter);
    m_mw.m_timeController->ensureClock();
}

// ---------------------------------------------------------------------------
// リプレイ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureReplayController()
{
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    if (!m_mw.m_replayController) {
        m_mw.m_replayController = new ReplayController(&m_mw);
    }
    m_mw.m_replayController->setClock(m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr);
    m_mw.m_replayController->setShogiView(m_mw.m_shogiView);
    m_mw.m_replayController->setGameController(m_mw.m_gameController);
    m_mw.m_replayController->setMatchCoordinator(m_mw.m_match);
}

// ---------------------------------------------------------------------------
// ゲーム状態コントローラ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureGameStateController()
{
    m_foundation->ensureUiStatePolicyManager();

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    if (!m_mw.m_gameStateController) {
        m_mw.m_gameStateController = new GameStateController(&m_mw);
    }
    GameStateController* controller = m_mw.m_gameStateController;
    controller->setMatchCoordinator(m_mw.m_match);
    controller->setTimeController(m_mw.m_timeController);
    controller->setKifuRecordModel(m_mw.m_models.kifuRecord);
    controller->setPlayMode(m_mw.m_state.playMode);

    GameStateController::Hooks hooks;
    hooks.enableArrowButtons = [this]() {
        m_mw.m_uiStatePolicy->transitionToIdle();
    };
    hooks.setReplayMode = [this](bool replayMode) {
        m_mw.setReplayMode(replayMode);
    };
    hooks.syncCursorToPly = [this](int ply) {
        m_foundation->ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->syncNavStateToPly(ply);
    };
    hooks.updateBoardView = [this]() {
        if (m_mw.m_shogiView) m_mw.m_shogiView->update();
    };
    hooks.setGameOverStyleLock = [this](bool locked) {
        if (m_mw.m_shogiView) m_mw.m_shogiView->setGameOverStyleLock(locked);
    };
    hooks.setMouseClickMode = [this](bool mode) {
        if (m_mw.m_shogiView) m_mw.m_shogiView->setMouseClickMode(mode);
    };
    hooks.removeHighlightAllData = [this]() {
        if (m_mw.m_shogiView) m_mw.m_shogiView->removeHighlightAllData();
    };
    hooks.setKifuViewSingleSelection = [this]() {
        if (m_mw.m_recordPane && m_mw.m_recordPane->kifuView())
            m_mw.m_recordPane->kifuView()->setSelectionMode(QAbstractItemView::SingleSelection);
    };
    controller->setHooks(hooks);
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

void MainWindowServiceRegistry::onTurnManagerChanged(Turn now)
{
    prepareTurnStateSyncService();
    m_mw.m_turnStateSync->onTurnManagerChanged(now);
}

void MainWindowServiceRegistry::setCurrentTurn()
{
    prepareTurnStateSyncService();
    m_mw.m_turnStateSync->setCurrentTurn();
}

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
    deps.isGameActivelyInProgress = [this]() {
        if (m_mw.m_state.playMode == PlayMode::CsaNetworkMode) {
            const CsaGameCoordinator* csa = csaGameCoordinator();
            return csa && csa->gameState() == CsaGameCoordinator::GameState::InGame;
        }
        return m_mw.m_playModePolicy && m_mw.m_playModePolicy->isGameActivelyInProgress();
    };
    deps.turnManagerParent = &m_mw;
    deps.updateTurnStatus = [this](Turn currentPlayer) {
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

void MainWindowServiceRegistry::undoLastTwoMoves()
{
    prepareUndoFlowService();
    m_mw.m_undoFlowService->undoLastTwoMoves();
}

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
    deps.undoTwoPlies = [this]() {
        return m_mw.m_match && !m_mw.m_match->gameOverState().isOver
            && m_mw.m_match->undoTwoPlies();
    };
    deps.markGameRecordDirty = [this]() {
        if (m_mw.m_models.gameRecord) m_mw.m_models.gameRecord->markDirty();
    };
    deps.liveSession = m_mw.m_branchNav.liveGameSession;
    deps.recordPresenter = m_mw.m_recordPresenter;
    deps.evalGraphController = m_mw.m_evalGraphController.get();
    deps.playMode = &m_mw.m_state.playMode;
    deps.sfenRecord = &m_mw.m_kifu.sfenRecord;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    m_mw.m_undoFlowService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 手番表示更新
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::updateTurnStatus(Turn currentPlayer)
{
    if (!m_mw.m_shogiView) return;

    ensureTimeController();
    ShogiClock* clock = m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr;
    if (!clock) {
        qCWarning(lcApp) << "ShogiClock not ready yet";
        return;
    }

    clock->setCurrentPlayer(currentPlayer);
    // 手番側の表示は、この直後に TurnStateSyncService が呼ぶ ShogiView::updateTurnIndicator が行う。
    // ここで setActiveSide を呼ぶと時計の緊急度（秒読み・残り10秒の警告色）が通常に戻り、
    // 着手の直前に時計が付けた警告色が次の秒まで消えるうえ、スタイルの付け直しが余分に走る。
}

// ---------------------------------------------------------------------------
// MainWindow スロットからの転送
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::resetToInitialState()
{
    if (!confirmDiscardUnsavedKifu()) return;
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
