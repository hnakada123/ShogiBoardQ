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
#include "kiftosfenconverter.h"
#include "navigationcontext.h"
#include "recordpane.h"
#include "engineanalysistab.h"
#include "boardinteractioncontroller.h"
#include "kifuvariationengine.h"
#include "branchcandidatescontroller.h"
#include "kifuloadcoordinator.h"
#include "kifutypes.h"
#include "positioneditcontroller.h"

// ==============================
// Macros / aliases
// ==============================
#define SHOGIBOARDQ_DEBUG_KIF 1   // 0にすればログは一切出ません

using VariationBucket = QVector<KifLine>;     // 手目からの候補群

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
class ShogiView;

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
    int      m_totalMove = 0;

    // 旧コード互換（startpos/sfen のラベル保持用）
    QString  m_startPosStr;

    // --- 将棋盤 / コントローラ類 ---
    ShogiView*                   m_shogiView = nullptr;
    ShogiGameController*         m_gameController = nullptr;
    BoardInteractionController*  m_boardController = nullptr;

    // --- USI / エンジン連携 ---
    Usi*        m_usi1 = nullptr;
    Usi*        m_usi2 = nullptr;
    QString     m_engineFile1;
    QString     m_positionStr1;
    QString     m_positionStr2;
    QStringList m_positionStrList;
    QString     m_positionPonder1;
    QString     m_positionPonder2;
    QStringList m_usiMoves;
    bool        m_engine1IsP1 = false; // シングルエンジン時、先手担当なら true

    // --- UI 構成 ---
    QTabWidget* m_tab = nullptr; // EngineAnalysisTab 内部の QTabWidget を流用
    QWidget*    m_gameRecordLayoutWidget = nullptr; // 右側（RecordPane）
    QWidget*    m_playerAndBoardLayoutWidget = nullptr;
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
    int               m_gameCount = 0;
    QStringList       m_kifuDataList;
    QString           defaultSaveFileName;
    QString           kifuSaveFileName;
    QVector<ShogiMove> m_gameMoves;

    // --- 時計 / 時刻管理 ---
    ShogiClock* m_shogiClock = nullptr;
    QTimer*     m_uiClockTimer = nullptr;
    qint64      m_initialTimeP1Ms = 0;
    qint64      m_initialTimeP2Ms = 0;
    static constexpr int kFlagFallGraceMs = 200;

    QVector<ResolvedRow> m_resolvedRows;
    int m_activeResolvedRow = 0;

    // --- 分岐候補（テキスト）側の索引 ---
    struct BranchCandidate {
        QString text;  // 「▲２六歩(27)」
        int row;       // resolved 行
        int ply;       // 1始まり
    };
    QHash<int, QHash<QString, QList<BranchCandidate>>> m_branchIndex;
    QList<QPair<int,int>> m_branchRowMap; // .first=row, .second=ply

    // --- KIFヘッダ（対局情報）タブ ---
    QDockWidget*  m_gameInfoDock  = nullptr;
    QTableWidget* m_gameInfoTable = nullptr;

    // --- 棋譜表示／分岐操作・表示関連 ---
    QTableView* m_kifuBranchView        = nullptr; // 旧UI互換の受け皿
    QTableView* m_analysisResultsView   = nullptr; // 解析結果テーブル
    QList<KifDisplayItem> m_dispMain;
    QList<KifDisplayItem> m_dispCurrent;
    QStringList           m_sfenMain;
    QVector<ShogiMove>    m_gmMain;
    QHash<int, QList<KifLine>> m_variationsByPly;
    QList<KifLine>             m_variationsSeq;
    QSet<int> m_branchablePlySet;
    QVector<QString> m_commentsByRow;
    int m_activePly          = 0;
    int m_currentSelectedPly = 0;
    QString m_initialSfen;
    QMetaObject::Connection m_connKifuRowChanged;
    bool m_onMainRowGuard = false; // 再入防止

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
    BranchRowDelegate* m_branchRowDelegate = nullptr;

    // --- 新UI部品 / ナビゲーション ---
    RecordPane*           m_recordPane = nullptr;
    NavigationController* m_nav = nullptr;
    EngineAnalysisTab*    m_analysisTab = nullptr;

    // --- 試合進行（司令塔） ---
    MatchCoordinator* m_match = nullptr;
    QMetaObject::Connection m_timeConn{};
    bool m_isReplayMode = false;

    // --- 各種フラグ ---
    bool m_p1HasMoved = false;
    bool m_p2HasMoved = false;

    // ========================================================
    // Private Methods（用途別）
    // ========================================================

    // --- UI / 表示更新 ---
    void displayAnalysisResults();
    void updateGameRecord(const QString& elapsedTime);
    void updateTurnAndTimekeepingDisplay();
    void updateTurnDisplay();
    void updateTurnStatus(int currentPlayer);
    void applySfenAtCurrentPly();
    void setGameInProgressActions(bool inProgress);
    void displayPositionEditMenu();
    void hidePositionEditMenu();
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
    void wireMatchSignals_();

    // --- ゲーム開始/切替 ---
    void initializeNewGame(QString& startSfenStr);
    QString parseStartPositionToSfen(QString startPositionStr);
    void startNewShogiGame(QString& startSfenStr);
    void startGameBasedOnMode();
    void setEngineNamesBasedOnMode();

    // --- 時計 / 時間管理 ---
    void setRemainingTimeAndCountDown();
    void setTimerAndStart();
    enum class Winner { P1, P2 };

    // --- 入出力 / 設定 ---
    void saveWindowAndBoardSettings();
    void loadWindowSettings();
    void makeDefaultSaveFileName();

    // --- KIF ヘッダ（対局情報）周り ---
    void ensureGameInfoTable();

    // --- 分岐 / 変化 ---
    void applyResolvedRowAndSelect(int row, int selPly);
    void populateBranchListForPly(int ply);

    // --- 取込 / 解析補助 ---
    void rebuildSfenRecord(const QString& initialSfen,
                           const QStringList& usiMoves,
                           bool hasTerminal);
    void rebuildGameMoves(const QString& initialSfen,
                          const QStringList& usiMoves);

    // --- ユーティリティ ---
    void handleUndoMove(int index);
    void setPlayersNamesForMode();
    PlayMode determinePlayMode(int initPositionNumber, bool isPlayer1Human, bool isPlayer2Human) const;
    PlayMode setPlayMode();
    void getOptionFromStartGameDialog();
    void prepareDataCurrentPosition();
    void prepareInitialPosition();
    void setCurrentTurn();
    void movePieceImmediately();
    void resetGameFlags();
    void initializePositionStringsForMatch_();
    void setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne);
    QChar glyphForPlayer(bool isPlayerOne) const;
    void appendKifuLine(const QString& text, const QString& elapsedTime);
    bool isGameOver_() const;
    void analyzeGameRecord();      // 旧実装の呼び出しあり
    void startConsidaration();     // 旧実装の呼び出しあり（綴りそのまま）
    void startTsumiSearch();       // 旧実装の呼び出しあり

    // コメントを両画面にブロードキャスト
    void broadcastComment(const QString& text, bool asHtml=false);

    std::unique_ptr<KifuVariationEngine> m_varEngine;

    // --- 小さなフォーマッタ ---
    static inline QString fmt_mmss(qint64 ms) {
        if (ms < 0) ms = 0;
        QTime t(0,0); return t.addMSecs(static_cast<int>(ms)).toString("mm:ss");
    }
    static inline QString fmt_hhmmss(qint64 ms) {
        if (ms < 0) ms = 0;
        QTime t(0,0); return t.addMSecs(static_cast<int>(ms)).toString("hh:mm:ss");
    }

