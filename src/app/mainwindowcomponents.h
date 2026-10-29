#ifndef MAINWINDOWCOMPONENTS_H
#define MAINWINDOWCOMPONENTS_H

/// @file mainwindowcomponents.h
/// @brief MainWindow が束ねる部品の置き場。生成と所有は MainWindowComposer

#include <QDockWidget>
#include <memory>


// 棋譜の正本と表示
class BranchTreeEditController;
class KifuDocument;
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
class PositionEditSession;
class ShogiClock;
class ShogiGameController;
class ShogiView;
class TimeDisplayPresenter;

// 画面部品
class BranchTreeView;
class CommentEditorPanel;
class ConsiderationPanel;
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
class EngineSlots;
class GameSession;
class HumanTurnPolicy;
class MatchCoordinator;
class SessionResetCoordinator;

// 棋譜のファイル
class KifuFileController;
class KifuLoader;

// 表示
class AnalysisResultsPanel;
class CandidateArrowPresenter;
class CommentCoordinator;
class DockRegistry;
class DockLayoutManager;
class EngineDisplayPresenter;
class EvaluationGraphPresenter;
class MainWindowAppearanceController;
class PlayerInfoPresenter;

// 利用者の操作
class ConsiderationUiController;
class DialogLauncher;
class ImageExportController;
class JosekiBookSession;
class JosekiRegistrationController;
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
class NotificationPresenter;
class UiStatePolicyManager;
class UsiCommandController;

// 自動化
class AutomationServer;
#ifdef QT_DEBUG
class DebugScreenshotShortcut;
#endif

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
    KifuDocument* document = nullptr;                   ///< 開いている棋譜（対局情報・対局の記録・保存先）

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
    PositionEditSession* positionEdit = nullptr;
    ShogiClock* clock = nullptr;
    TimeDisplayPresenter* timePresenter = nullptr;

    // --- 画面部品 ---
    RecordPane* recordPane = nullptr;
    ThinkingPanel* thinkingPanel = nullptr;             ///< 思考ドック
    ConsiderationPanel* considerationTab = nullptr; ///< 検討ドック
    UsiLogPanel* usiLog = nullptr;                      ///< USI通信ログドック
    CsaLogPanel* csaLog = nullptr;                      ///< CSA通信ログドック
    CommentEditorPanel* commentEditor = nullptr;        ///< 棋譜コメントドック
    BranchTreeView* branchTreeView = nullptr;           ///< 分岐ツリードック
    EvaluationChartWidget* evalChart = nullptr;
    GameInfoPane* gameInfoPane = nullptr;
    DockRegistry* dockRegistry = nullptr;  ///< ドック（表・作成・規定の配置・状態の保存）

    // --- 棋譜の表示と編集 ---
    KifuDisplayPresenter* kifuDisplay = nullptr;        ///< 棋譜欄・分岐候補・分岐ツリー
    BranchTreeEditController* branchEdit = nullptr;     ///< 分岐の編集と盤上の変化の記録
    CommentCoordinator* commentCoordinator = nullptr;

    // --- 対局・検討・通信対局 ---
    EngineSlots* engines = nullptr;          ///< エンジンの枠 E1・E2（対局と検討で共有）
    MatchCoordinator* match = nullptr;                  ///< ローカル対局の司令塔
    AnalysisSession* analysisSession = nullptr;         ///< 検討・詰み探索
    ConsecutiveGameSeries* consecutiveGames = nullptr;
    CsaGameCoordinator* csaGame = nullptr;              ///< 通信対局の司令塔
    GameSession* gameSession = nullptr;                 ///< 対局の利用者の操作と棋譜の受け手
    CsaGameSession* csaSession = nullptr;               ///< 通信対局の開始と画面への反映
    std::unique_ptr<HumanTurnPolicy> humanTurnPolicy;
    std::unique_ptr<SessionResetCoordinator> sessionReset;

    // --- 棋譜のファイル ---
    KifuLoader* kifuLoader = nullptr;
    KifuFileController* kifuFileController = nullptr;
    JosekiBookSession* josekiBook = nullptr;            ///< 開いている定跡ファイル（定跡ウィンドウが表示・編集する）

    // --- 表示 ---
    MainWindowAppearanceController* appearance = nullptr;
    PlayerInfoPresenter* playerInfo = nullptr;
    EngineDisplayPresenter* engineDisplay = nullptr;   ///< エンジンの表示（思考・検討・通信ログ・2つ目のエンジン欄・系列名）の持ち主
    EvaluationGraphPresenter* evalGraph = nullptr;
    CandidateArrowPresenter* arrows = nullptr;          ///< 候補手の矢印
    AnalysisResultsPanel* analysisResults = nullptr;
    DockLayoutManager* dockLayoutManager = nullptr;
    UiStatePolicyManager* uiStatePolicy = nullptr;
    NotificationPresenter* notifications = nullptr;

    // --- 利用者の操作 ---
    MainWindowActions* actions = nullptr;
    DialogLauncher* dialogLauncher = nullptr;
    KifuAnalysisController* kifuAnalysis = nullptr;
    TsumeSearchController* tsumeSearch = nullptr;
    ConsiderationUiController* considerationUi = nullptr;
    PvClickController* pvClickController = nullptr;
    UsiCommandController* usiCommandController = nullptr;
    JosekiWindowController* joseki = nullptr;
    JosekiRegistrationController* josekiRegistration = nullptr;  ///< 思考の表・棋譜解析の結果から定跡に登録
    MenuWindowController* menuWindowController = nullptr;
    LanguageController* languageController = nullptr;
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
