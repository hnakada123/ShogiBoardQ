#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// ==============================
// Qt includes
// ==============================
#include <QMainWindow>
#include <QDockWidget>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QActionGroup>

// ==============================
// Project includes (types used by value)
// ==============================
#include "playmode.h"
#include "shogimove.h"
#include "matchcoordinator.h"
#include "kifurecordlistmodel.h"
#include "kifuanalysislistmodel.h"

// ==============================
// Project includes（依存の強いモジュール）
// ==============================
#include "kifubranchlistmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "usi.h"
#include "startgamedialog.h"
#include "kifuanalysisdialog.h"
#include "usicommlogmodel.h"
#include "considerationdialog.h"
#include "tsumeshogisearchdialog.h"
#include "shogiclock.h"
#include "navigationcontext.h"
#include "recordpane.h"
#include "engineanalysistab.h"
#include "boardinteractioncontroller.h"
#include "kifuvariationengine.h"
#include "branchcandidatescontroller.h"
#include "kifuloadcoordinator.h"
#include "kifutypes.h"
#include "positioneditcontroller.h"
#include "gamestartcoordinator.h"
#include "analysisflowcontroller.h"
#include "pvboarddialog.h"
#include "kifuclipboardservice.h"
#include "gameinfopanecontroller.h"  // KifGameInfoItem

// ==============================
// Forward declarations (new controllers)
// ==============================
class JishogiScoreDialogController;
class NyugyokuDeclarationHandler;
class ConsecutiveGamesController;
class LanguageController;

// ==============================
// Macros / aliases
// ==============================
#define SHOGIBOARDQ_DEBUG_KIF 1

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ==============================
// Forward declarations
// ==============================
class QPainter;
class QStyleOptionViewItem;
class QModelIndex;
class NavigationController;
class QGraphicsView;
class QGraphicsPathItem;
class QTableView;
class QEvent;
class QLabel;
class QToolButton;
class QPushButton;
class ShogiView;
class BoardSyncPresenter;
class AnalysisResultsPresenter;
class GameStartCoordinator;
class AnalysisCoordinator;
class GameRecordPresenter;
class BranchWiringCoordinator;
class TimeDisplayPresenter;
class AnalysisTabWiring;
class RecordPaneWiring;
class UiActionsWiring;
class GameLayoutBuilder;
class INavigationContext;
class GameRecordModel;
class KifuPasteDialog;
class GameInfoPaneController;
class EvaluationGraphController;
class TimeControlController;
class ReplayController;
class DialogCoordinator;
class KifuExportController;
class GameStateController;
class PlayerInfoController;
class BoardSetupController;
class PvClickController;
class RecordNavigationController;
class PositionEditCoordinator;
class CsaGameDialog;
class CsaGameCoordinator;
class CsaWaitingDialog;
class JosekiWindow;
class CsaGameWiring;
class BranchRowDelegate;
class JosekiWindowWiring;
class PlayerInfoWiring;
class PreStartCleanupHandler;
class MenuWindowWiring;

// ============================================================
// MainWindow
// ============================================================
/**
 * @brief メインウィンドウクラス
 *
 * 責務:
 * - UIレイアウトの構築と管理
 * - 各コーディネータ/コントローラの統括
 * - メニュー/アクションのハンドリング
 * - ウィンドウ設定の保存/復元
 *
 * 分離済みの責務:
 * - GameInfoPaneController: 対局情報タブの管理
 * - KifuClipboardService: 棋譜のクリップボード操作
 * - KifuSaveCoordinator: 棋譜ファイルの保存
 * - GameStartCoordinator: 対局開始処理
 * - BoardSyncPresenter: 盤面同期
 * - AnalysisResultsPresenter: 解析結果表示
 * - BranchWiringCoordinator: 分岐配線
 * - CsaGameWiring: CSA通信対局のUI配線
 * - BranchRowDelegate: 分岐行の描画カスタマイズ
 */
class MainWindow : public QMainWindow, public INavigationContext
{
    Q_OBJECT

