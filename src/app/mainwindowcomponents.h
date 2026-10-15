#ifndef MAINWINDOWCOMPONENTS_H
#define MAINWINDOWCOMPONENTS_H

/// @file mainwindowcomponents.h
/// @brief MainWindow が束ねる部品の置き場。生成と所有は MainWindowComposer

#include <QDockWidget>
#include <QPointer>
#include <memory>

class QTabWidget;
class QTimer;
class QWidget;

// モデル
class GameRecordModel;
class KifuAnalysisListModel;
class KifuBranchListModel;
class KifuRecordListModel;
class ShogiEngineThinkingModel;
class UsiCommLogModel;

// 盤・対局の芯
class BoardInteractionController;
class ShogiGameController;
class ShogiView;

// 画面部品
class EngineAnalysisTab;
class EvaluationChartWidget;
class RecordPane;

// 分岐ナビゲーション
class KifuBranchTree;
class KifuDisplayCoordinator;
class KifuNavigationController;
class KifuNavigationState;
class LiveGameSession;

// 対局
class ConsecutiveGamesController;
class GameSessionOrchestrator;
class GameStartCoordinator;
class GameStateController;
class MatchCoordinator;
class MatchCoordinatorWiring;
class PreStartCleanupHandler;
class ReplayController;
class SessionLifecycleCoordinator;
class TimeControlController;
class TimeDisplayPresenter;

// 棋譜
class BranchNavigationWiring;
class GameRecordLoadService;
class GameRecordUpdateService;
class JosekiWindowWiring;
class KifuExportController;
class KifuFileController;
class KifuLoadCoordinator;
class KifuNavigationCoordinator;

// UI
class AnalysisResultsPresenter;
class AnalysisTabWiring;
class BoardLoadService;
class BoardSetupController;
class BoardSyncPresenter;
class CommentCoordinator;
class ConsiderationPositionService;
class ConsiderationWiring;
class CsaGameWiring;
class DialogCoordinator;
class DialogCoordinatorWiring;
class DialogLaunchWiring;
class DockCreationService;
class DockLayoutManager;
class EvaluationGraphController;
class GameInfoPaneController;
class GameRecordPresenter;
class JishogiScoreDialogController;
class LanguageController;
class MainWindowActions;
class MainWindowAppearanceController;
class MenuWindowWiring;
class NyugyokuDeclarationHandler;
class PieceSoundPlayer;
class PlayModePolicyService;
class PlayerInfoController;
class PlayerInfoWiring;
class PositionEditController;
class PositionEditCoordinator;
class PvClickController;
class RecordNavigationHandler;
class RecordPaneWiring;
class TurnStateSyncService;
class UiActionsWiring;
class UiNotificationService;
class UiStatePolicyManager;
class UndoFlowService;
class UsiCommandController;

// 自動化
class AutomationServer;
#ifdef QT_DEBUG
class DebugScreenshotWiring;
#endif

/// ドックウィジェット群
struct DockWidgets {
    QDockWidget* evalChart = nullptr;
    QDockWidget* recordPane = nullptr;
    QDockWidget* gameInfo = nullptr;
    QDockWidget* thinking = nullptr;
    QDockWidget* consideration = nullptr;
    QDockWidget* usiLog = nullptr;
    QDockWidget* csaLog = nullptr;
    QDockWidget* comment = nullptr;
    QDockWidget* branchTree = nullptr;
    QDockWidget* menuWindow = nullptr;
    QDockWidget* josekiWindow = nullptr;
    QDockWidget* analysisResults = nullptr;
};

/**
 * @brief 起動時に MainWindowComposer が依存順に作る部品の一覧
 *
 * QObject は MainWindow を親にして作る（生ポインタ、親が破棄する）。
 * QObject でないものは unique_ptr で持つ。どれも起動時に1回だけ作り、作り直さない。
 * 例外は AutomationServer（--automation 指定時だけ）と CSA 通信対局の司令塔（CsaGameWiring が対局ごとに作る）。
 */
struct MainWindowComponents {
    // --- モデル ---
    KifuRecordListModel* kifuRecord = nullptr;
    KifuBranchListModel* kifuBranch = nullptr;
    ShogiEngineThinkingModel* thinking1 = nullptr;      ///< AnalysisTabWiring が作る
    ShogiEngineThinkingModel* thinking2 = nullptr;      ///< AnalysisTabWiring が作る
    ShogiEngineThinkingModel* consideration = nullptr;  ///< 検討・読み筋表示・矢印が同じものを使う
    KifuAnalysisListModel* analysis = nullptr;          ///< 棋譜解析の結果
    UsiCommLogModel* commLog1 = nullptr;
    UsiCommLogModel* commLog2 = nullptr;
    GameRecordModel* gameRecord = nullptr;

    // --- 盤・対局の芯 ---
    ShogiGameController* gameController = nullptr;
    ShogiView* shogiView = nullptr;
    QPointer<BoardInteractionController> boardController; ///< BoardSetupController が作る