private slots:
    // ========================================================
    // Private Slots（connect先になるものはすべてここ）
    // ========================================================

    // --- エラー/一般UI ---
    void displayErrorMessage(const QString& message);
    void saveSettingsAndClose();
    void resetToInitialState();

    // --- ファイル I/O / 外部操作（★重複宣言禁止：ここだけに置く） ---
    void chooseAndLoadKifuFile();
    void saveShogiBoardImage();
    void copyBoardToClipboard();
    void openWebsiteInExternalBrowser();
    void saveKifuToFile();        // ← slots のみに配置
    void overwriteKifuFile();     // ← slots のみに配置

    // --- ダイアログ表示 ---
    void displayPromotionDialog();
    void displayEngineSettingsDialog();
    void displayVersionInformation();
    void displayConsiderationDialog();
    void displayKifuAnalysisDialog();
    void displayTsumeShogiSearchDialog();

    // --- ゲーム開始 / 終了 ---
    void initializeGame();
    void handleResignation();
    void onMatchGameEnded(const MatchCoordinator::GameEndInfo& info);

    // --- 盤編集 / 表示 ---
    void beginPositionEditing();
    void finishPositionEditing();
    void resetPiecesToStand();
    void setStandardStartPosition();
    void setTsumeShogiStartPosition();
    void flipBoardAndUpdatePlayerInfo();
    void enlargeBoard();
    void reduceBoardSize();
    void endDrag();

    // --- 時計 / タイムアウト ---
    void onPlayer1TimeOut();
    void onPlayer2TimeOut();

    // --- ボタン有効/無効 ---
    void disableArrowButtons();
    void enableArrowButtons();

    // --- 追加：不足でエラーになっていた slots ---
    void toggleEngineAnalysisVisibility();
    void undoLastTwoMoves();

    // --- 分岐 / 棋譜ナビゲーション ---
    void onMainMoveRowChanged(int row);
    void onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex& prev);

    // --- 盤面・反転 ---
    void onBoardFlipped(bool nowFlipped);
    void toggleEditSideToMove();
    void onActionFlipBoardTriggered(bool checked = false);
    void onReverseTriggered();

    // --- 司令塔（時計/進行）通知受け口 ---
    void onMatchTimeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 urgencyMs);
    void onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

    // --- クリック/移動要求（コントローラ配線） ---
    void onMoveRequested_(const QPoint& from, const QPoint& to);

    // --- リプレイ（UI 側単一ソース管理） ---
    void setReplayMode(bool on);

    // --- 内部配線 ---
    void connectBoardClicks_();
    void connectMoveRequested_();

    // --- moveRequested 後の分岐ハンドラ（対局モード別） ---
    void handleMove_HvH_(ShogiGameController::Player moverBefore,
                         const QPoint& from, const QPoint& to);
    void handleMove_HvE_(const QPoint& humanFrom, const QPoint& humanTo);

