#ifndef MAINWINDOWCOMPONENTS_H
#define MAINWINDOWCOMPONENTS_H

/// @file mainwindowcomponents.h
/// @brief MainWindow が束ねる部品の置き場。生成と所有は MainWindowComposer

#include <QDockWidget>
#include <memory>

#include "matchtimerecord.h"

// 棋譜の正本と表示
class BranchTreeEditController;
class GameRecordModel;
class KifuBranchTree;
class KifuDisplayPresenter;
class KifuNavigationController;
class KifuNavigationState;
class LiveGameSession;

// モデル
class KifuAnalysisListModel;
class KifuBranchListModel;
class KifuRecordListModel;
class ShogiEngineThinkingModel;
class UsiCommLogModel;

// 盤・時計
class BoardInteractionController;
class BoardMoveController;
class BoardSyncPresenter;
class PositionEditController;
class PositionEditCoordinator;
class ShogiClock;
class ShogiGameController;
class ShogiView;
class TimeDisplayPresenter;

// 画面部品
class BranchTreeManager;
class CommentEditorPanel;
class ConsiderationTabManager;
class CsaLogPanel;
class EvaluationChartWidget;
class GameInfoPane;
class RecordPane;
class ThinkingPanel;
class UsiLogPanel;

// 対局・検討・通信対局
class AnalysisSession;
class ConsecutiveGameSeries;
class CsaGameCoordinator;
class CsaGameSession;
class EngineLifecycleManager;
class GameSession;
class HumanTurnPolicy;
class MatchCoordinator;
class SessionResetCoordinator;

// 棋譜のファイル
class KifuExportController;
class KifuFileController;
class KifuLoadCoordinator;

// 表示
class AnalysisResultsPanel;
class CandidateArrowPresenter;
class CommentCoordinator;
class DockCreationService;
class DockLayoutManager;
class EngineDisplayPresenter;
class EvaluationGraphPresenter;
class MainWindowAppearanceController;
class PlayerInfoPresenter;

// 利用者の操作
class ConsiderationUiController;
class DialogLauncher;
class ImageExportController;
class JishogiScoreDialogController;
class JosekiWindowController;
class KifuAnalysisController;
class LanguageController;
class MainWindowActions;
class MenuWindowController;
class NyugyokuDeclarationHandler;
class PieceSoundPlayer;
class PvClickController;
class SettingsResetController;
class TsumeSearchController;
class UiNotificationService;
class UiStatePolicyManager;
class UsiCommandController;

// 自動化
class AutomationServer;
#ifdef QT_DEBUG
class DebugScreenshotShortcut;
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
 * QObject は MainWindow を親にして作る（生ポインタ、親が破棄する）。QObject でないものは unique_ptr で持つ。
 * どれも起動時に1回だけ作り、作り直さない（自動化 API のサーバーだけは --automation 指定時に作る）。
 */
struct MainWindowComponents {
    // --- 棋譜の正本（分岐ツリーと表示中の手） ---
    KifuBranchTree* branchTree = nullptr;
    KifuNavigationState* navState = nullptr;
    LiveGameSession* liveGameSession = nullptr;
    KifuNavigationController* kifuNavController = nullptr;
    GameRecordModel* gameRecord = nullptr;              ///< コメント・しおりの編集と書き出し

    // --- モデル ---
    KifuRecordListModel* kifuRecord = nullptr;
    KifuBranchListModel* kifuBranch = nullptr;
    ShogiEngineThinkingModel* thinking1 = nullptr;
    ShogiEngineThinkingModel* thinking2 = nullptr;
    ShogiEngineThinkingModel* consideration = nullptr;  ///< 検討・読み筋表示・矢印が同じものを使う
    KifuAnalysisListModel* analysis = nullptr;          ///< 棋譜解析の結果
    UsiCommLogModel* commLog1 = nullptr;
    UsiCommLogModel* commLog2 = nullptr;

    // --- 盤・時計 ---
    ShogiGameController* gameController = nullptr;
    ShogiView* shogiView = nullptr;
    BoardInteractionController* boardController = nullptr;
    BoardMoveController* boardMoves = nullptr;          ///< 盤で指した手を動作状態に応じて処理する
    BoardSyncPresenter* boardSync = nullptr;            ///< 盤を表示中の手に合わせる
    PositionEditController* posEdit = nullptr;
    PositionEditCoordinator* posEditCoordinator = nullptr;
    ShogiClock* clock = nullptr;
    MatchTimeRecord timeRecord;                         ///< 持ち時間と対局の開始・終了日時
    TimeDisplayPresenter* timePresenter = nullptr;