    // ========================================================
    // public
    // ========================================================
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // INavigationContext override
    bool hasResolvedRows() const override;
    int  resolvedRowCount() const override;
    int  activeResolvedRow() const override;
    int  maxPlyAtRow(int row) const override;
    int  currentPly() const override;
    void applySelect(int row, int ply) override;

    // ========================================================
    // public slots
    // ========================================================
public slots:
    // ファイル I/O
    void chooseAndLoadKifuFile();
    void saveShogiBoardImage();
    void copyBoardToClipboard();
    void openWebsiteInExternalBrowser();
    void saveKifuToFile();
    void overwriteKifuFile();

    // クリップボード操作（KifuClipboardServiceへ委譲）
    void copyKifToClipboard();
    void copyKi2ToClipboard();
    void copyCsaToClipboard();
    void copyUsiToClipboard();
    void copyUsiCurrentToClipboard();
    void copyJkfToClipboard();
    void copyUsenToClipboard();
    void copySfenToClipboard();
    void copyBodToClipboard();
    void pasteKifuFromClipboard();

    // エラー/一般UI
    void displayErrorMessage(const QString& message);
    void saveSettingsAndClose();
    void resetToInitialState();
    void onFlowError(const QString& msg);

    // ダイアログ表示
    void displayPromotionDialog();
    void displayEngineSettingsDialog();
    void displayVersionInformation();
    void displayConsiderationDialog();
    void displayKifuAnalysisDialog();
    void cancelKifuAnalysis();  // 棋譜解析中止
    void onKifuAnalysisProgress(int ply, int scoreCp);  // 棋譜解析進捗
    void onKifuAnalysisResultRowSelected(int row);  // 棋譜解析結果行選択
    void displayTsumeShogiSearchDialog();
    void stopTsumeSearch();  // 詰み探索エンジン終了
    void displayCsaGameDialog();
    void displayJosekiWindow();
    void updateJosekiWindow();  // 定跡ウィンドウの更新
    void displayMenuWindow();   // メニューウィンドウの表示
    void displayJishogiScoreDialog();  // 持将棋の点数ダイアログ
    void handleNyugyokuDeclaration();  // 入玉宣言

    // 言語設定
    void onLanguageSystemTriggered();
    void onLanguageJapaneseTriggered();
    void onLanguageEnglishTriggered();

    // ツールバー表示切替
    void onToolBarVisibilityToggled(bool visible);

    // その他操作
    void undoLastTwoMoves();
    void beginPositionEditing();
    void finishPositionEditing();
    void initializeGame();
    void handleResignation();
    void onPlayerNamesResolved(const QString& human1, const QString& human2,
                                const QString& engine1, const QString& engine2,
                                int playMode);
    void onActionFlipBoardTriggered(bool checked = false);
    void handleBreakOffGame();
    void movePieceImmediately();
    void onRecordPaneMainRowChanged(int row);

    // ========================================================
    // protected
    // ========================================================
protected:
    Ui::MainWindow* ui = nullptr;
    void closeEvent(QCloseEvent* event) override;
    /// ShogiViewのCtrl+ホイールイベントを横取りして処理
    bool eventFilter(QObject* watched, QEvent* event) override;

    // ========================================================
    // private slots
    // ========================================================
private slots:
    // 対局終了 / 状態変化
    void onMatchGameEnded(const MatchCoordinator::GameEndInfo& info);
    void onGameOverStateChanged(const MatchCoordinator::GameOverState& st);
    void onTurnManagerChanged(ShogiGameController::Player now);
    void flipBoardAndUpdatePlayerInfo();

    // タブ選択変更
    void onTabCurrentChanged(int index);

    // 検討モード開始時
    void onConsiderationModeStarted();

    // 検討モードの時間設定確定時
    void onConsiderationTimeSettingsReady(bool unlimited, int byoyomiSec);

    // 検討モード終了時
    void onConsiderationModeEnded();

    // 検討待機開始時（時間切れ後、次の局面選択待ち）
    void onConsiderationWaitingStarted();

    // 検討中にMultiPV変更時
    void onConsiderationMultiPVChanged(int value);

    // 検討ダイアログでMultiPVが設定されたとき
    void onConsiderationDialogMultiPVReady(int multiPV);

