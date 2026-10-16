/// @file mainwindowcomposer_create.cpp
/// @brief 部品の生成。依存する部品が先にできている順に、起動時に1回だけ作る

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "analysisresultspanel.h"
#include "analysissession.h"
#include "appsettings.h"
#include "boardinteractioncontroller.h"
#include "boardmovecontroller.h"
#include "boardsyncpresenter.h"
#include "branchtreeeditcontroller.h"
#include "branchtreemanager.h"
#include "candidatearrowpresenter.h"
#include "commentcoordinator.h"
#include "commenteditorpanel.h"
#include "consecutivegameseries.h"
#include "considerationtabmanager.h"
#include "considerationuicontroller.h"
#include "csagamecoordinator.h"
#include "csagamesession.h"
#include "csalogpanel.h"
#include "dialoglauncher.h"
#include "dockcreationservice.h"
#include "docklayoutmanager.h"
#include "docksettings.h"
#include "enginedisplaypresenter.h"
#include "enginelifecyclemanager.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphpresenter.h"
#include "gameinfopane.h"
#include "gamesession.h"
#include "humanturnpolicy.h"
#include "imageexportcontroller.h"
#include "jishogiscoredialogcontroller.h"
#include "josekiwindowcontroller.h"
#include "kifuanalysiscontroller.h"
#include "kifuanalysislistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifubranchtree.h"
#include "kifudisplaypresenter.h"
#include "kifudocument.h"
#include "kifufilecontroller.h"
#include "kifuloadcoordinator.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "languagecontroller.h"
#include "livegamesession.h"
#include "mainwindowactions.h"
#include "mainwindowappearancecontroller.h"
#include "matchcoordinator.h"
#include "menuwindowcontroller.h"
#include "nyugyokudeclarationhandler.h"
#include "piecesoundplayer.h"
#include "playerinfopresenter.h"
#include "positioneditcontroller.h"
#include "positioneditcoordinator.h"
#include "pvclickcontroller.h"
#include "recordpane.h"
#include "sessionresetcoordinator.h"
#include "settingsresetcontroller.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "thinkingpanel.h"
#include "timedisplaypresenter.h"
#include "tsumesearchcontroller.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#include "usicommandcontroller.h"
#include "usicommlogmodel.h"
#include "usilogpanel.h"
#ifdef QT_DEBUG
#include "debugscreenshotshortcut.h"
#endif

#include <QAction>
#include <functional>

// ---------------------------------------------------------------------------
// 棋譜の正本とモデル・画面の骨格
// ---------------------------------------------------------------------------

void MainWindowComposer::createRecordAndModels()
{
    // 棋譜の正本。起動時から平手の開始局面を持ち、空のツリーにはしない
    m_c.branchTree = new KifuBranchTree(&m_window);
    m_c.branchTree->setRootSfen(SfenUtils::hirateSfen());
    m_c.navState = new KifuNavigationState(m_c.branchTree, &m_window);
    m_c.liveGameSession = new LiveGameSession(m_c.branchTree, m_c.navState, &m_window);
    m_c.kifuNavController = new KifuNavigationController(m_c.navState, &m_window);
    m_c.document = new KifuDocument(m_c.branchTree, &m_window);

    m_c.kifuRecord = new KifuRecordListModel(m_c.branchTree, &m_window);
    m_c.kifuBranch = new KifuBranchListModel(m_c.branchTree, &m_window);
    m_c.commLog1 = new UsiCommLogModel(&m_window);
    m_c.commLog2 = new UsiCommLogModel(&m_window);
    m_c.thinking1 = new ShogiEngineThinkingModel(&m_window);
    m_c.thinking2 = new ShogiEngineThinkingModel(&m_window);
    // 検討タブの思考情報モデル。検討・読み筋表示・矢印が同じものを使う
    m_c.consideration = new ShogiEngineThinkingModel(&m_window);
    // 棋譜解析の結果。解析を始めるたびに中身を消して使い回す
    m_c.analysis = new KifuAnalysisListModel(&m_window);

    m_ui.setupUi(&m_window);
    // 同じエリア内でドックを左右に分割できるようにする
    m_window.setDockNestingEnabled(true);
}

// ---------------------------------------------------------------------------
// 盤・時計・盤の操作
// ---------------------------------------------------------------------------

