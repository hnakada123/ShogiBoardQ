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
#include "branchtreeview.h"
#include "boardarrowactions.h"
#include "candidatearrowpresenter.h"
#include "commentcoordinator.h"
#include "commenteditorpanel.h"
#include "consecutivegameseries.h"
#include "considerationpanel.h"
#include "considerationuicontroller.h"
#include "csagamecoordinator.h"
#include "csagamesession.h"
#include "csalogpanel.h"
#include "dialoglauncher.h"
#include "dockregistry.h"
#include "docklayoutmanager.h"
#include "enginedisplaypresenter.h"
#include "engineslots.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphpresenter.h"
#include "gameinfopane.h"
#include "gamesession.h"
#include "humanturnpolicy.h"
#include "imageexportcontroller.h"
#include "josekibooksession.h"
#include "josekiwindow.h"
#include "josekiregistrationcontroller.h"
#include "josekiwindowcontroller.h"
#include "kifuanalysiscontroller.h"
#include "kifuanalysissession.h"
#include "kifuanalysislistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifubranchtree.h"
#include "kifudisplaypresenter.h"
#include "kifudocument.h"
#include "kifufilecontroller.h"
#include "kifuloader.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "languagecontroller.h"
#include "livegamesession.h"
#include "mainwindowactions.h"
#include "mainwindowappearancecontroller.h"
#include "matchcoordinator.h"
#include "menuwindow.h"
#include "menuwindowcontroller.h"
#include "nyugyokudeclarationhandler.h"
#include "piecesoundplayer.h"
#include "playerinfopresenter.h"
#include "positioneditsession.h"
#include "pvclickcontroller.h"
#include "recordpane.h"
#include "sessionresetcoordinator.h"
#include "settingsresetcontroller.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "shogienginethinkingmodel.h"
#include "gameboard.h"
#include "shogiview.h"
#include "thinkingpanel.h"
#include "timedisplaypresenter.h"
#include "tsumesearchcontroller.h"
#include "notificationpresenter.h"
#include "uistatepolicymanager.h"
#include "usicommandcontroller.h"
#include "usicommlogmodel.h"
#include "usilogpanel.h"
#ifdef QT_DEBUG
#include "debugscreenshotshortcut.h"
#endif

#include <QAction>
#include <QDockWidget>
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
    m_c.gameBoard = new GameBoard(&m_window);
    m_c.gameBoard->newGame(m_c.branchTree->root()->sfen());
    m_c.shogiView = new ShogiView(&m_window);
    m_c.shogiView->setNameFontScale(0.30);
    m_c.shogiView->setBoard(m_c.gameBoard->board());

    m_c.clock = new ShogiClock(&m_window);
    m_c.timePresenter = new TimeDisplayPresenter(m_c.shogiView, m_c.clock, &m_window);

    // セントラルウィジェットに盤を置き、ツールバーを整える
    MainWindowAppearanceController::Deps ad;
    ad.window = &m_window;
    ad.centralWidget = m_ui.centralwidget;
    ad.toolBar = m_ui.toolBar;
    ad.shogiView = m_c.shogiView;
    ad.timePresenter = m_c.timePresenter;
    m_c.appearance = new MainWindowAppearanceController(ad, &m_window);

    m_c.boardController = new BoardInteractionController(m_c.shogiView, m_c.gameBoard->board(), &m_window);
}

// ---------------------------------------------------------------------------
// 棋譜欄・ドックの中身・評価値グラフと、棋譜の表示・編集
// ---------------------------------------------------------------------------

void MainWindowComposer::createPanels()
{
    m_c.recordPane = new RecordPane(m_c.appearance->central());
    m_c.recordPane->setModels(m_c.kifuRecord, m_c.kifuBranch);

    // 盤の矢印を出すかの切り替え（思考・検討・定跡ウィンドウのチェックと「表示」メニューが結ぶ）
    m_c.boardArrows = new BoardArrowActions(m_ui.actionShowJosekiArrows, &m_window);

    // ドックの中身（ドックへの配置は createDocks）
    m_c.thinkingPanel = new ThinkingPanel({m_c.boardArrows->match(), m_c.boardArrows->ponder()}, &m_window);
    m_c.thinkingPanel->setModels(m_c.thinking1, m_c.thinking2, m_c.commLog1, m_c.commLog2);
    m_c.considerationTab = new ConsiderationPanel(m_c.boardArrows->consideration(), &m_window);
    m_c.considerationTab->setConsiderationThinkingModel(m_c.consideration);
    m_c.usiLog = new UsiLogPanel(&m_window);
    m_c.usiLog->setModels(m_c.commLog1, m_c.commLog2);
    m_c.csaLog = new CsaLogPanel(&m_window);
    m_c.commentEditor = new CommentEditorPanel(&m_window);
    m_c.branchTreeView = new BranchTreeView(&m_window);

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
    bs.board = m_c.gameBoard->board();
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

    // 未更新のコメントは破棄してよいか確かめてから棋譜を移る（局面編集中の確認は局面編集を作るときに足す）
    m_c.kifuNavController->addLeaveGuard(std::bind(&CommentCoordinator::confirmLeaveEditingMove, m_c.commentCoordinator));
}