    // ボタン有効/無効
    void disableArrowButtons();
    void enableArrowButtons();
    void setNavigationEnabled(bool on);
    void disableNavigationForGame();
    void enableNavigationAfterGame();

    // 盤面・反転
    void onBoardFlipped(bool nowFlipped);
    void onBoardSizeChanged(QSize fieldSize);
    void onReverseTriggered();
    void performDeferredEvalChartResize();  // 評価値グラフ高さ調整（デバウンス用）

    // 司令塔通知
    void onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

    // 移動要求
    void onMoveRequested(const QPoint& from, const QPoint& to);

    // リプレイ
    void setReplayMode(bool on);

    // CSA通信対局関連（CsaGameWiringからのシグナル受信用）
    void onCsaPlayModeChanged(int mode);
    void onCsaShowGameEndDialog(const QString& title, const QString& message);
    void onCsaEngineScoreUpdated(int scoreCp, int ply);

    // 定跡ウィンドウ関連（JosekiWindowWiringからのシグナル受信用）
    void onJosekiForcedPromotion(bool forced, bool promote);

    // 内部配線
    void connectBoardClicks();
    void connectMoveRequested();

    // 棋譜表示 / 同期
    void onMoveCommitted(ShogiGameController::Player mover, int ply);
    void displayGameRecord(const QList<KifDisplayItem> disp);
    void syncBoardAndHighlightsAtRow(int ply1);
    void onRecordRowChangedByPresenter(int row, const QString& comment);
    void onCommentUpdated(int moveIndex, const QString& newComment);
    void onPvRowClicked(int engineIndex, int row);
    void onPvDialogClosed(int engineIndex);
    void onUsiCommandRequested(int target, const QString& command);
    void onKifuPasteImportRequested(const QString& content);
    void onGameRecordCommentChanged(int ply, const QString& comment);
    void onCommentUpdateCallback(int ply, const QString& comment);

    // 分岐ノード活性化
    void onBranchNodeActivated(int row, int ply);

    // エラー / 前準備
    void onErrorBusOccurred(const QString& msg);
    void onPreStartCleanupRequested();
    void onApplyTimeControlRequested(const GameStartCoordinator::TimeControl& tc);

    // 投了
    void onResignationTriggered();

    // ★ 新規: GameInfoPaneControllerからの通知
    void onGameInfoUpdated(const QList<KifGameInfoItem>& items);

    // 連続対局: 設定を受信
    void onConsecutiveGamesConfigured(int totalGames, bool switchTurn);

    // 連続対局: 対局開始時の設定を保存
    void onGameStarted(const MatchCoordinator::StartOptions& opt);

    // 連続対局: 次の対局を開始
    void startNextConsecutiveGame();

    // 対局開始後に棋譜欄の指定行を選択
    void onRequestSelectKifuRow(int row);

    // ========================================================
    // private
    // ========================================================
private:
    // --------------------------------------------------------
    // Private Data Members
    // --------------------------------------------------------

    // 基本状態 / ゲーム状態
    QString  m_startSfenStr;
    QString  m_currentSfenStr;
    QString  m_resumeSfenStr;
    bool     m_errorOccurred = false;
    int      m_currentMoveIndex = 0;
    QString  m_lastMove;
    PlayMode m_playMode = PlayMode::NotStarted;

    // 将棋盤 / コントローラ類
    ShogiView*                   m_shogiView = nullptr;
    ShogiGameController*         m_gameController = nullptr;
    BoardInteractionController*  m_boardController = nullptr;

    // USI / エンジン連携
    Usi*        m_usi1 = nullptr;
    Usi*        m_usi2 = nullptr;
    QString     m_positionStr1;
    QStringList m_positionStrList;
    QStringList m_gameUsiMoves;  // 対局時のUSI形式指し手リスト（棋譜読み込み時はKifuLoadCoordinator::m_kifuUsiMovesを使用）

    // UI 構成
    QTabWidget* m_tab = nullptr;
    QWidget*    m_gameRecordLayoutWidget = nullptr;
    QSplitter*  m_hsplit = nullptr;
    QWidget*    m_central = nullptr;
    QVBoxLayout* m_centralLayout = nullptr;

