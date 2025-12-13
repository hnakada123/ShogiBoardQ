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
#include <QVBoxLayout>

// ==============================
// Project includes (types used by value ＝前方宣言できないもの優先)
// ==============================
#include "playmode.h"                 // PlayMode を値で使用
#include "shogimove.h"                // QVector<ShogiMove> をメンバで保持
#include "matchcoordinator.h"         // MatchCoordinator::GameEndInfo を値で使用
#include "kifurecordlistmodel.h"      // KifDisplayItem を値で使用（disp 等）
#include "kifuanalysislistmodel.h"    // KifGameInfoItem を値で使用

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

// ==============================
// Macros / aliases
// ==============================
#define SHOGIBOARDQ_DEBUG_KIF 1   // 0にすればログは一切出ません


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ==============================
// Forward declarations（ポインタ/参照のみ使用する型）
// ==============================
class QPainter;
class QStyleOptionViewItem;
class QModelIndex;
class NavigationController;
class QGraphicsView;
class QGraphicsPathItem;
class QTableView;
class QEvent;
class QLabel;           // ★ 追加
class QToolButton;      // ★ 追加
class QPushButton;      // ★ 追加
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
class GameRecordModel;  // ★ 追加: 棋譜データ中央管理

// ============================================================
// MainWindow
// ============================================================
class MainWindow : public QMainWindow, public INavigationContext
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);

    // -------- INavigationContext (public) --------
    bool hasResolvedRows() const override;
    int  resolvedRowCount() const override;
    int  activeResolvedRow() const override;
    int  maxPlyAtRow(int row) const override;
    int  currentPly() const override;
    void applySelect(int row, int ply) override;

protected:
    Ui::MainWindow* ui = nullptr;

    // QMainWindow
    void closeEvent(QCloseEvent* event) override;

