#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTime>
#include <QTextBrowser>
#include <QDockWidget>
#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QSet>

#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "shogienginethinkingmodel.h"
#include "kifuanalysislistmodel.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "usi.h"
#include "startgamedialog.h"
#include "kifuanalysisdialog.h"
#include "playmode.h"
#include "shogimove.h"
#include "usicommlogmodel.h"
#include "considerationdialog.h"
#include "tsumeshogisearchdialog.h"
#include "shogiclock.h"
#include "kiftosfenconverter.h"
#include "navigationcontext.h"
#include "evaluationchartwidget.h"
#include "recordpane.h"
#include "engineanalysistab.h"

#define SHOGIBOARDQ_DEBUG_KIF 1   // 0にすればログは一切出ません

using VariationBucket = QVector<KifLine>;     // 手目からの候補群

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ShogiGameController;
class QPainter;
class QStyleOptionViewItem;
class QModelIndex;
class NavigationController;
class QGraphicsView;
class QGraphicsPathItem;
class QTableView;
class QTableWidget;
class QEvent;
class QGraphicsPathItem;
class EngineAnalysisTab;

class MainWindow : public QMainWindow, public INavigationContext
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);

protected:
    Ui::MainWindow *ui;

private slots:
    void displayErrorMessage(const QString& message);
    void chooseAndLoadKifuFile();

    void handlePlayerVsEngineClick(const QPoint& field);
    void handleHumanVsHumanClick(const QPoint& field);
    void handleEditModeClick(const QPoint& field);
    void togglePiecePromotionOnClick(const QPoint& field);
    void displayPromotionDialog();

    void displayEngineSettingsDialog();
    void displayVersionInformation();
    void openWebsiteInExternalBrowser();

    void initializeGame();
    void displayConsiderationDialog();
    void displayKifuAnalysisDialog();

    void flipBoardAndUpdatePlayerInfo();
    void copyBoardToClipboard();

    // EngineAnalysisTab の表示切り替え
    void toggleEngineAnalysisVisibility();

    void undoLastTwoMoves();
    void saveShogiBoardImage();
    void saveSettingsAndClose();
    void resetToInitialState();

    void displayGameOutcome(ShogiGameController::Result);
    void handleResignation();
    void handleEngineTwoResignation();
    void handleEngineOneResignation();
    void sendCommandsToEngineOne();
    void sendCommandsToEngineTwo();

    void setResignationMove(bool isPlayerOneResigning);
    void stopClockAndSendCommands();
    void displayResultsAndUpdateGui();

    void disableArrowButtons();
    void enableArrowButtons();

    void beginPositionEditing();
    void finishPositionEditing();
    void resetPiecesToStand();

    void switchTurns();
    void setStandardStartPosition();
    void setTsumeShogiStartPosition();
    void swapBoardSides();
    void enlargeBoard();
    void reduceBoardSize();

    void saveKifuToFile();
    void overwriteKifuFile();
    void processResignCommand();

    void setPlayer1TimeTextToRed();
    void setPlayer2TimeTextToRed();

    void onShogiViewClicked(const QPoint &pt);
    void onShogiViewRightClicked(const QPoint &);
    void endDrag();

    void onPlayer1TimeOut();
    void onPlayer2TimeOut();

    // 分岐候補欄（RecordPane 側）クリック
    void onBranchRowClicked(const QModelIndex& index);

