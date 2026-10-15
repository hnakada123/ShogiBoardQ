/// @file mainwindowcomposer_create.cpp
/// @brief 部品の生成。モデル → 画面の骨格 → 盤 → 画面部品とドック → サービスの順

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "analysisresultspresenter.h"
#include "analysistabwiring.h"
#include "appsettings.h"
#include "boardinteractioncontroller.h"
#include "boardloadservice.h"
#include "boardsetupcontroller.h"
#include "boardsyncpresenter.h"
#include "branchnavigationwiring.h"
#include "commentcoordinator.h"
#include "consecutivegamescontroller.h"
#include "considerationpositionservice.h"
#include "considerationwiring.h"
#include "dialogcoordinatorwiring.h"
#include "dockcreationservice.h"
#include "docklayoutmanager.h"
#include "docksettings.h"
#include "dockuicoordinator.h"
#include "engineanalysistab.h"
#include "considerationtabmanager.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "gamerecordloadservice.h"
#include "gamerecordmodel.h"
#include "gamerecordpresenter.h"
#include "gamesessionorchestrator.h"
#include "gamestatecontroller.h"
#include "jishogiscoredialogcontroller.h"
#include "josekiwindowwiring.h"
#include "kifuanalysislistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifubranchtree.h"
#include "kifudisplaycoordinator.h"
#include "kifuexportcontroller.h"
#include "kifufilecontroller.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationcoordinator.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "languagecontroller.h"
#include "livegamesession.h"
#include "mainwindowappearancecontroller.h"
#include "matchcoordinatorwiring.h"
#include "menuwindowwiring.h"
#include "nyugyokudeclarationhandler.h"
#include "piecesoundplayer.h"
#include "playerinfowiring.h"
#include "playmodepolicyservice.h"
#include "positioneditcontroller.h"
#include "positioneditcoordinator.h"
#include "prestartcleanuphandler.h"
#include "pvclickcontroller.h"
#include "recordnavigationwiring.h"
#include "recordpane.h"
#include "recordpanewiring.h"
#include "replaycontroller.h"
#include "sessionlifecyclecoordinator.h"
#include "sfenutils.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "timedisplaypresenter.h"
#include "turnstatesyncservice.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#include "undoflowservice.h"
#include "usicommandcontroller.h"
#include "usicommlogmodel.h"
#include "logcategories.h"

#include <QAction>
#include <QPushButton>

// ---------------------------------------------------------------------------
// モデル
// ---------------------------------------------------------------------------

void MainWindowComposer::createModels()
{
    // Lifetime: MainWindow の子（親が破棄する）。起動時に1回だけ作る
    m_c.commLog1 = new UsiCommLogModel(&m_window);
    m_c.commLog2 = new UsiCommLogModel(&m_window);
    // 検討タブの思考情報モデル。検討・読み筋表示・矢印が同じものを使う
    m_c.consideration = new ShogiEngineThinkingModel(&m_window);
    // 棋譜解析の結果。解析を始めるたびに中身を消して使い回す
    m_c.analysis = new KifuAnalysisListModel(&m_window);
    m_c.kifuRecord = new KifuRecordListModel(&m_window);
    m_c.kifuBranch = new KifuBranchListModel(&m_window);

    m_ui.setupUi(&m_window);
}

// ---------------------------------------------------------------------------
// 画面の骨格（central / toolbar）
// ---------------------------------------------------------------------------

void MainWindowComposer::setupUiSkeleton()
{
    // 同じエリア内でドックを左右に分割できるようにする
    m_window.setDockNestingEnabled(true);

    m_c.appearanceController = std::make_unique<MainWindowAppearanceController>();
    m_c.appearanceController->setupCentralWidgetContainer(m_ui.centralwidget);
    m_c.central = m_c.appearanceController->central();
    m_c.appearanceController->configureToolBarFromUi(m_ui.toolBar, m_ui.actionToolBar);
}

