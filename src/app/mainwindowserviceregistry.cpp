/// @file mainwindowserviceregistry.cpp
/// @brief ensure* 群の生成ロジック実装

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

// --- Qt ヘッダー ---
#include <functional>

// --- プロジェクトヘッダー ---
#include "evaluationgraphcontroller.h"
#include "evaluationchartwidget.h"
#include "timecontrolcontroller.h"
#include "replaycontroller.h"
#include "branchnavigationwiring.h"
#include "matchcoordinatorwiring.h"
#include "mainwindowwiringassembler.h"
#include "mainwindowruntimerefsfactory.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowcompositionroot.h"
#include "dialogcoordinator.h"
#include "dialogcoordinatorwiring.h"
#include "kifufilecontroller.h"
#include "kifuexportcontroller.h"
#include "kifuexportdepsassembler.h"
#include "gamestatecontroller.h"
#include "playerinfocontroller.h"
#include "playerinfowiring.h"
#include "boardsetupcontroller.h"
#include "pvclickcontroller.h"
#include "positioneditcoordinator.h"
#include "csagamewiring.h"
#include "josekiwindowwiring.h"
#include "menuwindowwiring.h"
#include "prestartcleanuphandler.h"
#include "turnsyncbridge.h"
#include "turnmanager.h"
#include "positioneditcontroller.h"
#include "boardsyncpresenter.h"
#include "boardloadservice.h"
#include "considerationpositionservice.h"
#include "analysisresultspresenter.h"
#include "gamestartcoordinator.h"
#include "gamerecordpresenter.h"
#include "livegamesessionupdater.h"
#include "gamerecordupdateservice.h"
#include "undoflowservice.h"
#include "gamerecordloadservice.h"
#include "turnstatesyncservice.h"
#include "gamerecordmodelbuilder.h"
#include "jishogiscoredialogcontroller.h"
#include "nyugyokudeclarationhandler.h"
#include "consecutivegamescontroller.h"
#include "languagecontroller.h"
#include "considerationwiring.h"
#include "docklayoutmanager.h"
#include "dockcreationservice.h"
#include "commentcoordinator.h"
#include "usicommandcontroller.h"
#include "recordnavigationwiring.h"
#include "uistatepolicymanager.h"
#include "kifunavigationcoordinator.h"
#include "kifunavigationdepsfactory.h"
#include "mainwindowappearancecontroller.h"
#include "gamesessionorchestrator.h"
#include "uinotificationservice.h"
#include "kifuloadcoordinatorfactory.h"
#include "sessionlifecyclecoordinator.h"
#include "sessionlifecycledepsfactory.h"
#include "engineanalysistab.h"
#include "shogiview.h"
#include "shogigamecontroller.h"
#include "recordpane.h"
#include "kifuloadcoordinator.h"
#include "matchruntimequeryservice.h"
#include "mainwindowmatchadapter.h"
#include "mainwindowcoreinitcoordinator.h"
#include "mainwindowresetservice.h"
#include "mainwindowdockbootstrapper.h"
#include "boardinteractioncontroller.h"
#include "shogiclock.h"
#include "csagamecoordinator.h"
#include "logcategories.h"

MainWindowServiceRegistry::MainWindowServiceRegistry(MainWindow& mw, QObject* parent)
    : QObject(parent)
    , m_mw(mw)
{
}

// ---------------------------------------------------------------------------
// 評価値グラフ / 時間制御
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureEvaluationGraphController()
{
    if (m_mw.m_evalGraphController) return;

    m_mw.m_evalGraphController = new EvaluationGraphController(&m_mw);
    m_mw.m_evalGraphController->setEvalChart(m_mw.m_evalChart);
    m_mw.m_evalGraphController->setMatchCoordinator(m_mw.m_match);
    m_mw.m_evalGraphController->setSfenRecord(m_mw.m_queryService->sfenRecord());
    m_mw.m_evalGraphController->setEngine1Name(m_mw.m_player.engineName1);
    m_mw.m_evalGraphController->setEngine2Name(m_mw.m_player.engineName2);

    // PlayerInfoControllerが既に存在する場合は、EvalGraphControllerを設定
    if (m_mw.m_playerInfoController) {
        m_mw.m_playerInfoController->setEvalGraphController(m_mw.m_evalGraphController);
    }
}

void MainWindowServiceRegistry::ensureTimeController()
{
    if (m_mw.m_timeController) return;

    m_mw.m_timeController = new TimeControlController(&m_mw);
    m_mw.m_timeController->setTimeDisplayPresenter(m_mw.m_timePresenter);
    m_mw.m_timeController->ensureClock();
}

void MainWindowServiceRegistry::ensureReplayController()
{
    if (m_mw.m_replayController) return;

    m_mw.m_replayController = new ReplayController(&m_mw);
    m_mw.m_replayController->setClock(m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr);
    m_mw.m_replayController->setShogiView(m_mw.m_shogiView);
    m_mw.m_replayController->setGameController(m_mw.m_gameController);
    m_mw.m_replayController->setMatchCoordinator(m_mw.m_match);
    m_mw.m_replayController->setRecordPane(m_mw.m_recordPane);
}