// ---------------------------------------------------------------------------
// 対局・検討・通信対局の司令塔と、盤を操作できるかの判定
// ---------------------------------------------------------------------------

void MainWindowComposer::createMatchAndAnalysis()
{
    // エンジンの枠（E1・E2）は対局と検討で共有する（同時には動かない）
    m_c.engines = new EngineSlots(&m_window);

    MatchCoordinator::Deps md;
    md.gameBoard = m_c.gameBoard;
    md.clock = m_c.clock;
    md.activity = &m_activity;
    md.document = m_c.document;
    md.engines = m_c.engines;
    m_c.match = new MatchCoordinator(md, &m_window);

    AnalysisSession::Deps sd;
    sd.engines = m_c.engines;
    sd.activity = &m_activity;
    sd.navState = m_c.navState;
    m_c.analysisSession = new AnalysisSession(sd, &m_window);
    m_c.kifuAnalysisSession = new KifuAnalysisSession({m_c.engines, &m_activity, nullptr}, &m_window);
    m_c.consecutiveGames = new ConsecutiveGameSeries(&m_window);

    CsaGameCoordinator::Deps cd;
    cd.gameBoard = m_c.gameBoard;
    cd.clock = m_c.clock;
    cd.engines = m_c.engines;
    cd.activity = &m_activity;
    m_c.csaGame = new CsaGameCoordinator(cd, &m_window);

    HumanTurnPolicy::Deps hd;
    hd.activity = &m_activity;
    hd.board = m_c.gameBoard->board();
    hd.match = m_c.match;
    hd.csaGame = m_c.csaGame;
    hd.branchEdit = m_c.branchEdit;
    m_c.humanTurnPolicy = std::make_unique<HumanTurnPolicy>(hd);

    PositionEditSession::Deps pd;
    pd.view = m_c.shogiView;
    pd.board = m_c.boardController;
    pd.navState = m_c.navState;
    pd.activity = &m_activity;
    m_c.positionEdit = new PositionEditSession(pd, &m_window);
    // 局面編集中は棋譜を移らない
    m_c.kifuNavController->addLeaveGuard(std::bind(&PositionEditSession::allowsLeavingMove, m_c.positionEdit));

    // 盤で指した手の行き先（動作状態ごとに、受け付けるか・どう処理するかを決める）
    BoardMoveController::Deps bd;
    bd.view = m_c.shogiView;
    bd.gameBoard = m_c.gameBoard;
    bd.board = m_c.boardController;
    bd.activity = &m_activity;
    bd.match = m_c.match;
    bd.positionEdit = m_c.positionEdit;
    bd.branchEdit = m_c.branchEdit;
    bd.turnPolicy = m_c.humanTurnPolicy.get();
    m_c.boardMoves = new BoardMoveController(bd, &m_window);
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

    // 開いている定跡（定跡ウィンドウが表示・編集し、盤の矢印と対局でも使う）。前回の履歴を戻し、
    // 自動読込なら前回のファイルは定跡ウィンドウを初めて表示したときに読み込む
    m_c.josekiBook = new JosekiBookSession(&m_window);
    m_c.josekiBook->restoreSaved();

    // 定跡ウィンドウ。定跡手を指せるかは盤のクリックと同じ判定を使う。定跡の表示・「盤に定跡手を表示」・
    // 「盤の手を登録」の QAction は盤の矢印と定跡への登録も読む
    JosekiWindowController::Deps jd;
    jd.parentWidget = &m_window;
    jd.book = m_c.josekiBook;
    jd.boardArrows = m_c.boardArrows->joseki();
    jd.navState = m_c.navState;
    jd.humanTurn = m_c.humanTurnPolicy.get();
    jd.boardMoves = m_c.boardMoves;
    jd.branchEdit = m_c.branchEdit;
    jd.navController = m_c.kifuNavController;
    m_c.joseki = new JosekiWindowController(jd, &m_window);

    CandidateArrowPresenter::Deps ad;
    ad.view = m_c.shogiView;
    ad.match = m_c.match;
    ad.engines = m_c.engines;
    ad.thinking = {m_c.thinking1, m_c.thinking2};
    ad.activity = &m_activity;
    ad.analysis = m_c.analysisSession;
    ad.consideration = m_c.consideration;
    ad.arrows = m_c.boardArrows;
    ad.josekiBook = m_c.josekiBook;
    ad.josekiDisplay = m_c.joseki->displayAction();
    ad.humanTurn = m_c.humanTurnPolicy.get();
    m_c.arrows = new CandidateArrowPresenter(ad, &m_window);

    PlayerInfoPresenter::Deps pd;
    pd.document = m_c.document;
    pd.activity = &m_activity;
    pd.shogiView = m_c.shogiView;
    m_c.playerInfo = new PlayerInfoPresenter(pd, &m_window);

    m_c.menuWindowController = new MenuWindowController({&m_window, m_window.menuBar()}, &m_window);

    // 思考の表・棋譜解析の結果と盤で指した手から定跡に登録する（定跡を変えられるときだけ）
    JosekiRegistrationController::Deps rd;
    rd.book = m_c.josekiBook;
    rd.recordBoardMoves = m_c.joseki->recordBoardMovesAction();
    rd.analysisModel = m_c.analysis;
    rd.parentWidget = &m_window;
    rd.statusBar = m_ui.statusbar;
    m_c.josekiRegistration = new JosekiRegistrationController(rd, &m_window);

    m_c.analysisResults = new AnalysisResultsPanel(&m_window);

    createDocks();
}

