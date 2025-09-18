#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QTime>
#include <QDockWidget>
#include <QTableWidget>
#include <QStyledItemDelegate>

#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "shogienginethinkingmodel.h"
#include "kifuanalysislistmodel.h"
#include "shogigamecontroller.h"
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
#include "boardinteractioncontroller.h"
#include "matchcoordinator.h"

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
class EngineAnalysisTab;
class ShogiView;

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

    void displayPromotionDialog();

    void displayEngineSettingsDialog();
    void displayVersionInformation();
    void openWebsiteInExternalBrowser();

    void initializeGame();
    void displayConsiderationDialog();
    void displayKifuAnalysisDialog();

    void copyBoardToClipboard();

    // EngineAnalysisTab の表示切り替え
    void toggleEngineAnalysisVisibility();

    void undoLastTwoMoves();
    void saveShogiBoardImage();
    void saveSettingsAndClose();
    void resetToInitialState();

    void handleResignation();
    void handleEngineTwoResignation();
    void handleEngineOneResignation();
    void sendCommandsToEngineOne();
    void sendCommandsToEngineTwo();

    void setResignationMove(bool isPlayerOneResigning);

    void disableArrowButtons();
    void enableArrowButtons();

    void beginPositionEditing();
    void finishPositionEditing();
    void resetPiecesToStand();

    void setStandardStartPosition();
    void setTsumeShogiStartPosition();
    void swapBoardSides();
    void enlargeBoard();
    void reduceBoardSize();

    void saveKifuToFile();
    void overwriteKifuFile();
    void processResignCommand();

    void endDrag();

    void onPlayer1TimeOut();
    void onPlayer2TimeOut();

    void onMatchGameEnded(const MatchCoordinator::GameEndInfo& info); // ★引数付きに変更

private:
    // --- 基本状態 ---
    QString m_startSfenStr;
    QString m_currentSfenStr;
    bool    m_errorOccurred = false;

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

    QString m_engineFile1;

    int  m_totalMove = 0;

    ShogiView*          m_shogiView = nullptr;
    ShogiGameController* m_gameController = nullptr;

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

    // --- ロジック ---
    void startNewShogiGame(QString& startSfenStr);
    void updateGameRecord(const QString& elapsedTime);

    void printGameDialogSettings(StartGameDialog* m_gamedlg);
    void handleUndoMove(int index);

    void setPlayersNamesForMode();

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

    void hideGameActions();

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

    void updateTurnDisplay();
    void updateTurnStatus(int currentPlayer);

    void displayTsumeShogiSearchDialog();
    void startTsumiSearch();

    ShogiGameController::Player humanPlayerSide() const;
    bool isHumanTurn() const;

    QTimer* m_uiClockTimer = nullptr;

    bool m_p1HasMoved = false;
    bool m_p2HasMoved = false;

    static constexpr int kFlagFallGraceMs = 200;

    void handleFlagFallForMover(bool moverP1);
    enum class Winner { P1, P2 };
    void stopClockAndSendGameOver(Winner w);

    void setGameOverMove(MatchCoordinator::Cause cause, bool loserIsPlayerOne);
    QChar glyphForPlayer(bool isPlayerOne) const;
    void onEngine1Resigns();
    void onEngine2Resigns();

    void appendKifuLine(const QString& text, const QString& elapsedTime);

    qint64 m_initialTimeP1Ms = 0;
    qint64 m_initialTimeP2Ms = 0;

    static inline QString fmt_mmss(qint64 ms) {
        if (ms < 0) ms = 0;
        QTime t(0,0); return t.addMSecs(static_cast<int>(ms)).toString("mm:ss");
    }
    static inline QString fmt_hhmmss(qint64 ms) {
        if (ms < 0) ms = 0;
        QTime t(0,0); return t.addMSecs(static_cast<int>(ms)).toString("hh:mm:ss");
    }

    QStringList m_usiMoves;

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

    void populateBranchListForPly(int ply);

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
    void    updateBranchTextForRow(int row);             // 同上

    // ---------- 棋譜表示／分岐操作 ----------
    void showRecordAtPly(const QList<KifDisplayItem>& disp, int selectPly);
    void syncBoardAndHighlightsAtRow(int row);
    void applySfenAtCurrentPly();

    // ---------- “旧UI”互換のエイリアス（nullable） ----------
    // 既存コードが m_kifuView / m_kifuBranchView を参照している箇所のための受け皿。
    // 実体は RecordPane 等から取得して代入しておく（下の NOTE 参照）。
    QTableView*      m_kifuBranchView  = nullptr;

    // 解析結果テーブル
    QTableView*      m_analysisResultsView = nullptr;

    // ---------- 分岐／本譜データのスナップショット ----------
    QList<KifDisplayItem> m_dispMain;
    QList<KifDisplayItem> m_dispCurrent;
    QStringList           m_sfenMain;
    QVector<ShogiMove>    m_gmMain;

    // 変化バケツ：手目 → 変化リスト
    QHash<int, QList<KifLine>> m_variationsByPly;
    QList<KifLine>             m_variationsSeq;

    QSet<int> m_branchablePlySet;  // = m_branchablePlies のミラー

    // ---------- カレント選択 ----------
    int m_activePly          = 0;
    int m_currentSelectedPly = 0;

    QString m_initialSfen;

    QMetaObject::Connection m_connKifuRowChanged;

    bool m_onMainRowGuard = false; // 再入防止

    BoardInteractionController* m_boardController = nullptr;

    // 配線ヘルパ
    void setupBoardInteractionController();

    bool m_engine1IsP1 = false; // シングルエンジン時、エンジン1の担当が先手なら true

    MatchCoordinator* m_match = nullptr;

    void ensureClockReady_();

    void initMatchCoordinator();

    void setGameInProgressActions(bool inProgress); // フックの受け口（残す）

    void initializePositionStringsForMatch_();

    // EvH（エンジンが先手）の初手を起動する（position ベースを作り、go を送って 1手適用）
    void startInitialEngineMoveEvH_();

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
    void onKifuCurrentRowChanged(const QModelIndex& cur, const QModelIndex& prev);
    void onBoardFlipped(bool nowFlipped);
    void toggleEditSideToMove();
    void onActionFlipBoardTriggered(bool checked = false);
    void onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

private:
    QMetaObject::Connection m_timeConn{};
    void wireMatchSignals_();

private slots:
    void onMatchTimeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 urgencyMs);
    void onMoveRequested_(const QPoint& from, const QPoint& to);
    void onReverseTriggered();

private:
    // 細分化したヘルパ
    void connectBoardClicks_();
    void connectMoveRequested_();

    // moveRequested 後の分岐ハンドラ
    void handleMove_HvH_(ShogiGameController::Player moverBefore,
                         const QPoint& from, const QPoint& to);
    void handleMove_HvE_(const QPoint& humanFrom, const QPoint& humanTo);

    // USIに渡す btime/wtime を毎回ローカル生成
    std::pair<QString, QString> currentBWTimesForUSI_() const;

    // ゲームオーバーの統一判定
    bool isGameOver_() const;

private slots:
    void onBranchNodeActivated(int row, int ply);
    void onRequestApplyStart();
    void onRequestApplyMainAtPly(int ply);
    void onRequestApplyVariation(int startPly, int bucketIndex);
};

#endif // MAINWINDOW_H