    // ダイアログ / 補助ウィンドウ
    StartGameDialog*         m_startGameDialog = nullptr;
    ConsiderationDialog*     m_considerationDialog = nullptr;
    TsumeShogiSearchDialog*  m_tsumeShogiSearchDialog = nullptr;
    KifuAnalysisDialog*      m_analyzeGameRecordDialog = nullptr;
    CsaGameDialog*           m_csaGameDialog = nullptr;
    CsaWaitingDialog*        m_csaWaitingDialog = nullptr;

    // CSA通信対局コーディネータ
    CsaGameCoordinator*      m_csaGameCoordinator = nullptr;

    // モデル群
    KifuRecordListModel*       m_kifuRecordModel  = nullptr;
    KifuBranchListModel*       m_kifuBranchModel  = nullptr;
    ShogiEngineThinkingModel*  m_modelThinking1   = nullptr;
    ShogiEngineThinkingModel*  m_modelThinking2   = nullptr;
    ShogiEngineThinkingModel*  m_considerationModel = nullptr;  // ★ 追加: 検討タブ専用モデル
    KifuAnalysisListModel*     m_analysisModel    = nullptr;
    UsiCommLogModel*           m_lineEditModel1   = nullptr;
    UsiCommLogModel*           m_lineEditModel2   = nullptr;

    // 記録 / 評価 / 表示用データ
    EvaluationGraphController* m_evalGraphController = nullptr;  // ★ 評価値グラフ管理
    QString           m_humanName1, m_humanName2;
    QString           m_engineName1, m_engineName2;
    QStringList*      m_sfenRecord   = nullptr;
    QList<KifuDisplay *>* m_moveRecords = nullptr;
    QStringList       m_kifuDataList;
    QString           defaultSaveFileName;
    QString           kifuSaveFileName;
    QVector<ShogiMove> m_gameMoves;
    QList<KifDisplayItem> m_liveDisp;

    // 時計 / 時刻管理（TimeControlControllerへ移行）
    TimeControlController* m_timeController = nullptr;

    // 行解決 / 選択状態
    QVector<ResolvedRow> m_resolvedRows;
    int m_activeResolvedRow = 0;

    // ★ 対局情報タブ（GameInfoPaneControllerへ移行）
    GameInfoPaneController* m_gameInfoController = nullptr;

    // 棋譜表示／分岐操作・表示関連
    QSet<int> m_branchablePlySet;
    QVector<QString> m_commentsByRow;
    int m_activePly          = 0;
    int m_currentSelectedPly = 0;
    QMetaObject::Connection m_connKifuRowChanged;
    bool m_onMainRowGuard = false;

    // 棋譜データ中央管理
    GameRecordModel* m_gameRecord = nullptr;

    // 新UI部品 / ナビゲーション
    RecordPane*           m_recordPane = nullptr;
    NavigationController* m_nav = nullptr;
    EngineAnalysisTab*    m_analysisTab = nullptr;

    // 試合進行（司令塔）
    MatchCoordinator* m_match = nullptr;
    QMetaObject::Connection m_timeConn;

    // 連続対局設定
    int  m_consecutiveGamesRemaining = 0;  // 残り連続対局回数
    int  m_consecutiveGamesTotal     = 1;  // 連続対局の設定回数（合計）
    int  m_consecutiveGameNumber     = 1;  // 現在の対局番号（1から始まる）
    bool m_switchTurnEachGame        = false;  // 1局ごとに手番を入れ替えるかどうか
    MatchCoordinator::StartOptions m_lastStartOptions;  // 前回の対局設定（連続対局用）
    GameStartCoordinator::TimeControl m_lastTimeControl;  // 前回の時間設定（連続対局用）

    // リプレイ制御（ReplayControllerへ移行）
    ReplayController* m_replayController = nullptr;

    // 分岐表示計画
    QHash<int, QMap<int, BranchCandidateDisplay>> m_branchDisplayPlan;

    // 直近の手番と残時間
    bool   m_lastP1Turn = true;
    qint64 m_lastP1Ms   = 0;
    qint64 m_lastP2Ms   = 0;

    // 手番方向
    bool m_bottomIsP1 = true;