void MainWindowComposer::createBoard()
{
    // 盤は起動から終了まで1つ。局面は表示中の手（KifuNavigationState）に合わせて入れ替える
    m_c.gameController = new ShogiGameController(&m_window);
    m_c.gameController->newGame(m_c.branchTree->root()->sfen());
    m_c.shogiView = new ShogiView(&m_window);
    m_c.shogiView->setNameFontScale(0.30);
    m_c.shogiView->setBoard(m_c.gameController->board());

    m_c.clock = new ShogiClock(&m_window);
    m_c.timePresenter = new TimeDisplayPresenter(m_c.shogiView, m_c.clock, &m_window);

    // セントラルウィジェットに盤を置き、ツールバーを整える
    MainWindowAppearanceController::Deps ad;
    ad.window = &m_window;
    ad.centralWidget = m_ui.centralwidget;
    ad.toolBar = m_ui.toolBar;
    ad.toolBarAction = m_ui.actionToolBar;
    ad.shogiView = m_c.shogiView;
    ad.timePresenter = m_c.timePresenter;
    m_c.appearance = new MainWindowAppearanceController(ad, &m_window);

    m_c.posEdit = new PositionEditController(&m_window);
    m_c.boardController = new BoardInteractionController(m_c.shogiView, m_c.gameController, &m_window);
    m_ui.actionLegalMoves->setChecked(AppSettings::legalMovesVisible());
    m_c.boardController->setLegalMovesVisible(m_ui.actionLegalMoves->isChecked());
}

// ---------------------------------------------------------------------------
// 棋譜欄・ドックの中身・評価値グラフと、棋譜の表示・編集
// ---------------------------------------------------------------------------

void MainWindowComposer::createPanels()
{
    m_c.recordPane = new RecordPane(m_c.appearance->central());
    m_c.recordPane->setModels(m_c.kifuRecord, m_c.kifuBranch);

    // ドックの中身（ドックへの配置は createDocks）
    m_c.thinkingPanel = new ThinkingPanel(&m_window);
    m_c.thinkingPanel->setModels(m_c.thinking1, m_c.thinking2, m_c.commLog1, m_c.commLog2);
    m_c.considerationTab = new ConsiderationTabManager(&m_window);
    m_c.considerationTab->setConsiderationThinkingModel(m_c.consideration);
    m_c.usiLog = new UsiLogPanel(&m_window);
    m_c.usiLog->setModels(m_c.commLog1, m_c.commLog2);
    m_c.csaLog = new CsaLogPanel(&m_window);
    m_c.commentEditor = new CommentEditorPanel(&m_window);
    m_c.branchTreeView = new BranchTreeManager(&m_window);

    m_c.evalChart = new EvaluationChartWidget(&m_window);
    m_c.gameInfoPane = new GameInfoPane(&m_window);

    KifuDisplayPresenter::Deps dd;
    dd.navState = m_c.navState;
    dd.liveSession = m_c.liveGameSession;
    dd.recordModel = m_c.kifuRecord;
    dd.branchModel = m_c.kifuBranch;
    dd.recordView = m_c.recordPane->kifuView();
    dd.treeView = m_c.branchTreeView;
    m_c.kifuDisplay = new KifuDisplayPresenter(dd, &m_window);

    BoardSyncPresenter::Deps bs;
    bs.gc = m_c.gameController;
    bs.bic = m_c.boardController;
    bs.navState = m_c.navState;
    m_c.boardSync = new BoardSyncPresenter(bs, &m_window);

    CommentCoordinator::Deps cd;
    cd.editor = m_c.commentEditor;
    cd.navState = m_c.navState;
    cd.document = m_c.document;
    cd.parentWidget = &m_window;
    cd.statusBar = m_ui.statusbar;
    m_c.commentCoordinator = new CommentCoordinator(cd, &m_window);

    BranchTreeEditController::Deps ed;
    ed.tree = m_c.branchTree;
    ed.navState = m_c.navState;
    ed.navController = m_c.kifuNavController;
    ed.liveSession = m_c.liveGameSession;
    ed.parentWidget = &m_window;
    m_c.branchEdit = new BranchTreeEditController(ed, &m_window);

    // 局面編集中は棋譜を移らない。未更新のコメントは破棄してよいか確かめる
    m_c.kifuNavController->setLeaveGuard([this]() {
        return !m_c.shogiView->positionEditMode() && m_c.commentCoordinator->confirmLeaveEditingMove();
    });
}

// ---------------------------------------------------------------------------
// 対局・検討・通信対局の司令塔と、盤を操作できるかの判定
// ---------------------------------------------------------------------------

