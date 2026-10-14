/// @file mainwindowruntimeassembly.cpp
/// @brief 起動時に常設サービスの依存を確定する構築フェーズ
#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowactions.h"
#include "mainwindowfoundationregistry.h"
#include "kifusubregistry.h"
#include "boardinteractioncontroller.h"
#include "boardsyncpresenter.h"
#include "positioneditcontroller.h"
#include "kifuloadcoordinator.h"
#include "commentcoordinator.h"

void MainWindowServiceRegistry::assembleRuntimeServices()
{
    if (m_runtimeServicesReady) return;
    // 盤・棋譜ツリー・司令塔が揃ってから、常設サービスの依存を一度確定する。
    ensureReplayController();
    ensureGameStateController();
    ensureGameStartCoordinator();
    ensureRecordPresenter();
    ensureLiveGameSessionUpdater();
    m_kifu->ensureGameRecordUpdateService();
    ensureGameSessionOrchestrator();
    ensureSessionLifecycleCoordinator();
    ensurePreStartCleanupHandler();
    ensureBoardSetupController();
    ensurePositionEditCoordinator();
    ensureBoardLoadService();
    prepareTurnStateSyncService();
    prepareUndoFlowService();

    MainWindowActions::Deps d;
    d.boardSetup = m_mw.m_boardSetupController;
    d.boardController = m_mw.m_boardController;
    d.branchWiring = m_mw.m_branchNavWiring.get();
    d.gameController = m_mw.m_gameController;
    d.view = m_mw.m_shogiView;
    d.uiPolicy = m_mw.m_uiStatePolicy;
    d.recordUpdates = m_mw.m_gameRecordUpdateService.get();
    d.gameSession = m_mw.m_gameSessionOrchestrator;
    d.positionEdit = m_mw.m_registryParts.posEditCoordinator;
    d.comments = m_mw.m_commentCoordinator;
    d.playMode = &m_mw.m_state.playMode;
    d.rowGuard = &m_mw.m_kifu.onMainRowGuard;
    d.startSfen = &m_mw.m_state.startSfenStr;
    d.currentSfen = &m_mw.m_state.currentSfenStr;
    d.gameMoves = &m_mw.m_kifu.gameMoves;
    d.editedPositionReset.navState = m_mw.m_branchNav.navState;
    d.editedPositionReset.displayCoordinator = m_mw.m_branchNav.displayCoordinator;
    d.editedPositionReset.gameRecord = m_mw.m_models.gameRecord;
    d.editedPositionReset.sfenRecord = &m_mw.m_kifu.sfenRecord;
    d.editedPositionReset.branchTree = m_mw.m_branchNav.branchTree;
    d.editedPositionReset.liveGameSession = m_mw.m_branchNav.liveGameSession;
    d.confirmDiscardUnsavedKifu = [this]() { return confirmDiscardUnsavedKifu(); };
    d.updateJosekiWindow = [this]() { m_kifu->updateJosekiWindow(); };
    m_actions = std::make_unique<MainWindowActions>(std::move(d));
    m_runtimeServicesReady = true;
}