private slots:
    void onBranchCandidateActivated(const QModelIndex& index); // QModelIndex 版に変更
    void onMoveCommitted(ShogiGameController::Player mover, int ply);
    void displayGameRecord(const QList<KifDisplayItem> disp);
    void syncBoardAndHighlightsAtRow(int ply1);

private:
    QVector<int> m_branchVarIds;   // 行→variationId の対応（末尾の「本譜へ戻る」は -1）
    int m_branchPlyContext = -1;   // どの ply の候補か（必要なら使用）

    // --- MainWindow.h にメンバを追加 ---
    QWidget*     m_central = nullptr;
    QVBoxLayout* m_centralLayout = nullptr;

    BranchCandidatesController* m_branchCtl = nullptr;

    void setupBranchView_();

    // 行 index -> (ply -> 許可する variationId 集合)
    QVector<QHash<int, QSet<int>>> m_branchWhitelist;

    // 分岐ホワイトリスト: 行(row) → 手数(ply) → 許可する variationId の集合
    QHash<int, QHash<int, QSet<int>>> m_branchWL;

    // mainwindow.h （private: あたりに追加）
    bool m_loadingKifu = false;   // KIF読込～WL完成まで分岐更新を抑止

    bool m_branchTreeLocked = false;  // ← 分岐ツリーの追加・変更を禁止するロック

    // 分岐候補の表示アイテム（どの行のどのラベルか）
    struct BranchCandidateDisplayItem {
        int     row;        // 0=Main, 1=Var0, 2=Var1, ...
        int     varN;       // -1=Main, それ以外は VarN の N
        QString lineName;   // "Main" or "VarN"
        QString label;      // "▲２六歩(27)" 等
    };

    // 1行・1手（ply）に対する表示計画
    /*
    struct BranchCandidateDisplay {
        int     ply;        // 1-based。表示する手数（「分岐あり」を1手先へ移した位置）
        QString baseLabel;  // その行の ply 手目の指し手（見出し用）
        QVector<BranchCandidateDisplayItem> items; // 表示候補（重複整理後）
    };
    */

    // 行(row) → (ply → 表示計画) の保持
    // 例: m_branchDisplayPlan[row][ply]
    QHash<int, QMap<int, BranchCandidateDisplay>> m_branchDisplayPlan;

    void showBranchCandidatesFromPlan(int row, int ply1);

    // 選択（行row, 手数ply）から、ハイライトすべき(variation id, ply)を解決
    std::pair<int,int> resolveBranchHighlightTarget(int row, int ply) const;

    // 直近の手番と残時間（回転直後の復元に使う）
    bool   m_lastP1Turn = true;
    qint64 m_lastP1Ms   = 0;
    qint64 m_lastP2Ms   = 0;

    // しきい値（必要なら QSettings で差し替え可）
    int m_warnMs  = 10 * 1000; // 10秒
    int m_dangerMs=  5 * 1000; // 5秒

    // 描画ヘルパ
    void updateUrgencyStyles_(bool p1turn);
    void applyTurnHighlights_(bool p1turn);   // 中で updateUrgencyStyles_ を呼ぶようにします
    void setupNameAndClockFonts_();           // フォント明示設定

