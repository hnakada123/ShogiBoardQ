/// @file gamewiringsubregistry.cpp
/// @brief Game系配線管理（MC配線・CSA配線・連続対局・ターン同期ブリッジ・MC初期化）の ensure* 実装

#include "gamewiringsubregistry.h"
#include "gamesessionsubregistry.h"
#include "gamesubregistry.h"
#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowmatchadapter.h"
#include "mainwindowmatchwiringdepsservice.h"
#include "mainwindowserviceregistry.h"
#include "mainwindowsignalrouter.h"
#include "mainwindowcoreinitcoordinator.h"
#include "ui_mainwindow.h"

#include "boardinteractioncontroller.h"
#include "consecutivegamescontroller.h"
#include "csagamewiring.h"
#include "gamerecordupdateservice.h"
#include "gamesessionorchestrator.h"
#include "kifufilecontroller.h"
#include "kifunavigationcoordinator.h"
#include "matchruntimequeryservice.h"
#include "playerinfowiring.h"
#include "timecontrolcontroller.h"
#include "turnmanager.h"
#include "turnsyncbridge.h"
#include "logcategories.h"

GameWiringSubRegistry::GameWiringSubRegistry(MainWindow& mw,
                                             GameSubRegistry* gameReg,
                                             MainWindowServiceRegistry* registry,
                                             MainWindowFoundationRegistry* foundation,
                                             QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_gameReg(gameReg)
    , m_registry(registry)
    , m_foundation(foundation)
{
}

// ---------------------------------------------------------------------------
// MatchCoordinator 配線
// ---------------------------------------------------------------------------

void GameWiringSubRegistry::ensureMatchCoordinatorWiring()
{
    if (!m_mw.m_matchWiring) {
        createMatchCoordinatorWiring();
    }
    refreshMatchWiringDeps();
}

void GameWiringSubRegistry::createMatchCoordinatorWiring()
{
    m_mw.m_matchWiring = std::make_unique<MatchCoordinatorWiring>();

    // 初回のみ: 転送シグナルを GameSessionOrchestrator / 各コントローラに接続
    m_gameReg->session()->ensureGameSessionOrchestrator();
    m_foundation->ensurePlayerInfoWiring();
    m_foundation->ensureKifuNavigationCoordinator();
    m_registry->kifu()->ensureBranchNavigationWiring();

    MatchCoordinatorWiring::ForwardingTargets targets;
    targets.appearance = m_mw.m_appearanceController.get();
    targets.playerInfo = m_mw.m_playerInfoWiring;
    targets.kifuNav = m_mw.m_kifuNavCoordinator.get();
    targets.branchNav = m_mw.m_branchNavWiring.get();
    m_mw.m_matchWiring->wireForwardingSignals(targets, m_mw.m_gameSessionOrchestrator);
}

void GameWiringSubRegistry::refreshMatchWiringDeps()
{
    if (!m_mw.m_matchWiring) return;

    m_foundation->ensureEvaluationGraphController();
    m_foundation->ensurePlayerInfoWiring();

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
        m_gameReg->session()->ensureCoreInitCoordinator();
        ad.initializeNewGame = std::bind(&MainWindowCoreInitCoordinator::initializeNewGame,
                                         m_mw.m_coreInit.get(), std::placeholders::_1);
        ad.ensureAndGetPlayerInfoWiring = [this]() -> PlayerInfoWiring* {
            m_foundation->ensurePlayerInfoWiring();
            return m_mw.m_playerInfoWiring;
        };
        ad.ensureAndGetDialogCoordinator = [this]() -> DialogCoordinator* {
            m_registry->ensureDialogCoordinator();
            return m_mw.m_dialogCoordinator;
        };
        ad.ensureAndGetTurnStateSync = [this]() -> TurnStateSyncService* {
            m_gameReg->ensureTurnStateSyncService();
            return m_mw.m_turnStateSync.get();
        };
        m_mw.m_matchAdapter->updateDeps(ad);
    }

    auto deps = buildMatchWiringDeps();
    m_mw.m_matchWiring->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// MatchCoordinatorWiring::Deps 構築
// ---------------------------------------------------------------------------

MatchCoordinatorWiring::Deps GameWiringSubRegistry::buildMatchWiringDeps()
{
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;

    MainWindowMatchWiringDepsService::Inputs in;
    in.evalGraphController = m_mw.m_evalGraphController.get();
    in.onTurnChanged = std::bind(&MainWindow::onTurnManagerChanged, &m_mw, _1);
    in.sendGo = [this](Usi* which, const MatchCoordinator::GoTimes& t) {
        if (m_mw.m_match) m_mw.m_match->sendGoToEngine(which, t);
    };
    in.sendStop = [this](Usi* which) {
        if (m_mw.m_match) m_mw.m_match->sendStopToEngine(which);
    };
    in.sendRaw = [this](Usi* which, const QString& cmd) {
        if (m_mw.m_match) m_mw.m_match->sendRawToEngine(which, cmd);
    };
    auto* adapter = m_mw.m_matchAdapter.get();
    in.initializeNewGame = std::bind(&MainWindowMatchAdapter::initializeNewGameHook, adapter, _1);
    in.renderBoard = std::bind(&MainWindowMatchAdapter::renderBoardFromGc, adapter);
    in.showMoveHighlights = std::bind(&MainWindowMatchAdapter::showMoveHighlights, adapter, _1, _2);
    in.appendKifuLine = [this](const QString& text, const QString& elapsed) {
        m_registry->kifu()->ensureGameRecordUpdateService();
        if (m_mw.m_gameRecordUpdateService) {
            m_mw.m_gameRecordUpdateService->appendKifuLine(text, elapsed);
        }
    };
    in.showGameOverDialog = std::bind(&MainWindowMatchAdapter::showGameOverMessageBox, adapter, _1, _2);
    in.remainingMsFor = std::bind(&MatchRuntimeQueryService::getRemainingMsFor, m_mw.m_queryService.get(), _1);
    in.incrementMsFor = std::bind(&MatchRuntimeQueryService::getIncrementMsFor, m_mw.m_queryService.get(), _1);
    in.byoyomiMs = std::bind(&MatchRuntimeQueryService::getByoyomiMs, m_mw.m_queryService.get());
    in.setPlayersNames = std::bind(&PlayerInfoWiring::onSetPlayersNames, m_mw.m_playerInfoWiring, _1, _2);
    in.setEngineNames = std::bind(&PlayerInfoWiring::onSetEngineNames, m_mw.m_playerInfoWiring, _1, _2);
    m_registry->kifu()->ensureKifuFileController();
    in.autoSaveKifu = std::bind(&KifuFileController::autoSaveKifuToFile, m_mw.m_kifuFileController, _1, _2, _3, _4, _5, _6);

    m_foundation->ensureKifuNavigationCoordinator();
    in.updateHighlightsForPly = std::bind(&KifuNavigationCoordinator::syncBoardAndHighlightsAtRow, m_mw.m_kifuNavCoordinator.get(), _1);
    in.updateTurnAndTimekeepingDisplay = std::bind(&MainWindowMatchAdapter::updateTurnAndTimekeepingDisplay, adapter);
    in.isHumanSide = std::bind(&MatchRuntimeQueryService::isHumanSide, m_mw.m_queryService.get(), _1);
    in.setMouseClickMode = [this](bool mode) {
        if (m_mw.m_shogiView) m_mw.m_shogiView->setMouseClickMode(mode);
    };

    in.gc    = m_mw.m_gameController;
    in.view  = m_mw.m_shogiView;
    in.usi1  = m_mw.m_usi1;
    in.usi2  = m_mw.m_usi2;
    in.comm1  = m_mw.m_models.commLog1;
    in.think1 = m_mw.m_models.thinking1;
    in.comm2  = m_mw.m_models.commLog2;
    in.think2 = m_mw.m_models.thinking2;
    in.sfenRecord = m_mw.m_queryService->sfenRecord();
    in.playMode   = &m_mw.m_state.playMode;
    in.timePresenter   = m_mw.m_timePresenter;
    in.boardController = m_mw.m_boardController;
    in.kifuRecordModel  = m_mw.m_models.kifuRecord;
    in.gameMoves        = &m_mw.m_kifu.gameMoves;
    in.positionStrList  = &m_mw.m_kifu.positionStrList;
    in.currentMoveIndex = &m_mw.m_state.currentMoveIndex;

    in.ensureTimeController           = std::bind(&GameSubRegistry::ensureTimeController, m_gameReg);
    in.ensureEvaluationGraphController = std::bind(&MainWindowFoundationRegistry::ensureEvaluationGraphController, m_foundation);
    in.ensurePlayerInfoWiring         = std::bind(&MainWindowFoundationRegistry::ensurePlayerInfoWiring, m_foundation);
    in.ensureUsiCommandController     = std::bind(&MainWindowServiceRegistry::ensureUsiCommandController, m_registry);
    in.ensureUiStatePolicyManager     = std::bind(&MainWindowFoundationRegistry::ensureUiStatePolicyManager, m_foundation);
    in.connectBoardClicks             = std::bind(&MainWindowSignalRouter::connectBoardClicks, m_mw.m_signalRouter.get());
    in.connectMoveRequested           = std::bind(&MainWindowSignalRouter::connectMoveRequested, m_mw.m_signalRouter.get());

    in.getClock = [this]() -> ShogiClock* {
        return m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr;
    };
    in.getTimeController = [this]() -> TimeControlController* {
        return m_mw.m_timeController;
    };
    in.getEvalGraphController = [this]() -> EvaluationGraphController* {
        return m_mw.m_evalGraphController.get();
    };
    in.getUiStatePolicy = [this]() -> UiStatePolicyManager* {
        return m_mw.m_uiStatePolicy;
    };

    const MainWindowMatchWiringDepsService depsService;
    return depsService.buildDeps(in);
}

// ---------------------------------------------------------------------------
// CSA通信対局配線
// ---------------------------------------------------------------------------

void GameWiringSubRegistry::ensureCsaGameWiring()
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

    m_mw.m_csaGameWiring = std::make_unique<CsaGameWiring>(deps);

    // 外部シグナル接続を CsaGameWiring に委譲
    m_foundation->ensureUiStatePolicyManager();
    m_registry->kifu()->ensureGameRecordUpdateService();
    m_foundation->ensureUiNotificationService();
    m_mw.m_csaGameWiring->wireExternalSignals(m_mw.m_uiStatePolicy,
                                               m_mw.m_gameRecordUpdateService.get(),
                                               m_mw.m_notificationService);

    qCDebug(lcApp).noquote() << "ensureCsaGameWiring_: created and connected";
}