// ---------------------------------------------------------------------------
// 分岐ナビゲーション
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureBranchNavigationWiring()
{
    if (!m_mw.m_branchNavWiring) {
        m_mw.m_branchNavWiring = new BranchNavigationWiring(&m_mw);

        // 転送シグナルを MainWindow スロットに接続
        connect(m_mw.m_branchNavWiring, &BranchNavigationWiring::boardWithHighlightsRequired,
                &m_mw, &MainWindow::loadBoardWithHighlights);
        connect(m_mw.m_branchNavWiring, &BranchNavigationWiring::boardSfenChanged,
                &m_mw, &MainWindow::loadBoardFromSfen);
        ensureKifuNavigationCoordinator();
        connect(m_mw.m_branchNavWiring, &BranchNavigationWiring::branchNodeHandled,
                m_mw.m_kifuNavCoordinator, &KifuNavigationCoordinator::handleBranchNodeHandled);
    }

    BranchNavigationWiring::Deps deps;
    deps.branchTree = &m_mw.m_branchNav.branchTree;
    deps.navState = &m_mw.m_branchNav.navState;
    deps.kifuNavController = &m_mw.m_branchNav.kifuNavController;
    deps.displayCoordinator = &m_mw.m_branchNav.displayCoordinator;
    deps.branchTreeWidget = &m_mw.m_branchNav.branchTreeWidget;
    deps.liveGameSession = &m_mw.m_branchNav.liveGameSession;
    deps.recordPane = m_mw.m_recordPane;
    deps.analysisTab = m_mw.m_analysisTab;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.kifuBranchModel = m_mw.m_models.kifuBranch;
    deps.gameRecordModel = m_mw.m_models.gameRecord;
    deps.gameController = m_mw.m_gameController;
    deps.commentCoordinator = m_mw.m_commentCoordinator;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.ensureCommentCoordinator = [this]() { ensureCommentCoordinator(); };
    m_mw.m_branchNavWiring->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// MatchCoordinator 配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureMatchCoordinatorWiring()
{
    const bool firstTime = !m_mw.m_matchWiring;
    if (!m_mw.m_matchWiring) {
        m_mw.m_matchWiring = new MatchCoordinatorWiring(&m_mw);
    }

    ensureEvaluationGraphController();
    ensurePlayerInfoWiring();

    // アダプタの生成と Deps 更新（buildMatchWiringDeps より前に必要）
    if (!m_mw.m_matchAdapter) {
        m_mw.m_matchAdapter = std::make_unique<MainWindowMatchAdapter>();
    }
    {
        MainWindowMatchAdapter::Deps ad;
        ad.shogiView = m_mw.m_shogiView;
        ad.gameController = m_mw.m_gameController;
        ad.boardController = m_mw.m_boardController;
        ad.playMode = &m_mw.m_state.playMode;
        ensureCoreInitCoordinator();
        ad.initializeNewGame = std::bind(&MainWindowCoreInitCoordinator::initializeNewGame,
                                         m_mw.m_coreInit.get(), std::placeholders::_1);
        ad.ensureAndGetPlayerInfoWiring = [this]() -> PlayerInfoWiring* {
            ensurePlayerInfoWiring();
            return m_mw.m_playerInfoWiring;
        };
        ad.ensureAndGetDialogCoordinator = [this]() -> DialogCoordinator* {
            ensureDialogCoordinator();
            return m_mw.m_dialogCoordinator;
        };
        ad.ensureAndGetTurnStateSync = [this]() -> TurnStateSyncService* {
            ensureTurnStateSyncService();
            return m_mw.m_turnStateSync.get();
        };
        m_mw.m_matchAdapter->updateDeps(ad);
    }

    auto deps = MainWindowWiringAssembler::buildMatchWiringDeps(m_mw);
    m_mw.m_matchWiring->updateDeps(deps);

    // 初回のみ: 転送シグナルを GameSessionOrchestrator / MainWindow スロットに接続
    if (firstTime) {
        ensureGameSessionOrchestrator();
        m_mw.m_matchWiring->wireForwardingSignals(&m_mw, m_mw.m_gameSessionOrchestrator);
    }
}

// ---------------------------------------------------------------------------
// ダイアログ / 棋譜ファイル
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureDialogCoordinator()
{
    if (m_mw.m_dialogCoordinator) return;

    auto refs = m_mw.buildRuntimeRefs();

    MainWindowDepsFactory::DialogCoordinatorCallbacks callbacks;
    callbacks.getBoardFlipped = [this]() { return m_mw.m_shogiView ? m_mw.m_shogiView->getFlipMode() : false; };
    callbacks.getConsiderationWiring = [this]() { ensureConsiderationWiring(); return m_mw.m_considerationWiring; };
    callbacks.getUiStatePolicyManager = [this]() { ensureUiStatePolicyManager(); return m_mw.m_uiStatePolicy; };
    ensureKifuNavigationCoordinator();
    callbacks.navigateKifuViewToRow = std::bind(&KifuNavigationCoordinator::navigateToRow, m_mw.m_kifuNavCoordinator, std::placeholders::_1);

    m_mw.m_compositionRoot->ensureDialogCoordinator(refs, callbacks, &m_mw,
        m_mw.m_dialogCoordinatorWiring, m_mw.m_dialogCoordinator);
}

void MainWindowServiceRegistry::ensureKifuFileController()
{
    auto refs = m_mw.buildRuntimeRefs();

    MainWindowDepsFactory::KifuFileCallbacks callbacks;
    callbacks.clearUiBeforeKifuLoad = std::bind(&MainWindowServiceRegistry::clearUiBeforeKifuLoad, this);
    callbacks.setReplayMode = std::bind(&MainWindow::setReplayMode, &m_mw, std::placeholders::_1);
    callbacks.ensurePlayerInfoAndGameInfo = [this]() {
        ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->ensureGameInfoController();
            m_mw.m_gameInfoController = m_mw.m_playerInfoWiring->gameInfoController();
        }
    };
    callbacks.ensureGameRecordModel = std::bind(&MainWindowServiceRegistry::ensureGameRecordModel, this);
    callbacks.ensureKifuExportController = std::bind(&MainWindowServiceRegistry::ensureKifuExportController, this);
    callbacks.updateKifuExportDependencies = std::bind(&MainWindow::updateKifuExportDependencies, &m_mw);
    callbacks.createAndWireKifuLoadCoordinator = [this]() { KifuLoadCoordinatorFactory::createAndWire(m_mw); };
    callbacks.ensureKifuLoadCoordinatorForLive = std::bind(&MainWindowServiceRegistry::ensureKifuLoadCoordinatorForLive, this);
    callbacks.getKifuExportController = [this]() { return m_mw.m_kifuExportController; };
    callbacks.getKifuLoadCoordinator = [this]() { return m_mw.m_kifuLoadCoordinator; };

    m_mw.m_compositionRoot->ensureKifuFileController(refs, callbacks, &m_mw,
        m_mw.m_kifuFileController);
}

void MainWindowServiceRegistry::ensureKifuExportController()
{
    if (m_mw.m_kifuExportController) return;

    m_mw.m_kifuExportController = new KifuExportController(&m_mw, &m_mw);

    // 準備コールバックを設定（クリップボードコピー等の前に呼ばれる）
    m_mw.m_kifuExportController->setPrepareCallback([this]() {
        ensureGameRecordModel();
        m_mw.updateKifuExportDependencies();
    });

    // ステータスバーへのメッセージ転送
    connect(m_mw.m_kifuExportController, &KifuExportController::statusMessage,
            &m_mw, [this](const QString& msg, int timeout) {
                if (m_mw.ui && m_mw.ui->statusbar) {
                    m_mw.ui->statusbar->showMessage(msg, timeout);
                }
            });
}

// ---------------------------------------------------------------------------
// ゲーム状態
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureGameStateController()
{
    if (m_mw.m_gameStateController) return;

    ensureUiStatePolicyManager();

    MainWindowDepsFactory::GameStateControllerCallbacks cbs;
    cbs.enableArrowButtons = std::bind(&UiStatePolicyManager::transitionToIdle, m_mw.m_uiStatePolicy);
    cbs.setReplayMode = std::bind(&MainWindow::setReplayMode, &m_mw, std::placeholders::_1);
    cbs.refreshBranchTree = [this]() { m_mw.m_kifu.currentSelectedPly = 0; };
    cbs.updatePlyState = [this](int ap, int sp, int mi) {
        m_mw.m_kifu.activePly = ap;
        m_mw.m_kifu.currentSelectedPly = sp;
        m_mw.m_state.currentMoveIndex = mi;
        ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->syncNavStateToPly(sp);
    };

    m_mw.m_compositionRoot->ensureGameStateController(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_gameStateController);
}

void MainWindowServiceRegistry::ensurePlayerInfoController()
{
    if (m_mw.m_playerInfoController) return;
    ensurePlayerInfoWiring();
    m_mw.m_compositionRoot->ensurePlayerInfoController(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_playerInfoController);
}

