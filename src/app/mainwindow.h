#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/// @file mainwindow.h
/// @brief MainWindow の宣言
/// @todo remove コメントスタイルガイド適用済み


// --- Qt ヘッダー ---
#include <QMainWindow>
#include <QDockWidget>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QActionGroup>

// --- プロジェクトヘッダー（値型で使用） ---
#include "playmode.h"
#include "shogimove.h"
#include "matchcoordinator.h"
#include "kifurecordlistmodel.h"
#include "kifuanalysislistmodel.h"

// --- プロジェクトヘッダー（依存の強いモジュール） ---
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
#include "recordpane.h"
#include "engineanalysistab.h"
#include "boardinteractioncontroller.h"
#include "kifuloadcoordinator.h"
#include "kifutypes.h"
#include "positioneditcontroller.h"
#include "gamestartcoordinator.h"
#include "analysisflowcontroller.h"
#include "pvboarddialog.h"
#include "kifuclipboardservice.h"
#include "gameinfopanecontroller.h"  // KifGameInfoItem

// --- 前方宣言（新規コントローラ） ---
class JishogiScoreDialogController;
class NyugyokuDeclarationHandler;
class ConsecutiveGamesController;
class LanguageController;
class ConsiderationModeUIController;
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

// --- マクロ ---
#define SHOGIBOARDQ_DEBUG_KIF 1  ///< 棋譜デバッグ出力の有効化フラグ

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// --- 前方宣言（Qt / 各種コントローラ） ---
class QPainter;
class QStyleOptionViewItem;
class QModelIndex;
class QGraphicsView;
class QGraphicsPathItem;
class QTableView;
class QEvent;
class QLabel;
class QToolButton;
class QPushButton;
class ShogiView;
class EvaluationChartWidget;
class BoardSyncPresenter;
class AnalysisResultsPresenter;
class GameStartCoordinator;
class AnalysisCoordinator;
class GameRecordPresenter;
class TimeDisplayPresenter;
class AnalysisTabWiring;
class RecordPaneWiring;
class UiActionsWiring;
class GameLayoutBuilder;
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
class UsiCommandController;
class TestAutomationHelper;
class RecordNavigationHandler;

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

    // --- public ---
public:
    explicit MainWindow(QWidget* parent = nullptr);

    EvaluationChartWidget* evalChart() const { return m_evalChart; }

    /// UiActionsWiring向けに棋譜エクスポートコントローラを返す
    KifuExportController* kifuExportController();

    // --- public slots ---
public slots:
    // --- ファイル I/O ---
    void chooseAndLoadKifuFile();
    void saveShogiBoardImage();
    void copyBoardToClipboard();
    void openWebsiteInExternalBrowser();
    void saveKifuToFile();
    void overwriteKifuFile();

    // --- クリップボード操作 ---
    /// クリップボード上の棋譜テキストを読み込む
    void pasteKifuFromClipboard();

    // --- テスト自動化 ---
    void setTestMode(bool enabled);
    void loadKifuFile(const QString& path);
    void navigateToPly(int ply);
    void clickBranchCandidate(int index);
    void clickNextButton();
    void clickPrevButton();
    void clickFirstButton();
    void clickLastButton();
    void clickKifuRow(int row);
    void clickBranchTreeNode(int row, int ply);
    void dumpTestState();
    /// 盤面・棋譜・分岐・SFEN の4方向一致を検証する
    bool verify4WayConsistency();

    // --- 対局シミュレーション用テスト ---
    /// テスト用対局を開始する（平手、人間vs人間）
    void startTestGame();
    /// USI形式の指し手を入力して局面を進める
    bool makeTestMove(const QString& usiMove);
    int getBranchTreeNodeCount();
    /// 分岐ツリーのノード数が期待値以上か検証する
    bool verifyBranchTreeNodeCount(int minExpected);

    // --- エラー / 一般UI ---
    void displayErrorMessage(const QString& message);
    /// ウィンドウ設定を保存してアプリを終了する
    void saveSettingsAndClose();
    /// 盤面・棋譜・状態を初期状態にリセットする
    void resetToInitialState();
    /// AnalysisFlowController等からのエラーを受け取る
    void onFlowError(const QString& msg);

    // --- ダイアログ表示 ---
    void displayPromotionDialog();
    void displayEngineSettingsDialog();
    void displayVersionInformation();
    void displayKifuAnalysisDialog();
    /// 実行中の棋譜解析を中止する
    void cancelKifuAnalysis();
    /// 棋譜解析の進捗を受け取る（AnalysisCoordinator::analysisProgress に接続）
    void onKifuAnalysisProgress(int ply, int scoreCp);
    /// 棋譜解析結果リストの行選択時に該当局面へ遷移する
    void onKifuAnalysisResultRowSelected(int row);
    void displayTsumeShogiSearchDialog();
    /// 詰み探索エンジンを終了する
    void stopTsumeSearch();
    void displayCsaGameDialog();
    void displayJosekiWindow();
    /// 現在の局面で定跡ウィンドウを更新する
    void updateJosekiWindow();
    void displayMenuWindow();
    void displayJishogiScoreDialog();
    /// 入玉宣言の判定と処理を行う
    void handleNyugyokuDeclaration();

    // --- ツールバー / ドック ---
    /// ツールバーの表示/非表示を切り替える（actionToolBar に接続）
    void onToolBarVisibilityToggled(bool visible);
    /// 全ドックの移動・リサイズのロック状態を切り替える
    void onDocksLockToggled(bool locked);

    // --- その他操作 ---
    /// 直近2手を取り消す（「待った」相当）
    void undoLastTwoMoves();
    /// 局面編集モードを開始する
    void beginPositionEditing();
    /// 局面編集モードを終了し結果を反映する
    void finishPositionEditing();
    void initializeGame();
    void handleResignation();
    /// PlayerInfoWiring から対局者名の解決結果を受け取る
    void onPlayerNamesResolved(const QString& human1, const QString& human2,
                                const QString& engine1, const QString& engine2,
                                int playMode);
    void onActionFlipBoardTriggered(bool checked = false);
    void onActionEnlargeBoardTriggered(bool checked = false);
    void onActionShrinkBoardTriggered(bool checked = false);
    /// 中断ボタン押下時の対局中断処理
    void handleBreakOffGame();
    /// エンジンの指し手を即座に盤面に反映する
    void movePieceImmediately();
    /// 棋譜欄の行選択変更を処理する（RecordPane::mainRowChanged に接続）
    void onRecordPaneMainRowChanged(int row);

    // --- protected ---