private slots:
    void onBranchPlanActivated_(int row, int ply1);                // Plan選択 → 行/手へジャンプ
    void onRecordPaneBranchActivated_(const QModelIndex& index);   // QModelIndex → row へアダプト
    void onBranchNodeActivated_(int row, int ply);
    void onGameOverStateChanged(const MatchCoordinator::GameOverState& st);
    void onTurnManagerChanged(ShogiGameController::Player now);
    void setupBranchCandidatesWiring_();

private:
    // いま下段が先手(P1)か？ true=先手が手前、false=後手が手前
    bool m_bottomIsP1 = true;

    void syncBoardToPly_(int selPly);
    void updateHighlightsForPly_(int selPly); // まだ無ければ宣言を追加

    // 待った中は棋譜追記を抑止するフラグ
    bool m_isUndoInProgress = false;

    void handleBreakOffGame();

    // 検討（ConsidarationMode）の手動終了（quit 送信）
    void handleBreakOffConsidaration();
    bool m_isLiveAppendMode = false;
    void exitLiveAppendMode_();   // 終局で選択を元に戻す
    void trimEvalChartForResume_(int selPly);

    void prepareFallbackEvenStartForResume_();

    bool m_isResumeFromCurrent = false;
    int  m_pendingEvalTrimPly  = -1;
    void applyPendingEvalTrim_();

    void ensureTurnSyncBridge_();

    void showEditExitButtonOnBoard_();
    void hideEditExitButtonOnBoard_();

    KifuLoadCoordinator *m_kifuLoadCoordinator = nullptr;

    void ensureHumanAtBottomIfApplicable_();

private:
    PositionEditController* m_posEdit = nullptr;

    void ensurePositionEditController_();
};

#endif // MAINWINDOW_H
