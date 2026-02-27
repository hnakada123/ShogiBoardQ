#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/// @file mainwindow.h
/// @brief MainWindow の宣言


// --- Qt ヘッダー ---
#include <QMainWindow>
#include <QDockWidget>
#include <QPointer>
#include <memory>

// --- プロジェクトヘッダー（値型・MOC制約で必要） ---
#include "playmode.h"
#include "shogimove.h"
#include "kifdisplayitem.h"
#include "matchcoordinator.h"
#include "gamestartcoordinator.h"
#include "shogigamecontroller.h"
#include "mainwindowruntimerefs.h"

// --- 前方宣言（Qt） ---
class QTimer;

// --- 前方宣言（ポインタメンバのみ使用） ---
class KifuRecordListModel;
class KifuAnalysisListModel;
class KifuBranchListModel;
class KifuDisplay;
class ShogiEngineThinkingModel;
class Usi;
class UsiCommLogModel;
class RecordPane;
class EngineAnalysisTab;
class BoardInteractionController;
class KifuLoadCoordinator;
class PositionEditController;

// --- 前方宣言（新規コントローラ） ---
class JishogiScoreDialogController;
class NyugyokuDeclarationHandler;
class ConsecutiveGamesController;
class LanguageController;
class ConsiderationWiring;
class DockLayoutManager;
class DockCreationService;
class CommentCoordinator;

// --- 前方宣言（分岐ナビゲーション） ---
class KifuBranchTree;
class KifuNavigationState;
class KifuNavigationController;
class KifuDisplayCoordinator;
class BranchTreeWidget;
class LiveGameSession;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// --- 前方宣言（Qt / 各種コントローラ） ---
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
class GameRecordModel;
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
class MainWindowDockBootstrapper;
class MainWindowSignalRouter;
class MainWindowAppearanceController;
class MainWindowMatchAdapter;
class MainWindowCoreInitCoordinator;
class MatchCoordinatorWiring;
class MainWindowLifecyclePipeline;

#ifdef QT_DEBUG
class DebugScreenshotWiring;
#endif

