/// @file mainwindowcomposer_wire.cpp
/// @brief 共通基盤・棋譜・UI・ダイアログ系の依存の注入

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "analysisresultspresenter.h"
#include "analysistabwiring.h"
#include "boardinteractioncontroller.h"
#include "boardloadservice.h"
#include "boardsetupcontroller.h"
#include "boardsyncpresenter.h"
#include "branchnavigationwiring.h"
#include "commentcoordinator.h"
#include "considerationpositionservice.h"
#include "considerationwiring.h"
#include "csagamewiring.h"
#include "dialogcoordinatorwiring.h"
#include "dialoglaunchwiring.h"
#include "engineanalysistab.h"
#include "evaluationgraphcontroller.h"
#include "gamerecordloadservice.h"
#include "gamerecordmodel.h"
#include "gamerecordupdateservice.h"
#include "gamesessionorchestrator.h"
#include "josekiwindowwiring.h"
#include "kifubranchtree.h"
#include "kifuexportcontroller.h"
#include "kifufilecontroller.h"
#include "kifuloadcoordinator.h"
#include "kifuloadcoordinatorfactory.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationcoordinator.h"
#include "kifunavigationstate.h"
#include "languagecontroller.h"
#include "livegamesession.h"
#include "mainwindowactions.h"
#include "menuwindowwiring.h"
#include "nyugyokudeclarationhandler.h"
#include "piecesoundplayer.h"
#include "playerinfocontroller.h"
#include "playerinfowiring.h"
#include "positioneditcontroller.h"
#include "positioneditcoordinator.h"
#include "pvclickcontroller.h"
#include "recordnavigationhandler.h"
#include "recordpane.h"
#include "replaycontroller.h"
#include "sessionlifecyclecoordinator.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "turnstatesyncservice.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#include "usicommandcontroller.h"
#include "candidatearrowcontroller.h"

#include <QAction>
#include <QStatusBar>

// ---------------------------------------------------------------------------
// 共通基盤（UI 状態・通知・評価値グラフ・言語・駒音）
// ---------------------------------------------------------------------------

void MainWindowComposer::wireFoundation()
{
    // UI 状態ポリシー。started 通知では MainWindow 側のモード同期より先に呼ばれるので、司令塔のモードを見る
    UiStatePolicyManager::Deps ud;
    ud.ui = &m_ui;
    ud.activity = &m_state.activity;
    ud.recordPane = m_c.recordPane;
    ud.analysisTab = m_c.analysisTab;
    ud.boardController = m_c.boardController;
    ud.getPlayMode = [this]() {
        return m_c.match ? m_c.match->playMode() : PlayMode::NotStarted;
    };
    m_c.uiStatePolicy->updateDeps(ud);

    UiNotificationService::Deps nd;
    nd.errorOccurred = &m_state.errorOccurred;
    nd.parentWidget = &m_window;
    m_c.notificationService->updateDeps(nd);

    // 評価値グラフ。表示中の手順の行数（終局行を含む）- 1 を長さとする
    m_c.evalGraphController->setEvalChart(m_c.evalChart);
    m_c.evalGraphController->setSfenRecord(&m_state.sfenRecord);
    m_c.evalGraphController->setNavigation(m_c.branchTree, m_c.navState);
    m_c.evalGraphController->setEngine1Name(m_state.engineName1);
    m_c.evalGraphController->setEngine2Name(m_state.engineName2);
    m_c.playerInfoController->setEvalGraphController(m_c.evalGraphController.get());
    m_c.playerInfoController->setLogModels(m_c.commLog1, m_c.commLog2);
    m_c.playerInfoController->setAnalysisTab(m_c.analysisTab);

    // コメント
    CommentCoordinator* cc = m_c.commentCoordinator;
    cc->setCommentEditor(m_c.analysisTab ? m_c.analysisTab->commentEditor() : nullptr);
    cc->setRecordPane(m_c.recordPane);
    cc->setRecordPresenter(m_c.recordPresenter);
    cc->setStatusBar(m_ui.statusbar);
    cc->setNavigationState(m_c.navState);
    cc->setGameRecordModel(m_c.gameRecord);
    cc->setKifuRecordListModel(m_c.kifuRecord);

    // 言語メニュー（相互排他）と表記
    m_c.languageController->setParentWidget(&m_window);
    m_c.languageController->setActions(
        m_ui.actionLanguageSystem, m_ui.actionLanguageJapanese, m_ui.actionLanguageEnglish,
        m_ui.actionLanguageChineseSimplified, m_ui.actionLanguageChineseTraditional);
    m_c.languageController->setNotationActions(
        m_ui.actionNotationAuto, m_ui.actionNotationJapanese,
        m_ui.actionNotationWestern, m_ui.actionNotationOrigin, m_ui.actionNotationHelp);

    // 駒音。メニュー「駒音」のチェック状態を設定と同期する
    QAction* soundAction = m_ui.actionPieceSound;
    soundAction->setIconVisibleInMenu(false);  // チェックマークを見せるためアイコンは非表示
    soundAction->setChecked(m_c.pieceSoundPlayer->isEnabled());
}