void MainWindowComposer::createMatchAndAnalysis()
{
    // エンジンの枠（E1・E2）は対局と検討で共有する（同時には動かない）
    m_c.engines = new EngineLifecycleManager(m_c.clock, &m_window);

    MatchCoordinator::Deps md;
    md.gc = m_c.gameController;
    md.clock = m_c.clock;
    md.activity = &m_state.activity;
    md.document = m_c.document;
    md.engines = m_c.engines;
    m_c.match = new MatchCoordinator(md, &m_window);

    AnalysisSession::Deps sd;
    sd.engines = m_c.engines;
    sd.activity = &m_state.activity;
    sd.navState = m_c.navState;
    m_c.analysisSession = new AnalysisSession(sd, &m_window);
    m_c.consecutiveGames = new ConsecutiveGameSeries(&m_window);

    CsaGameCoordinator::Deps cd;
    cd.gameController = m_c.gameController;
    cd.clock = m_c.clock;
    cd.engines = m_c.engines;
    m_c.csaGame = new CsaGameCoordinator(cd, &m_window);

    HumanTurnPolicy::Deps hd;
    hd.activity = &m_state.activity;
    hd.gameController = m_c.gameController;
    hd.match = m_c.match;
    hd.csaGame = m_c.csaGame;
    hd.branchEdit = m_c.branchEdit;
    m_c.humanTurnPolicy = std::make_unique<HumanTurnPolicy>(hd);
    m_c.boardController->setIsHumanTurnCallback(std::bind(&MainWindowComposer::isHumanTurnNow, this));

    BoardMoveController::Deps bd;
    bd.view = m_c.shogiView;
    bd.gameController = m_c.gameController;
    bd.board = m_c.boardController;
    bd.activity = &m_state.activity;
    bd.match = m_c.match;
    bd.positionEdit = m_c.posEdit;
    bd.branchEdit = m_c.branchEdit;
    m_c.boardMoves = new BoardMoveController(bd, &m_window);

    PositionEditCoordinator::Deps pd;
    pd.view = m_c.shogiView;
    pd.gameController = m_c.gameController;
    pd.boardController = m_c.boardController;
    pd.posEdit = m_c.posEdit;
    pd.navState = m_c.navState;
    pd.activity = &m_state.activity;
    pd.actions.returnAllPiecesToStand = m_ui.actionReturnAllPiecesToStand;
    pd.actions.setHiratePosition = m_ui.actionSetHiratePosition;
    pd.actions.setTsumePosition = m_ui.actionSetTsumePosition;
    pd.actions.changeTurn = m_ui.actionChangeTurn;
    m_c.posEditCoordinator = new PositionEditCoordinator(pd, &m_window);
}

// ---------------------------------------------------------------------------
// 表示（評価値グラフ・矢印・対局者情報）とドック
// ---------------------------------------------------------------------------