void MainWindowServiceRegistry::ensureBoardSetupController()
{
    if (m_mw.m_boardSetupController) return;

    MainWindowDepsFactory::BoardSetupControllerCallbacks cbs;
    cbs.ensurePositionEdit = [this]() { ensurePositionEditController(); };
    cbs.ensureTimeController = [this]() { ensureTimeController(); };
    cbs.updateGameRecord = [this](const QString& moveText, const QString& elapsed) {
        ensureGameRecordUpdateService();
        if (m_mw.m_gameRecordUpdateService) {
            m_mw.m_gameRecordUpdateService->updateGameRecord(moveText, elapsed);
        }
    };
    cbs.redrawEngine1Graph = [this](int ply) {
        ensureEvaluationGraphController();
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine1Graph(ply);
    };
    cbs.redrawEngine2Graph = [this](int ply) {
        ensureEvaluationGraphController();
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine2Graph(ply);
    };

    m_mw.m_compositionRoot->ensureBoardSetupController(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_boardSetupController);
}

void MainWindowServiceRegistry::ensurePvClickController()
{
    if (m_mw.m_pvClickController) return;
    m_mw.m_compositionRoot->ensurePvClickController(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_pvClickController);

    // 動的状態へのポインタを設定（onPvRowClicked 呼び出し時に自動同期）
    PvClickController::StateRefs stateRefs;
    stateRefs.playMode = &m_mw.m_state.playMode;
    stateRefs.humanName1 = &m_mw.m_player.humanName1;
    stateRefs.humanName2 = &m_mw.m_player.humanName2;
    stateRefs.engineName1 = &m_mw.m_player.engineName1;
    stateRefs.engineName2 = &m_mw.m_player.engineName2;
    stateRefs.currentSfenStr = &m_mw.m_state.currentSfenStr;
    stateRefs.startSfenStr = &m_mw.m_state.startSfenStr;
    stateRefs.currentMoveIndex = &m_mw.m_state.currentMoveIndex;
    stateRefs.considerationModel = &m_mw.m_models.consideration;
    m_mw.m_pvClickController->setStateRefs(stateRefs);
    m_mw.m_pvClickController->setShogiView(m_mw.m_shogiView);

    if (m_mw.m_analysisTab) {
        connect(m_mw.m_pvClickController, &PvClickController::pvDialogClosed,
                m_mw.m_analysisTab, &EngineAnalysisTab::clearThinkingViewSelection, Qt::UniqueConnection);
    }
}

void MainWindowServiceRegistry::ensurePositionEditCoordinator()
{
    if (m_mw.m_posEditCoordinator) return;

    ensureUiStatePolicyManager();

    MainWindowDepsFactory::PositionEditCoordinatorCallbacks cbs;
    cbs.applyEditMenuState = [this](bool editing) {
        if (editing) m_mw.m_uiStatePolicy->transitionToDuringPositionEdit();
        else m_mw.m_uiStatePolicy->transitionToIdle();
    };
    cbs.ensurePositionEdit = [this]() {
        ensurePositionEditController();
        if (m_mw.m_posEditCoordinator) m_mw.m_posEditCoordinator->setPositionEditController(m_mw.m_posEdit);
    };
    cbs.actionReturnAllPiecesToStand = m_mw.ui->actionReturnAllPiecesToStand;
    cbs.actionSetHiratePosition = m_mw.ui->actionSetHiratePosition;
    cbs.actionSetTsumePosition = m_mw.ui->actionSetTsumePosition;
    cbs.actionChangeTurn = m_mw.ui->actionChangeTurn;

    m_mw.m_compositionRoot->ensurePositionEditCoordinator(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_posEditCoordinator);
}

// ---------------------------------------------------------------------------
// 通信対局 / ウィンドウ配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureCsaGameWiring()
{
    if (m_mw.m_csaGameWiring) return;

    CsaGameWiring::Dependencies deps;
    deps.coordinator = m_mw.m_csaGameCoordinator;
    deps.gameController = m_mw.m_gameController;
    deps.shogiView = m_mw.m_shogiView;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.recordPane = m_mw.m_recordPane;
    deps.boardController = m_mw.m_boardController;
    deps.statusBar = m_mw.ui ? m_mw.ui->statusbar : nullptr;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    deps.analysisTab = m_mw.m_analysisTab;
    deps.boardSetupController = m_mw.m_boardSetupController;
    deps.usiCommLog = m_mw.m_models.commLog1;
    deps.engineThinking = m_mw.m_models.thinking1;
    deps.timeController = m_mw.m_timeController;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.playMode = &m_mw.m_state.playMode;
    deps.parentWidget = &m_mw;

    m_mw.m_csaGameWiring = new CsaGameWiring(deps, &m_mw);

    // 外部シグナル接続を CsaGameWiring に委譲
    ensureUiStatePolicyManager();
    ensureGameRecordUpdateService();
    ensureUiNotificationService();
    m_mw.m_csaGameWiring->wireExternalSignals(m_mw.m_uiStatePolicy,
                                               m_mw.m_gameRecordUpdateService,
                                               m_mw.m_notificationService);

    qCDebug(lcApp).noquote() << "ensureCsaGameWiring_: created and connected";
}

void MainWindowServiceRegistry::ensureJosekiWiring()
{
    if (m_mw.m_josekiWiring) return;

    JosekiWindowWiring::Dependencies deps;
    deps.parentWidget = &m_mw;
    deps.gameController = m_mw.m_gameController;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    deps.usiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.currentMoveIndex = &m_mw.m_state.currentMoveIndex;
    deps.currentSelectedPly = &m_mw.m_kifu.currentSelectedPly;
    deps.playMode = &m_mw.m_state.playMode;

    m_mw.m_josekiWiring = new JosekiWindowWiring(deps, &m_mw);

    connect(m_mw.m_josekiWiring, &JosekiWindowWiring::moveRequested,
            &m_mw, &MainWindow::onMoveRequested);
    // forcedPromotionRequested → ShogiGameController::setForcedPromotion 直接接続
    if (m_mw.m_gameController) {
        connect(m_mw.m_josekiWiring, &JosekiWindowWiring::forcedPromotionRequested,
                m_mw.m_gameController, &ShogiGameController::setForcedPromotion);
    }

    qCDebug(lcApp).noquote() << "ensureJosekiWiring_: created and connected";
}

void MainWindowServiceRegistry::ensureMenuWiring()
{
    if (m_mw.m_menuWiring) return;

    MenuWindowWiring::Dependencies deps;
    deps.parentWidget = &m_mw;
    deps.menuBar = m_mw.menuBar();

    m_mw.m_menuWiring = new MenuWindowWiring(deps, &m_mw);

    qCDebug(lcApp).noquote() << "ensureMenuWiring_: created and connected";
}