// ---------------------------------------------------------------------------
// 連続対局コントローラ
// ---------------------------------------------------------------------------

void GameWiringSubRegistry::ensureConsecutiveGamesController()
{
    if (m_mw.m_consecutiveGamesController) return;

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_consecutiveGamesController = new ConsecutiveGamesController(&m_mw);
    m_mw.m_consecutiveGamesController->setTimeController(m_mw.m_timeController);
    m_mw.m_consecutiveGamesController->setGameStartCoordinator(m_mw.m_gameStart);

    m_gameReg->session()->ensureGameSessionOrchestrator();
    connect(m_mw.m_consecutiveGamesController, &ConsecutiveGamesController::requestPreStartCleanup,
            m_mw.m_gameSessionOrchestrator, &GameSessionOrchestrator::onPreStartCleanupRequested);
    connect(m_mw.m_consecutiveGamesController, &ConsecutiveGamesController::requestStartNextGame,
            m_mw.m_gameSessionOrchestrator, &GameSessionOrchestrator::onConsecutiveStartRequested);
}

// ---------------------------------------------------------------------------
// ターン同期ブリッジ
// ---------------------------------------------------------------------------

void GameWiringSubRegistry::ensureTurnSyncBridge()
{
    auto* gc = m_mw.m_gameController;
    auto* tm = m_mw.findChild<TurnManager*>("TurnManager");
    if (!gc || !tm) return;

    TurnSyncBridge::wire(gc, tm, &m_mw, &MainWindow::onTurnManagerChanged);
}

// ---------------------------------------------------------------------------
// MatchCoordinator初期化
// ---------------------------------------------------------------------------

void GameWiringSubRegistry::initMatchCoordinator()
{
    if (!m_mw.m_gameController || !m_mw.m_shogiView) return;

    ensureMatchCoordinatorWiring();

    // 旧 MC が wireConnections() 内で破棄されると m_match がダングリングポインタに
    // なり、m_queryService->sfenRecord() 経由で解放済みメモリにアクセスするクラッシュの
    // 原因となる。再初期化前に nullptr にしてガードで安全に弾けるようにする。
    m_mw.m_match = nullptr;

    if (!m_mw.m_matchWiring->initializeSession(
            std::bind(&GameWiringSubRegistry::ensureMatchCoordinatorWiring, this))) {
        return;
    }

    m_mw.m_match = m_mw.m_matchWiring->match();
}
