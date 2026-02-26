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
 * @todo remove コメントスタイルガイド適用済み
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

    friend class RecordNavigationWiring;
    friend class MainWindowUiBootstrapper;
    friend class MainWindowRuntimeRefsFactory;
    friend class MainWindowWiringAssembler;
    friend class KifuLoadCoordinatorFactory;
    friend class KifuExportDepsAssembler;
    friend class MainWindowServiceRegistry;
    friend class MainWindowDockBootstrapper;
    friend class MainWindowSignalRouter;
    friend class AnalysisTabWiring;
    friend class CsaGameWiring;
    friend class MatchCoordinatorWiring;
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

    // --- protected ---
protected:
    std::unique_ptr<Ui::MainWindow> ui;        ///< Ui::MainWindowフォームへのポインタ
    void closeEvent(QCloseEvent* event) override;

    // --- private slots ---
private slots:
    // --- 対局終了 / 状態変化 ---

    /// 手番変更を処理する（TurnManager::turnChanged に接続）
    void onTurnManagerChanged(ShogiGameController::Player now);


    // --- 移動要求 ---

    /// 盤面上の駒移動要求を処理する（BoardInteractionController::moveRequested に接続）
    void onMoveRequested(const QPoint& from, const QPoint& to);

    // --- リプレイ ---

    /// リプレイモードの開始/終了を切り替える
    void setReplayMode(bool on);

    // --- 棋譜表示 / 同期 ---

    /// 指し手確定時の棋譜・盤面更新（ShogiGameController::moveCommitted に接続）
    void onMoveCommitted(ShogiGameController::Player mover, int ply);
    /// 棋譜表示リストを更新する
    void displayGameRecord(const QList<KifDisplayItem> disp);
    /// SFEN文字列から直接盤面を読み込む（分岐ナビゲーション用）
    void loadBoardFromSfen(const QString& sfen);

    /// SFEN文字列から盤面とハイライトを更新する（分岐ナビゲーション用）
    void loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen);

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
    MainWindowCompositionRoot* m_compositionRoot = nullptr;  ///< ensure*生成ロジック集約（非所有）
    MainWindowServiceRegistry* m_registry = nullptr;         ///< ensure*メソッド実装の集約先（非所有）
    MainWindowSignalRouter* m_signalRouter = nullptr;        ///< シグナル配線ルーター（非所有）
    MainWindowAppearanceController* m_appearanceController = nullptr; ///< UI外観コントローラ（非所有）
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
    EvaluationGraphController* m_evalGraphController = nullptr;  ///< 評価値グラフ管理（非所有）

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
    QTimer* m_evalChartResizeTimer = nullptr;  ///< 高さ調整用デバウンスタイマー（非所有）

    // --- コーディネータ / プレゼンタ ---
    KifuLoadCoordinator*      m_kifuLoadCoordinator = nullptr; ///< 棋譜読込コーディネータ（非所有）
    PositionEditController*   m_posEdit = nullptr;             ///< 局面編集コントローラ（非所有）
    BoardSyncPresenter*       m_boardSync = nullptr;           ///< 盤面同期プレゼンタ（非所有）
    BoardLoadService*         m_boardLoadService = nullptr;    ///< 盤面読み込みサービス（非所有）
    ConsiderationPositionService* m_considerationPositionService = nullptr; ///< 検討局面解決サービス（非所有）
    AnalysisResultsPresenter* m_analysisPresenter = nullptr;   ///< 解析結果プレゼンタ（非所有）
    /// メニュー操作など通常の「対局開始」入口を担当
    GameStartCoordinator*     m_gameStart = nullptr;           ///< 対局開始コーディネータ（非所有）
    /// MatchCoordinator 生成/配線側の起動管理を担当
    GameStartCoordinator*     m_gameStartCoordinator = nullptr; ///< 対局起動管理コーディネータ（非所有）
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
    KifuExportController*     m_kifuExportController = nullptr; ///< 棋譜エクスポートコントローラ（非所有）

    // --- ゲーム状態管理 ---
    GameStateController*      m_gameStateController = nullptr;  ///< ゲーム状態コントローラ（非所有）
    PlayModePolicyService*    m_playModePolicy = nullptr;       ///< プレイモード判定サービス（所有）
    MatchRuntimeQueryService* m_queryService = nullptr;          ///< 対局実行時クエリサービス（所有）

    // --- 対局者情報管理 ---
    PlayerInfoController*     m_playerInfoController = nullptr; ///< 対局者情報コントローラ（非所有）

    // --- 盤面操作配線 ---
    BoardSetupController*     m_boardSetupController = nullptr; ///< 盤面操作配線コントローラ（非所有）

    // --- 読み筋クリック処理 ---
    PvClickController*        m_pvClickController = nullptr;    ///< 読み筋クリックコントローラ（非所有）

    // --- 局面編集調整 ---
    PositionEditCoordinator*  m_posEditCoordinator = nullptr;   ///< 局面編集コーディネータ（非所有）

    // --- 通信対局 / ウィンドウ配線 ---
    CsaGameWiring*            m_csaGameWiring = nullptr;        ///< CSA通信対局UI配線（非所有）
    JosekiWindowWiring*       m_josekiWiring = nullptr;         ///< 定跡ウィンドウUI配線（非所有）
    MenuWindowWiring*         m_menuWiring = nullptr;           ///< メニューウィンドウUI配線（非所有）
    PlayerInfoWiring*         m_playerInfoWiring = nullptr;     ///< 対局者情報UI配線（非所有）
    DialogLaunchWiring*       m_dialogLaunchWiring = nullptr;   ///< ダイアログ起動配線（非所有）
    MatchCoordinatorWiring*   m_matchWiring = nullptr;          ///< MatchCoordinator配線（非所有）

    // --- 対局開始前クリーンアップ ---
    PreStartCleanupHandler*   m_preStartCleanupHandler = nullptr; ///< 対局開始前クリーンアップハンドラ（非所有）

    // --- 言語設定 ---
    // --- 補助コントローラ群 ---
    JishogiScoreDialogController* m_jishogiController = nullptr;       ///< 持将棋スコアダイアログコントローラ（非所有）
    NyugyokuDeclarationHandler* m_nyugyokuHandler = nullptr;           ///< 入玉宣言ハンドラ（非所有）
    ConsecutiveGamesController* m_consecutiveGamesController = nullptr; ///< 連続対局コントローラ（非所有）
    LanguageController* m_languageController = nullptr;                ///< 言語設定コントローラ（非所有）
    ConsiderationWiring* m_considerationWiring = nullptr;              ///< 検討モード配線（非所有）
    DockLayoutManager* m_dockLayoutManager = nullptr;                  ///< ドックレイアウト管理（非所有）
    DockCreationService* m_dockCreationService = nullptr;              ///< ドック生成サービス（非所有）
    CommentCoordinator* m_commentCoordinator = nullptr;                ///< コメントコーディネータ（非所有）
    UsiCommandController* m_usiCommandController = nullptr;            ///< USIコマンドコントローラ（非所有）
    RecordNavigationWiring* m_recordNavWiring = nullptr;               ///< 棋譜ナビゲーション配線（非所有）
    UiStatePolicyManager* m_uiStatePolicy = nullptr;                   ///< UI状態ポリシーマネージャ（非所有）
    std::unique_ptr<LiveGameSessionUpdater> m_liveGameSessionUpdater;  ///< LiveGameSession更新ロジック（所有）
    GameRecordUpdateService* m_gameRecordUpdateService = nullptr;       ///< 棋譜追記・ライブ更新サービス（非所有）
    UiNotificationService* m_notificationService = nullptr;            ///< エラー通知サービス（非所有）
    std::unique_ptr<UndoFlowService> m_undoFlowService;                 ///< 待った巻き戻し後処理サービス（所有）
    std::unique_ptr<GameRecordLoadService> m_gameRecordLoadService;     ///< 棋譜表示初期化サービス（所有）
    std::unique_ptr<TurnStateSyncService> m_turnStateSync;             ///< 手番同期サービス（所有）
    BranchNavigationWiring* m_branchNavWiring = nullptr;              ///< 分岐ナビゲーション配線（非所有）
    KifuNavigationCoordinator* m_kifuNavCoordinator = nullptr;        ///< 棋譜ナビゲーション同期（非所有）
    SessionLifecycleCoordinator* m_sessionLifecycle = nullptr;        ///< セッションライフサイクル管理（非所有）
    GameSessionOrchestrator* m_gameSessionOrchestrator = nullptr;    ///< 対局ライフサイクルオーケストレータ（非所有）

#ifdef QT_DEBUG
    DebugScreenshotWiring* m_debugScreenshotWiring = nullptr;        ///< デバッグ用スクリーンショット配線
#endif

    // --- privateメソッド ---

    // --- 遅延初期化（初期化 / セットアップ） ---
    /// 遅延初期化: TimeControlControllerを生成し依存を設定する
    void ensureTimeController();
    /// MatchCoordinatorWiringを遅延初期化する
    void ensureMatchCoordinatorWiring();

    // --- ユーティリティ ---
    /// 現在の手番を設定する
    void setCurrentTurn();

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