// ---------------------------------------------------------------------------
// 盤・ゲームコントローラ・時計
// ---------------------------------------------------------------------------

void MainWindowComposer::createBoardAndGameController()
{
    m_c.gameController = new ShogiGameController(&m_window);
    m_c.shogiView = new ShogiView(&m_window);
    m_c.shogiView->setNameFontScale(0.30);

    // 盤モデルの初期化。"startpos ..." なら完全 SFEN に正規化する
    QString start = m_state.startSfenStr;
    if (start.isEmpty()) {
        start = SfenUtils::hirateSfen();
    } else if (start.startsWith(QLatin1String("startpos"))) {
        start = SfenUtils::normalizeStart(start);
    }
    m_c.gameController->newGame(start);
    if (m_c.shogiView->board() != m_c.gameController->board()) {
        m_c.shogiView->setBoard(m_c.gameController->board());
    }
    m_state.gameMoves.clear();

    // 時間表示と時計
    m_c.timePresenter = new TimeDisplayPresenter(m_c.shogiView, &m_window);
    m_c.timeController = new TimeControlController(&m_window);
    m_c.timeController->setTimeDisplayPresenter(m_c.timePresenter);
    m_c.timeController->ensureClock();
    m_c.timePresenter->setClock(m_c.timeController->clock());

    // 局面編集と盤操作
    m_c.posEdit = new PositionEditController(&m_window);
    m_c.boardSetupController = new BoardSetupController(&m_window);
    m_c.boardSetupController->setShogiView(m_c.shogiView);
    m_c.boardSetupController->setGameController(m_c.gameController);
    m_c.boardSetupController->setPositionEditController(m_c.posEdit);
    m_c.boardSetupController->setSfenRecord(&m_state.sfenRecord);
    m_c.boardSetupController->setGameMoves(&m_state.gameMoves);
    m_c.boardSetupController->setupBoardInteractionController();
    m_c.boardController = m_c.boardSetupController->boardController();

    if (m_c.boardController) {
        QAction* action = m_ui.actionLegalMoves;
        action->setChecked(AppSettings::legalMovesVisible());
        m_c.boardController->setLegalMovesVisible(action->isChecked());
        connect(action, &QAction::toggled,
                m_c.boardController, &BoardInteractionController::setLegalMovesVisible);
    }
}

// ---------------------------------------------------------------------------
// 棋譜欄・盤の配置・解析タブ
// ---------------------------------------------------------------------------

void MainWindowComposer::createPanelsAndDocks()
{
    m_c.commentCoordinator = new CommentCoordinator(&m_window);

    // 棋譜欄
    RecordPaneWiring::Deps rp;
    rp.parent = m_c.central;
    rp.recordModel = m_c.kifuRecord;
    rp.branchModel = m_c.kifuBranch;
    rp.commentCoordinator = m_c.commentCoordinator;
    m_c.recordPaneWiring = new RecordPaneWiring(rp, &m_window);
    m_c.recordPaneWiring->buildUiAndWire();
    m_c.recordPane = m_c.recordPaneWiring->pane();

    m_c.dockCreationService = std::make_unique<DockCreationService>(&m_window);
    m_c.dockCreationService->setDisplayMenu(m_ui.Display);
    m_c.dockCreationService->setRecordPane(m_c.recordPane);
    m_c.docks.recordPane = m_c.dockCreationService->createRecordPaneDock();

    // 盤をセントラルに配置する（司令塔はまだ無いので後で updateDeps し直す）
    MainWindowAppearanceController::Deps ad;
    ad.shogiView = m_c.shogiView;
    ad.timePresenter = m_c.timePresenter;
    ad.bottomSide = &m_state.bottomSide;
    m_c.appearanceController->updateDeps(ad);
    m_c.appearanceController->setupBoardInCenter();

    // エンジン解析タブ（思考モデル1・2はここで作られる）
    AnalysisTabWiring::Deps at;
    at.centralParent = m_c.central;
    m_c.analysisWiring = new AnalysisTabWiring(at, &m_window);
    m_c.analysisWiring->buildUiAndWire();
    m_c.analysisTab = m_c.analysisWiring->analysisTab();
    m_c.tab = m_c.analysisWiring->tab();
    m_c.thinking1 = m_c.analysisWiring->thinking1();
    m_c.thinking2 = m_c.analysisWiring->thinking2();
    if (Q_UNLIKELY(!m_c.analysisTab) || Q_UNLIKELY(!m_c.thinking1) || Q_UNLIKELY(!m_c.thinking2)) {
        qCWarning(lcApp, "createPanelsAndDocks: analysis wiring produced null components");
    }
    if (m_c.analysisTab) {
        m_c.analysisTab->considerationTabManager()->setConsiderationThinkingModel(m_c.consideration);
    }
}