void MainWindowComposer::createDocks()
{
    using Id = DockRegistry::DockId;
    // ドックの名前・規定の配置・保存は DockRegistry の表で決まる。ここでは中身の部品を渡すだけ
    DockRegistry::Contents contents{};
    const auto put = [&contents](Id id, QWidget* content) { contents.at(static_cast<std::size_t>(id)) = content; };
    put(Id::Record, m_c.recordPane);
    put(Id::GameInfo, m_c.gameInfoPane);
    put(Id::Thinking, m_c.thinkingPanel);
    put(Id::Consideration, m_c.considerationTab);
    put(Id::UsiLog, m_c.usiLog);
    put(Id::CsaLog, m_c.csaLog);
    put(Id::Comment, m_c.commentEditor);
    put(Id::BranchTree, m_c.branchTreeView);
    put(Id::EvalChart, m_c.evalChart);
    put(Id::Menu, m_c.menuWindowController->menuWindow());
    put(Id::Joseki, m_c.joseki->josekiWindow());
    put(Id::AnalysisResults, m_c.analysisResults);
    m_c.dockRegistry = new DockRegistry(&m_window, m_ui.Display, &m_window);
    DockRegistry* docks = m_c.dockRegistry;
    docks->createAll(contents);
    // メニューウィンドウの分類に、ドックの表示の切り替えも入れる。ツールバーの「メニュー」はドックの表示を切り替える
    m_c.menuWindowController->collectCategoriesFromMenuBar();
    m_c.menuWindowController->attachDock(docks->dock(Id::Menu));

    // 定跡ウィンドウはドックが前面に出ている間だけ更新する（閉じるときは未保存の変更を確かめる）
    m_c.joseki->attachDock(docks->dock(Id::Joseki));

    // 保存した固定の状態にする。メニューの項目は MenuActionsWiring と setSavedLayoutsMenu() でつなぐ（connectAnalysisAndUi）
    m_c.dockLayoutManager = new DockLayoutManager(&m_window, docks, &m_window);
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
    ud.navigation = m_c.kifuNavController;
    ud.evalChart = m_c.evalChart;
    ud.branchTree = m_c.branchTreeView;
    ud.consideration = m_c.considerationTab;
    ud.localGameHasHuman = std::bind(&MatchCoordinator::hasHumanPlayer, m_c.match);
    ud.canResumeGame = std::bind(&MatchCoordinator::canResumeInterruptedGame, m_c.match);
    ud.activity = &m_activity;
    m_c.uiStatePolicy = new UiStatePolicyManager(ud, &m_window);
    m_c.notifications = new NotificationPresenter({&m_window}, &m_window);
    m_c.pieceSoundPlayer = new PieceSoundPlayer(&m_window);

    // 棋譜の読み込み・保存
    m_c.kifuLoader = new KifuLoader(m_c.document, &m_window);

    SessionResetCoordinator::Deps rd;
    rd.document = m_c.document;
    rd.liveSession = m_c.liveGameSession;
    rd.kifuLoader = m_c.kifuLoader;
    rd.evalGraph = m_c.evalGraph;
    rd.engineDisplay = m_c.engineDisplay;
    m_c.sessionReset = std::make_unique<SessionResetCoordinator>(rd);

    KifuFileController::Deps fd;
    fd.parentWidget = &m_window;
    fd.document = m_c.document;
    fd.loader = m_c.kifuLoader;
    fd.board = m_c.gameBoard->board();
    fd.navState = m_c.navState;
    fd.activity = &m_activity;
    m_c.kifuFileController = new KifuFileController(fd, &m_window);

    // 対局の利用者の操作と、対局の手順の書き先
    m_c.nyugyokuHandler = new NyugyokuDeclarationHandler(&m_window, m_c.document, &m_window);
    GameSession::Deps gd;
    gd.activity = &m_activity;
    gd.match = m_c.match;
    gd.consecutive = m_c.consecutiveGames;
    gd.reset = m_c.sessionReset.get();
    gd.navState = m_c.navState;
    gd.liveSession = m_c.liveGameSession;
    gd.document = m_c.document;
    gd.timePresenter = m_c.timePresenter;
    gd.kifuFileController = m_c.kifuFileController;
    gd.shogiView = m_c.shogiView;
    gd.appearance = m_c.appearance;
    gd.csaGame = m_c.csaGame;
    gd.nyugyoku = m_c.nyugyokuHandler;
    gd.josekiBook = m_c.josekiBook;
    m_c.gameSession = new GameSession(gd, &m_window);
    // 司令塔（ローカル対局・通信対局）は棋譜の受け手（GameSession）より先に作るので、ここで渡す
    m_c.match->setRecordSink(m_c.gameSession);
    m_c.csaGame->setRecordSink(m_c.gameSession);

    CsaGameSession::Deps cd;
    cd.coordinator = m_c.csaGame;
    cd.shogiView = m_c.shogiView;
    cd.appearance = m_c.appearance;
    cd.statusBar = m_ui.statusbar;
    cd.parentWidget = &m_window;
    cd.kifuFiles = m_c.kifuFileController;
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
    m_c.languageController = new LanguageController(ld, &m_window);

    PvClickController::Deps pv;
    pv.navState = m_c.navState;
    pv.document = m_c.document;
    pv.shogiView = m_c.shogiView;
    m_c.pvClickController = new PvClickController(pv, &m_window);
    m_c.usiCommandController = new UsiCommandController({m_c.engines, m_c.usiLog}, &m_window);

    ConsiderationUiController::Deps cw;
    cw.parentWidget = &m_window;
    cw.considerationPanel = m_c.considerationTab;
    cw.session = m_c.analysisSession;
    cw.navState = m_c.navState;
    m_c.considerationUi = new ConsiderationUiController(cw, &m_window);

    // 棋譜解析・詰み探索（メニューから始める）
    KifuAnalysisController::Deps kd;
    kd.parentWidget = &m_window;
    kd.session = m_c.kifuAnalysisSession;
    kd.analysisModel = m_c.analysis;
    kd.resultsPanel = m_c.analysisResults;
    kd.resultsDock = m_c.dockRegistry->dock(DockRegistry::DockId::AnalysisResults);
    kd.navState = m_c.navState;
    kd.pvBoards = m_c.pvClickController;
    kd.navController = m_c.kifuNavController;
    kd.evalGraph = m_c.evalGraph;
    m_c.kifuAnalysis = new KifuAnalysisController(kd, &m_window);
    m_c.tsumeSearch = new TsumeSearchController({&m_window, m_c.navState, m_c.analysisSession}, &m_window);

    // 複数の部品にまたがる利用者の操作（局面編集の開始・新規・終了）
    MainWindowActions::Deps ad;
    ad.window = &m_window;
    ad.gameSession = m_c.gameSession;
    ad.positionEdit = m_c.positionEdit;
    ad.kifuFileController = m_c.kifuFileController;
    ad.reset = m_c.sessionReset.get();
    ad.analysis = m_c.analysisSession;
    ad.engines = m_c.engines;
    m_c.actions = new MainWindowActions(ad, &m_window);

    // メニューから開くダイアログ
    DialogLauncher::Deps dl;
    dl.parentWidget = &m_window;
    dl.shogiView = m_c.shogiView;
    dl.pieceSoundPlayer = m_c.pieceSoundPlayer;
    dl.evalChart = m_c.evalChart;
    m_c.dialogLauncher = new DialogLauncher(dl, &m_window);
    // 成り・不成を選べる着手では、成りダイアログで対局者に選ばせる
    m_c.boardController->setPromotionChooser(std::bind(&DialogLauncher::choosePromotion, m_c.dialogLauncher));

    m_c.images = new ImageExportController({&m_window, m_c.shogiView, m_c.evalChart}, &m_window);
    m_c.settingsReset = new SettingsResetController(&m_window);
#ifdef QT_DEBUG
    // デバッグ用スクリーンショット（F12 でキャッシュのフォルダの screenshots/ に PNG 保存）
    m_c.debugScreenshot = new DebugScreenshotShortcut(&m_window, &m_window);
#endif
}