    // 評価値グラフ高さ調整用タイマー（デバウンス処理）
    QTimer* m_evalChartResizeTimer = nullptr;

    // 各種コーディネータ / プレゼンタ
    KifuLoadCoordinator*      m_kifuLoadCoordinator = nullptr;
    PositionEditController*   m_posEdit = nullptr;
    BoardSyncPresenter*       m_boardSync = nullptr;
    AnalysisResultsPresenter* m_analysisPresenter = nullptr;
    GameStartCoordinator*     m_gameStart = nullptr;
    GameStartCoordinator*     m_gameStartCoordinator = nullptr;
    GameRecordPresenter*      m_recordPresenter = nullptr;
    BranchWiringCoordinator*  m_branchWiring = nullptr;
    TimeDisplayPresenter*     m_timePresenter = nullptr;
    AnalysisTabWiring*        m_analysisWiring = nullptr;
    RecordPaneWiring*         m_recordPaneWiring = nullptr;
    UiActionsWiring*          m_actionsWiring    = nullptr;
    GameLayoutBuilder*        m_layoutBuilder    = nullptr;

    // ダイアログ管理
    DialogCoordinator*        m_dialogCoordinator = nullptr;

    // 棋譜エクスポート管理
    KifuExportController*     m_kifuExportController = nullptr;

    // ゲーム状態管理
    GameStateController*      m_gameStateController = nullptr;

    // 対局者情報管理
    PlayerInfoController*     m_playerInfoController = nullptr;

    // 盤面操作配線管理
    BoardSetupController*     m_boardSetupController = nullptr;

    // 読み筋クリック処理
    PvClickController*        m_pvClickController = nullptr;

    // 棋譜ナビゲーション管理
    RecordNavigationController* m_recordNavController = nullptr;

    // 局面編集調整
    PositionEditCoordinator*  m_posEditCoordinator = nullptr;

    // CSA通信対局UI配線
    CsaGameWiring*            m_csaGameWiring = nullptr;

    // 定跡ウィンドウUI配線
    JosekiWindowWiring*       m_josekiWiring = nullptr;

    // メニューウィンドウUI配線
    MenuWindowWiring*         m_menuWiring = nullptr;

    // 対局情報・プレイヤー情報UI配線
    PlayerInfoWiring*         m_playerInfoWiring = nullptr;

    // 対局開始前クリーンアップハンドラ
    PreStartCleanupHandler*   m_preStartCleanupHandler = nullptr;

    // 変化エンジン
    std::unique_ptr<KifuVariationEngine> m_varEngine;

    // 言語設定用アクショングループ
    QActionGroup* m_languageActionGroup = nullptr;

    // ★ 新規コントローラ（MainWindowから分離した責務）
    JishogiScoreDialogController* m_jishogiController = nullptr;
    NyugyokuDeclarationHandler* m_nyugyokuHandler = nullptr;
    ConsecutiveGamesController* m_consecutiveGamesController = nullptr;
    LanguageController* m_languageController = nullptr;

    // --------------------------------------------------------
    // Private Methods
    // --------------------------------------------------------

    // 言語設定
    void updateLanguageMenuState();
    void changeLanguage(const QString& lang);

    // UI / 表示更新
    void updateGameRecord(const QString& elapsedTime);
    void updateTurnStatus(int currentPlayer);
    void redrawEngine1EvaluationGraph(int ply = -1);  // EvaluationGraphControllerへ委譲
    void redrawEngine2EvaluationGraph(int ply = -1);  // EvaluationGraphControllerへ委譲
    void ensureEvaluationGraphController();          // ★ 追加

    // 初期化 / セットアップ
    void initializeComponents();
    void setupHorizontalGameLayout();
    void initializeCentralGameDisplay();
    void ensureTimeController();  // ★ TimeControlControllerへ移行
    void initMatchCoordinator();
    void setupRecordPane();
    void setupEngineAnalysisTab();
    void setupBoardInteractionController();

    // ゲーム開始/切替
    void initializeNewGame(QString& startSfenStr);
    void startNewShogiGame(QString& startSfenStr);
    void setEngineNamesBasedOnMode();
    void updateSecondEngineVisibility();