// ---------------------------------------------------------------------------
// 分岐ナビゲーション（棋譜の正本）
// ---------------------------------------------------------------------------

void MainWindowComposer::createBranchNavigation()
{
    m_c.branchTree = new KifuBranchTree(&m_window);
    m_c.navState = new KifuNavigationState(&m_window);
    m_c.navState->setTree(m_c.branchTree);
    m_c.kifuNavController = new KifuNavigationController(&m_window);
    m_c.kifuNavController->setTreeAndState(m_c.branchTree, m_c.navState);
    m_c.liveGameSession = new LiveGameSession(&m_window);
    m_c.liveGameSession->setTree(m_c.branchTree);
    m_c.displayCoordinator = new KifuDisplayCoordinator(
        m_c.branchTree, m_c.navState, m_c.kifuNavController, &m_window);
}

// ---------------------------------------------------------------------------
// 棋譜モデル・対局者情報・解析ドック・評価値グラフ
// ---------------------------------------------------------------------------

void MainWindowComposer::createRecordAndPlayerInfo()
{
    GameRecordPresenter::Deps pd;
    pd.model = m_c.kifuRecord;
    pd.recordPane = m_c.recordPane;
    m_c.recordPresenter = new GameRecordPresenter(pd, &m_window);

    // 棋譜モデル（コメント・しおりの保存先と書き出し）。中身は分岐ツリーから読む
    m_c.gameRecord = new GameRecordModel(&m_window);
    m_c.gameRecord->setBranchTree(m_c.branchTree);
    m_c.gameRecord->setNavigationState(m_c.navState);

    PlayerInfoWiring::Dependencies pw;
    pw.parentWidget = &m_window;
    pw.tabWidget = m_c.tab;
    pw.shogiView = m_c.shogiView;
    pw.playMode = &m_state.playMode;
    pw.humanName1 = &m_state.humanName1;
    pw.humanName2 = &m_state.humanName2;
    pw.engineName1 = &m_state.engineName1;
    pw.engineName2 = &m_state.engineName2;
    pw.startSfenStr = &m_state.startSfenStr;
    pw.timeController = m_c.timeController;
    pw.gameRecord = m_c.gameRecord;
    pw.liveGameSession = m_c.liveGameSession;
    m_c.playerInfoWiring = new PlayerInfoWiring(pw, &m_window);
    m_c.playerInfoWiring->setAnalysisTab(m_c.analysisTab);
    m_c.playerInfoWiring->addGameInfoTabAtStartup();
    m_c.playerInfoWiring->ensureGameInfoController();
    m_c.gameInfoController = m_c.playerInfoWiring->gameInfoController();
    m_c.playerInfoController = m_c.playerInfoWiring->playerInfoController();

    // 解析用ドック（対局情報・思考・検討・USIログ・CSAログ・コメント・分岐ツリー）
    m_c.dockCreationService->setAnalysisTab(m_c.analysisTab);
    m_c.dockCreationService->setGameInfoController(m_c.gameInfoController);
    m_c.dockCreationService->setModels(m_c.thinking1, m_c.thinking2, m_c.commLog1, m_c.commLog2);
    m_c.dockCreationService->createAnalysisDocks();
    m_c.docks.gameInfo = m_c.dockCreationService->gameInfoDock();
    m_c.docks.thinking = m_c.dockCreationService->thinkingDock();
    m_c.docks.consideration = m_c.dockCreationService->considerationDock();
    m_c.docks.usiLog = m_c.dockCreationService->usiLogDock();
    m_c.docks.csaLog = m_c.dockCreationService->csaLogDock();
    m_c.docks.comment = m_c.dockCreationService->commentDock();
    m_c.docks.branchTree = m_c.dockCreationService->branchTreeDock();

    // 評価値グラフ。ナビゲーションボタンは棋譜欄のボタンと同じ操作をし、有効/無効も棋譜欄に合わせる
    m_c.evalChart = new EvaluationChartWidget(&m_window);
    KifuNavigationController::Buttons buttons;
    buttons.first = m_c.evalChart->firstButton();
    buttons.back10 = m_c.evalChart->back10Button();
    buttons.prev = m_c.evalChart->prevButton();
    buttons.next = m_c.evalChart->nextButton();
    buttons.fwd10 = m_c.evalChart->fwd10Button();
    buttons.last = m_c.evalChart->lastButton();
    m_c.kifuNavController->connectButtons(buttons);
    connect(m_c.recordPane, &RecordPane::arrowButtonsEnabledChanged,
            m_c.evalChart, &EvaluationChartWidget::setNavigationEnabled);
    m_c.evalChart->setNavigationEnabled(m_c.recordPane->firstButton()->isEnabled());
    m_c.dockCreationService->setEvalChart(m_c.evalChart);
    m_c.docks.evalChart = m_c.dockCreationService->createEvalChartDock();

    m_ui.actionLockDocks->setChecked(DockSettings::docksLocked());
}