void MainWindowServiceRegistry::ensurePlayerInfoWiring()
{
    if (m_mw.m_playerInfoWiring) return;

    PlayerInfoWiring::Dependencies deps;
    deps.parentWidget = &m_mw;
    deps.tabWidget = m_mw.m_tab;
    deps.shogiView = m_mw.m_shogiView;
    deps.playMode = &m_mw.m_state.playMode;
    deps.humanName1 = &m_mw.m_player.humanName1;
    deps.humanName2 = &m_mw.m_player.humanName2;
    deps.engineName1 = &m_mw.m_player.engineName1;
    deps.engineName2 = &m_mw.m_player.engineName2;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.timeControllerRef = &m_mw.m_timeController;

    m_mw.m_playerInfoWiring = new PlayerInfoWiring(deps, &m_mw);

    // 検討タブが既に作成済みなら設定
    if (m_mw.m_analysisTab) {
        m_mw.m_playerInfoWiring->setAnalysisTab(m_mw.m_analysisTab);
    }

    // PlayerInfoWiringからのシグナルを外観コントローラに接続
    connect(m_mw.m_playerInfoWiring, &PlayerInfoWiring::tabCurrentChanged,
            m_mw.m_appearanceController, &MainWindowAppearanceController::onTabCurrentChanged);

    // PlayerInfoControllerも同期
    m_mw.m_playerInfoController = m_mw.m_playerInfoWiring->playerInfoController();

    qCDebug(lcApp).noquote() << "ensurePlayerInfoWiring_: created and connected";
}

void MainWindowServiceRegistry::ensurePreStartCleanupHandler()
{
    if (m_mw.m_preStartCleanupHandler) return;

    PreStartCleanupHandler::Dependencies deps;
    deps.boardController = m_mw.m_boardController;
    deps.shogiView = m_mw.m_shogiView;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.kifuBranchModel = m_mw.m_models.kifuBranch;
    deps.lineEditModel1 = m_mw.m_models.commLog1;
    deps.lineEditModel2 = m_mw.m_models.commLog2;
    deps.timeController = m_mw.m_timeController;
    deps.evalChart = m_mw.m_evalChart;
    deps.evalGraphController = m_mw.m_evalGraphController;
    deps.recordPane = m_mw.m_recordPane;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.activePly = &m_mw.m_kifu.activePly;
    deps.currentSelectedPly = &m_mw.m_kifu.currentSelectedPly;
    deps.currentMoveIndex = &m_mw.m_state.currentMoveIndex;
    deps.liveGameSession = m_mw.m_branchNav.liveGameSession;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;

    m_mw.m_preStartCleanupHandler = new PreStartCleanupHandler(deps, &m_mw);

    qCDebug(lcApp).noquote() << "ensurePreStartCleanupHandler_: created and connected";
}

// ---------------------------------------------------------------------------
// 同期 / 盤面
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureTurnSyncBridge()
{
    auto* gc = m_mw.m_gameController;
    auto* tm = m_mw.findChild<TurnManager*>("TurnManager");
    if (!gc || !tm) return;

    TurnSyncBridge::wire(gc, tm, &m_mw);
}

void MainWindowServiceRegistry::ensurePositionEditController()
{
    if (m_mw.m_posEdit) return;
    m_mw.m_posEdit = new PositionEditController(&m_mw);
}

void MainWindowServiceRegistry::ensureBoardSyncPresenter()
{
    if (m_mw.m_boardSync) {
        // sfenRecord ポインタが変わっている場合は更新する
        const QStringList* current = m_mw.m_queryService->sfenRecord();
        m_mw.m_boardSync->setSfenRecord(current);
        return;
    }

    qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: creating BoardSyncPresenter"
                       << "sfenRecord()*=" << static_cast<const void*>(m_mw.m_queryService->sfenRecord())
                       << "sfenRecord().size=" << (m_mw.m_queryService->sfenRecord() ? m_mw.m_queryService->sfenRecord()->size() : -1);

    BoardSyncPresenter::Deps d;
    d.gc         = m_mw.m_gameController;
    d.view       = m_mw.m_shogiView;
    d.bic        = m_mw.m_boardController;
    d.sfenRecord = m_mw.m_queryService->sfenRecord();
    d.gameMoves  = &m_mw.m_kifu.gameMoves;

    m_mw.m_boardSync = new BoardSyncPresenter(d, &m_mw);

    qCDebug(lcApp).noquote() << "ensureBoardSyncPresenter: created m_boardSync*=" << static_cast<const void*>(m_mw.m_boardSync);
}

void MainWindowServiceRegistry::ensureBoardLoadService()
{
    if (!m_mw.m_boardLoadService) {
        m_mw.m_boardLoadService = new BoardLoadService(&m_mw);
    }

    ensureBoardSyncPresenter();

    BoardLoadService::Deps deps;
    deps.gc = m_mw.m_gameController;
    deps.view = m_mw.m_shogiView;
    deps.boardSync = m_mw.m_boardSync;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    deps.ensureBoardSyncPresenter = std::bind(&MainWindowServiceRegistry::ensureBoardSyncPresenter, this);
    deps.beginBranchNavGuard = [this]() {
        ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->beginBranchNavGuard();
    };
    m_mw.m_boardLoadService->updateDeps(deps);
}

void MainWindowServiceRegistry::ensureConsiderationPositionService()
{
    if (!m_mw.m_considerationPositionService) {
        m_mw.m_considerationPositionService = new ConsiderationPositionService(&m_mw);
    }

    ConsiderationPositionService::Deps deps;
    deps.playMode = &m_mw.m_state.playMode;
    deps.positionStrList = &m_mw.m_kifu.positionStrList;
    deps.gameUsiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.kifuLoadCoordinator = m_mw.m_kifuLoadCoordinator;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;
    deps.match = m_mw.m_match;
    deps.analysisTab = m_mw.m_analysisTab;
    m_mw.m_considerationPositionService->updateDeps(deps);
}

void MainWindowServiceRegistry::ensureAnalysisPresenter()
{
    if (!m_mw.m_analysisPresenter)
        m_mw.m_analysisPresenter = new AnalysisResultsPresenter(&m_mw);
}

// ---------------------------------------------------------------------------
// 対局開始
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureGameStartCoordinator()
{
    if (m_mw.m_gameStart) return;

    ensureMatchCoordinatorWiring();
    m_mw.m_matchWiring->ensureMenuGameStartCoordinator();
    m_mw.m_gameStart = m_mw.m_matchWiring->menuGameStartCoordinator();
}

// ---------------------------------------------------------------------------
// 棋譜表示
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureRecordPresenter()
{
    if (m_mw.m_recordPresenter) return;

    GameRecordPresenter::Deps d;
    d.model      = m_mw.m_models.kifuRecord;
    d.recordPane = m_mw.m_recordPane;

    m_mw.m_recordPresenter = new GameRecordPresenter(d, &m_mw);

    ensureCommentCoordinator();
    QObject::connect(
        m_mw.m_recordPresenter,
        &GameRecordPresenter::currentRowChanged,
        m_mw.m_commentCoordinator,
        &CommentCoordinator::handleRecordRowChangeRequest,
        Qt::UniqueConnection
        );
}