void MainWindowComposer::createPresentersAndDocks()
{
    m_c.evalGraph = new EvaluationGraphPresenter({m_c.evalChart, m_c.navState, m_c.liveGameSession}, &m_window);

    // エンジンの表示（思考・検討・通信ログ・2つ目のエンジン欄・評価値グラフの系列名）
    EngineDisplayPresenter::Deps ed;
    ed.engines = m_c.engines;
    ed.analysis = m_c.analysisSession;
    ed.document = m_c.document;
    ed.commLog1 = m_c.commLog1;
    ed.commLog2 = m_c.commLog2;
    ed.thinking1 = m_c.thinking1;
    ed.thinking2 = m_c.thinking2;
    ed.consideration = m_c.consideration;
    ed.analysisModel = m_c.analysis;
    ed.thinkingPanel = m_c.thinkingPanel;
    ed.considerationTab = m_c.considerationTab;
    ed.evalGraph = m_c.evalGraph;
    ed.usiLog = m_c.usiLog;
    ed.csaLog = m_c.csaLog;
    m_c.engineDisplay = new EngineDisplayPresenter(ed, &m_window);

    CandidateArrowPresenter::Deps ad;
    ad.view = m_c.shogiView;
    ad.match = m_c.match;
    ad.engines = m_c.engines;
    ad.activity = &m_state.activity;
    ad.analysis = m_c.analysisSession;
    ad.consideration = m_c.consideration;
    ad.showConsiderationArrows = std::bind(&MainWindowComposer::showConsiderationArrows, this);
    m_c.arrows = new CandidateArrowPresenter(ad, &m_window);

    PlayerInfoPresenter::Deps pd;
    pd.document = m_c.document;
    pd.activity = &m_state.activity;
    pd.shogiView = m_c.shogiView;
    m_c.playerInfo = new PlayerInfoPresenter(pd, &m_window);

    m_c.menuWindowController = new MenuWindowController({&m_window, m_window.menuBar()}, &m_window);

    // 定跡ウィンドウ。定跡手を指せるかは盤のクリックと同じ判定を使う
    JosekiWindowController::Deps jd;
    jd.parentWidget = &m_window;
    jd.gameController = m_c.gameController;
    jd.navState = m_c.navState;
    jd.isHumanTurnNow = std::bind(&MainWindowComposer::isHumanTurnNow, this);
    jd.boardMoves = m_c.boardMoves;
    m_c.joseki = new JosekiWindowController(jd, &m_window);

    m_c.analysisResults = new AnalysisResultsPanel(&m_window);

    DockCreationService::Deps dd;
    dd.mainWindow = &m_window;
    dd.displayMenu = m_ui.Display;
    dd.evalChart = m_c.evalChart;
    dd.recordPane = m_c.recordPane;
    dd.thinking = m_c.thinkingPanel;
    dd.consideration = m_c.considerationTab;
    dd.usiLog = m_c.usiLog;
    dd.csaLog = m_c.csaLog;
    dd.comment = m_c.commentEditor;
    dd.branchTree = m_c.branchTreeView;
    dd.gameInfo = m_c.gameInfoPane;
    dd.analysisResults = m_c.analysisResults;
    dd.menuWindow = m_c.menuWindowController;
    dd.joseki = m_c.joseki;
    m_c.dockCreationService = new DockCreationService(dd, &m_window);
    createDocks();
}

void MainWindowComposer::createDocks()
{
    DockCreationService* docks = m_c.dockCreationService;
    // 作る順で、同じ場所に重ねたドックのタブの並びが決まる
    m_c.docks.recordPane = docks->createRecordPaneDock();
    docks->createAnalysisDocks();
    m_c.docks.gameInfo = docks->gameInfoDock();
    m_c.docks.thinking = docks->thinkingDock();
    m_c.docks.consideration = docks->considerationDock();
    m_c.docks.usiLog = docks->usiLogDock();
    m_c.docks.csaLog = docks->csaLogDock();
    m_c.docks.comment = docks->commentDock();
    m_c.docks.branchTree = docks->branchTreeDock();
    m_c.docks.evalChart = docks->createEvalChartDock();
    m_c.docks.menuWindow = docks->createMenuWindowDock();
    m_c.docks.josekiWindow = docks->createJosekiWindowDock();
    // 定跡ウィンドウはドックが前面に出ている間だけ更新する
    if (m_c.docks.josekiWindow) m_c.joseki->attachDock(m_c.docks.josekiWindow);
    m_c.docks.analysisResults = docks->createAnalysisResultsDock();

    // 重ねたドックのうち、前面に出すタブ
    if (m_c.docks.thinking) m_c.docks.thinking->raise();
    if (m_c.docks.recordPane) m_c.docks.recordPane->raise();
    m_ui.actionLockDocks->setChecked(DockSettings::docksLocked());

    m_c.dockLayoutManager = new DockLayoutManager(&m_window, &m_window);
    DockLayoutManager* layout = m_c.dockLayoutManager;
    layout->registerDock(DockLayoutManager::DockType::Menu, m_c.docks.menuWindow);
    layout->registerDock(DockLayoutManager::DockType::Joseki, m_c.docks.josekiWindow);
    layout->registerDock(DockLayoutManager::DockType::Record, m_c.docks.recordPane);
    layout->registerDock(DockLayoutManager::DockType::GameInfo, m_c.docks.gameInfo);
    layout->registerDock(DockLayoutManager::DockType::Thinking, m_c.docks.thinking);
    layout->registerDock(DockLayoutManager::DockType::Consideration, m_c.docks.consideration);
    layout->registerDock(DockLayoutManager::DockType::UsiLog, m_c.docks.usiLog);
    layout->registerDock(DockLayoutManager::DockType::CsaLog, m_c.docks.csaLog);
    layout->registerDock(DockLayoutManager::DockType::Comment, m_c.docks.comment);
    layout->registerDock(DockLayoutManager::DockType::BranchTree, m_c.docks.branchTree);
    layout->registerDock(DockLayoutManager::DockType::EvalChart, m_c.docks.evalChart);
    layout->registerDock(DockLayoutManager::DockType::AnalysisResults, m_c.docks.analysisResults);
    layout->setSavedLayoutsMenu(m_ui.menuSavedLayouts);
}