// ---------------------------------------------------------------------------
// メニュー・定跡・解析結果のドックと配置管理
// ---------------------------------------------------------------------------

void MainWindowComposer::createSecondaryDocks()
{
    MenuWindowWiring::Dependencies md;
    md.parentWidget = &m_window;
    md.menuBar = m_window.menuBar();
    m_c.menuWiring = new MenuWindowWiring(md, &m_window);
    m_c.dockCreationService->setMenuWiring(m_c.menuWiring);
    m_c.docks.menuWindow = m_c.dockCreationService->createMenuWindowDock();

    // 定跡ウィンドウ。人間の手番かどうかは対局モードの判定サービスに聞く（wireGame で渡す）
    JosekiWindowWiring::Dependencies jd;
    jd.parentWidget = &m_window;
    jd.gameController = m_c.gameController;
    jd.currentSfenStr = &m_state.currentSfenStr;
    jd.navState = m_c.navState;
    m_c.josekiWiring = std::make_unique<JosekiWindowWiring>(jd);
    m_c.dockCreationService->setJosekiWiring(m_c.josekiWiring.get());
    m_c.docks.josekiWindow = m_c.dockCreationService->createJosekiWindowDock();
    // ドックが前面に出ている間だけ更新する
    if (m_c.docks.josekiWindow) m_c.josekiWiring->attachDock(m_c.docks.josekiWindow);

    m_c.analysisPresenter = std::make_unique<AnalysisResultsPresenter>();
    m_c.dockCreationService->setAnalysisPresenter(m_c.analysisPresenter.get());
    m_c.docks.analysisResults = m_c.dockCreationService->createAnalysisResultsDock();

    // 全ドック作成後にアクティブタブを設定
    if (m_c.docks.thinking) m_c.docks.thinking->raise();
    if (m_c.docks.recordPane) m_c.docks.recordPane->raise();

    m_c.dockLayoutManager = std::make_unique<DockLayoutManager>(&m_window);
    auto* layout = m_c.dockLayoutManager.get();
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
    DockUiCoordinator::wireLayoutMenu(layout, m_ui.actionResetDockLayout, m_ui.actionSaveDockLayout,
                                      m_ui.actionLockDocks, m_ui.menuSavedLayouts);
}