private:
    // ========================================================
    // Private Data Members（用途別にグルーピング）
    // ========================================================

    // --- 基本状態 / ゲーム状態 ---
    QString  m_startSfenStr;
    QString  m_currentSfenStr;
    QString  m_resumeSfenStr; // 再開用
    bool     m_errorOccurred = false;
    int      m_currentMoveIndex = 0;
    QString  m_lastMove;
    PlayMode m_playMode = NotStarted;

    // --- 将棋盤 / コントローラ類 ---
    ShogiView*                   m_shogiView = nullptr;
    ShogiGameController*         m_gameController = nullptr;
    BoardInteractionController*  m_boardController = nullptr;

    // --- USI / エンジン連携 ---
    Usi*        m_usi1 = nullptr;
    Usi*        m_usi2 = nullptr;
    QString     m_positionStr1;
    QStringList m_positionStrList;
    QStringList m_usiMoves;

    // --- UI 構成 ---
    QTabWidget* m_tab = nullptr; // EngineAnalysisTab 内部の QTabWidget を流用
    QWidget*    m_gameRecordLayoutWidget = nullptr; // 右側（RecordPane）
    QSplitter*  m_hsplit = nullptr;

    // --- ダイアログ / 補助ウィンドウ ---
    StartGameDialog*         m_startGameDialog = nullptr;
    ConsiderationDialog*     m_considerationDialog = nullptr;
    TsumeShogiSearchDialog*  m_tsumeShogiSearchDialog = nullptr;
    KifuAnalysisDialog*      m_analyzeGameRecordDialog = nullptr;

    // --- モデル群 ---
    KifuRecordListModel*       m_kifuRecordModel  = nullptr;
    KifuBranchListModel*       m_kifuBranchModel  = nullptr;
    ShogiEngineThinkingModel*  m_modelThinking1   = nullptr;
    ShogiEngineThinkingModel*  m_modelThinking2   = nullptr;
    KifuAnalysisListModel*     m_analysisModel    = nullptr;
    UsiCommLogModel*           m_lineEditModel1   = nullptr; // Engine 1 info
    UsiCommLogModel*           m_lineEditModel2   = nullptr; // Engine 2 info

    // --- 記録 / 評価 / 表示用データ ---
    QList<int>        m_scoreCp;
    QString           m_humanName1, m_humanName2;
    QString           m_engineName1, m_engineName2;
    QStringList*      m_sfenRecord   = nullptr;
    QList<KifuDisplay *>* m_moveRecords = nullptr;
    QStringList       m_kifuDataList;
    QString           defaultSaveFileName;
    QString           kifuSaveFileName;
    QVector<ShogiMove> m_gameMoves;

    // --- 時計 / 時刻管理 ---
    ShogiClock* m_shogiClock = nullptr;
    qint64      m_initialTimeP1Ms = 0;
    qint64      m_initialTimeP2Ms = 0;

    QVector<ResolvedRow> m_resolvedRows;
    int m_activeResolvedRow = 0;

    // --- KIFヘッダ（対局情報）タブ ---
    QDockWidget*  m_gameInfoDock  = nullptr;
    QTableWidget* m_gameInfoTable = nullptr;
    
    // ★ 追加: 対局情報編集用UI
    QWidget*      m_gameInfoContainer = nullptr;   // テーブル＋ツールバーのコンテナ
    QWidget*      m_gameInfoToolbar = nullptr;
    QToolButton*  m_btnGameInfoFontIncrease = nullptr;
    QToolButton*  m_btnGameInfoFontDecrease = nullptr;
    QToolButton*  m_btnGameInfoUndo = nullptr;     // undoボタン
    QToolButton*  m_btnGameInfoRedo = nullptr;     // ★ 追加: redoボタン
    QToolButton*  m_btnGameInfoCut = nullptr;      // ★ 追加: 切り取りボタン
    QToolButton*  m_btnGameInfoCopy = nullptr;     // ★ 追加: コピーボタン
    QToolButton*  m_btnGameInfoPaste = nullptr;    // ★ 追加: 貼り付けボタン
    QLabel*       m_gameInfoEditingLabel = nullptr;
    QPushButton*  m_btnGameInfoUpdate = nullptr;
    int           m_gameInfoFontSize = 10;
    bool          m_gameInfoDirty = false;
    QList<KifGameInfoItem> m_originalGameInfo;  // 元の対局情報（変更検知用）
    QString       m_gameInfoClipboard;  // ★ 追加: 対局情報用の内部クリップボード

    // --- 棋譜表示／分岐操作・表示関連 ---
    QSet<int> m_branchablePlySet;
    QVector<QString> m_commentsByRow;  // ★ 互換性のため残す（GameRecordModelと同期）
    int m_activePly          = 0;
    int m_currentSelectedPly = 0;
    QMetaObject::Connection m_connKifuRowChanged;
    bool m_onMainRowGuard = false; // 再入防止

    // --- ★ 追加: 棋譜データ中央管理 ---
    GameRecordModel* m_gameRecord = nullptr;

    // --- 装飾（棋譜テーブル マーカー描画） ---
    class BranchRowDelegate : public QStyledItemDelegate {
    public:
        explicit BranchRowDelegate(QObject* parent = nullptr)
            : QStyledItemDelegate(parent) {}
        void setMarkers(const QSet<int>* marks) { m_marks = marks; }
        void paint(QPainter* painter,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
    private:
        const QSet<int>* m_marks = nullptr;
    };

    // --- 新UI部品 / ナビゲーション ---
    RecordPane*           m_recordPane = nullptr;
    NavigationController* m_nav = nullptr;
    EngineAnalysisTab*    m_analysisTab = nullptr;

    // --- 試合進行（司令塔） ---
    MatchCoordinator* m_match = nullptr;
    QMetaObject::Connection m_timeConn{};
    bool m_isReplayMode = false;

    // ========================================================
    // Private Methods（用途別）
    // ========================================================

    // --- UI / 表示更新 ---
    void updateGameRecord(const QString& elapsedTime);
    void updateTurnStatus(int currentPlayer);
    void redrawEngine1EvaluationGraph();
    void redrawEngine2EvaluationGraph();

    // --- 初期化 / セットアップ ---
    void initializeComponents();
    void setupHorizontalGameLayout();
    void initializeCentralGameDisplay();
    void ensureClockReady_();
    void initMatchCoordinator();
    void setupRecordPane();
    void setupEngineAnalysisTab();
    void setupBoardInteractionController();

    // --- ゲーム開始/切替 ---
    void initializeNewGame(QString& startSfenStr);
    void startNewShogiGame(QString& startSfenStr);
    void setEngineNamesBasedOnMode();

    // --- 入出力 / 設定 ---
    void saveWindowAndBoardSettings();
    void loadWindowSettings();

    // --- KIF ヘッダ（対局情報）周り ---
    void ensureGameInfoTable();
    
    // ★ 追加: 対局情報編集用メソッド
    void buildGameInfoToolbar();
    void updateGameInfoFontSize(int delta);
    void updateGameInfoEditingIndicator();
    void onGameInfoFontIncrease();
    void onGameInfoFontDecrease();
    void onGameInfoUndo();   // undoボタン
    void onGameInfoRedo();   // ★ 追加: redoボタン
    void onGameInfoCut();    // ★ 追加: 切り取り
    void onGameInfoCopy();   // ★ 追加: コピー
    void onGameInfoPaste();  // ★ 追加: 貼り付け
    void onGameInfoUpdateClicked();
    void onGameInfoCellChanged(int row, int column);
    void setOriginalGameInfo(const QList<KifGameInfoItem>& items);
    bool hasUnsavedGameInfo() const { return m_gameInfoDirty; }
    bool confirmDiscardUnsavedGameInfo();

    // --- 分岐 / 変化 ---
    void applyResolvedRowAndSelect(int row, int selPly);

    // --- ユーティリティ ---
    void setPlayersNamesForMode();
    void setCurrentTurn();

    void setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne);
    void appendKifuLine(const QString& text, const QString& elapsedTime);

    // コメントを両画面にブロードキャスト
    void broadcastComment(const QString& text, bool asHtml=false);

    std::unique_ptr<KifuVariationEngine> m_varEngine;