// ---------------------------------------------------------------------------
// 共通基盤・棋譜のファイル・対局のセッション
// ---------------------------------------------------------------------------

void MainWindowComposer::createSessions()
{
    // UI 状態ポリシー。待った・投了・入玉宣言を人間が参加する対局だけにするため、司令塔の対局者の組を見る
    UiStatePolicyManager::Deps ud;
    ud.ui = &m_ui;
    ud.recordPane = m_c.recordPane;
    ud.branchTree = m_c.branchTreeView;
    ud.consideration = m_c.considerationTab;
    ud.boardController = m_c.boardController;
    ud.localGameHasHuman = [this]() {
        return m_c.match->isHumanSide(Turn::Black) || m_c.match->isHumanSide(Turn::White);
    };
    ud.canResumeGame = std::bind(&MatchCoordinator::canResumeInterruptedGame, m_c.match);
    ud.activity = &m_state.activity;
    m_c.uiStatePolicy = new UiStatePolicyManager(ud, &m_window);
    m_c.notificationService = new UiNotificationService({&m_window}, &m_window);
    m_c.pieceSoundPlayer = new PieceSoundPlayer(&m_window);
    // メニュー「駒音」はチェックマークを見せるためアイコンを出さず、設定のオン・オフを映す
    m_ui.actionPieceSound->setIconVisibleInMenu(false);
    m_ui.actionPieceSound->setChecked(m_c.pieceSoundPlayer->isEnabled());

    // 棋譜の読み込み・保存
    m_c.kifuLoadCoordinator = new KifuLoadCoordinator(m_c.document, &m_window);

    SessionResetCoordinator::Deps rd;
    rd.document = m_c.document;
    rd.liveSession = m_c.liveGameSession;
    rd.kifuLoader = m_c.kifuLoadCoordinator;
    rd.evalGraph = m_c.evalGraph;
    rd.engineDisplay = m_c.engineDisplay;
    m_c.sessionReset = std::make_unique<SessionResetCoordinator>(rd);

    KifuFileController::Deps fd;
    fd.parentWidget = &m_window;
    fd.document = m_c.document;
    fd.loadCoordinator = m_c.kifuLoadCoordinator;
    fd.gameController = m_c.gameController;
    fd.navState = m_c.navState;
    fd.clearUiBeforeKifuLoad = std::bind(&SessionResetCoordinator::clearBeforeKifuLoad, m_c.sessionReset.get());
    m_c.kifuFileController = new KifuFileController(fd, &m_window);

    // 対局の利用者の操作と、対局の手順の書き先
    m_c.nyugyokuHandler = new NyugyokuDeclarationHandler(&m_window, m_c.document, &m_window);
    GameSession::Deps gd;
    gd.state = &m_state;
    gd.match = m_c.match;
    gd.analysis = m_c.analysisSession;
    gd.consecutive = m_c.consecutiveGames;
    gd.reset = m_c.sessionReset.get();
    gd.navState = m_c.navState;
    gd.liveSession = m_c.liveGameSession;
    gd.document = m_c.document;
    gd.clock = m_c.clock;
    gd.timePresenter = m_c.timePresenter;
    gd.playerInfo = m_c.playerInfo;
    gd.kifuFileController = m_c.kifuFileController;
    gd.shogiView = m_c.shogiView;
    gd.appearance = m_c.appearance;
    gd.csaGame = m_c.csaGame;
    gd.nyugyoku = m_c.nyugyokuHandler;
    m_c.gameSession = new GameSession(gd, &m_window);
    // 司令塔は手順の書き先（GameSession）より先に作るので、ここで渡す
    m_c.match->setRecordSink(m_c.gameSession);

    CsaGameSession::Deps cd;
    cd.coordinator = m_c.csaGame;
    cd.activity = &m_state.activity;
    cd.shogiView = m_c.shogiView;
    cd.boardController = m_c.boardController;
    cd.boardMoves = m_c.boardMoves;
    cd.csaLog = m_c.csaLog;
    cd.evalGraph = m_c.evalGraph;
    cd.notifications = m_c.notificationService;
    cd.appearance = m_c.appearance;
    cd.statusBar = m_ui.statusbar;
    cd.parentWidget = &m_window;
    m_c.csaSession = new CsaGameSession(cd, &m_window);
}

