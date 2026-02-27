#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/// @file mainwindow.h
/// @brief MainWindow の宣言

#include <QMainWindow>
#include <QPointer>
#include <memory>

#include "kifdisplayitem.h"
#include "matchcoordinator.h"
#include "gamestartcoordinator.h"
#include "shogigamecontroller.h"
#include "mainwindowruntimerefs.h"
#include "mainwindowstate.h"

// --- 前方宣言 ---
class QTimer;
class Usi;
class RecordPane;
class EngineAnalysisTab;
class BoardInteractionController;
class KifuLoadCoordinator;
class PositionEditController;
class JishogiScoreDialogController;
class NyugyokuDeclarationHandler;
class ConsecutiveGamesController;
class LanguageController;
class ConsiderationWiring;
class DockLayoutManager;
class DockCreationService;
class CommentCoordinator;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ShogiView;
class EvaluationChartWidget;
class BoardSyncPresenter;
class BoardLoadService;
class ConsiderationPositionService;
class AnalysisResultsPresenter;
class GameRecordPresenter;
class TimeDisplayPresenter;
class AnalysisTabWiring;
class RecordPaneWiring;
class UiActionsWiring;
class SfenCollectionDialog;
class GameInfoPaneController;
class EvaluationGraphController;
class TimeControlController;
class ReplayController;
class DialogCoordinator;
class KifuExportController;
class KifuFileController;
class GameStateController;
class PlayerInfoController;
class BoardSetupController;
class PvClickController;
class PositionEditCoordinator;
class CsaGameDialog;
class CsaGameCoordinator;
class CsaGameWiring;
class JosekiWindowWiring;
class PlayerInfoWiring;
class PreStartCleanupHandler;
class MenuWindowWiring;
class DialogLaunchWiring;
class UsiCommandController;
class RecordNavigationHandler;
class RecordNavigationWiring;
class DialogCoordinatorWiring;
class UiStatePolicyManager;
class MainWindowCompositionRoot;
class LiveGameSessionUpdater;
class KifuNavigationCoordinator;
class BranchNavigationWiring;
class PlayModePolicyService;
class MatchRuntimeQueryService;
class GameRecordUpdateService;
class UiNotificationService;
class UndoFlowService;
class TurnStateSyncService;
class GameRecordLoadService;
class GameSessionOrchestrator;
class SessionLifecycleCoordinator;
class MainWindowServiceRegistry;
class MainWindowSignalRouter;
class MainWindowAppearanceController;
class MainWindowMatchAdapter;
class MainWindowCoreInitCoordinator;
class MatchCoordinatorWiring;
class MainWindowLifecyclePipeline;

#ifdef QT_DEBUG
class DebugScreenshotWiring;
#endif

/// @brief アプリ全体の UI と対局進行を束ねるファサード
class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class MainWindowServiceRegistry;
    friend class MainWindowLifecyclePipeline;

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    EvaluationChartWidget* evalChart() const { return m_evalChart; }

public slots:
    void saveSettingsAndClose();
    void resetToInitialState();
    void resetGameState();
    void updateJosekiWindow();
    void undoLastTwoMoves();
    void beginPositionEditing();
    void finishPositionEditing();
    void onRecordPaneMainRowChanged(int row);
    void loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen);
    void setCurrentTurn();
    void displayGameRecord(const QList<KifDisplayItem>& disp);
    void onMoveRequested(const QPoint& from, const QPoint& to);
    void onMoveCommitted(ShogiGameController::Player mover, int ply);

protected:
    std::unique_ptr<Ui::MainWindow> ui;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onTurnManagerChanged(ShogiGameController::Player now);
    void setReplayMode(bool on);
    void loadBoardFromSfen(const QString& sfen);