public slots:
    // --- ファイル I/O / 外部操作（★重複宣言禁止：ここだけに置く） ---
    void chooseAndLoadKifuFile();
    void saveShogiBoardImage();
    void copyBoardToClipboard();
    void openWebsiteInExternalBrowser();
    void saveKifuToFile();
    void overwriteKifuFile();

    // --- エラー/一般UI ---
    void displayErrorMessage(const QString& message);
    void saveSettingsAndClose();
    void resetToInitialState();
    void onFlowError_(const QString& msg);

    // --- ダイアログ表示 ---
    void displayPromotionDialog();
    void displayEngineSettingsDialog();
    void displayVersionInformation();
    void displayConsiderationDialog();
    void displayKifuAnalysisDialog();
    void displayTsumeShogiSearchDialog();

    // --- 追加：不足でエラーになっていた slots ---
    void toggleEngineAnalysisVisibility();
    void undoLastTwoMoves();

    // --- 盤編集 / 表示 ---
    void beginPositionEditing();
    void finishPositionEditing();

    // --- ゲーム開始 / 終了 ---
    void initializeGame();
    void handleResignation();

    void onActionFlipBoardTriggered(bool checked = false);

    void handleBreakOffGame();

    // 検討（ConsidarationMode）の手動終了（quit 送信）
    void handleBreakOffConsidaration();

    void movePieceImmediately();

    void onRecordPaneMainRowChanged_(int row);

private slots:
    void onMatchGameEnded(const MatchCoordinator::GameEndInfo& info);
    void flipBoardAndUpdatePlayerInfo();

    // --- 時計 / タイムアウト ---
    void onPlayer1TimeOut();
    void onPlayer2TimeOut();

    // --- ボタン有効/無効 ---
    void disableArrowButtons();
    void enableArrowButtons();

    // --- 盤面・反転 ---
    void onBoardFlipped(bool nowFlipped);

    void onReverseTriggered();

    // --- 司令塔（時計/進行）通知受け口 ---
    void onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

    // --- クリック/移動要求（コントローラ配線） ---
    void onMoveRequested_(const QPoint& from, const QPoint& to);

    // --- リプレイ（UI 側単一ソース管理） ---
    void setReplayMode(bool on);

    // --- 内部配線 ---
    void connectBoardClicks_();
    void connectMoveRequested_();

private slots:
    void onMoveCommitted(ShogiGameController::Player mover, int ply);
    void displayGameRecord(const QList<KifDisplayItem> disp);
    void syncBoardAndHighlightsAtRow(int ply1);
    void onRecordRowChangedByPresenter(int row, const QString& comment);
    void onCommentUpdated(int moveIndex, const QString& newComment);  // ★ 追加: コメント更新スロット

private:
    QWidget*     m_central = nullptr;
    QVBoxLayout* m_centralLayout = nullptr;

    // 行(row) → (ply → 表示計画) の保持
    // 例: m_branchDisplayPlan[row][ply]
    QHash<int, QMap<int, BranchCandidateDisplay>> m_branchDisplayPlan;

    // 選択（行row, 手数ply）から、ハイライトすべき(variation id, ply)を解決
    std::pair<int,int> resolveBranchHighlightTarget(int row, int ply) const;

    // 直近の手番と残時間（回転直後の復元に使う）
    bool   m_lastP1Turn = true;
    qint64 m_lastP1Ms   = 0;
    qint64 m_lastP2Ms   = 0;

    // 描画ヘルパ
    void setupNameAndClockFonts_();           // フォント明示設定