// ---------------------------------------------------------------------------
// 利用者の操作（メニュー・検討・読み筋・ダイアログ）
// ---------------------------------------------------------------------------

void MainWindowComposer::createControllers()
{
    LanguageController::Deps ld;
    ld.parentWidget = &m_window;
    ld.systemLanguage = m_ui.actionLanguageSystem;
    ld.japanese = m_ui.actionLanguageJapanese;
    ld.english = m_ui.actionLanguageEnglish;
    ld.chineseSimplified = m_ui.actionLanguageChineseSimplified;
    ld.chineseTraditional = m_ui.actionLanguageChineseTraditional;
    ld.notationAuto = m_ui.actionNotationAuto;
    ld.notationJapanese = m_ui.actionNotationJapanese;
    ld.notationWestern = m_ui.actionNotationWestern;
    ld.notationOrigin = m_ui.actionNotationOrigin;
    ld.notationHelp = m_ui.actionNotationHelp;
    m_c.languageController = new LanguageController(ld, &m_window);

    PvClickController::Deps pv;
    pv.navState = m_c.navState;
    pv.document = m_c.document;
    pv.shogiView = m_c.shogiView;
    m_c.pvClickController = new PvClickController(pv, &m_window);
    m_c.usiCommandController = new UsiCommandController({m_c.engines, m_c.usiLog}, &m_window);

    ConsiderationUiController::Deps cw;
    cw.parentWidget = &m_window;
    cw.considerationTabManager = m_c.considerationTab;
    cw.session = m_c.analysisSession;
    cw.navState = m_c.navState;
    m_c.considerationUi = new ConsiderationUiController(cw, &m_window);

    // 棋譜解析・詰み探索（メニューから始める）
    KifuAnalysisController::Deps kd;
    kd.parentWidget = &m_window;
    kd.activity = &m_state.activity;
    kd.analysisModel = m_c.analysis;
    kd.resultsPanel = m_c.analysisResults;
    kd.engines = m_c.engines;
    kd.navState = m_c.navState;
    kd.recordModel = m_c.kifuRecord;
    kd.document = m_c.document;
    kd.shogiView = m_c.shogiView;
    kd.navController = m_c.kifuNavController;
    kd.evalChart = m_c.evalChart;
    kd.evalGraph = m_c.evalGraph;
    m_c.kifuAnalysis = new KifuAnalysisController(kd, &m_window);
    m_c.tsumeSearch = new TsumeSearchController({&m_window, m_c.navState, m_c.analysisSession}, &m_window);

    // 複数の部品にまたがる利用者の操作（局面編集の開始・新規・終了）
    MainWindowActions::Deps ad;
    ad.window = &m_window;
    ad.gameController = m_c.gameController;
    ad.gameSession = m_c.gameSession;
    ad.positionEdit = m_c.posEditCoordinator;
    ad.kifuFileController = m_c.kifuFileController;
    ad.reset = m_c.sessionReset.get();
    m_c.actions = new MainWindowActions(ad, &m_window);

    // メニューから開くダイアログ
    m_c.jishogiController = new JishogiScoreDialogController(&m_window);
    DialogLauncher::Deps dl;
    dl.parentWidget = &m_window;
    dl.kifuFileController = m_c.kifuFileController;
    dl.shogiView = m_c.shogiView;
    dl.jishogiController = m_c.jishogiController;
    dl.csaSession = m_c.csaSession;
    dl.pieceSoundPlayer = m_c.pieceSoundPlayer;
    dl.menuWindowDock = m_c.docks.menuWindow;
    m_c.dialogLauncher = new DialogLauncher(dl, &m_window);
    // 成り・不成を選べる着手では、成りダイアログで対局者に選ばせる
    m_c.gameController->setPromotionChooser(std::bind(&DialogLauncher::choosePromotion, m_c.dialogLauncher));

    m_c.images = new ImageExportController({&m_window, m_c.shogiView, m_c.evalChart}, &m_window);
    m_c.settingsReset = new SettingsResetController(&m_window);
#ifdef QT_DEBUG
    // デバッグ用スクリーンショット（F12 で /tmp/shogiboardq-debug/ に PNG 保存）
    m_c.debugScreenshot = new DebugScreenshotShortcut(&m_window, &m_window);
#endif
}