void MainWindowServiceRegistry::ensureLiveGameSessionStarted()
{
    ensureLiveGameSessionUpdater();
    m_mw.m_liveGameSessionUpdater->ensureSessionStarted();
}

void MainWindowServiceRegistry::ensureLiveGameSessionUpdater()
{
    if (!m_mw.m_liveGameSessionUpdater) {
        m_mw.m_liveGameSessionUpdater = std::make_unique<LiveGameSessionUpdater>();
    }

    LiveGameSessionUpdater::Deps deps;
    deps.liveSession = m_mw.m_branchNav.liveGameSession;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.gameController = m_mw.m_gameController;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    m_mw.m_liveGameSessionUpdater->updateDeps(deps);
}

void MainWindowServiceRegistry::ensureGameRecordUpdateService()
{
    if (!m_mw.m_gameRecordUpdateService) {
        m_mw.m_gameRecordUpdateService = new GameRecordUpdateService(&m_mw);
    }

    GameRecordUpdateService::Deps deps;
    deps.ensureRecordPresenter = [this]() -> GameRecordPresenter* {
        ensureRecordPresenter();
        return m_mw.m_recordPresenter;
    };
    deps.ensureLiveGameSessionUpdater = [this]() -> LiveGameSessionUpdater* {
        ensureLiveGameSessionUpdater();
        return m_mw.m_liveGameSessionUpdater.get();
    };
    deps.match = m_mw.m_match;
    deps.liveGameSession = m_mw.m_branchNav.liveGameSession;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.gameUsiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    m_mw.m_gameRecordUpdateService->updateDeps(deps);
}

void MainWindowServiceRegistry::ensureUndoFlowService()
{
    if (!m_mw.m_undoFlowService) {
        m_mw.m_undoFlowService = std::make_unique<UndoFlowService>();
    }

    UndoFlowService::Deps deps;
    deps.match = m_mw.m_match;
    deps.evalGraphController = m_mw.m_evalGraphController;
    deps.playMode = &m_mw.m_state.playMode;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    m_mw.m_undoFlowService->updateDeps(deps);
}

void MainWindowServiceRegistry::ensureGameRecordLoadService()
{
    if (!m_mw.m_gameRecordLoadService) {
        m_mw.m_gameRecordLoadService = std::make_unique<GameRecordLoadService>();
    }

    GameRecordLoadService::Deps deps;
    deps.gameUsiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.commentsByRow = &m_mw.m_kifu.commentsByRow;
    deps.recordPane = m_mw.m_recordPane;
    deps.ensureRecordPresenter = [this]() -> GameRecordPresenter* {
        ensureRecordPresenter();
        return m_mw.m_recordPresenter;
    };
    deps.ensureGameRecordModel = [this]() -> GameRecordModel* {
        ensureGameRecordModel();
        return m_mw.m_models.gameRecord;
    };
    deps.sfenRecord = [this]() -> const QStringList* {
        return m_mw.m_queryService->sfenRecord();
    };
    m_mw.m_gameRecordLoadService->updateDeps(deps);
}

void MainWindowServiceRegistry::ensureTurnStateSyncService()
{
    if (!m_mw.m_turnStateSync) {
        m_mw.m_turnStateSync = std::make_unique<TurnStateSyncService>();
    }

    TurnStateSyncService::Deps deps;
    deps.gameController = m_mw.m_gameController;
    deps.shogiView = m_mw.m_shogiView;
    deps.timeController = m_mw.m_timeController;
    deps.timePresenter = m_mw.m_timePresenter;
    deps.playMode = &m_mw.m_state.playMode;
    deps.turnManagerParent = &m_mw;
    deps.updateTurnStatus = std::bind(&MainWindowServiceRegistry::updateTurnStatus, this, std::placeholders::_1);
    deps.onTurnManagerCreated = [this](TurnManager* tm) {
        connect(tm, &TurnManager::changed,
                &m_mw, &MainWindow::onTurnManagerChanged,
                Qt::UniqueConnection);
    };
    m_mw.m_turnStateSync->updateDeps(deps);
}

void MainWindowServiceRegistry::ensureKifuLoadCoordinatorForLive()
{
    if (m_mw.m_kifuLoadCoordinator) {
        return;
    }

    KifuLoadCoordinatorFactory::createAndWire(m_mw);
}

void MainWindowServiceRegistry::ensureGameRecordModel()
{
    if (m_mw.m_models.gameRecord) return;

    ensureCommentCoordinator();

    GameRecordModelBuilder::Deps deps;
    deps.parent = &m_mw;
    deps.recordPresenter = m_mw.m_recordPresenter;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;
    deps.commentCoordinator = m_mw.m_commentCoordinator;
    deps.kifuRecordModel = m_mw.m_models.kifuRecord;

    m_mw.m_models.gameRecord = GameRecordModelBuilder::build(deps);
}

// ---------------------------------------------------------------------------
// 補助コントローラ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureJishogiController()
{
    if (m_mw.m_jishogiController) return;
    m_mw.m_jishogiController = new JishogiScoreDialogController(&m_mw);
}

void MainWindowServiceRegistry::ensureNyugyokuHandler()
{
    if (m_mw.m_nyugyokuHandler) return;
    m_mw.m_nyugyokuHandler = new NyugyokuDeclarationHandler(&m_mw);
}

void MainWindowServiceRegistry::ensureConsecutiveGamesController()
{
    if (m_mw.m_consecutiveGamesController) return;

    m_mw.m_consecutiveGamesController = new ConsecutiveGamesController(&m_mw);
    m_mw.m_consecutiveGamesController->setTimeController(m_mw.m_timeController);
    m_mw.m_consecutiveGamesController->setGameStartCoordinator(m_mw.m_gameStart);

    ensureGameSessionOrchestrator();
    connect(m_mw.m_consecutiveGamesController, &ConsecutiveGamesController::requestPreStartCleanup,
            m_mw.m_gameSessionOrchestrator, &GameSessionOrchestrator::onPreStartCleanupRequested);
    connect(m_mw.m_consecutiveGamesController, &ConsecutiveGamesController::requestStartNextGame,
            m_mw.m_gameSessionOrchestrator, &GameSessionOrchestrator::onConsecutiveStartRequested);
}

void MainWindowServiceRegistry::ensureLanguageController()
{
    if (m_mw.m_languageController) return;

    m_mw.m_languageController = new LanguageController(&m_mw);
    m_mw.m_languageController->setParentWidget(&m_mw);
    m_mw.m_languageController->setActions(
        m_mw.ui->actionLanguageSystem,
        m_mw.ui->actionLanguageJapanese,
        m_mw.ui->actionLanguageEnglish);
}

void MainWindowServiceRegistry::ensureConsiderationWiring()
{
    if (m_mw.m_considerationWiring) return;

    MainWindowDepsFactory::ConsiderationWiringCallbacks cbs;
    cbs.ensureDialogCoordinator = [this]() {
        ensureDialogCoordinator();
        if (m_mw.m_considerationWiring)
            m_mw.m_considerationWiring->setDialogCoordinator(m_mw.m_dialogCoordinator);
    };

    m_mw.m_compositionRoot->ensureConsiderationWiring(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_considerationWiring);
}