// ---------------------------------------------------------------------------
// 棋譜（ナビゲーション・分岐・読み込み・保存・定跡）
// ---------------------------------------------------------------------------

void MainWindowComposer::wireKifu()
{
    KifuNavigationCoordinator::Deps kd;
    kd.shogiView = m_c.shogiView;
    kd.navState = m_c.navState;
    kd.navController = m_c.kifuNavController;
    kd.currentSfenStr = &m_state.currentSfenStr;
    kd.playMode = &m_state.playMode;
    kd.match = m_c.match;
    kd.analysisTab = m_c.analysisTab;
    kd.uiStatePolicy = m_c.uiStatePolicy;
    kd.josekiWiring = m_c.josekiWiring.get();
    m_c.kifuNavCoordinator->updateDeps(kd);

    BranchNavigationWiring::Deps bd;
    bd.branchTree = m_c.branchTree;
    bd.navState = m_c.navState;
    bd.kifuNavController = m_c.kifuNavController;
    bd.displayCoordinator = m_c.displayCoordinator;
    bd.liveGameSession = m_c.liveGameSession;
    bd.recordPane = m_c.recordPane;
    bd.analysisTab = m_c.analysisTab;
    bd.kifuRecordModel = m_c.kifuRecord;
    bd.kifuBranchModel = m_c.kifuBranch;
    bd.gameRecordModel = m_c.gameRecord;
    bd.gameController = m_c.gameController;
    bd.commentCoordinator = m_c.commentCoordinator;
    bd.startSfenStr = &m_state.startSfenStr;
    m_c.branchNavWiring->updateDeps(bd);
    m_c.branchNavWiring->initialize();

    GameRecordLoadService::Deps ld;
    ld.recordPane = m_c.recordPane;
    ld.recordPresenter = m_c.recordPresenter;
    ld.gameRecord = m_c.gameRecord;
    m_c.gameRecordLoadService->updateDeps(ld);

    // 棋譜読み込み担当（1つだけ作り、全部の読み込みで使い回す）
    KifuLoadCoordinatorFactory::Params p;
    p.gameInfoController = m_c.gameInfoController;
    p.recordPane = m_c.recordPane;
    p.kifuRecordModel = m_c.kifuRecord;
    p.kifuBranchModel = m_c.kifuBranch;
    p.parent = &m_window;
    p.branchTree = m_c.branchTree;
    p.navState = m_c.navState;
    p.analysisTab = m_c.analysisTab;
    p.shogiView = m_c.shogiView;
    p.recordLoad = m_c.gameRecordLoadService.get();
    p.branchNavWiring = m_c.branchNavWiring.get();
    p.kifuNavCoordinator = m_c.kifuNavCoordinator.get();
    p.uiStatePolicy = m_c.uiStatePolicy;
    p.playerInfoWiring = m_c.playerInfoWiring;
    p.notificationService = m_c.notificationService;
    m_c.kifuLoadCoordinator = KifuLoadCoordinatorFactory::createAndWire(p);

    // 保存・書き出し
    KifuExportController::Dependencies ed;
    ed.gameRecord = m_c.gameRecord;
    ed.kifuRecordModel = m_c.kifuRecord;
    ed.gameInfoController = m_c.gameInfoController;
    ed.timeController = m_c.timeController;
    ed.gameController = m_c.gameController;
    ed.state = &m_state;
    ed.navState = m_c.navState;
    m_c.kifuExportController->setDependencies(ed);

    KifuFileController::Deps fd;
    fd.parentWidget = &m_window;
    fd.statusBar = m_ui.statusbar;
    fd.gameInfo = m_c.gameInfoController;
    fd.saveFileName = &m_state.saveFileName;
    fd.gameRecord = m_c.gameRecord;
    fd.exportController = m_c.kifuExportController.get();
    fd.loadCoordinator = m_c.kifuLoadCoordinator;
    fd.replayController = m_c.replayController;
    fd.clearUiBeforeKifuLoad = [this]() { m_c.sessionLifecycle->clearUiBeforeKifuLoad(); };
    m_c.kifuFileController->updateDeps(fd);
}

// ---------------------------------------------------------------------------
// UI（盤の読み込み・局面編集・読み筋クリック・検討・USI コマンド・棋譜欄の行変更）
// ---------------------------------------------------------------------------

