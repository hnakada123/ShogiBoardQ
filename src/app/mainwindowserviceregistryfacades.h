#ifndef MAINWINDOWSERVICEREGISTRYFACADES_H
#define MAINWINDOWSERVICEREGISTRYFACADES_H

/// @file mainwindowserviceregistryfacades.h
/// @brief MainWindowServiceRegistry の責務別ファサード

class MainWindowServiceRegistry;
class QPoint;
class QString;

class MainWindowUiRegistryFacade
{
public:
    explicit MainWindowUiRegistryFacade(MainWindowServiceRegistry& registry);

    void ensureRecordPresenter();
    void ensureDialogCoordinator();
    void ensureDockLayoutManager();
    void ensureRecordNavigationHandler();
    void unlockGameOverStyle();
    void createMenuWindowDock();
    void clearEvalState();
    void buildGamePanels();
    void restoreWindowAndSync();
    void finalizeCoordinators();
    void initializeDialogLaunchWiring();
    void onRecordPaneMainRowChanged(int row);

private:
    MainWindowServiceRegistry& m_registry;
};

class MainWindowAnalysisRegistryFacade
{
public:
    explicit MainWindowAnalysisRegistryFacade(MainWindowServiceRegistry& registry);

    void ensurePvClickController();
    void ensureConsiderationPositionService();
    void ensureConsiderationWiring();
    void ensureUsiCommandController();

private:
    MainWindowServiceRegistry& m_registry;
};

class MainWindowBoardRegistryFacade
{
public:
    explicit MainWindowBoardRegistryFacade(MainWindowServiceRegistry& registry);

    void ensureBoardSetupController();
    void ensurePositionEditCoordinator();
    void ensureBoardLoadService();
    void setupBoardInteractionController();
    void handleMoveRequested(const QPoint& from, const QPoint& to);
    void handleMoveCommitted(int mover, int ply);
    void handleBeginPositionEditing();
    void handleFinishPositionEditing();
    void resetModels(const QString& hirateStartSfen);
    void resetUiState(const QString& hirateStartSfen);
    void clearSessionDependentUi();
    void loadBoardFromSfen(const QString& sfen);
    void loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen);

private:
    MainWindowServiceRegistry& m_registry;
};

#endif // MAINWINDOWSERVICEREGISTRYFACADES_H