private:
    // --- 基本状態 ---
    QString m_startSfenStr;
    QString m_currentSfenStr;
    bool    m_errorOccurred = false;

    QString m_bTime;  // 先手/下手の残り
    QString m_wTime;  // 後手/上手の残り
    bool    m_isLoseOnTimeout = false;

    int     m_currentMoveIndex = 0;
    QString m_lastMove;

    Usi* m_usi1 = nullptr;
    Usi* m_usi2 = nullptr;

    QString m_positionStr1;
    QString m_positionStr2;
    QStringList m_positionStrList;
    QString m_positionPonder1;
    QString m_positionPonder2;

    PlayMode m_playMode = NotStarted;

    QString m_engineFile1, m_engineFile2;

    int  m_byoyomiMilliSec1 = 0;
    int  m_byoyomiMilliSec2 = 0;
    int  m_addEachMoveMiliSec1 = 0;
    int  m_addEachMoveMiliSec2 = 0;
    bool m_useByoyomi = false;

    int  m_totalMove = 0;

    ShogiView*          m_shogiView = nullptr;
    ShogiGameController* m_gameController = nullptr;

    QPoint m_clickPoint;
    ShogiView::FieldHighlight* m_selectedField = nullptr;
    ShogiView::FieldHighlight* m_selectedField2 = nullptr;
    ShogiView::FieldHighlight* m_movedField    = nullptr;

    QString m_startPosStr;

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

    UsiCommLogModel* m_lineEditModel1 = nullptr; // Engine 1 info
    UsiCommLogModel* m_lineEditModel2 = nullptr; // Engine 2 info

    // --- 画面構成 ---
    QTabWidget* m_tab = nullptr;              // EngineAnalysisTab 内部の QTabWidget を流用
    QWidget*    m_gameRecordLayoutWidget = nullptr; // 右側（RecordPane）
    QWidget*    m_playerAndBoardLayoutWidget = nullptr;
    QSplitter*  m_hsplit = nullptr;

    // 評価グラフ（RecordPane から受け取る。必要なら保持）
    EvaluationChartWidget* m_evalChart = nullptr;

    QList<int>       m_scoreCp;
    QString          m_humanName1, m_humanName2;
    QString          m_engineName1, m_engineName2;

    QStringList*             m_sfenRecord   = nullptr;
    QList<KifuDisplay *>*    m_moveRecords  = nullptr;

    int         m_gameCount = 0;
    QStringList m_kifuDataList;
    QString     defaultSaveFileName;
    QString     kifuSaveFileName;

    QVector<ShogiMove> m_gameMoves;

    ShogiClock* m_shogiClock = nullptr;

    bool   m_waitingSecondClick = false;
    QPoint m_firstClick;

    // --- ロジック ---
    void startNewShogiGame(QString& startSfenStr);
    void updateGameRecord(const QString& elapsedTime);

    void printGameDialogSettings(StartGameDialog* m_gamedlg);
    void handleUndoMove(int index);

    void createPlayerNameAndMoveDisplay();
    void setPlayersNamesForMode();
    void setupTimeAndTurnIndicators();
    void renderShogiBoard();
    void displayAnalysisResults();

    void initializeNewGame(QString& startSfenStr);
    QString parseStartPositionToSfen(QString startPositionStr);

    void startHumanVsHumanGame();
    void startHumanVsEngineGame();
    void startEngineVsHumanGame();
    void startEngineVsEngineGame();

    void startConsidaration();
    void analyzeGameRecord();
    void startGameBasedOnMode();

    void redrawEngine1EvaluationGraph();
    void redrawEngine2EvaluationGraph();

    void clearMoveHighlights();
    void addMoveHighlights();

    void updateRemainingTimeDisplay();
    void hideGameActions();
    void setGameInProgressActions();

    void initializeComponents();
    void setupHorizontalGameLayout();
    void initializeCentralGameDisplay();

    void setEngineNamesBasedOnMode();

    void saveWindowAndBoardSettings();
    void loadWindowSettings();
    void closeEvent(QCloseEvent* event) override;

    void displayPositionEditMenu();
    void hidePositionEditMenu();

    void makeKifuFileHeader();
    void getPlayersName(QString& playersName1, QString& playersName2);
    void makeDefaultSaveFileName();

    void displayGameRecord(const QList<KifDisplayItem> disp);

    void setRemainingTimeAndCountDown();
    void getOptionFromStartGameDialog();
    void prepareDataCurrentPosition();
    void prepareInitialPosition();
    void setTimerAndStart();
    void setCurrentTurn();

    PlayMode determinePlayMode(int initPositionNumber, bool isPlayer1Human, bool isPlayer2Human) const;
    PlayMode setPlayMode();

    void selectPieceAndHighlight(const QPoint& field);
    void handleClickForPlayerVsEngine(const QPoint& field);
    void handleClickForHumanVsHuman(const QPoint& field);
    void handleClickForEditMode(const QPoint &field);

    void resetSelectionAndHighlight();
    void updateHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& field, const QColor& color);
    void clearAndSetNewHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& to, const QColor& color);
    void deleteHighlight(ShogiView::FieldHighlight*& highlightField);
    void addNewHighlight(ShogiView::FieldHighlight*& highlightField, const QPoint& position, const QColor& color);

    void updateGameRecordAndGUI();
    void movePieceImmediately();

    void loadKifuFromFile(const QString &filePath);
    void createPositionCommands();
    QStringList loadTextLinesFromFile(const QString& filePath);

    void initializeAndStartPlayer2WithEngine1();
    void initializeAndStartPlayer1WithEngine1();
    void initializeAndStartPlayer2WithEngine2();
    void initializeAndStartPlayer1WithEngine2();

    void updateTurnAndTimekeepingDisplay();
    void updateRemainingTimeDisplayHumanVsHuman();
    void updateTurnDisplay();
    void updateTurnStatus(int currentPlayer);

    void displayTsumeShogiSearchDialog();
    void startTsumiSearch();
    void finalizeDrag();

    QElapsedTimer m_humanTurnTimer;
    bool m_humanTimerArmed = false;

    ShogiGameController::Player humanPlayerSide() const;
    bool isHumanTurn() const;
    void armHumanTimerIfNeeded();
    void finishHumanTimerAndSetConsideration();

    QElapsedTimer m_turnTimer;
    bool          m_turnTimerArmed = false;

    void armTurnTimerIfNeeded();
    void finishTurnTimerAndSetConsiderationFor(ShogiGameController::Player mover);

    void disarmHumanTimerIfNeeded();

    QTimer* m_uiClockTimer = nullptr;
    void refreshClockLabels();
    static QString formatMs(int ms);

    bool m_p1HasMoved = false;
    bool m_p2HasMoved = false;

    void computeGoTimesForUSI(qint64& outB, qint64& outW);
    void refreshGoTimes();

    static constexpr int kFlagFallGraceMs = 200;
    int  computeMoveBudgetMsForCurrentTurn() const;
    void handleFlagFallForMover(bool moverP1);
    enum class Winner { P1, P2 };
    void stopClockAndSendGameOver(Winner w);

    enum class GameOverCause { Resignation, Timeout };
    void setGameOverMove(GameOverCause cause, bool loserIsPlayerOne);
    QChar glyphForPlayer(bool isPlayerOne) const;
    void onEngine1Resigns();
    void onEngine2Resigns();

    bool m_gameIsOver = false;
    bool m_gameoverMoveAppended = false;
    bool m_hasLastGameOver = false;
    GameOverCause m_lastGameOverCause = GameOverCause::Resignation;
    bool m_lastLoserIsP1 = false;

    void appendKifuLine(const QString& text, const QString& elapsedTime);

    qint64 m_initialTimeP1Ms = 0;
    qint64 m_initialTimeP2Ms = 0;
    qint64 m_turnEpochP1Ms   = -1;
    qint64 m_turnEpochP2Ms   = -1;

    static inline QString fmt_mmss(qint64 ms) {
        if (ms < 0) ms = 0;
        QTime t(0,0); return t.addMSecs(static_cast<int>(ms)).toString("mm:ss");
    }
    static inline QString fmt_hhmmss(qint64 ms) {
        if (ms < 0) ms = 0;
        QTime t(0,0); return t.addMSecs(static_cast<int>(ms)).toString("hh:mm:ss");
    }

    QStringList m_usiMoves;
    QStringList m_loadedSfens;

    QString prepareInitialSfen(const QString& filePath, QString& teaiLabel) const;
    QList<KifDisplayItem> parseDisplayMovesAndDetectTerminal(const QString& filePath,
                                                             bool& hasTerminal,
                                                             QString* warn = nullptr) const;
    QStringList convertKifToUsiMoves(const QString& filePath, QString* warn = nullptr) const;
    void rebuildSfenRecord(const QString& initialSfen,
                           const QStringList& usiMoves,
                           bool hasTerminal);
    void rebuildGameMoves(const QString& initialSfen,
                          const QStringList& usiMoves);
    void logImportSummary(const QString& filePath,
                          const QStringList& usiMoves,
                          const QList<KifDisplayItem>& disp,
                          const QString& teaiLabel,
                          const QString& warnParse,
                          const QString& warnConvert) const;