// ---------------------------------------------------------------------------
// 残りのサービス・コントローラ（依存は wire* で結ぶ）
// ---------------------------------------------------------------------------

void MainWindowComposer::createServices()
{
    // 共通基盤
    m_c.uiStatePolicy = new UiStatePolicyManager(&m_window);
    m_c.notificationService = new UiNotificationService(&m_window);
    m_c.kifuNavCoordinator = std::make_unique<KifuNavigationCoordinator>();
    m_c.evalGraphController = std::make_unique<EvaluationGraphController>();
    m_c.languageController = std::make_unique<LanguageController>();
    m_c.jishogiController = std::make_unique<JishogiScoreDialogController>();
    m_c.nyugyokuHandler = std::make_unique<NyugyokuDeclarationHandler>();
    m_c.pieceSoundPlayer = new PieceSoundPlayer(&m_window);
    m_c.playModePolicy = std::make_unique<PlayModePolicyService>();

    BoardSyncPresenter::Deps bs;
    bs.gc = m_c.gameController;
    bs.view = m_c.shogiView;
    bs.bic = m_c.boardController;
    bs.navState = m_c.navState;
    m_c.boardSync = new BoardSyncPresenter(bs, &m_window);

    // 対局
    m_c.turnStateSync = std::make_unique<TurnStateSyncService>();
    m_c.undoFlowService = std::make_unique<UndoFlowService>();
    m_c.replayController = new ReplayController(&m_window);
    m_c.gameStateController = new GameStateController(&m_window);
    m_c.consecutiveGamesController = new ConsecutiveGamesController(&m_window);
    m_c.gameSessionOrchestrator = new GameSessionOrchestrator(&m_window);
    m_c.sessionLifecycle = new SessionLifecycleCoordinator(&m_window);
    m_c.matchWiring = std::make_unique<MatchCoordinatorWiring>();

    PreStartCleanupHandler::Dependencies pc;
    pc.boardController = m_c.boardController;
    pc.shogiView = m_c.shogiView;
    pc.kifuRecordModel = m_c.kifuRecord;
    pc.kifuBranchModel = m_c.kifuBranch;
    pc.lineEditModel1 = m_c.commLog1;
    pc.lineEditModel2 = m_c.commLog2;
    pc.timeController = m_c.timeController;
    pc.evalChart = m_c.evalChart;
    pc.evalGraphController = m_c.evalGraphController.get();
    pc.recordPane = m_c.recordPane;
    pc.liveGameSession = m_c.liveGameSession;
    pc.branchTree = m_c.branchTree;
    pc.navState = m_c.navState;
    m_c.preStartCleanupHandler = new PreStartCleanupHandler(pc, &m_window);

    // 棋譜
    m_c.gameRecordLoadService = std::make_unique<GameRecordLoadService>();
    m_c.branchNavWiring = std::make_unique<BranchNavigationWiring>();
    m_c.kifuFileController = new KifuFileController(&m_window);
    m_c.kifuExportController = std::make_unique<KifuExportController>(&m_window);

    // UI
    m_c.pvClickController = new PvClickController(&m_window);
    m_c.considerationPositionService = new ConsiderationPositionService(&m_window);
    m_c.usiCommandController = std::make_unique<UsiCommandController>();
    m_c.dialogCoordinatorWiring = new DialogCoordinatorWiring(&m_window);
    m_c.recordNavWiring = new RecordNavigationWiring(&m_window);
    m_c.posEditCoordinator = new PositionEditCoordinator(&m_window);
    m_c.boardLoadService = new BoardLoadService(&m_window);
}