protected:
    Ui::MainWindow* ui = nullptr;              ///< Ui::MainWindowフォームへのポインタ
    void closeEvent(QCloseEvent* event) override;

    // --- private slots ---
private slots:
    // --- 対局終了 / 状態変化 ---

    /// 対局終了を処理する（MatchCoordinator::gameEnded に接続）
    void onMatchGameEnded(const MatchCoordinator::GameEndInfo& info);
    /// 対局終了状態の変化を処理する（MatchCoordinator::gameOverStateChanged に接続）
    void onGameOverStateChanged(const MatchCoordinator::GameOverState& st);
    /// 手番変更を処理する（TurnManager::turnChanged に接続）
    void onTurnManagerChanged(ShogiGameController::Player now);
    /// 盤面反転時に対局者情報の表示位置を更新する
    void flipBoardAndUpdatePlayerInfo();

    // --- タブ選択変更 ---

    /// タブ切替時の処理（m_tab::currentChanged に接続）
    void onTabCurrentChanged(int index);

    // --- ナビゲーションボタン制御 ---

    /// ナビゲーション矢印ボタンを無効化する
    void disableArrowButtons();
    /// ナビゲーション矢印ボタンを有効化する
    void enableArrowButtons();
    /// ナビゲーション全体の有効/無効を切り替える
    void setNavigationEnabled(bool on);
    /// 対局開始時にナビゲーションを無効化する
    void disableNavigationForGame();
    /// 対局終了時にナビゲーションを有効化する
    void enableNavigationAfterGame();

    // --- 盤面・反転 ---

    /// 盤面反転完了後の後処理（ShogiView::boardFlipped に接続）
    void onBoardFlipped(bool nowFlipped);
    /// 盤面サイズ変更時の後処理（ShogiView::boardSizeChanged に接続）
    void onBoardSizeChanged(QSize fieldSize);
    /// 反転アクション発火時の処理
    void onReverseTriggered();
    /// 評価値グラフの高さ調整を遅延実行する（デバウンスタイマーから呼ばれる）
    void performDeferredEvalChartResize();

    // --- 司令塔通知 ---

    /// 対局終了手（投了等）の棋譜追記を要求する（MatchCoordinator::requestAppendGameOverMove に接続）
    void onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

    // --- 移動要求 ---

    /// 盤面上の駒移動要求を処理する（BoardInteractionController::moveRequested に接続）
    void onMoveRequested(const QPoint& from, const QPoint& to);

    // --- リプレイ ---

    /// リプレイモードの開始/終了を切り替える
    void setReplayMode(bool on);

    // --- CSA通信対局（CsaGameWiring経由） ---

    /// CSA通信対局のPlayMode変更通知（CsaGameWiring::playModeChanged に接続）
    void onCsaPlayModeChanged(int mode);
    /// CSA通信対局の終了ダイアログ表示要求（CsaGameWiring::showGameEndDialog に接続）
    void onCsaShowGameEndDialog(const QString& title, const QString& message);
    /// CSA通信対局のエンジン評価値更新（CsaGameWiring::engineScoreUpdated に接続）
    void onCsaEngineScoreUpdated(int scoreCp, int ply);

    // --- 定跡ウィンドウ（JosekiWindowWiring経由） ---

    /// 定跡ウィンドウからの強制成り設定変更（JosekiWindowWiring::forcedPromotion に接続）
    void onJosekiForcedPromotion(bool forced, bool promote);

    // --- 内部配線 ---

    /// ShogiViewのクリックシグナルをBoardInteractionControllerに接続する
    void connectBoardClicks();
    /// 駒移動要求シグナルを接続する
    void connectMoveRequested();

    // --- 棋譜表示 / 同期 ---

    /// 指し手確定時の棋譜・盤面更新（ShogiGameController::moveCommitted に接続）
    void onMoveCommitted(ShogiGameController::Player mover, int ply);
    /// 棋譜表示リストを更新する
    void displayGameRecord(const QList<KifDisplayItem> disp);
    /// 指定手数の盤面とハイライトを同期する
    void syncBoardAndHighlightsAtRow(int ply1);
    /// RecordPresenterからの行変更通知を処理する
    void onRecordRowChangedByPresenter(int row, const QString& comment);
    /// コメント更新通知を処理する
    void onCommentUpdated(int moveIndex, const QString& newComment);
    /// 読み筋行クリック時にPVボードダイアログを表示/更新する
    void onPvRowClicked(int engineIndex, int row);
    /// PVボードダイアログ閉時のクリーンアップ
    void onPvDialogClosed(int engineIndex);
    /// 棋譜貼り付けダイアログからのインポート要求を処理する
    void onKifuPasteImportRequested(const QString& content);
    /// GameRecordModelのコメント変更を検出して反映する
    void onGameRecordCommentChanged(int ply, const QString& comment);
    /// CommentCoordinatorからのコメント更新コールバック
    void onCommentUpdateCallback(int ply, const QString& comment);

    // --- 分岐ノード活性化 ---

    /// 分岐ツリーのノード活性化を処理する（BranchTreeWidget::nodeActivated に接続）
    void onBranchNodeActivated(int row, int ply);
    /// 分岐ノード処理完了後の盤面・ハイライト更新
    void onBranchNodeHandled(int ply, const QString& sfen,
                             int previousFileTo, int previousRankTo,
                             const QString& lastUsiMove);

    /// 検討モードで選択行の局面から検討を再開する
    void onBuildPositionRequired(int row);

    /// 分岐ツリーの構築完了を処理する（KifuBranchTreeBuilder::treeBuilt に接続）
    void onBranchTreeBuilt();

    /// 分岐ラインの選択変更を処理する（BranchTreeWidget::lineSelectionChanged に接続）
    void onLineSelectionChanged(int newLineIndex);

    /// SFEN文字列から直接盤面を読み込む（分岐ナビゲーション用）
    void loadBoardFromSfen(const QString& sfen);

    /// SFEN文字列から盤面とハイライトを更新する（分岐ナビゲーション用）
    void loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen);

    // --- エラー / 前準備 ---

    /// ErrorBusからのエラー通知を処理する（ErrorBus::errorOccurred に接続）
    void onErrorBusOccurred(const QString& msg);
    /// 対局開始前のクリーンアップ処理（PreStartCleanupHandler::cleanupRequested に接続）
    void onPreStartCleanupRequested();
    /// 時間設定の適用要求を処理する（GameStartCoordinator::applyTimeControlRequested に接続）
    void onApplyTimeControlRequested(const GameStartCoordinator::TimeControl& tc);

    // --- 投了 ---

    /// 投了操作のトリガーを処理する
    void onResignationTriggered();

    // --- 連続対局 ---

    /// 連続対局の設定を受信する（ConsecutiveGamesController::configured に接続）
    void onConsecutiveGamesConfigured(int totalGames, bool switchTurn);
    /// 対局開始時の設定を保存する（MatchCoordinator::gameStarted に接続）
    void onGameStarted(const MatchCoordinator::StartOptions& opt);
    /// 次の連続対局を開始する
    void startNextConsecutiveGame();

    /// 対局開始後に棋譜欄の指定行を選択する（GameStartCoordinator::requestSelectKifuRow に接続）
    void onRequestSelectKifuRow(int row);

    // --- private ---