#ifdef SHOGIBOARDQ_DEBUG_KIF
    void dbgDumpMoves(const QList<KifDisplayItem>& disp,
                      const QString& tag,
                      int startPly = 1) const;
    void dbgCompareGraphVsKifu(const QList<KifDisplayItem>& graphDisp,
                               const QList<KifDisplayItem>& kifuDisp,
                               int selPly, const QString& where) const;
#endif

    // --- “後勝ち”で作った解決済み行 ---
    struct ResolvedRow {
        int startPly = 1;
        QList<KifDisplayItem> disp;
        QStringList sfen;
        QVector<ShogiMove> gm;
        int varIndex = -1; // 本譜=-1
    };
    QVector<ResolvedRow> m_resolvedRows;
    int m_activeResolvedRow = 0;

    void buildResolvedLinesAfterLoad();
    void applyResolvedRowAndSelect(int row, int selPly);

    // 分岐候補（テキスト）側の索引（※分岐ツリー表示は EngineAnalysisTab へ移管）
    struct BranchCandidate {
        QString text;  // 「▲２六歩(27)」
        int row;       // resolved 行
        int ply;       // 1始まり
    };
    QHash<int, QHash<QString, QList<BranchCandidate>>> m_branchIndex;
    QList<QPair<int,int>> m_branchRowMap; // .first=row, .second=ply

    void buildBranchCandidateIndex();
    void populateBranchListForPly(int ply);
    void applyVariation(int parentPly, int branchIndex);
    void applyVariationByKey(int startPly, int bucketIndex);

    // KIFヘッダ（対局情報）タブ
    QDockWidget*  m_gameInfoDock  = nullptr;
    QTableWidget* m_gameInfoTable = nullptr;
    void ensureGameInfoTable();
    void addGameInfoTabIfMissing();
    void populateGameInfo(const QList<KifGameInfoItem>& items);
    QString findGameInfoValue(const QList<KifGameInfoItem>& items,
                              const QStringList& keys) const;
    void applyPlayersFromGameInfo(const QList<KifGameInfoItem>& items);

    // 分岐候補の手数マーカー描画（棋譜テーブル用）
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
    void ensureBranchRowDelegateInstalled();
    void updateKifuBranchMarkersForActiveRow();

    // --- 新UI部品 ---
    RecordPane*          m_recordPane = nullptr;
    NavigationController* m_nav = nullptr;
    EngineAnalysisTab*    m_analysisTab = nullptr;

    void setupRecordPane();
    void setupEngineAnalysisTab();

    // 接続タイプの補助
    Qt::ConnectionType connTypeFor(QObject* obj) const;

    // 対局状態フラグや時計の同期
    void resetGameFlags();
    void syncClockTurnAndEpoch();

    // 記録用のエポック開始（オーバーロード2種）
    void startMatchEpoch(const char* tag);
    void startMatchEpoch(const QString& tag);

    // 投了通知の配線
    void wireResignToArbiter(Usi* engine, bool asP1);

    // 直前に指した側の思考時間を時計へ反映
    void setConsiderationForJustMoved(qint64 thinkMs);

    // エンジンの思考→指し手適用（true=適用した）
    bool engineThinkApplyMove(Usi* engine,
                              QString& positionStr,
                              QString& ponderStr,
                              QPoint* outFrom,
                              QPoint* outTo);

    // エンジン生成/破棄と初期化
    void destroyEngine(Usi*& e);
    void initSingleEnginePvE(bool engineIsP1);
    void initEnginesForEvE();
    void setupInitialPositionStrings();

    // クリックハンドラ（対局モードに応じて差し替え）
    void setPvEClickHandler();
    void setPvPClickHandler();

    // UIと時計の同期（タイトル等）
    void syncAndEpoch(const QString& title);

    // 人間手番かどうか
    bool isHumanTurnNow(bool engineIsP1) const;

    // エンジンに1手だけ指させる共通処理
    bool engineMoveOnce(Usi* eng,
                        QString& positionStr,
                        QString& ponderStr,
                        bool useSelectedField2,
                        int engineIndex,
                        QPoint* outTo = nullptr);

    // EvEで「片側が1手指す」
    bool playOneEngineTurn(Usi* mover, Usi* receiver,
                           QString& positionStr,
                           QString& ponderStr,
                           int engineIndex);

    // サイド割当（HvE/EvH/EvE）
    void assignSidesHumanVsEngine();
    void assignSidesEngineVsHuman();
    void assignEnginesEngineVsEngine();

    // KIF読み込み/再生中フラグ
    bool m_isKifuReplay = false;

    QVector<QString> m_commentsByRow;

    // ---------- 分岐コメント ----------
    QString makeBranchHtml(const QString& text) const;   // 宣言だけでOK（実装はcpp）
    void    updateBranchTextForRow(int row);             // 同上

    // ---------- 棋譜表示／分岐操作 ----------
    void showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly);
    void syncBoardAndHighlightsAtRow(int row);
    void applySfenAtCurrentPly();
    void refreshBoardAndHighlightsForRow(int row);
    void rebuildBranchTreeView();
    QGraphicsPathItem* branchNodeFor(int row, int ply);

    void buildResolvedRowsAfterLoad();   // 既にあれば重複可。なければ追加。

    // ---------- “旧UI”互換のエイリアス（nullable） ----------
    // 既存コードが m_kifuView / m_kifuBranchView を参照している箇所のための受け皿。
    // 実体は RecordPane 等から取得して代入しておく（下の NOTE 参照）。
    QTableView*      m_kifuView        = nullptr;
    QTableView*      m_kifuBranchView  = nullptr;

    // 解析結果テーブル
    QTableView*      m_analysisResultsView = nullptr;

    // ---------- 分岐ツリー ----------
    QGraphicsView*   m_branchTreeView  = nullptr;
    QHash<QPair<int,int>, QGraphicsPathItem*> m_branchNodeIndex;
    bool             m_branchTreeSelectGuard = false;

    // ---------- 分岐／本譜データのスナップショット ----------
    QList<KifDisplayItem> m_dispMain;
    QList<KifDisplayItem> m_dispCurrent;
    QStringList           m_sfenMain;
    QVector<ShogiMove>    m_gmMain;

    // 変化バケツ：手目 → 変化リスト
    QHash<int, QList<KifLine>> m_variationsByPly;
    QList<KifLine>             m_variationsSeq;

    // 分岐可能手集合（名前揺れ対策で両方持つ）
    QSet<int> m_branchablePlies;
    QSet<int> m_branchablePlySet;  // = m_branchablePlies のミラー

    // ---------- カレント選択 ----------
    int m_activePly          = 0;
    int m_currentSelectedPly = 0;

    QString m_initialSfen;

    // ---------- 分岐ツリーの role / kind 定数 ----------
    enum BranchNodeKind { BNK_Start=0, BNK_Main=1, BNK_Var=2 };
    static constexpr int BR_ROLE_KIND     = 0;
    static constexpr int BR_ROLE_PLY      = 1;
    static constexpr int BR_ROLE_ROW      = 2;
    static constexpr int BR_ROLE_STARTPLY = 3;
    static constexpr int BR_ROLE_BUCKET   = 4;

    void highlightBranchTreeAt(int row, int ply, bool centerOn = true);
    QGraphicsPathItem* branchNodeFor(int row, int ply) const;

    EngineAnalysisTab* m_engineAnalysisTab = nullptr;

public: // INavigationContext
    bool hasResolvedRows() const override;
    int  resolvedRowCount() const override;
    int  activeResolvedRow() const override;
    int  maxPlyAtRow(int row) const override;
    int  currentPly() const override;
    void applySelect(int row, int ply) override;

private slots:
    void onBranchCandidateActivated(const QModelIndex& idx);
    void onMainMoveRowChanged(int row);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
};

#endif // MAINWINDOW_H