// ---------------------------------------------------------------------------
// ドック / UI
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureDockLayoutManager()
{
    if (m_mw.m_dockLayoutManager) return;

    m_mw.m_dockLayoutManager = new DockLayoutManager(&m_mw, &m_mw);

    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Menu, m_mw.m_docks.menuWindow);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Joseki, m_mw.m_docks.josekiWindow);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Record, m_mw.m_docks.recordPane);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::GameInfo, m_mw.m_docks.gameInfo);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Thinking, m_mw.m_docks.thinking);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Consideration, m_mw.m_docks.consideration);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::UsiLog, m_mw.m_docks.usiLog);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::CsaLog, m_mw.m_docks.csaLog);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::Comment, m_mw.m_docks.comment);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::BranchTree, m_mw.m_docks.branchTree);
    m_mw.m_dockLayoutManager->registerDock(DockLayoutManager::DockType::EvalChart, m_mw.m_docks.evalChart);

    m_mw.m_dockLayoutManager->setSavedLayoutsMenu(m_mw.ui->menuSavedLayouts);
}

void MainWindowServiceRegistry::ensureDockCreationService()
{
    if (m_mw.m_dockCreationService) return;

    m_mw.m_dockCreationService = new DockCreationService(&m_mw, &m_mw);
    m_mw.m_dockCreationService->setDisplayMenu(m_mw.ui->Display);
}

void MainWindowServiceRegistry::ensureCommentCoordinator()
{
    if (m_mw.m_commentCoordinator) return;
    m_mw.m_compositionRoot->ensureCommentCoordinator(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_commentCoordinator);
    connect(m_mw.m_commentCoordinator, &CommentCoordinator::ensureGameRecordModelRequested,
            &m_mw, &MainWindow::ensureGameRecordModel);
}

void MainWindowServiceRegistry::ensureUsiCommandController()
{
    if (!m_mw.m_usiCommandController) {
        m_mw.m_usiCommandController = new UsiCommandController(&m_mw);
    }
    m_mw.m_usiCommandController->setMatchCoordinator(m_mw.m_match);
    m_mw.m_usiCommandController->setAnalysisTab(m_mw.m_analysisTab);
}

void MainWindowServiceRegistry::ensureRecordNavigationHandler()
{
    auto refs = m_mw.buildRuntimeRefs();
    m_mw.m_compositionRoot->ensureRecordNavigationWiring(refs, &m_mw, m_mw.m_recordNavWiring);
}

void MainWindowServiceRegistry::ensureUiStatePolicyManager()
{
    m_mw.m_compositionRoot->ensureUiStatePolicyManager(m_mw.buildRuntimeRefs(), &m_mw, m_mw.m_uiStatePolicy);
}

// ---------------------------------------------------------------------------
// ナビゲーション / セッション
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureKifuNavigationCoordinator()
{
    if (!m_mw.m_kifuNavCoordinator) {
        m_mw.m_kifuNavCoordinator = new KifuNavigationCoordinator(&m_mw);
    }

    KifuNavigationDepsFactory::Callbacks callbacks;
    callbacks.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    callbacks.updateJosekiWindow = std::bind(&MainWindowServiceRegistry::updateJosekiWindow, this);
    callbacks.ensureBoardSyncPresenter = std::bind(&MainWindowServiceRegistry::ensureBoardSyncPresenter, this);
    callbacks.isGameActivelyInProgress = std::bind(&MatchRuntimeQueryService::isGameActivelyInProgress, m_mw.m_queryService);
    callbacks.getSfenRecord = [this]() -> QStringList* { return m_mw.m_queryService->sfenRecord(); };

    m_mw.m_kifuNavCoordinator->updateDeps(
        KifuNavigationDepsFactory::createDeps(m_mw.buildRuntimeRefs(), callbacks));
}

void MainWindowServiceRegistry::ensureSessionLifecycleCoordinator()
{
    if (!m_mw.m_sessionLifecycle) {
        m_mw.m_sessionLifecycle = new SessionLifecycleCoordinator(&m_mw);
    }

    SessionLifecycleDepsFactory::Callbacks callbacks;
    callbacks.clearGameStateFields = std::bind(&MainWindowServiceRegistry::clearGameStateFields, this);
    callbacks.resetEngineState = std::bind(&MainWindowServiceRegistry::resetEngineState, this);
    ensureGameSessionOrchestrator();
    callbacks.onPreStartCleanupRequested = std::bind(&GameSessionOrchestrator::onPreStartCleanupRequested,
                                                     m_mw.m_gameSessionOrchestrator);
    callbacks.resetModels = std::bind(&MainWindowServiceRegistry::resetModels, this, std::placeholders::_1);
    callbacks.resetUiState = std::bind(&MainWindowServiceRegistry::resetUiState, this, std::placeholders::_1);
    callbacks.clearEvalState = std::bind(&MainWindowServiceRegistry::clearEvalState, this);
    callbacks.unlockGameOverStyle = std::bind(&MainWindowServiceRegistry::unlockGameOverStyle, this);
    callbacks.startGame = std::bind(&GameSessionOrchestrator::invokeStartGame,
                                    m_mw.m_gameSessionOrchestrator);
    callbacks.updateEndTime = [this](const QDateTime& endTime) {
        ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->updateGameInfoWithEndTime(endTime);
        }
    };
    callbacks.startNextConsecutiveGame = std::bind(&GameSessionOrchestrator::startNextConsecutiveGame,
                                                    m_mw.m_gameSessionOrchestrator);
    callbacks.lastTimeControl = &m_mw.m_lastTimeControl;
    callbacks.updateGameInfoWithTimeControl = [this](bool enabled, qint64 baseMs, qint64 byoyomiMs, qint64 incMs) {
        ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->updateGameInfoWithTimeControl(enabled, baseMs, byoyomiMs, incMs);
        }
    };

    m_mw.m_sessionLifecycle->updateDeps(
        SessionLifecycleDepsFactory::createDeps(m_mw.buildRuntimeRefs(), callbacks));
}

// ---------------------------------------------------------------------------
// 通知サービス
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureUiNotificationService()
{
    if (m_mw.m_notificationService) return;

    m_mw.m_notificationService = new UiNotificationService(&m_mw);

    UiNotificationService::Deps deps;
    deps.errorOccurred = &m_mw.m_state.errorOccurred;
    deps.shogiView = m_mw.m_shogiView;
    deps.parentWidget = &m_mw;
    m_mw.m_notificationService->updateDeps(deps);
}