    // --- 画面部品 ---
    RecordPane* recordPane = nullptr;
    ThinkingPanel* thinkingPanel = nullptr;             ///< 思考ドック
    ConsiderationTabManager* considerationTab = nullptr; ///< 検討ドック
    UsiLogPanel* usiLog = nullptr;                      ///< USI通信ログドック
    CsaLogPanel* csaLog = nullptr;                      ///< CSA通信ログドック
    CommentEditorPanel* commentEditor = nullptr;        ///< 棋譜コメントドック
    BranchTreeManager* branchTreeView = nullptr;        ///< 分岐ツリードック
    EvaluationChartWidget* evalChart = nullptr;
    GameInfoPane* gameInfoPane = nullptr;
    DockWidgets docks;

    // --- 棋譜の表示と編集 ---
    KifuDisplayPresenter* kifuDisplay = nullptr;        ///< 棋譜欄・分岐候補・分岐ツリー
    BranchTreeEditController* branchEdit = nullptr;     ///< 分岐の編集と盤上の変化の記録
    CommentCoordinator* commentCoordinator = nullptr;

    // --- 対局・検討・通信対局 ---
    EngineLifecycleManager* engines = nullptr;          ///< エンジンの枠 E1・E2（対局と検討で共有）
    MatchCoordinator* match = nullptr;                  ///< ローカル対局の司令塔
    AnalysisSession* analysisSession = nullptr;         ///< 検討・詰み探索
    ConsecutiveGameSeries* consecutiveGames = nullptr;
    CsaGameCoordinator* csaGame = nullptr;              ///< 通信対局の司令塔
    GameSession* gameSession = nullptr;                 ///< 対局の利用者の操作と棋譜の受け手
    CsaGameSession* csaSession = nullptr;               ///< 通信対局の開始と画面への反映
    std::unique_ptr<HumanTurnPolicy> humanTurnPolicy;
    std::unique_ptr<SessionResetCoordinator> sessionReset;

    // --- 棋譜のファイル ---
    KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
    KifuExportController* kifuExportController = nullptr;
    KifuFileController* kifuFileController = nullptr;

    // --- 表示 ---
    MainWindowAppearanceController* appearance = nullptr;
    PlayerInfoPresenter* playerInfo = nullptr;
    EngineDisplayPresenter* engineDisplay = nullptr;   ///< 枠のエンジンの思考情報・通信ログの表示
    EvaluationGraphPresenter* evalGraph = nullptr;
    CandidateArrowPresenter* arrows = nullptr;          ///< 候補手の矢印
    AnalysisResultsPanel* analysisResults = nullptr;
    DockCreationService* dockCreationService = nullptr;
    DockLayoutManager* dockLayoutManager = nullptr;
    UiStatePolicyManager* uiStatePolicy = nullptr;
    UiNotificationService* notificationService = nullptr;

    // --- 利用者の操作 ---
    MainWindowActions* actions = nullptr;
    DialogLauncher* dialogLauncher = nullptr;
    KifuAnalysisController* kifuAnalysis = nullptr;
    TsumeSearchController* tsumeSearch = nullptr;
    ConsiderationUiController* considerationUi = nullptr;
    PvClickController* pvClickController = nullptr;
    UsiCommandController* usiCommandController = nullptr;
    JosekiWindowController* joseki = nullptr;
    MenuWindowController* menuWindowController = nullptr;
    LanguageController* languageController = nullptr;
    JishogiScoreDialogController* jishogiController = nullptr;
    NyugyokuDeclarationHandler* nyugyokuHandler = nullptr;
    PieceSoundPlayer* pieceSoundPlayer = nullptr;
    ImageExportController* images = nullptr;
    SettingsResetController* settingsReset = nullptr;

    // --- 自動化 ---
    AutomationServer* automationServer = nullptr;       ///< --automation 指定時だけ作る
#ifdef QT_DEBUG
    DebugScreenshotShortcut* debugScreenshot = nullptr;
#endif
};

#endif // MAINWINDOWCOMPONENTS_H