private:
    // --- メンバー変数 ---

    // --- 基本状態 / ゲーム状態 ---
    QString  m_startSfenStr;                  ///< 開始局面のSFEN文字列
    QString  m_currentSfenStr;                ///< 現在の局面のSFEN文字列
    QString  m_resumeSfenStr;                 ///< 再開時の局面SFEN文字列
    bool     m_errorOccurred = false;         ///< エラー発生フラグ
    int      m_currentMoveIndex = 0;          ///< 現在の手数インデックス
    QString  m_lastMove;                      ///< 直近の指し手（USI形式）
    PlayMode m_playMode = PlayMode::NotStarted; ///< 現在のプレイモード

    // --- 将棋盤 / コントローラ ---
    ShogiView*                   m_shogiView = nullptr;       ///< 将棋盤ビュー（非所有）
    ShogiGameController*         m_gameController = nullptr;  ///< ゲーム進行コントローラ（非所有）
    BoardInteractionController*  m_boardController = nullptr; ///< 盤面操作コントローラ（非所有）

    // --- USI / エンジン連携 ---
    Usi*        m_usi1 = nullptr;              ///< 先手側USIエンジン（非所有）
    Usi*        m_usi2 = nullptr;              ///< 後手側USIエンジン（非所有）
    QString     m_positionStr1;               ///< エンジン1用positionコマンド文字列
    QStringList m_positionStrList;            ///< 各手数のpositionコマンドリスト
    /// 対局中に生成したUSI指し手列。
    /// 棋譜読み込み時は KifuLoadCoordinator 側のUSI列を参照する。
    QStringList m_gameUsiMoves;

    // --- UI 構成 ---
    QTabWidget* m_tab = nullptr;              ///< メインタブウィジェット
    QWidget*    m_gameRecordLayoutWidget = nullptr; ///< 棋譜レイアウト用コンテナ
    QSplitter*  m_hsplit = nullptr;           ///< 水平スプリッター
    QWidget*    m_central = nullptr;          ///< セントラルウィジェット
    QVBoxLayout* m_centralLayout = nullptr;   ///< セントラルウィジェットのレイアウト

    // --- ダイアログ / 補助ウィンドウ ---
    StartGameDialog*         m_startGameDialog = nullptr;         ///< 対局開始ダイアログ
    ConsiderationDialog*     m_considerationDialog = nullptr;     ///< 検討ダイアログ
    TsumeShogiSearchDialog*  m_tsumeShogiSearchDialog = nullptr;  ///< 詰将棋探索ダイアログ
    KifuAnalysisDialog*      m_analyzeGameRecordDialog = nullptr; ///< 棋譜解析ダイアログ
    CsaGameDialog*           m_csaGameDialog = nullptr;           ///< CSA対局ダイアログ
    CsaWaitingDialog*        m_csaWaitingDialog = nullptr;        ///< CSA待機ダイアログ

    // --- CSA通信対局コーディネータ ---
    CsaGameCoordinator*      m_csaGameCoordinator = nullptr;     ///< CSA通信対局管理（非所有）

    // --- モデル群 ---
    KifuRecordListModel*       m_kifuRecordModel  = nullptr;     ///< 棋譜レコードリストモデル
    KifuBranchListModel*       m_kifuBranchModel  = nullptr;     ///< 分岐リストモデル
    ShogiEngineThinkingModel*  m_modelThinking1   = nullptr;     ///< エンジン1思考モデル
    ShogiEngineThinkingModel*  m_modelThinking2   = nullptr;     ///< エンジン2思考モデル
    ShogiEngineThinkingModel*  m_considerationModel = nullptr;   ///< 検討タブ専用モデル
    KifuAnalysisListModel*     m_analysisModel    = nullptr;     ///< 棋譜解析結果モデル
    UsiCommLogModel*           m_lineEditModel1   = nullptr;     ///< USI通信ログモデル（エンジン1）
    UsiCommLogModel*           m_lineEditModel2   = nullptr;     ///< USI通信ログモデル（エンジン2）

    // --- 記録 / 評価 / 表示用データ ---
    EvaluationGraphController* m_evalGraphController = nullptr;  ///< 評価値グラフ管理
    QString           m_humanName1, m_humanName2;   ///< 対局者名（人間側）
    QString           m_engineName1, m_engineName2; ///< 対局者名（エンジン側）
    QStringList*      m_sfenRecord   = nullptr;     ///< 各手数のSFENレコード（非所有）
    QList<KifuDisplay *>* m_moveRecords = nullptr;  ///< 指し手表示レコード（非所有）
    QStringList       m_kifuDataList;               ///< 棋譜データリスト
    QString           defaultSaveFileName;          ///< デフォルトの保存ファイル名
    QString           kifuSaveFileName;             ///< 棋譜保存ファイル名
    QVector<ShogiMove> m_gameMoves;                 ///< 対局中の指し手列
    QList<KifDisplayItem> m_liveDisp;               ///< リアルタイム棋譜表示データ

    // --- 時計 / 時刻管理 ---
    TimeControlController* m_timeController = nullptr; ///< 時間制御コントローラ（非所有）

    // --- 対局情報タブ ---
    GameInfoPaneController* m_gameInfoController = nullptr; ///< 対局情報ペイン管理（非所有）

    // --- 棋譜表示 / 分岐操作 ---
    QSet<int> m_branchablePlySet;             ///< 分岐可能な手数の集合
    QVector<QString> m_commentsByRow;         ///< 行ごとのコメント
    int m_activePly          = 0;             ///< 現在アクティブな手数
    int m_currentSelectedPly = 0;             ///< 現在選択中の手数
    QMetaObject::Connection m_connKifuRowChanged; ///< 棋譜行変更シグナルの接続ハンドル
    bool m_onMainRowGuard = false;            ///< 行変更処理の再入防止ガード

    // --- 棋譜データ中央管理 ---
    GameRecordModel* m_gameRecord = nullptr;  ///< 棋譜データモデル（非所有）

    // --- 新UI部品 / ナビゲーション ---
    RecordPane*        m_recordPane = nullptr;   ///< 棋譜欄ウィジェット（非所有）
    EngineAnalysisTab* m_analysisTab = nullptr;  ///< エンジン解析タブ（非所有）

    // --- 評価値グラフドック ---
    QDockWidget*           m_evalChartDock = nullptr; ///< 評価値グラフ用ドック
    EvaluationChartWidget* m_evalChart = nullptr;     ///< 評価値グラフウィジェット

    // --- 棋譜欄ドック ---
    QDockWidget*           m_recordPaneDock = nullptr; ///< 棋譜欄用ドック

    // --- 解析ドック群 ---
    QDockWidget*           m_gameInfoDock = nullptr;      ///< 対局情報ドック
    QDockWidget*           m_thinkingDock = nullptr;      ///< 思考ドック
    QDockWidget*           m_considerationDock = nullptr;  ///< 検討ドック
    QDockWidget*           m_usiLogDock = nullptr;        ///< USI通信ログドック
    QDockWidget*           m_csaLogDock = nullptr;        ///< CSA通信ログドック
    QDockWidget*           m_commentDock = nullptr;       ///< 棋譜コメントドック
    QDockWidget*           m_branchTreeDock = nullptr;    ///< 分岐ツリードック

    // --- その他ドック ---
    QDockWidget*           m_menuWindowDock = nullptr;      ///< メニューウィンドウドック
    QDockWidget*           m_josekiWindowDock = nullptr;    ///< 定跡ウィンドウドック
    QDockWidget*           m_analysisResultsDock = nullptr;  ///< 棋譜解析結果ドック
    QMenu*                 m_savedLayoutsMenu = nullptr;    ///< 保存済みドックレイアウトのサブメニュー

    // --- 試合進行（司令塔） ---
    MatchCoordinator* m_match = nullptr;      ///< 対局進行の司令塔（非所有）
    QMetaObject::Connection m_timeConn;       ///< 時間更新シグナルの接続ハンドル

    // --- 連続対局設定 ---
    GameStartCoordinator::TimeControl m_lastTimeControl; ///< 前回の時間設定（連続対局用）

    // --- リプレイ制御 ---
    ReplayController* m_replayController = nullptr; ///< リプレイコントローラ（非所有）

    // --- 手番 / 残時間 ---
    bool   m_lastP1Turn = true;               ///< 直近の手番（true=先手）
    qint64 m_lastP1Ms   = 0;                  ///< 先手の残り時間（ms）
    qint64 m_lastP2Ms   = 0;                  ///< 後手の残り時間（ms）

    // --- 手番方向 ---
    bool m_bottomIsP1 = true;                 ///< 画面下側が先手かどうか

    // --- 評価値グラフ高さ調整 ---
    QTimer* m_evalChartResizeTimer = nullptr;  ///< 高さ調整用デバウンスタイマー

    // --- コーディネータ / プレゼンタ ---
    KifuLoadCoordinator*      m_kifuLoadCoordinator = nullptr; ///< 棋譜読込コーディネータ（非所有）
    PositionEditController*   m_posEdit = nullptr;             ///< 局面編集コントローラ（非所有）
    BoardSyncPresenter*       m_boardSync = nullptr;           ///< 盤面同期プレゼンタ（非所有）
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
    GameLayoutBuilder*        m_layoutBuilder    = nullptr;    ///< ゲームレイアウト構築（非所有）

    // --- ダイアログ管理 ---
    DialogCoordinator*        m_dialogCoordinator = nullptr;   ///< ダイアログ管理コーディネータ（非所有）

    // --- 棋譜エクスポート管理 ---
    KifuExportController*     m_kifuExportController = nullptr; ///< 棋譜エクスポートコントローラ（非所有）

    // --- ゲーム状態管理 ---
    GameStateController*      m_gameStateController = nullptr;  ///< ゲーム状態コントローラ（非所有）

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

    // --- 対局開始前クリーンアップ ---
    PreStartCleanupHandler*   m_preStartCleanupHandler = nullptr; ///< 対局開始前クリーンアップハンドラ（非所有）

    // --- 言語設定 ---
    QActionGroup* m_languageActionGroup = nullptr; ///< 言語切替用アクショングループ

    // --- 補助コントローラ群 ---
    JishogiScoreDialogController* m_jishogiController = nullptr;       ///< 持将棋スコアダイアログコントローラ（非所有）
    NyugyokuDeclarationHandler* m_nyugyokuHandler = nullptr;           ///< 入玉宣言ハンドラ（非所有）
    ConsecutiveGamesController* m_consecutiveGamesController = nullptr; ///< 連続対局コントローラ（非所有）
    LanguageController* m_languageController = nullptr;                ///< 言語設定コントローラ（非所有）
    ConsiderationModeUIController* m_considerationUIController = nullptr; ///< 検討モードUIコントローラ（非所有）
    ConsiderationWiring* m_considerationWiring = nullptr;              ///< 検討モード配線（非所有）
    DockLayoutManager* m_dockLayoutManager = nullptr;                  ///< ドックレイアウト管理（非所有）
    DockCreationService* m_dockCreationService = nullptr;              ///< ドック生成サービス（非所有）
    CommentCoordinator* m_commentCoordinator = nullptr;                ///< コメントコーディネータ（非所有）
    UsiCommandController* m_usiCommandController = nullptr;            ///< USIコマンドコントローラ（非所有）
    TestAutomationHelper* m_testHelper = nullptr;                      ///< テスト自動化ヘルパー（非所有）
    RecordNavigationHandler* m_recordNavHandler = nullptr;             ///< 棋譜ナビゲーションハンドラ（非所有）

    // --- 分岐ナビゲーション ---
    KifuBranchTree* m_branchTree = nullptr;              ///< 分岐ツリーデータ（非所有）
    KifuNavigationState* m_navState = nullptr;           ///< 棋譜ナビゲーション状態（非所有）
    KifuNavigationController* m_kifuNavController = nullptr; ///< 棋譜ナビゲーションコントローラ（非所有）
    KifuDisplayCoordinator* m_displayCoordinator = nullptr;  ///< 棋譜表示コーディネータ（非所有）
    BranchTreeWidget* m_branchTreeWidget = nullptr;      ///< 分岐ツリーウィジェット（非所有）
    LiveGameSession* m_liveGameSession = nullptr;        ///< リアルタイム対局セッション（非所有）

    // --- privateメソッド ---

    // --- UI / 表示更新 ---
    /// 指し手確定後に棋譜表示を更新する
    void updateGameRecord(const QString& elapsedTime);
    /// 手番表示を更新する
    void updateTurnStatus(int currentPlayer);
    /// 遅延初期化: EvaluationGraphControllerを生成し依存を設定する
    void ensureEvaluationGraphController();

    // --- 初期化 / セットアップ ---
    /// 各種コンポーネントの初期化を行う
    void initializeComponents();
    /// 水平方向のゲームレイアウトを構築する
    void setupHorizontalGameLayout();
    /// セントラルゲーム表示を初期化する
    void initializeCentralGameDisplay();
    /// 遅延初期化: TimeControlControllerを生成し依存を設定する
    void ensureTimeController();
    /// MatchCoordinatorを初期化し配線する
    void initMatchCoordinator();
    /// 棋譜欄ウィジェットをセットアップする
    void setupRecordPane();
    /// エンジン解析タブをセットアップする
    void setupEngineAnalysisTab();
    /// 盤面操作コントローラをセットアップする
    void setupBoardInteractionController();
    /// 評価値グラフのQDockWidgetを作成する
    void createEvalChartDock();
    /// 棋譜欄のQDockWidgetを作成する
    void createRecordPaneDock();
    /// 解析関連のQDockWidget群を作成する
    void createAnalysisDocks();
    /// 将棋盤をセントラルウィジェットに配置する
    void setupBoardInCenter();
    /// メニューウィンドウのQDockWidgetを作成する
    void createMenuWindowDock();
    /// 定跡ウィンドウのQDockWidgetを作成する
    void createJosekiWindowDock();
    /// 棋譜解析結果のQDockWidgetを作成する
    void createAnalysisResultsDock();
    /// 分岐ナビゲーション関連クラスを初期化する
    void initializeBranchNavigationClasses();

    // --- ゲーム開始 / 切替 ---
    /// SFEN文字列で新規対局を初期化する
    void initializeNewGame(QString& startSfenStr);
    /// 新規将棋対局を開始する
    void startNewShogiGame(QString& startSfenStr);
    /// プレイモードに応じてエンジン名を設定する
    void setEngineNamesBasedOnMode();
    /// 第2エンジンの表示/非表示を更新する
    void updateSecondEngineVisibility();

    // --- 入出力 / 設定 ---
    /// ウィンドウサイズと盤面設定を保存する
    void saveWindowAndBoardSettings();
    /// 保存済みウィンドウ設定を復元する
    void loadWindowSettings();

    // --- ユーティリティ ---
    /// プレイモードに応じて対局者名を設定する
    void setPlayersNamesForMode();
    /// 現在の手番を設定する
    void setCurrentTurn();
    /// 対局終了手（投了・詰み等）を棋譜に追加する
    void setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne);
    /// 棋譜行を追加する
    void appendKifuLine(const QString& text, const QString& elapsedTime);
    /// コメントを各関連コンポーネントに配信する
    void broadcastComment(const QString& text, bool asHtml=false);

    // --- 手番チェック ---
    /// 現在の手番が人間かどうかを判定する
    bool isHumanTurnNow() const;

    // --- フォント / 描画 ---
    /// 対局者名と時計のフォントを設定する
    void setupNameAndClockFonts();

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
    /// 遅延初期化: AnalysisResultsPresenterを生成し依存を設定する
    void ensureAnalysisPresenter();
    /// 遅延初期化: GameStartCoordinatorを生成し依存を設定する
    void ensureGameStartCoordinator();
    /// 遅延初期化: GameRecordPresenterを生成し依存を設定する
    void ensureRecordPresenter();
    /// 遅延初期化: LiveGameSessionが未開始なら開始する
    void ensureLiveGameSessionStarted();
    /// 遅延初期化: リアルタイム対局用にKifuLoadCoordinatorを準備する
    void ensureKifuLoadCoordinatorForLive();
    /// 遅延初期化: GameRecordModelを生成する
    void ensureGameRecordModel();
    /// 遅延初期化: DialogCoordinatorを生成し依存を設定する
    void ensureDialogCoordinator();
    /// 遅延初期化: KifuExportControllerを生成し依存を設定する
    void ensureKifuExportController();
    /// KifuExportControllerの依存オブジェクトを最新状態に更新する
    void updateKifuExportDependencies();
    /// 遅延初期化: GameStateControllerを生成し依存を設定する
    void ensureGameStateController();
    /// 遅延初期化: PlayerInfoControllerを生成し依存を設定する
    void ensurePlayerInfoController();
    /// 遅延初期化: BoardSetupControllerを生成し依存を設定する
    void ensureBoardSetupController();
    /// 遅延初期化: PvClickControllerを生成し依存を設定する
    void ensurePvClickController();
    /// 遅延初期化: PositionEditCoordinatorを生成し依存を設定する
    void ensurePositionEditCoordinator();
    /// 遅延初期化: CsaGameWiringを生成し依存を設定する
    void ensureCsaGameWiring();
    /// 遅延初期化: JosekiWindowWiringを生成し依存を設定する
    void ensureJosekiWiring();
    /// 遅延初期化: MenuWindowWiringを生成し依存を設定する
    void ensureMenuWiring();
    /// 遅延初期化: PlayerInfoWiringを生成し依存を設定する
    void ensurePlayerInfoWiring();
    /// 遅延初期化: PreStartCleanupHandlerを生成し依存を設定する
    void ensurePreStartCleanupHandler();
    /// 遅延初期化: JishogiScoreDialogControllerを生成し依存を設定する
    void ensureJishogiController();
    /// 遅延初期化: NyugyokuDeclarationHandlerを生成し依存を設定する
    void ensureNyugyokuHandler();
    /// 遅延初期化: ConsecutiveGamesControllerを生成し依存を設定する
    void ensureConsecutiveGamesController();
    /// 遅延初期化: LanguageControllerを生成し依存を設定する
    void ensureLanguageController();
    /// 遅延初期化: ConsiderationModeUIControllerを生成し依存を設定する
    void ensureConsiderationUIController();
    /// 遅延初期化: ConsiderationWiringを生成し依存を設定する
    void ensureConsiderationWiring();
    /// 遅延初期化: DockLayoutManagerを生成し依存を設定する
    void ensureDockLayoutManager();
    /// 遅延初期化: DockCreationServiceを生成し依存を設定する
    void ensureDockCreationService();
    /// 遅延初期化: CommentCoordinatorを生成し依存を設定する
    void ensureCommentCoordinator();
    /// 遅延初期化: UsiCommandControllerを生成し依存を設定する
    void ensureUsiCommandController();
    /// 遅延初期化: TestAutomationHelperを生成し依存を設定する
    void ensureTestAutomationHelper();
    /// 遅延初期化: RecordNavigationHandlerを生成し依存を設定する
    void ensureRecordNavigationHandler();

    // --- コンストラクタ分割先 ---
    /// セントラルウィジェットのコンテナを構築する
    void setupCentralWidgetContainer();
    /// UIデザイナーで定義したツールバーを設定する
    void configureToolBarFromUi();
    /// ゲーム表示パネル群を構築する
    void buildGamePanels();
    /// ウィンドウ設定を復元し状態を同期する
    void restoreWindowAndSync();
    /// 全メニューアクションのシグナル/スロットを接続する
    void connectAllActions();
    /// コアシグナル群を接続する
    void connectCoreSignals();
    /// アプリケーション全体のツールチップフィルタを設定する
    void installAppToolTips();
    /// コーディネータの最終初期化を行う
    void finalizeCoordinators();

    // --- KifuLoadCoordinator ヘルパー ---
    /// KifuLoadCoordinatorを作成しシグナル/スロットを配線する
    void createAndWireKifuLoadCoordinator();
    /// 指定パスの棋譜ファイルを読み込む
    void dispatchKifuLoad(const QString& filePath);

    // --- 棋譜ナビゲーション ---
    /// 棋譜ビューを指定手数の行にスクロールする
    void navigateKifuViewToRow(int ply);

    // --- フック用メンバー関数 ---
    /// 新規対局初期化のフックポイント（GameStartCoordinator用コールバック）
    void initializeNewGameHook(const QString& s);
    /// 指し手のハイライトを盤面に表示する
    void showMoveHighlights(const QPoint& from, const QPoint& to);
    /// 棋譜行追加のフックポイント（GameStartCoordinator用コールバック）
    void appendKifuLineHook(const QString& text, const QString& elapsed);

    // --- 時間取得ヘルパー ---
    /// 指定プレイヤーの残り時間を取得する（ms）
    qint64 getRemainingMsFor(MatchCoordinator::Player p) const;
    /// 指定プレイヤーの加算時間を取得する（ms）
    qint64 getIncrementMsFor(MatchCoordinator::Player p) const;

    // --- 検討用ヘルパー ---
    /// 指定手数のpositionコマンド文字列を構築する
    QString buildPositionStringForIndex(int moveIndex) const;
    /// 秒読み時間を取得する（ms）
    qint64 getByoyomiMs() const;

    // --- ゲームオーバー関連 ---
    /// 対局終了メッセージボックスを表示する
    void showGameOverMessageBox(const QString& title, const QString& message);

    // --- 棋譜自動保存 ---
    /// プレイモードと対局者名に基づいて棋譜を自動保存する
    void autoSaveKifuToFile(const QString& saveDir, PlayMode playMode,
                             const QString& humanName1, const QString& humanName2,
                             const QString& engineName1, const QString& engineName2);

    // --- 分岐ツリー更新 ---
    /// リアルタイム対局中の分岐ツリーを再構築する
    void refreshBranchTreeLive();

    // --- ガード / 判定ヘルパー ---
    bool getMainRowGuard() const;
    void setMainRowGuard(bool on);
    /// 人間vs人間モードかどうかを判定する
    bool isHvH() const;
    /// 指定プレイヤーが人間側かどうかを判定する
    bool isHumanSide(ShogiGameController::Player p) const;

    // --- 表示更新 ---
    /// 手番と持ち時間の表示を更新する
    void updateTurnAndTimekeepingDisplay();

    // --- 編集メニュー ---
    /// 起動時の編集メニュー状態を初期化する
    void initializeEditMenuForStartup();
    /// 局面編集中の編集メニュー状態を適用する
    void applyEditMenuEditingState(bool editing);

    // --- 開始局面解決 ---
    /// 対局開始時に使用するSFEN文字列を決定する
    QString resolveCurrentSfenForGameStart() const;

    /// 分岐ナビゲーション由来の盤面更新中に、通常の同期処理が再入しないようにするガード
    bool m_skipBoardSyncForBranchNav = false;
};

#endif // MAINWINDOW_H