void MainWindowServiceRegistry::ensureGameSessionOrchestrator()
{
    if (m_mw.m_gameSessionOrchestrator) {
        // Deps を最新に更新して返す
    } else {
        m_mw.m_gameSessionOrchestrator = new GameSessionOrchestrator(&m_mw);
    }

    GameSessionOrchestrator::Deps deps;

    // === Controllers（ダブルポインタ） ===
    deps.gameStateController = &m_mw.m_gameStateController;
    deps.sessionLifecycle = &m_mw.m_sessionLifecycle;
    deps.consecutiveGamesController = &m_mw.m_consecutiveGamesController;
    deps.gameStart = &m_mw.m_gameStart;
    deps.preStartCleanupHandler = &m_mw.m_preStartCleanupHandler;
    deps.dialogCoordinator = &m_mw.m_dialogCoordinator;
    deps.replayController = &m_mw.m_replayController;
    deps.timeController = &m_mw.m_timeController;
    deps.csaGameCoordinator = &m_mw.m_csaGameCoordinator;
    deps.match = &m_mw.m_match;

    // === Core objects ===
    deps.shogiView = m_mw.m_shogiView;
    deps.gameController = m_mw.m_gameController;
    deps.kifuLoadCoordinator = m_mw.m_kifuLoadCoordinator;
    deps.kifuModel = m_mw.m_models.kifuRecord;

    // === State pointers ===
    deps.playMode = &m_mw.m_state.playMode;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.currentSelectedPly = &m_mw.m_kifu.currentSelectedPly;
    deps.bottomIsP1 = &m_mw.m_player.bottomIsP1;
    deps.lastTimeControl = &m_mw.m_lastTimeControl;

    // === Branch navigation ===
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;
    deps.liveGameSession = &m_mw.m_branchNav.liveGameSession;

    // === Lazy-init callbacks ===
    deps.ensureGameStateController = std::bind(&MainWindowServiceRegistry::ensureGameStateController, this);
    deps.ensureSessionLifecycleCoordinator = std::bind(&MainWindowServiceRegistry::ensureSessionLifecycleCoordinator, this);
    deps.ensureConsecutiveGamesController = std::bind(&MainWindowServiceRegistry::ensureConsecutiveGamesController, this);
    deps.ensureGameStartCoordinator = std::bind(&MainWindowServiceRegistry::ensureGameStartCoordinator, this);
    deps.ensurePreStartCleanupHandler = std::bind(&MainWindowServiceRegistry::ensurePreStartCleanupHandler, this);
    deps.ensureDialogCoordinator = std::bind(&MainWindowServiceRegistry::ensureDialogCoordinator, this);
    deps.ensureReplayController = std::bind(&MainWindowServiceRegistry::ensureReplayController, this);

    // === Action callbacks ===
    deps.initMatchCoordinator = std::bind(&MainWindowServiceRegistry::initMatchCoordinator, this);
    deps.clearSessionDependentUi = std::bind(&MainWindowServiceRegistry::clearSessionDependentUi, this);
    deps.updateJosekiWindow = std::bind(&MainWindowServiceRegistry::updateJosekiWindow, this);
    deps.sfenRecord = [this]() -> QStringList* { return m_mw.m_queryService->sfenRecord(); };

    m_mw.m_gameSessionOrchestrator->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// コア初期化（MainWindow から移譲）
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureCoreInitCoordinator()
{
    if (!m_mw.m_coreInit) {
        m_mw.m_coreInit = std::make_unique<MainWindowCoreInitCoordinator>();
    }

    MainWindowCoreInitCoordinator::Deps deps;
    deps.gameController = &m_mw.m_gameController;
    deps.shogiView = &m_mw.m_shogiView;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.resumeSfenStr = &m_mw.m_state.resumeSfenStr;
    deps.moveRecords = &m_mw.m_kifu.moveRecords;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.gameUsiMoves = &m_mw.m_kifu.gameUsiMoves;
    deps.parent = &m_mw;
    deps.setupBoardInteractionController = std::bind(&MainWindowServiceRegistry::setupBoardInteractionController, this);
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    deps.ensureTurnSyncBridge = std::bind(&MainWindowServiceRegistry::ensureTurnSyncBridge, this);
    deps.ensurePlayerInfoWiringAndApply = [this]() {
        ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->applyPlayersNamesForMode();
            m_mw.m_playerInfoWiring->applyEngineNamesToLogModels();
            m_mw.m_playerInfoWiring->applySecondEngineVisibility();
        }
    };
    m_mw.m_coreInit->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// セッションリセット / UI クリア（MainWindow から移譲）
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::clearGameStateFields()
{
    m_mw.m_state.resumeSfenStr.clear();
    m_mw.m_state.errorOccurred = false;

    m_mw.m_player.humanName1.clear();
    m_mw.m_player.humanName2.clear();
    m_mw.m_player.engineName1.clear();
    m_mw.m_player.engineName2.clear();

    m_mw.m_kifu.positionStrList.clear();

    m_mw.m_player.lastP1Turn = true;
    m_mw.m_player.lastP1Ms = 0;
    m_mw.m_player.lastP2Ms = 0;

    m_mw.m_kifu.commentsByRow.clear();
    m_mw.m_kifu.saveFileName.clear();

    m_mw.m_state.skipBoardSyncForBranchNav = false;
    m_mw.m_kifu.onMainRowGuard = false;

    m_mw.m_kifu.gameUsiMoves.clear();
    m_mw.m_kifu.gameMoves.clear();
}

void MainWindowServiceRegistry::resetEngineState()
{
    if (m_mw.m_match) {
        m_mw.m_match->stopAnalysisEngine();
        m_mw.m_match->clearGameOverState();
    }
    if (m_mw.m_csaGameCoordinator) {
        m_mw.m_csaGameCoordinator->stopGame();
    }
    if (m_mw.m_consecutiveGamesController) {
        m_mw.m_consecutiveGamesController->reset();
    }
}

void MainWindowServiceRegistry::clearEvalState()
{
    if (m_mw.m_evalChart) {
        m_mw.m_evalChart->clearAll();
    }
    ensureEvaluationGraphController();
    if (m_mw.m_evalGraphController) {
        m_mw.m_evalGraphController->clearScores();
    }
    if (m_mw.m_recordPresenter) {
        m_mw.m_recordPresenter->clearLiveDisp();
    }
}

void MainWindowServiceRegistry::unlockGameOverStyle()
{
    if (m_mw.m_shogiView) {
        m_mw.m_shogiView->setGameOverStyleLock(false);
    }
}

void MainWindowServiceRegistry::updateTurnStatus(int currentPlayer)
{
    if (!m_mw.m_shogiView) return;

    ensureTimeController();
    ShogiClock* clock = m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr;
    if (!clock) {
        qCWarning(lcApp) << "ShogiClock not ready yet";
        return;
    }

    clock->setCurrentPlayer(currentPlayer);
    m_mw.m_shogiView->setActiveSide(currentPlayer == 1);
}

void MainWindowServiceRegistry::resetModels(const QString& hirateStartSfen)
{
    m_mw.m_kifu.moveRecords.clear();

    MainWindowResetService::ModelResetDeps deps;
    deps.navState = m_mw.m_branchNav.navState;
    deps.recordPresenter = m_mw.m_recordPresenter;
    deps.displayCoordinator = m_mw.m_branchNav.displayCoordinator;
    deps.boardController = m_mw.m_boardController;
    deps.gameRecord = m_mw.m_models.gameRecord;
    deps.evalGraphController = m_mw.m_evalGraphController;
    deps.timeController = m_mw.m_timeController;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    deps.gameInfoController = m_mw.m_gameInfoController;
    deps.kifuLoadCoordinator = m_mw.m_kifuLoadCoordinator;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.liveGameSession = m_mw.m_branchNav.liveGameSession;

    const MainWindowResetService resetService;
    resetService.resetModels(deps, hirateStartSfen);
}

void MainWindowServiceRegistry::resetUiState(const QString& hirateStartSfen)
{
    MainWindowResetService::UiResetDeps deps;
    deps.gameController = m_mw.m_gameController;
    deps.shogiView = m_mw.m_shogiView;
    deps.uiStatePolicy = m_mw.m_uiStatePolicy;
    deps.updateJosekiWindow = [this]() {
        updateJosekiWindow();
    };

    const MainWindowResetService resetService;
    resetService.resetUiState(deps, hirateStartSfen);
}

void MainWindowServiceRegistry::clearSessionDependentUi()
{
    ensureCommentCoordinator();

    MainWindowResetService::SessionUiDeps deps;
    deps.commLog1 = m_mw.m_models.commLog1;
    deps.commLog2 = m_mw.m_models.commLog2;
    deps.thinking1 = m_mw.m_models.thinking1;
    deps.thinking2 = m_mw.m_models.thinking2;
    deps.consideration = m_mw.m_models.consideration;
    deps.analysisTab = m_mw.m_analysisTab;
    deps.analysisModel = m_mw.m_models.analysis;
    deps.evalChart = m_mw.m_evalChart;
    deps.evalGraphController = m_mw.m_evalGraphController;
    deps.broadcastComment = std::bind(&CommentCoordinator::broadcastComment,
                                       m_mw.m_commentCoordinator,
                                       std::placeholders::_1, std::placeholders::_2);

    const MainWindowResetService resetService;
    resetService.clearSessionDependentUi(deps);
}

void MainWindowServiceRegistry::clearUiBeforeKifuLoad()
{
    ensureCommentCoordinator();

    MainWindowResetService::SessionUiDeps deps;
    deps.commLog1 = m_mw.m_models.commLog1;
    deps.commLog2 = m_mw.m_models.commLog2;
    deps.thinking1 = m_mw.m_models.thinking1;
    deps.thinking2 = m_mw.m_models.thinking2;
    deps.consideration = m_mw.m_models.consideration;
    deps.analysisTab = m_mw.m_analysisTab;
    deps.analysisModel = m_mw.m_models.analysis;
    deps.evalChart = m_mw.m_evalChart;
    deps.evalGraphController = m_mw.m_evalGraphController;
    deps.broadcastComment = std::bind(&CommentCoordinator::broadcastComment,
                                       m_mw.m_commentCoordinator,
                                       std::placeholders::_1, std::placeholders::_2);

    const MainWindowResetService resetService;
    resetService.clearUiBeforeKifuLoad(deps);
}

// ---------------------------------------------------------------------------
// 盤面操作 / 対局初期化（MainWindow から移譲）
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::setupBoardInteractionController()
{
    ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->setupBoardInteractionController();
        m_mw.m_boardController = m_mw.m_boardSetupController->boardController();

        if (m_mw.m_boardController) {
            m_mw.m_boardController->setIsHumanTurnCallback([this]() -> bool {
                return m_mw.m_queryService->isHumanTurnNow();
            });
        }
    }
}