void MainWindowComposer::wireUi()
{
    BoardLoadService::Deps bl;
    bl.gc = m_c.gameController;
    bl.view = m_c.shogiView;
    bl.boardSync = m_c.boardSync;
    bl.currentSfenStr = &m_state.currentSfenStr;
    bl.turnSync = m_c.turnStateSync.get();
    bl.kifuNav = m_c.kifuNavCoordinator.get();
    m_c.boardLoadService->updateDeps(bl);

    // 盤操作
    BoardSetupController* bsc = m_c.boardSetupController;
    bsc->setMatchCoordinator(m_c.match);
    bsc->setEvaluationGraphController(m_c.evalGraphController.get());

    // 局面編集
    PositionEditCoordinator* pec = m_c.posEditCoordinator;
    pec->setShogiView(m_c.shogiView);
    pec->setGameController(m_c.gameController);
    pec->setBoardController(m_c.boardController);
    pec->setPositionEditController(m_c.posEdit);
    pec->setMatchCoordinator(m_c.match);
    pec->setReplayController(m_c.replayController);
    pec->setNavigationState(m_c.navState);
    pec->setStartSfenStr(&m_state.startSfenStr);
    pec->setCurrentSfenStr(&m_state.currentSfenStr);
    pec->setOnMainRowGuard(&m_state.onMainRowGuard);
    pec->setUiStatePolicy(m_c.uiStatePolicy);
    PositionEditCoordinator::EditActions actions;
    actions.actionReturnAllPiecesToStand = m_ui.actionReturnAllPiecesToStand;
    actions.actionSetHiratePosition = m_ui.actionSetHiratePosition;
    actions.actionSetTsumePosition = m_ui.actionSetTsumePosition;
    actions.actionChangeTurn = m_ui.actionChangeTurn;
    pec->setEditActions(actions);

    // 読み筋クリック
    PvClickController* pv = m_c.pvClickController;
    pv->setThinkingModels(m_c.thinking1, m_c.thinking2);
    pv->setConsiderationModel(m_c.consideration);
    PvClickController::StateRefs refs;
    refs.playMode = &m_state.playMode;
    refs.humanName1 = &m_state.humanName1;
    refs.humanName2 = &m_state.humanName2;
    refs.engineName1 = &m_state.engineName1;
    refs.engineName2 = &m_state.engineName2;
    refs.currentSfenStr = &m_state.currentSfenStr;
    refs.startSfenStr = &m_state.startSfenStr;
    refs.navState = m_c.navState;
    pv->setStateRefs(refs);
    pv->setShogiView(m_c.shogiView);

    // 検討する局面
    ConsiderationPositionService::Deps cd;
    cd.playMode = &m_state.playMode;
    cd.startSfenStr = &m_state.startSfenStr;
    cd.navState = m_c.navState;
    cd.match = m_c.match;
    cd.analysisTab = m_c.analysisTab;
    m_c.considerationPositionService->updateDeps(cd);

    // USI コマンド送信
    m_c.usiCommandController->setMatchCoordinator(m_c.match);
    m_c.usiCommandController->setAnalysisTab(m_c.analysisTab);
    m_c.analysisTab->setArrowController(CandidateArrowController::forView(m_c.shogiView));

    // 検討モード（ダイアログの司令塔は wireDialogs で渡す）
    ConsiderationWiring::Deps cw;
    cw.parentWidget = &m_window;
    cw.considerationTabManager = m_c.analysisTab->considerationTabManager();
    cw.thinkingInfo1 = m_c.analysisTab->info1();
    cw.shogiView = m_c.shogiView;
    cw.match = m_c.match;
    cw.considerationModel = m_c.consideration;
    cw.commLogModel = m_c.commLog1;
    cw.playMode = &m_state.playMode;
    cw.currentSfenStr = &m_state.currentSfenStr;
    m_c.considerationWiring = new ConsiderationWiring(cw, &m_window);

    // 棋譜欄の行変更
    RecordNavigationHandler::Deps rd;
    rd.navState = m_c.navState;
    rd.displayCoordinator = m_c.displayCoordinator;
    rd.liveSession = m_c.liveGameSession;
    m_c.recordNavHandler->updateDeps(rd);

    // 解析タブの外部シグナル
    AnalysisTabWiring::ExternalSignalDeps ext;
    ext.branchNavigationWiring = m_c.branchNavWiring.get();
    ext.pvClickController = m_c.pvClickController;
    ext.commentCoordinator = m_c.commentCoordinator;
    ext.usiCommandController = m_c.usiCommandController.get();
    ext.considerationWiring = m_c.considerationWiring;
    m_c.analysisWiring->wireExternalSignals(ext);

    // 起動後の利用者操作（着手・局面編集・待った・新規・棋譜欄の行変更）
    MainWindowActions::Deps ad;
    ad.boardSetup = m_c.boardSetupController;
    ad.boardController = m_c.boardController;
    ad.branchWiring = m_c.branchNavWiring.get();
    ad.gameController = m_c.gameController;
    ad.view = m_c.shogiView;
    ad.uiPolicy = m_c.uiStatePolicy;
    ad.recordUpdates = m_c.gameRecordUpdateService.get();
    ad.gameSession = m_c.gameSessionOrchestrator;
    ad.sessionLifecycle = m_c.sessionLifecycle;
    ad.positionEdit = m_c.posEditCoordinator;
    ad.comments = m_c.commentCoordinator;
    ad.displayCoordinator = m_c.displayCoordinator;
    ad.recordNavigation = m_c.recordNavHandler;
    ad.undoFlow = m_c.undoFlowService.get();
    ad.kifuFileController = m_c.kifuFileController;
    ad.josekiWiring = m_c.josekiWiring.get();
    ad.state = &m_state;
    ad.editedPositionReset.navState = m_c.navState;
    ad.editedPositionReset.displayCoordinator = m_c.displayCoordinator;
    ad.editedPositionReset.gameRecord = m_c.gameRecord;
    ad.editedPositionReset.sfenRecord = &m_state.sfenRecord;
    ad.editedPositionReset.branchTree = m_c.branchTree;
    ad.editedPositionReset.liveGameSession = m_c.liveGameSession;
    m_c.actions = std::make_unique<MainWindowActions>(std::move(ad));
    bsc->setVariationInputCallbacks(
        [this]() { return m_c.actions->canRecordVariationMove(); },
        [this](QPoint& from, QPoint& to) { return m_c.actions->recordVariationMove(from, to); });
}

