/// @file mainwindowserviceregistryfacades.cpp
/// @brief MainWindowServiceRegistry の責務別ファサード実装

#include "mainwindowserviceregistryfacades.h"

#include "mainwindowserviceregistry.h"

MainWindowUiRegistryFacade::MainWindowUiRegistryFacade(MainWindowServiceRegistry& registry)
    : m_registry(registry)
{
}

void MainWindowUiRegistryFacade::ensureRecordPresenter() { m_registry.ensureRecordPresenter(); }
void MainWindowUiRegistryFacade::ensureDialogCoordinator() { m_registry.ensureDialogCoordinator(); }
void MainWindowUiRegistryFacade::ensureDockLayoutManager() { m_registry.ensureDockLayoutManager(); }
void MainWindowUiRegistryFacade::ensureRecordNavigationHandler() { m_registry.ensureRecordNavigationHandler(); }
void MainWindowUiRegistryFacade::unlockGameOverStyle() { m_registry.unlockGameOverStyle(); }
void MainWindowUiRegistryFacade::createMenuWindowDock() { m_registry.createMenuWindowDock(); }
void MainWindowUiRegistryFacade::clearEvalState() { m_registry.clearEvalState(); }
void MainWindowUiRegistryFacade::buildGamePanels() { m_registry.buildGamePanels(); }
void MainWindowUiRegistryFacade::restoreWindowAndSync() { m_registry.restoreWindowAndSync(); }
void MainWindowUiRegistryFacade::finalizeCoordinators() { m_registry.finalizeCoordinators(); }
void MainWindowUiRegistryFacade::initializeDialogLaunchWiring() { m_registry.initializeDialogLaunchWiring(); }
void MainWindowUiRegistryFacade::onRecordPaneMainRowChanged(int row)
{
    m_registry.onRecordPaneMainRowChanged(row);
}

MainWindowAnalysisRegistryFacade::MainWindowAnalysisRegistryFacade(MainWindowServiceRegistry& registry)
    : m_registry(registry)
{
}

void MainWindowAnalysisRegistryFacade::ensurePvClickController() { m_registry.ensurePvClickController(); }
void MainWindowAnalysisRegistryFacade::ensureConsiderationPositionService()
{
    m_registry.ensureConsiderationPositionService();
}
void MainWindowAnalysisRegistryFacade::ensureConsiderationWiring() { m_registry.ensureConsiderationWiring(); }
void MainWindowAnalysisRegistryFacade::ensureUsiCommandController() { m_registry.ensureUsiCommandController(); }

MainWindowBoardRegistryFacade::MainWindowBoardRegistryFacade(MainWindowServiceRegistry& registry)
    : m_registry(registry)
{
}

void MainWindowBoardRegistryFacade::ensureBoardSetupController() { m_registry.ensureBoardSetupController(); }
void MainWindowBoardRegistryFacade::ensurePositionEditCoordinator()
{
    m_registry.ensurePositionEditCoordinator();
}
void MainWindowBoardRegistryFacade::ensureBoardLoadService() { m_registry.ensureBoardLoadService(); }
void MainWindowBoardRegistryFacade::setupBoardInteractionController()
{
    m_registry.setupBoardInteractionController();
}
void MainWindowBoardRegistryFacade::handleMoveRequested(const QPoint& from, const QPoint& to)
{
    m_registry.handleMoveRequested(from, to);
}
void MainWindowBoardRegistryFacade::handleMoveCommitted(int mover, int ply)
{
    m_registry.handleMoveCommitted(mover, ply);
}
void MainWindowBoardRegistryFacade::handleBeginPositionEditing()
{
    m_registry.handleBeginPositionEditing();
}
void MainWindowBoardRegistryFacade::handleFinishPositionEditing()
{
    m_registry.handleFinishPositionEditing();
}
void MainWindowBoardRegistryFacade::resetModels(const QString& hirateStartSfen)
{
    m_registry.resetModels(hirateStartSfen);
}
void MainWindowBoardRegistryFacade::resetUiState(const QString& hirateStartSfen)
{
    m_registry.resetUiState(hirateStartSfen);
}
void MainWindowBoardRegistryFacade::clearSessionDependentUi()
{
    m_registry.clearSessionDependentUi();
}
void MainWindowBoardRegistryFacade::loadBoardFromSfen(const QString& sfen)
{
    m_registry.loadBoardFromSfen(sfen);
}
void MainWindowBoardRegistryFacade::loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen)
{
    m_registry.loadBoardWithHighlights(currentSfen, prevSfen);
}