    // --- 画面部品 ---
    QWidget* central = nullptr;
    QTabWidget* tab = nullptr;
    RecordPane* recordPane = nullptr;
    EngineAnalysisTab* analysisTab = nullptr;
    EvaluationChartWidget* evalChart = nullptr;
    DockWidgets docks;

    // --- 分岐ナビゲーション（棋譜の正本） ---
    KifuBranchTree* branchTree = nullptr;
    KifuNavigationState* navState = nullptr;
    KifuNavigationController* kifuNavController = nullptr;
    KifuDisplayCoordinator* displayCoordinator = nullptr;
    LiveGameSession* liveGameSession = nullptr;

    // --- 対局 ---
    std::unique_ptr<MatchCoordinatorWiring> matchWiring;
    MatchCoordinator* match = nullptr;                  ///< matchWiring が所有
    GameStartCoordinator* gameStart = nullptr;          ///< matchWiring の子
    TimeControlController* timeController = nullptr;
    TimeDisplayPresenter* timePresenter = nullptr;
    ReplayController* replayController = nullptr;
    GameStateController* gameStateController = nullptr;
    ConsecutiveGamesController* consecutiveGamesController = nullptr;
    PreStartCleanupHandler* preStartCleanupHandler = nullptr;
    GameSessionOrchestrator* gameSessionOrchestrator = nullptr;
    SessionLifecycleCoordinator* sessionLifecycle = nullptr;
    std::unique_ptr<PlayModePolicyService> playModePolicy;
    std::unique_ptr<TurnStateSyncService> turnStateSync;
    std::unique_ptr<UndoFlowService> undoFlowService;
    std::unique_ptr<CsaGameWiring> csaGameWiring;

    // --- 棋譜 ---
    std::unique_ptr<BranchNavigationWiring> branchNavWiring;
    std::unique_ptr<KifuNavigationCoordinator> kifuNavCoordinator;
    std::unique_ptr<GameRecordUpdateService> gameRecordUpdateService;
    std::unique_ptr<GameRecordLoadService> gameRecordLoadService;
    std::unique_ptr<KifuExportController> kifuExportController;
    std::unique_ptr<JosekiWindowWiring> josekiWiring;
    KifuFileController* kifuFileController = nullptr;
    KifuLoadCoordinator* kifuLoadCoordinator = nullptr;

    // --- UI ---
    std::unique_ptr<MainWindowAppearanceController> appearanceController;
    std::unique_ptr<DockCreationService> dockCreationService;
    std::unique_ptr<DockLayoutManager> dockLayoutManager;
    std::unique_ptr<EvaluationGraphController> evalGraphController;
    std::unique_ptr<AnalysisResultsPresenter> analysisPresenter;
    std::unique_ptr<LanguageController> languageController;
    std::unique_ptr<JishogiScoreDialogController> jishogiController;
    std::unique_ptr<NyugyokuDeclarationHandler> nyugyokuHandler;
    std::unique_ptr<UsiCommandController> usiCommandController;
    std::unique_ptr<QTimer> evalChartResizeTimer;
    std::unique_ptr<MainWindowActions> actions;
    RecordPaneWiring* recordPaneWiring = nullptr;
    AnalysisTabWiring* analysisWiring = nullptr;
    UiActionsWiring* actionsWiring = nullptr;
    DialogCoordinatorWiring* dialogCoordinatorWiring = nullptr;
    DialogCoordinator* dialogCoordinator = nullptr;     ///< dialogCoordinatorWiring が作る
    DialogLaunchWiring* dialogLaunchWiring = nullptr;
    MenuWindowWiring* menuWiring = nullptr;
    PieceSoundPlayer* pieceSoundPlayer = nullptr;
    PlayerInfoWiring* playerInfoWiring = nullptr;
    PlayerInfoController* playerInfoController = nullptr;   ///< playerInfoWiring が作る
    GameInfoPaneController* gameInfoController = nullptr;   ///< playerInfoWiring が作る
    UiStatePolicyManager* uiStatePolicy = nullptr;
    UiNotificationService* notificationService = nullptr;
    CommentCoordinator* commentCoordinator = nullptr;
    GameRecordPresenter* recordPresenter = nullptr;
    BoardSyncPresenter* boardSync = nullptr;
    BoardLoadService* boardLoadService = nullptr;
    BoardSetupController* boardSetupController = nullptr;
    PositionEditController* posEdit = nullptr;
    PositionEditCoordinator* posEditCoordinator = nullptr;
    PvClickController* pvClickController = nullptr;
    ConsiderationPositionService* considerationPositionService = nullptr;
    ConsiderationWiring* considerationWiring = nullptr;
    RecordNavigationHandler* recordNavHandler = nullptr;

    // --- 自動化 ---
    AutomationServer* automationServer = nullptr;       ///< --automation 指定時だけ作る
#ifdef QT_DEBUG
    std::unique_ptr<DebugScreenshotWiring> debugScreenshotWiring;
#endif
};

#endif // MAINWINDOWCOMPONENTS_H