void MainWindowServiceRegistry::initMatchCoordinator()
{
    if (!m_mw.m_gameController || !m_mw.m_shogiView) return;

    ensureMatchCoordinatorWiring();

    if (!m_mw.m_matchWiring->initializeSession(
            std::bind(&MainWindowServiceRegistry::ensureMatchCoordinatorWiring, this))) {
        return;
    }

    m_mw.m_match = m_mw.m_matchWiring->match();
    m_mw.m_gameStartCoordinator = m_mw.m_matchWiring->gameStartCoordinator();
}

void MainWindowServiceRegistry::createMenuWindowDock()
{
    MainWindowDockBootstrapper dockBoot(m_mw);
    dockBoot.createMenuWindowDock();
}

// ---------------------------------------------------------------------------
// スロット転送先（MainWindow から移譲）
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::handleMoveRequested(const QPoint& from, const QPoint& to)
{
    qCDebug(lcApp).noquote() << "onMoveRequested_: from=" << from << " to=" << to
                       << " m_state.playMode=" << static_cast<int>(m_mw.m_state.playMode)
                       << " m_match=" << (m_mw.m_match ? "valid" : "null")
                       << " matchMode=" << (m_mw.m_match ? static_cast<int>(m_mw.m_match->playMode()) : -1);

    ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->setPlayMode(m_mw.m_state.playMode);
        m_mw.m_boardSetupController->setMatchCoordinator(m_mw.m_match);
        m_mw.m_boardSetupController->setSfenRecord(m_mw.m_queryService->sfenRecord());
        m_mw.m_boardSetupController->setPositionEditController(m_mw.m_posEdit);
        m_mw.m_boardSetupController->setTimeController(m_mw.m_timeController);
        m_mw.m_boardSetupController->onMoveRequested(from, to);
    }
}

void MainWindowServiceRegistry::handleMoveCommitted(int mover, int ply)
{
    ensureBoardSetupController();
    if (m_mw.m_boardSetupController) {
        m_mw.m_boardSetupController->setPlayMode(m_mw.m_state.playMode);
        m_mw.m_boardSetupController->onMoveCommitted(
            static_cast<ShogiGameController::Player>(mover), ply);
    }

    ensureGameRecordUpdateService();
    m_mw.m_gameRecordUpdateService->recordUsiMoveAndUpdateSfen();

    updateJosekiWindow();
}

void MainWindowServiceRegistry::handleBeginPositionEditing()
{
    ensurePositionEditCoordinator();
    if (m_mw.m_posEditCoordinator) {
        m_mw.m_posEditCoordinator->setPositionEditController(m_mw.m_posEdit);
        m_mw.m_posEditCoordinator->setBoardController(m_mw.m_boardController);
        m_mw.m_posEditCoordinator->setMatchCoordinator(m_mw.m_match);
        m_mw.m_posEditCoordinator->beginPositionEditing();
    }
}

void MainWindowServiceRegistry::handleFinishPositionEditing()
{
    ensurePositionEditCoordinator();
    if (m_mw.m_posEditCoordinator) {
        m_mw.m_posEditCoordinator->setPositionEditController(m_mw.m_posEdit);
        m_mw.m_posEditCoordinator->setBoardController(m_mw.m_boardController);
        m_mw.m_posEditCoordinator->finishPositionEditing();
    }
}

void MainWindowServiceRegistry::updateJosekiWindow()
{
    if (m_mw.m_docks.josekiWindow && !m_mw.m_docks.josekiWindow->isVisible()) {
        return;
    }
    if (m_mw.m_josekiWiring) {
        m_mw.m_josekiWiring->updateJosekiWindow();
    }
}