private:
    // --- 状態集約構造体（定義は mainwindowstate.h） ---
    DockWidgets m_docks;
    PlayerState m_player;
    DataModels m_models;
    BranchNavigation m_branchNav;
    KifuState m_kifu;
    GameState m_state;

    // --- CompositionRoot / ServiceRegistry / SignalRouter ---
    std::unique_ptr<MainWindowCompositionRoot> m_compositionRoot;
    std::unique_ptr<MainWindowServiceRegistry> m_registry;
    std::unique_ptr<MainWindowSignalRouter> m_signalRouter;
    std::unique_ptr<MainWindowAppearanceController> m_appearanceController;
    std::unique_ptr<MainWindowLifecyclePipeline> m_pipeline;
    std::unique_ptr<MainWindowMatchAdapter> m_matchAdapter;
    std::unique_ptr<MainWindowCoreInitCoordinator> m_coreInit;

    // --- 将棋盤 / コントローラ ---
    ShogiView* m_shogiView = nullptr;
    ShogiGameController* m_gameController = nullptr;
    BoardInteractionController* m_boardController = nullptr;
    Usi* m_usi1 = nullptr;
    Usi* m_usi2 = nullptr;

    // --- UI 構成 ---
    QTabWidget* m_tab = nullptr;
    QWidget* m_central = nullptr;
    RecordPane* m_recordPane = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
    EvaluationChartWidget* m_evalChart = nullptr;

    // --- ダイアログ / 補助ウィンドウ ---
    CsaGameDialog* m_csaGameDialog = nullptr;
    QPointer<SfenCollectionDialog> m_sfenCollectionDialog;
    CsaGameCoordinator* m_csaGameCoordinator = nullptr;

    // --- 試合進行 ---
    MatchCoordinator* m_match = nullptr;
    GameStartCoordinator::TimeControl m_lastTimeControl;
    ReplayController* m_replayController = nullptr;
    std::unique_ptr<EvaluationGraphController> m_evalGraphController;
    std::unique_ptr<QTimer> m_evalChartResizeTimer;

    // --- コーディネータ / プレゼンタ ---
    TimeControlController* m_timeController = nullptr;
    GameInfoPaneController* m_gameInfoController = nullptr;
    KifuLoadCoordinator* m_kifuLoadCoordinator = nullptr;
    PositionEditController* m_posEdit = nullptr;
    BoardSyncPresenter* m_boardSync = nullptr;
    BoardLoadService* m_boardLoadService = nullptr;
    ConsiderationPositionService* m_considerationPositionService = nullptr;
    std::unique_ptr<AnalysisResultsPresenter> m_analysisPresenter;
    GameStartCoordinator* m_gameStart = nullptr;
    GameRecordPresenter* m_recordPresenter = nullptr;
    TimeDisplayPresenter* m_timePresenter = nullptr;
    AnalysisTabWiring* m_analysisWiring = nullptr;
    RecordPaneWiring* m_recordPaneWiring = nullptr;
    UiActionsWiring* m_actionsWiring = nullptr;

    // --- ダイアログ管理 ---
    DialogCoordinatorWiring* m_dialogCoordinatorWiring = nullptr;
    DialogCoordinator* m_dialogCoordinator = nullptr;

    // --- 棋譜ファイル / ゲーム状態 ---
    KifuFileController* m_kifuFileController = nullptr;
    std::unique_ptr<KifuExportController> m_kifuExportController;
    GameStateController* m_gameStateController = nullptr;
    std::unique_ptr<PlayModePolicyService> m_playModePolicy;
    std::unique_ptr<MatchRuntimeQueryService> m_queryService;
    PlayerInfoController* m_playerInfoController = nullptr;

    // --- 盤面操作 / 局面編集 ---
    BoardSetupController* m_boardSetupController = nullptr;
    PvClickController* m_pvClickController = nullptr;
    PositionEditCoordinator* m_posEditCoordinator = nullptr;

    // --- 通信対局 / ウィンドウ配線 ---
    std::unique_ptr<CsaGameWiring> m_csaGameWiring;
    std::unique_ptr<JosekiWindowWiring> m_josekiWiring;
    MenuWindowWiring* m_menuWiring = nullptr;
    PlayerInfoWiring* m_playerInfoWiring = nullptr;
    DialogLaunchWiring* m_dialogLaunchWiring = nullptr;
    std::unique_ptr<MatchCoordinatorWiring> m_matchWiring;
    PreStartCleanupHandler* m_preStartCleanupHandler = nullptr;

    // --- 補助コントローラ群 ---
    std::unique_ptr<JishogiScoreDialogController> m_jishogiController;
    std::unique_ptr<NyugyokuDeclarationHandler> m_nyugyokuHandler;
    ConsecutiveGamesController* m_consecutiveGamesController = nullptr;
    std::unique_ptr<LanguageController> m_languageController;
    ConsiderationWiring* m_considerationWiring = nullptr;
    std::unique_ptr<DockLayoutManager> m_dockLayoutManager;
    std::unique_ptr<DockCreationService> m_dockCreationService;
    CommentCoordinator* m_commentCoordinator = nullptr;
    std::unique_ptr<UsiCommandController> m_usiCommandController;
    RecordNavigationWiring* m_recordNavWiring = nullptr;
    UiStatePolicyManager* m_uiStatePolicy = nullptr;
    std::unique_ptr<LiveGameSessionUpdater> m_liveGameSessionUpdater;
    std::unique_ptr<GameRecordUpdateService> m_gameRecordUpdateService;
    UiNotificationService* m_notificationService = nullptr;
    std::unique_ptr<UndoFlowService> m_undoFlowService;
    std::unique_ptr<GameRecordLoadService> m_gameRecordLoadService;
    std::unique_ptr<TurnStateSyncService> m_turnStateSync;
    std::unique_ptr<BranchNavigationWiring> m_branchNavWiring;
    std::unique_ptr<KifuNavigationCoordinator> m_kifuNavCoordinator;
    SessionLifecycleCoordinator* m_sessionLifecycle = nullptr;
    GameSessionOrchestrator* m_gameSessionOrchestrator = nullptr;

#ifdef QT_DEBUG
    std::unique_ptr<DebugScreenshotWiring> m_debugScreenshotWiring;
#endif

    // --- private メソッド ---
    void updateKifuExportDependencies();
    MainWindowRuntimeRefs buildRuntimeRefs();
};

#endif // MAINWINDOW_H