    // 入出力 / 設定
    void saveWindowAndBoardSettings();
    void loadWindowSettings();

    // ★ 対局情報関連（GameInfoPaneControllerへ委譲）
    void ensureGameInfoController();
    void addGameInfoTabAtStartup();
    void populateDefaultGameInfo();
    void updateGameInfoForCurrentMatch();

    // ★ 互換性のため残す古い関数（GameStartCoordinatorのhooksで使用）
    void onSetPlayersNames(const QString& p1, const QString& p2);
    void onSetEngineNames(const QString& e1, const QString& e2);
    void updateGameInfoPlayerNames(const QString& blackName, const QString& whiteName);
    void setOriginalGameInfo(const QList<KifGameInfoItem>& items);

    // 分岐 / 変化
    void applyResolvedRowAndSelect(int row, int selPly);
    std::pair<int,int> resolveBranchHighlightTarget(int row, int ply) const;

    // ユーティリティ
    void setPlayersNamesForMode();
    void setCurrentTurn();
    void setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne);
    void appendKifuLine(const QString& text, const QString& elapsedTime);
    void broadcastComment(const QString& text, bool asHtml=false);

    // 手番チェックヘルパー
    bool isHumanTurnNow() const;

    // フォント/描画ヘルパ
    void setupNameAndClockFonts();

    // リプレイ制御
    void ensureReplayController();
    void ensureTurnSyncBridge();

    // 各種 ensure メソッド
    void ensurePositionEditController();
    void ensureBoardSyncPresenter();
    void ensureAnalysisPresenter();
    void ensureGameStartCoordinator();
    void ensureRecordPresenter();
    void ensureKifuLoadCoordinatorForLive();
    void ensureGameRecordModel();
    void ensureDialogCoordinator();
    void ensureKifuExportController();
    void updateKifuExportDependencies();
    void ensureGameStateController();
    void ensurePlayerInfoController();
    void ensureBoardSetupController();
    void ensurePvClickController();
    void ensureRecordNavigationController();
    void ensurePositionEditCoordinator();
    void ensureCsaGameWiring();
    void ensureJosekiWiring();
    void ensureMenuWiring();
    void ensurePlayerInfoWiring();
    void ensurePreStartCleanupHandler();
    void ensureJishogiController();
    void ensureNyugyokuHandler();
    void ensureConsecutiveGamesController();
    void ensureLanguageController();

    // ctor の分割先
    void setupCentralWidgetContainer();
    void configureToolBarFromUi();
    void buildGamePanels();
    void restoreWindowAndSync();
    void connectAllActions();
    void connectCoreSignals();
    void installAppToolTips();
    void finalizeCoordinators();

    // hooks 用メンバー関数
    void requestRedrawEngine1Eval();
    void requestRedrawEngine2Eval();
    void initializeNewGameHook(const QString& s);
    void showMoveHighlights(const QPoint& from, const QPoint& to);
    void appendKifuLineHook(const QString& text, const QString& elapsed);

    // 時間取得ヘルパ
    qint64 getRemainingMsFor(MatchCoordinator::Player p) const;
    qint64 getIncrementMsFor(MatchCoordinator::Player p) const;
    qint64 getByoyomiMs() const;

    // ゲームオーバー関連
    void showGameOverMessageBox(const QString& title, const QString& message);

    // 棋譜自動保存
    void autoSaveKifuToFile(const QString& saveDir, PlayMode playMode,
                             const QString& humanName1, const QString& humanName2,
                             const QString& engineName1, const QString& engineName2);

    // 分岐ツリー更新
    void refreshBranchTreeLive();

    // ガード / 判定ヘルパ
    bool getMainRowGuard() const;
    void setMainRowGuard(bool on);
    bool isHvH() const;
    bool isHumanSide(ShogiGameController::Player p) const;

    // 表示更新
    void updateTurnAndTimekeepingDisplay();

    // 編集メニュー
    void initializeEditMenuForStartup();
    void applyEditMenuEditingState(bool editing);

    // 開始局面解決
    QString resolveCurrentSfenForGameStart() const;
};

#endif // MAINWINDOW_H