// ---------------------------------------------------------------------------
// ダイアログ（起動配線と、検討・詰み探索・棋譜解析の司令塔）
// ---------------------------------------------------------------------------

void MainWindowComposer::wireDialogs()
{
    DialogCoordinatorWiring::Deps dc;
    dc.parentWidget = &m_window;
    dc.match = m_c.match;
    dc.gameController = m_c.gameController;
    dc.startSfenStr = &m_state.startSfenStr;
    dc.kifuRecordModel = m_c.kifuRecord;
    dc.currentSfenStr = &m_state.currentSfenStr;
    dc.navState = m_c.navState;
    dc.considerationModel = m_c.consideration;
    dc.gameInfoController = m_c.gameInfoController;
    dc.evalChart = m_c.evalChart;
    dc.presenter = m_c.analysisPresenter.get();
    dc.shogiView = m_c.shogiView;
    dc.considerationWiring = m_c.considerationWiring;
    dc.uiStatePolicy = m_c.uiStatePolicy;
    dc.analysisTab = m_c.analysisTab;
    dc.playMode = &m_state.playMode;
    dc.kifuNavController = m_c.kifuNavController;
    m_c.dialogCoordinatorWiring->wire(dc);
    m_c.dialogCoordinator = m_c.dialogCoordinatorWiring->coordinator();
    // 検討モードの配線とダイアログの司令塔は互いを参照する。両方できたのでここで渡す
    m_c.considerationWiring->setDialogCoordinator(m_c.dialogCoordinator);

    DialogLaunchWiring::Deps dl;
    dl.parentWidget = &m_window;
    dl.kifuFileController = m_c.kifuFileController;
    dl.playMode = &m_state.playMode;
    dl.dialogCoordinator = m_c.dialogCoordinator;
    dl.gameController = m_c.gameController;
    dl.match = m_c.match;
    dl.shogiView = m_c.shogiView;
    dl.jishogiController = m_c.jishogiController.get();
    dl.nyugyokuHandler = m_c.nyugyokuHandler.get();
    dl.csaGameWiring = m_c.csaGameWiring.get();
    dl.boardSetupController = m_c.boardSetupController;
    dl.gameInfoController = m_c.gameInfoController;
    dl.analysisPresenter = m_c.analysisPresenter.get();
    dl.analysisTab = m_c.analysisTab;
    dl.lineEditModel1 = m_c.commLog1;
    dl.modelThinking1 = m_c.thinking1;
    dl.kifuRecordModel = m_c.kifuRecord;
    dl.navState = m_c.navState;
    dl.evalChart = m_c.evalChart;
    dl.pieceSoundPlayer = m_c.pieceSoundPlayer;
    dl.analysisModel = m_c.analysis;
    dl.menuWindowDock = m_c.docks.menuWindow;
    m_c.dialogLaunchWiring = new DialogLaunchWiring(dl, &m_window);
}
