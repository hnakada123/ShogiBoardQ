/// @file mainwindowserviceregistry.cpp
/// @brief 全サブレジストリを束ねるファサード実装

#include "mainwindowserviceregistry.h"
#include "mainwindowanalysisregistry.h"
#include "mainwindowboardregistry.h"
#include "mainwindowgameregistry.h"
#include "mainwindowkifuregistry.h"
#include "mainwindowuiregistry.h"

MainWindowServiceRegistry::MainWindowServiceRegistry(MainWindow& mw, QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_analysisRegistry(new MainWindowAnalysisRegistry(mw, *this, this))
    , m_boardRegistry(new MainWindowBoardRegistry(mw, *this, this))
    , m_gameRegistry(new MainWindowGameRegistry(mw, *this, this))
    , m_kifuRegistry(new MainWindowKifuRegistry(mw, *this, this))
    , m_uiRegistry(new MainWindowUiRegistry(mw, *this, this))
{
}

// ===== UI系 =====
void MainWindowServiceRegistry::ensureEvaluationGraphController() { m_uiRegistry->ensureEvaluationGraphController(); }
void MainWindowServiceRegistry::ensureRecordPresenter() { m_uiRegistry->ensureRecordPresenter(); }
void MainWindowServiceRegistry::ensurePlayerInfoController() { m_uiRegistry->ensurePlayerInfoController(); }
void MainWindowServiceRegistry::ensurePlayerInfoWiring() { m_uiRegistry->ensurePlayerInfoWiring(); }
void MainWindowServiceRegistry::ensureDialogCoordinator() { m_uiRegistry->ensureDialogCoordinator(); }
void MainWindowServiceRegistry::ensureMenuWiring() { m_uiRegistry->ensureMenuWiring(); }
void MainWindowServiceRegistry::ensureDockLayoutManager() { m_uiRegistry->ensureDockLayoutManager(); }
void MainWindowServiceRegistry::ensureDockCreationService() { m_uiRegistry->ensureDockCreationService(); }
void MainWindowServiceRegistry::ensureUiStatePolicyManager() { m_uiRegistry->ensureUiStatePolicyManager(); }
void MainWindowServiceRegistry::ensureUiNotificationService() { m_uiRegistry->ensureUiNotificationService(); }
void MainWindowServiceRegistry::ensureRecordNavigationHandler() { m_uiRegistry->ensureRecordNavigationHandler(); }
void MainWindowServiceRegistry::ensureLanguageController() { m_uiRegistry->ensureLanguageController(); }
void MainWindowServiceRegistry::unlockGameOverStyle() { m_uiRegistry->unlockGameOverStyle(); }
void MainWindowServiceRegistry::createMenuWindowDock() { m_uiRegistry->createMenuWindowDock(); }
void MainWindowServiceRegistry::clearEvalState() { m_uiRegistry->clearEvalState(); }

// ===== Game系 =====
void MainWindowServiceRegistry::ensureTimeController() { m_gameRegistry->ensureTimeController(); }
void MainWindowServiceRegistry::ensureReplayController() { m_gameRegistry->ensureReplayController(); }
void MainWindowServiceRegistry::ensureMatchCoordinatorWiring() { m_gameRegistry->ensureMatchCoordinatorWiring(); }
void MainWindowServiceRegistry::ensureGameStateController() { m_gameRegistry->ensureGameStateController(); }
void MainWindowServiceRegistry::ensureGameStartCoordinator() { m_gameRegistry->ensureGameStartCoordinator(); }
void MainWindowServiceRegistry::ensureCsaGameWiring() { m_gameRegistry->ensureCsaGameWiring(); }
void MainWindowServiceRegistry::ensurePreStartCleanupHandler() { m_gameRegistry->ensurePreStartCleanupHandler(); }
void MainWindowServiceRegistry::ensureTurnSyncBridge() { m_gameRegistry->ensureTurnSyncBridge(); }
void MainWindowServiceRegistry::ensureTurnStateSyncService() { m_gameRegistry->ensureTurnStateSyncService(); }
void MainWindowServiceRegistry::ensureLiveGameSessionStarted() { m_gameRegistry->ensureLiveGameSessionStarted(); }
void MainWindowServiceRegistry::ensureLiveGameSessionUpdater() { m_gameRegistry->ensureLiveGameSessionUpdater(); }
void MainWindowServiceRegistry::ensureUndoFlowService() { m_gameRegistry->ensureUndoFlowService(); }
void MainWindowServiceRegistry::ensureJishogiController() { m_gameRegistry->ensureJishogiController(); }
void MainWindowServiceRegistry::ensureNyugyokuHandler() { m_gameRegistry->ensureNyugyokuHandler(); }
void MainWindowServiceRegistry::ensureConsecutiveGamesController() { m_gameRegistry->ensureConsecutiveGamesController(); }
void MainWindowServiceRegistry::ensureGameSessionOrchestrator() { m_gameRegistry->ensureGameSessionOrchestrator(); }
void MainWindowServiceRegistry::ensureSessionLifecycleCoordinator() { m_gameRegistry->ensureSessionLifecycleCoordinator(); }
void MainWindowServiceRegistry::ensureCoreInitCoordinator() { m_gameRegistry->ensureCoreInitCoordinator(); }
void MainWindowServiceRegistry::clearGameStateFields() { m_gameRegistry->clearGameStateFields(); }
void MainWindowServiceRegistry::resetEngineState() { m_gameRegistry->resetEngineState(); }
void MainWindowServiceRegistry::updateTurnStatus(int currentPlayer) { m_gameRegistry->updateTurnStatus(currentPlayer); }
void MainWindowServiceRegistry::initMatchCoordinator() { m_gameRegistry->initMatchCoordinator(); }

// ===== Kifu系 =====
void MainWindowServiceRegistry::ensureBranchNavigationWiring() { m_kifuRegistry->ensureBranchNavigationWiring(); }
void MainWindowServiceRegistry::ensureKifuFileController() { m_kifuRegistry->ensureKifuFileController(); }
void MainWindowServiceRegistry::ensureKifuExportController() { m_kifuRegistry->ensureKifuExportController(); }
void MainWindowServiceRegistry::ensureGameRecordUpdateService() { m_kifuRegistry->ensureGameRecordUpdateService(); }
void MainWindowServiceRegistry::ensureGameRecordLoadService() { m_kifuRegistry->ensureGameRecordLoadService(); }
void MainWindowServiceRegistry::ensureKifuLoadCoordinatorForLive() { m_kifuRegistry->ensureKifuLoadCoordinatorForLive(); }
void MainWindowServiceRegistry::ensureGameRecordModel() { m_kifuRegistry->ensureGameRecordModel(); }
void MainWindowServiceRegistry::ensureCommentCoordinator() { m_kifuRegistry->ensureCommentCoordinator(); }
void MainWindowServiceRegistry::ensureKifuNavigationCoordinator() { m_kifuRegistry->ensureKifuNavigationCoordinator(); }
void MainWindowServiceRegistry::ensureJosekiWiring() { m_kifuRegistry->ensureJosekiWiring(); }
void MainWindowServiceRegistry::clearUiBeforeKifuLoad() { m_kifuRegistry->clearUiBeforeKifuLoad(); }
void MainWindowServiceRegistry::updateJosekiWindow() { m_kifuRegistry->updateJosekiWindow(); }

// ===== Analysis系 =====
void MainWindowServiceRegistry::ensurePvClickController() { m_analysisRegistry->ensurePvClickController(); }
void MainWindowServiceRegistry::ensureConsiderationPositionService() { m_analysisRegistry->ensureConsiderationPositionService(); }
void MainWindowServiceRegistry::ensureAnalysisPresenter() { m_analysisRegistry->ensureAnalysisPresenter(); }
void MainWindowServiceRegistry::ensureConsiderationWiring() { m_analysisRegistry->ensureConsiderationWiring(); }
void MainWindowServiceRegistry::ensureUsiCommandController() { m_analysisRegistry->ensureUsiCommandController(); }

// ===== Board/共通系 =====
void MainWindowServiceRegistry::ensureBoardSetupController() { m_boardRegistry->ensureBoardSetupController(); }
void MainWindowServiceRegistry::ensurePositionEditCoordinator() { m_boardRegistry->ensurePositionEditCoordinator(); }
void MainWindowServiceRegistry::ensurePositionEditController() { m_boardRegistry->ensurePositionEditController(); }
void MainWindowServiceRegistry::ensureBoardSyncPresenter() { m_boardRegistry->ensureBoardSyncPresenter(); }
void MainWindowServiceRegistry::ensureBoardLoadService() { m_boardRegistry->ensureBoardLoadService(); }
void MainWindowServiceRegistry::setupBoardInteractionController() { m_boardRegistry->setupBoardInteractionController(); }
void MainWindowServiceRegistry::handleMoveRequested(const QPoint& from, const QPoint& to) { m_boardRegistry->handleMoveRequested(from, to); }
void MainWindowServiceRegistry::handleMoveCommitted(int mover, int ply) { m_boardRegistry->handleMoveCommitted(mover, ply); }
void MainWindowServiceRegistry::handleBeginPositionEditing() { m_boardRegistry->handleBeginPositionEditing(); }
void MainWindowServiceRegistry::handleFinishPositionEditing() { m_boardRegistry->handleFinishPositionEditing(); }
void MainWindowServiceRegistry::resetModels(const QString& hirateStartSfen) { m_boardRegistry->resetModels(hirateStartSfen); }
void MainWindowServiceRegistry::resetUiState(const QString& hirateStartSfen) { m_boardRegistry->resetUiState(hirateStartSfen); }
void MainWindowServiceRegistry::clearSessionDependentUi() { m_boardRegistry->clearSessionDependentUi(); }