/**
 * @brief アプリ全体の UI と対局進行を束ねるファサード
 *
 * MainWindow は「UI 表示の起点」でありつつ、各コントローラ/コーディネータへ
 * 処理を委譲するハブとして動作する。
 *
 * 設計上の方針:
 * - 直接ロジックは最小化し、`ensureXxx()` で遅延生成した専用クラスへ委譲する
 * - UI 操作（メニュー/ダイアログ/ドック）と対局進行（MatchCoordinator）を明確に接続する
 * - 棋譜行、盤面、手番、評価値グラフの同期点を MainWindow に集約する
 *
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

    friend class MainWindowUiBootstrapper;
    friend class MainWindowRuntimeRefsFactory;
    friend class MainWindowWiringAssembler;
    friend class MainWindowServiceRegistry;
    friend class MainWindowAnalysisRegistry;
    friend class MainWindowBoardRegistry;
    friend class MainWindowGameRegistry;
    friend class MainWindowKifuRegistry;
    friend class MainWindowUiRegistry;
    friend class MainWindowDockBootstrapper;
    friend class MainWindowLifecyclePipeline;

    // --- public ---
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    EvaluationChartWidget* evalChart() const { return m_evalChart; }

    // --- public slots ---
public slots:
    // --- エラー / 一般UI ---
    /// ウィンドウ設定を保存してアプリを終了する
    void saveSettingsAndClose();
    /// 盤面・棋譜・状態を初期状態にリセットする（SessionLifecycleCoordinatorへ委譲）
    void resetToInitialState();
    /// ゲーム状態変数のリセット（SessionLifecycleCoordinatorへ委譲）
    void resetGameState();
    /// 現在の局面で定跡ウィンドウを更新する
    void updateJosekiWindow();

    // --- その他操作 ---
    /// 直近2手を取り消す（「待った」相当）
    void undoLastTwoMoves();
    /// 局面編集モードを開始する
    void beginPositionEditing();
    /// 局面編集モードを終了し結果を反映する
    void finishPositionEditing();
    /// 棋譜欄の行選択変更を処理する（RecordPane::mainRowChanged に接続）
    void onRecordPaneMainRowChanged(int row);
    /// SFEN文字列から盤面とハイライトを更新する（分岐ナビゲーション用）
    void loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen);
    /// 現在の手番を設定する
    void setCurrentTurn();
    /// 棋譜表示リストを更新する
    void displayGameRecord(const QList<KifDisplayItem> disp);
    /// 盤面上の駒移動要求を処理する（BoardInteractionController::moveRequested に接続）
    void onMoveRequested(const QPoint& from, const QPoint& to);
    /// 指し手確定時の棋譜・盤面更新（ShogiGameController::moveCommitted に接続）
    void onMoveCommitted(ShogiGameController::Player mover, int ply);

    // --- protected ---
protected:
    std::unique_ptr<Ui::MainWindow> ui;        ///< Ui::MainWindowフォームへのポインタ
    void closeEvent(QCloseEvent* event) override;

    // --- private slots ---
private slots:
    // --- 対局終了 / 状態変化 ---

    /// 手番変更を処理する（TurnManager::turnChanged に接続）
    void onTurnManagerChanged(ShogiGameController::Player now);


    // --- リプレイ ---

    /// リプレイモードの開始/終了を切り替える
    void setReplayMode(bool on);

    // --- 棋譜表示 / 同期 ---

    /// SFEN文字列から直接盤面を読み込む（分岐ナビゲーション用）
    void loadBoardFromSfen(const QString& sfen);


    // --- private ---
private:
    // --- メンバー変数 ---

    // === サブシステム単位の状態集約構造体 ===

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
    DockWidgets m_docks;

    /// 対局者状態
    struct PlayerState {
        QString humanName1;
        QString humanName2;
        QString engineName1;
        QString engineName2;
        bool bottomIsP1 = true;
        bool lastP1Turn = true;
        qint64 lastP1Ms = 0;
        qint64 lastP2Ms = 0;
    };
    PlayerState m_player;

    /// データモデル群
    struct DataModels {
        KifuRecordListModel* kifuRecord = nullptr;
        KifuBranchListModel* kifuBranch = nullptr;
        ShogiEngineThinkingModel* thinking1 = nullptr;
        ShogiEngineThinkingModel* thinking2 = nullptr;
        ShogiEngineThinkingModel* consideration = nullptr;
        KifuAnalysisListModel* analysis = nullptr;
        UsiCommLogModel* commLog1 = nullptr;
        UsiCommLogModel* commLog2 = nullptr;
        GameRecordModel* gameRecord = nullptr;
    };
    DataModels m_models;

    /// 分岐ナビゲーション
    struct BranchNavigation {
        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;
        KifuNavigationController* kifuNavController = nullptr;
        KifuDisplayCoordinator* displayCoordinator = nullptr;
        BranchTreeWidget* branchTreeWidget = nullptr;
        LiveGameSession* liveGameSession = nullptr;
    };
    BranchNavigation m_branchNav;

    /// 棋譜状態
    struct KifuState {
        QStringList positionStrList;
        QStringList gameUsiMoves;
        QVector<QString> commentsByRow;
        int activePly = 0;
        int currentSelectedPly = 0;
        bool onMainRowGuard = false;
        QList<KifuDisplay*> moveRecords;
        QVector<ShogiMove> gameMoves;
        QString saveFileName;
    };
    KifuState m_kifu;

    /// ゲーム状態
    struct GameState {
        QString startSfenStr = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        QString currentSfenStr = QStringLiteral("startpos");
        QString resumeSfenStr;
        bool errorOccurred = false;
        int currentMoveIndex = 0;
        PlayMode playMode = PlayMode::NotStarted;
        bool skipBoardSyncForBranchNav = false;
    };
    GameState m_state;

    // --- CompositionRoot / ServiceRegistry / SignalRouter ---
    std::unique_ptr<MainWindowCompositionRoot> m_compositionRoot;    ///< ensure*生成ロジック集約（所有）
    std::unique_ptr<MainWindowServiceRegistry> m_registry;           ///< ensure*メソッド実装の集約先（所有）
    std::unique_ptr<MainWindowSignalRouter> m_signalRouter;          ///< シグナル配線ルーター（所有）
    std::unique_ptr<MainWindowAppearanceController> m_appearanceController; ///< UI外観コントローラ（所有）
    std::unique_ptr<MainWindowLifecyclePipeline> m_pipeline;            ///< 起動/終了フローパイプライン（所有）
    std::unique_ptr<MainWindowMatchAdapter> m_matchAdapter;            ///< 対局フック集約アダプタ（所有）
    std::unique_ptr<MainWindowCoreInitCoordinator> m_coreInit;       ///< コア初期化コーディネータ（所有）

    // --- 将棋盤 / コントローラ ---
    ShogiView*                   m_shogiView = nullptr;       ///< 将棋盤ビュー（非所有）
    ShogiGameController*         m_gameController = nullptr;  ///< ゲーム進行コントローラ（非所有）
    BoardInteractionController*  m_boardController = nullptr; ///< 盤面操作コントローラ（非所有）

    // --- USI / エンジン連携 ---
    Usi*        m_usi1 = nullptr;              ///< 先手側USIエンジン（非所有）
    Usi*        m_usi2 = nullptr;              ///< 後手側USIエンジン（非所有）

    // --- UI 構成 ---
    QTabWidget* m_tab = nullptr;              ///< メインタブウィジェット（非所有、AnalysisTabWiring管理）
    QWidget*    m_central = nullptr;          ///< セントラルウィジェット（非所有、ui管理）

    // --- ダイアログ / 補助ウィンドウ ---
    CsaGameDialog*           m_csaGameDialog = nullptr;           ///< CSA対局ダイアログ（非所有）
    QPointer<SfenCollectionDialog> m_sfenCollectionDialog;        ///< 局面集ビューアダイアログ（キャッシュ）

    // --- CSA通信対局コーディネータ ---
    CsaGameCoordinator*      m_csaGameCoordinator = nullptr;     ///< CSA通信対局管理（非所有）


    // --- 記録 / 評価 ---
    std::unique_ptr<EvaluationGraphController> m_evalGraphController; ///< 評価値グラフ管理（所有）

    // --- 時計 / 時刻管理 ---
    TimeControlController* m_timeController = nullptr; ///< 時間制御コントローラ（非所有）

    // --- 対局情報タブ ---
    GameInfoPaneController* m_gameInfoController = nullptr; ///< 対局情報ペイン管理（非所有）



    // --- 新UI部品 / ナビゲーション ---
    RecordPane*        m_recordPane = nullptr;   ///< 棋譜欄ウィジェット（非所有）
    EngineAnalysisTab* m_analysisTab = nullptr;  ///< エンジン解析タブ（非所有）

    // --- 評価値グラフ ---
    EvaluationChartWidget* m_evalChart = nullptr;     ///< 評価値グラフウィジェット（非所有）

    // --- 試合進行（司令塔） ---
    MatchCoordinator* m_match = nullptr;      ///< 対局進行の司令塔（非所有）

    // --- 連続対局設定 ---
    GameStartCoordinator::TimeControl m_lastTimeControl; ///< 前回の時間設定（連続対局用）

    // --- リプレイ制御 ---
    ReplayController* m_replayController = nullptr; ///< リプレイコントローラ（非所有）


    // --- 評価値グラフ高さ調整 ---
    std::unique_ptr<QTimer> m_evalChartResizeTimer;  ///< 高さ調整用デバウンスタイマー（所有）

    // --- コーディネータ / プレゼンタ ---
    KifuLoadCoordinator*      m_kifuLoadCoordinator = nullptr; ///< 棋譜読込コーディネータ（非所有）
    PositionEditController*   m_posEdit = nullptr;             ///< 局面編集コントローラ（非所有）
    BoardSyncPresenter*       m_boardSync = nullptr;           ///< 盤面同期プレゼンタ（非所有）
    BoardLoadService*         m_boardLoadService = nullptr;    ///< 盤面読み込みサービス（非所有）
    ConsiderationPositionService* m_considerationPositionService = nullptr; ///< 検討局面解決サービス（非所有）
    std::unique_ptr<AnalysisResultsPresenter> m_analysisPresenter; ///< 解析結果プレゼンタ（所有）
    GameStartCoordinator*     m_gameStart = nullptr;           ///< 対局開始コーディネータ（非所有）
    GameRecordPresenter*      m_recordPresenter = nullptr;     ///< 棋譜表示プレゼンタ（非所有）
    TimeDisplayPresenter*     m_timePresenter = nullptr;       ///< 時間表示プレゼンタ（非所有）
    AnalysisTabWiring*        m_analysisWiring = nullptr;      ///< 解析タブ配線（非所有）
    RecordPaneWiring*         m_recordPaneWiring = nullptr;    ///< 棋譜欄配線（非所有）
    UiActionsWiring*          m_actionsWiring    = nullptr;    ///< UIアクション配線（非所有）

    // --- ダイアログ管理 ---
    DialogCoordinatorWiring*  m_dialogCoordinatorWiring = nullptr; ///< DialogCoordinator配線（非所有）
    DialogCoordinator*        m_dialogCoordinator = nullptr;   ///< ダイアログ管理コーディネータ（非所有）

    // --- 棋譜ファイル管理 ---
    KifuFileController*       m_kifuFileController = nullptr;   ///< 棋譜ファイル操作コントローラ（所有）
    std::unique_ptr<KifuExportController> m_kifuExportController; ///< 棋譜エクスポートコントローラ（所有）

    // --- ゲーム状態管理 ---
    GameStateController*      m_gameStateController = nullptr;  ///< ゲーム状態コントローラ（非所有）
    std::unique_ptr<PlayModePolicyService> m_playModePolicy;     ///< プレイモード判定サービス（所有）
    std::unique_ptr<MatchRuntimeQueryService> m_queryService;    ///< 対局実行時クエリサービス（所有）

    // --- 対局者情報管理 ---
    PlayerInfoController*     m_playerInfoController = nullptr; ///< 対局者情報コントローラ（非所有）

    // --- 盤面操作配線 ---
    BoardSetupController*     m_boardSetupController = nullptr; ///< 盤面操作配線コントローラ（非所有）

    // --- 読み筋クリック処理 ---
    PvClickController*        m_pvClickController = nullptr;    ///< 読み筋クリックコントローラ（非所有）

    // --- 局面編集調整 ---
    PositionEditCoordinator*  m_posEditCoordinator = nullptr;   ///< 局面編集コーディネータ（非所有）

    // --- 通信対局 / ウィンドウ配線 ---
    std::unique_ptr<CsaGameWiring> m_csaGameWiring;              ///< CSA通信対局UI配線（所有）
    std::unique_ptr<JosekiWindowWiring> m_josekiWiring;         ///< 定跡ウィンドウUI配線（所有）
    MenuWindowWiring*         m_menuWiring = nullptr;           ///< メニューウィンドウUI配線（非所有）
    PlayerInfoWiring*         m_playerInfoWiring = nullptr;     ///< 対局者情報UI配線（非所有）
    DialogLaunchWiring*       m_dialogLaunchWiring = nullptr;   ///< ダイアログ起動配線（非所有）
    std::unique_ptr<MatchCoordinatorWiring> m_matchWiring;       ///< MatchCoordinator配線（所有）

    // --- 対局開始前クリーンアップ ---
    PreStartCleanupHandler*   m_preStartCleanupHandler = nullptr; ///< 対局開始前クリーンアップハンドラ（非所有）

    // --- 言語設定 ---
    // --- 補助コントローラ群 ---
    std::unique_ptr<JishogiScoreDialogController> m_jishogiController;  ///< 持将棋スコアダイアログコントローラ（所有）
    std::unique_ptr<NyugyokuDeclarationHandler> m_nyugyokuHandler;    ///< 入玉宣言ハンドラ（所有）
    ConsecutiveGamesController* m_consecutiveGamesController = nullptr; ///< 連続対局コントローラ（非所有）
    std::unique_ptr<LanguageController> m_languageController;          ///< 言語設定コントローラ（所有）
    ConsiderationWiring* m_considerationWiring = nullptr;              ///< 検討モード配線（非所有）
    std::unique_ptr<DockLayoutManager> m_dockLayoutManager;             ///< ドックレイアウト管理（所有）
    std::unique_ptr<DockCreationService> m_dockCreationService;        ///< ドック生成サービス（所有）
    CommentCoordinator* m_commentCoordinator = nullptr;                ///< コメントコーディネータ（非所有）
    std::unique_ptr<UsiCommandController> m_usiCommandController;       ///< USIコマンドコントローラ（所有）
    RecordNavigationWiring* m_recordNavWiring = nullptr;               ///< 棋譜ナビゲーション配線（非所有）
    UiStatePolicyManager* m_uiStatePolicy = nullptr;                   ///< UI状態ポリシーマネージャ（非所有）
    std::unique_ptr<LiveGameSessionUpdater> m_liveGameSessionUpdater;  ///< LiveGameSession更新ロジック（所有）
    std::unique_ptr<GameRecordUpdateService> m_gameRecordUpdateService; ///< 棋譜追記・ライブ更新サービス（所有）
    UiNotificationService* m_notificationService = nullptr;            ///< エラー通知サービス（非所有）
    std::unique_ptr<UndoFlowService> m_undoFlowService;                 ///< 待った巻き戻し後処理サービス（所有）
    std::unique_ptr<GameRecordLoadService> m_gameRecordLoadService;     ///< 棋譜表示初期化サービス（所有）
    std::unique_ptr<TurnStateSyncService> m_turnStateSync;             ///< 手番同期サービス（所有）
    std::unique_ptr<BranchNavigationWiring> m_branchNavWiring;         ///< 分岐ナビゲーション配線（所有）
    std::unique_ptr<KifuNavigationCoordinator> m_kifuNavCoordinator;  ///< 棋譜ナビゲーション同期（所有）
    SessionLifecycleCoordinator* m_sessionLifecycle = nullptr;        ///< セッションライフサイクル管理（非所有）
    GameSessionOrchestrator* m_gameSessionOrchestrator = nullptr;    ///< 対局ライフサイクルオーケストレータ（非所有）

#ifdef QT_DEBUG
    std::unique_ptr<DebugScreenshotWiring> m_debugScreenshotWiring;  ///< デバッグ用スクリーンショット配線（所有）
#endif

    // --- privateメソッド ---

    // --- 遅延初期化（初期化 / セットアップ） ---
    /// 遅延初期化: TimeControlControllerを生成し依存を設定する
    void ensureTimeController();
    /// MatchCoordinatorWiringを遅延初期化する
    void ensureMatchCoordinatorWiring();

    // --- リプレイ制御 ---
    /// 遅延初期化: ReplayControllerを生成し依存を設定する
    void ensureReplayController();
    /// 遅延初期化: TurnSyncBridgeを生成し依存を設定する
    void ensureTurnSyncBridge();

    // --- 遅延初期化（ensure系） ---
    /// 遅延初期化: PositionEditControllerを生成し依存を設定する
    void ensurePositionEditController();
    /// 遅延初期化: BoardSyncPresenterを生成し依存を設定する
    void ensureBoardSyncPresenter();
    /// 遅延初期化: BoardLoadServiceを生成し依存を設定する
    void ensureBoardLoadService();
    /// 遅延初期化: ConsiderationPositionServiceを生成し依存を設定する
    void ensureConsiderationPositionService();
    /// 遅延初期化: AnalysisResultsPresenterを生成し依存を設定する
    void ensureAnalysisPresenter();
    /// 遅延初期化: GameStartCoordinatorを生成し依存を設定する
    void ensureGameStartCoordinator();
    /// 遅延初期化: GameRecordPresenterを生成し依存を設定する
    void ensureRecordPresenter();
    /// 遅延初期化: LiveGameSessionが未開始なら開始する
    void ensureLiveGameSessionStarted();
    /// 遅延初期化: LiveGameSessionUpdaterを生成し依存を設定する
    void ensureLiveGameSessionUpdater();
    /// 遅延初期化: GameRecordUpdateServiceを生成し依存を設定する
    void ensureGameRecordUpdateService();
    /// 遅延初期化: UndoFlowServiceを生成し依存を設定する
    void ensureUndoFlowService();
    /// 遅延初期化: GameRecordLoadServiceを生成し依存を設定する
    void ensureGameRecordLoadService();
    /// 遅延初期化: TurnStateSyncServiceを生成し依存を設定する
    void ensureTurnStateSyncService();
    /// 遅延初期化: GameRecordModelを生成する
    void ensureGameRecordModel();
    /// 遅延初期化: DialogCoordinatorを生成し依存を設定する
    void ensureDialogCoordinator();
    /// 遅延初期化: KifuFileControllerを生成し依存を設定する
    void ensureKifuFileController();
    /// 遅延初期化: KifuExportControllerを生成し依存を設定する
    void ensureKifuExportController();
    /// KifuExportControllerの依存オブジェクトを最新状態に更新する
    void updateKifuExportDependencies();
    /// 遅延初期化: GameStateControllerを生成し依存を設定する
    void ensureGameStateController();
    /// 遅延初期化: BoardSetupControllerを生成し依存を設定する
    void ensureBoardSetupController();
    /// 遅延初期化: PvClickControllerを生成し依存を設定する
    void ensurePvClickController();
    /// 遅延初期化: PositionEditCoordinatorを生成し依存を設定する
    void ensurePositionEditCoordinator();
    /// 遅延初期化: MenuWindowWiringを生成し依存を設定する
    void ensureMenuWiring();
    /// 遅延初期化: PlayerInfoWiringを生成し依存を設定する
    void ensurePlayerInfoWiring();
    /// 遅延初期化: PreStartCleanupHandlerを生成し依存を設定する
    void ensurePreStartCleanupHandler();
    /// 遅延初期化: ConsecutiveGamesControllerを生成し依存を設定する
    void ensureConsecutiveGamesController();
    /// 遅延初期化: LanguageControllerを生成し依存を設定する
    void ensureLanguageController();
    /// 遅延初期化: DockLayoutManagerを生成し依存を設定する
    void ensureDockLayoutManager();
    /// 遅延初期化: DockCreationServiceを生成し依存を設定する
    void ensureDockCreationService();
    /// 遅延初期化: CommentCoordinatorを生成し依存を設定する
    void ensureCommentCoordinator();
    /// 遅延初期化: RecordNavigationHandlerを生成し依存を設定する
    void ensureRecordNavigationHandler();
    void ensureUiStatePolicyManager();
    /// 遅延初期化: KifuNavigationCoordinatorを生成し依存を設定する
    void ensureKifuNavigationCoordinator();
    /// 遅延初期化: SessionLifecycleCoordinatorを生成し依存を設定する
    void ensureSessionLifecycleCoordinator();
    /// BranchNavigationWiring を初期化し外部シグナルを接続する
    void ensureBranchNavigationWiring();

    // --- ファクトリヘルパー ---
    /// 現在のメンバ変数から MainWindowRuntimeRefs スナップショットを構築する
    MainWindowRuntimeRefs buildRuntimeRefs();
};

#endif // MAINWINDOW_H