private slots:
    void onBranchNodeActivated_(int row, int ply);
    void onGameOverStateChanged(const MatchCoordinator::GameOverState& st);
    void onTurnManagerChanged(ShogiGameController::Player now);

private:
    // いま下段が先手(P1)か？ true=先手が手前、false=後手が手前
    bool m_bottomIsP1 = true;


    bool m_isLiveAppendMode = false;
    void exitLiveAppendMode_();   // 終局で選択を元に戻す

    bool m_isResumeFromCurrent = false;

    void ensureTurnSyncBridge_();

    KifuLoadCoordinator* m_kifuLoadCoordinator = nullptr;

private:
    PositionEditController* m_posEdit = nullptr;

    void ensurePositionEditController_();

private slots:
    void onErrorBusOccurred(const QString& msg);
    void onPreStartCleanupRequested_();
    void onApplyTimeControlRequested_(const GameStartCoordinator::TimeControl& tc);

private:
    // --- ctorの分割先 ---
    void setupCentralWidgetContainer_();   // centralWidget と QVBoxLayout を一度だけ構築
    void configureToolBarFromUi_();        // ツールバーのアイコン/スタイル初期化
    void buildGamePanels_();               // 棋譜ペイン/分岐配線/レイアウト/タブなどUI骨格
    void restoreWindowAndSync_();          // ウィンドウ設定の復元（同期は initializeComponents 内で実施）
    void connectAllActions_();             // メニュー/アクション群のconnect
    void connectCoreSignals_();            // GC/ビュー/エラーバス等のconnect
    void installAppToolTips_();            // コンパクトツールチップのインストール
    void finalizeCoordinators_();          // 司令塔やフォント/位置編集コントローラの最終初期化

private:
    BoardSyncPresenter* m_boardSync = nullptr;
    AnalysisResultsPresenter* m_analysisPresenter = nullptr;

    void ensureBoardSyncPresenter_();
    void ensureAnalysisPresenter_();

    GameStartCoordinator* m_gameStart = nullptr;
    void ensureGameStartCoordinator_();

    GameRecordPresenter* m_recordPresenter {nullptr};
    void ensureRecordPresenter_();
    QPointer<AnalysisFlowController> m_analysisFlow;

    GameStartCoordinator* m_gameStartCoordinator = nullptr;

    // hooks 用のメンバー関数（ラムダ不使用）
    void requestRedrawEngine1Eval_();
    void requestRedrawEngine2Eval_();
    void initializeNewGame_(const QString& s);
    void showMoveHighlights_(const QPoint& from, const QPoint& to);
    void appendKifuLineHook_(const QString& text, const QString& elapsed);

    BranchWiringCoordinator* m_branchWiring = nullptr;

    TimeDisplayPresenter* m_timePresenter = nullptr;

    AnalysisTabWiring* m_analysisWiring = nullptr;

    qint64 getRemainingMsFor_(MatchCoordinator::Player p) const;
    qint64 getIncrementMsFor_(MatchCoordinator::Player p) const;
    qint64 getByoyomiMs_() const;

    void showGameOverMessageBox_(const QString& title, const QString& message);

private slots:
    void onResignationTriggered();

    // MainWindow の private メンバに追加
private:
    RecordPaneWiring*     m_recordPaneWiring = nullptr;
    UiActionsWiring*      m_actionsWiring    = nullptr;   // 既に作っていればそのまま
    GameLayoutBuilder*    m_layoutBuilder    = nullptr;   // 既に作っていればそのまま

    void ensureKifuLoadCoordinatorForLive_();
    void refreshBranchTreeLive_();

    QList<KifDisplayItem> m_liveDisp;

private:
    bool getMainRowGuard_() const;
    void setMainRowGuard_(bool on);
    bool isHvH_() const;
    bool isHumanSide_(ShogiGameController::Player p) const;
    void updateTurnAndTimekeepingDisplay_();

    void initializeEditMenuForStartup();
    void applyEditMenuEditingState(bool editing);

    QString resolveCurrentSfenForGameStart_() const;

    // ★ 追加: GameRecordModel 初期化
    void ensureGameRecordModel_();
};

#endif // MAINWINDOW_H
