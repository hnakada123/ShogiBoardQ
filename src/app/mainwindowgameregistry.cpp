/// @file mainwindowgameregistry.cpp
/// @brief Game系の ensure* 実装

#include "mainwindowgameregistry.h"
#include "mainwindow.h"
#include "mainwindowserviceregistry.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowmatchadapter.h"
#include "mainwindowwiringassembler.h"
#include "mainwindowcoreinitcoordinator.h"
#include "ui_mainwindow.h"

#include "consecutivegamescontroller.h"
#include "csagamewiring.h"
#include "csagamecoordinator.h"
#include "dialogcoordinator.h"
#include "evaluationgraphcontroller.h"
#include "gamestatecontroller.h"
#include "gamestartcoordinator.h"
#include "gamesessionorchestrator.h"
#include "jishogiscoredialogcontroller.h"
#include "kifunavigationcoordinator.h"
#include "livegamesessionupdater.h"
#include "matchcoordinatorwiring.h"
#include "matchruntimequeryservice.h"
#include "nyugyokudeclarationhandler.h"
#include "playerinfowiring.h"
#include "prestartcleanuphandler.h"
#include "replaycontroller.h"
#include "sessionlifecyclecoordinator.h"
#include "sessionlifecycledepsfactory.h"
#include "shogiclock.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "turnmanager.h"
#include "turnsyncbridge.h"
#include "turnstatesyncservice.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#include "undoflowservice.h"
#include "logcategories.h"

#include <functional>

MainWindowGameRegistry::MainWindowGameRegistry(MainWindow& mw,
                                                 MainWindowServiceRegistry& registry,
                                                 QObject* parent)
    : QObject(parent)
    , m_mw(mw)
    , m_registry(registry)
{
}

// ---------------------------------------------------------------------------
// 時間制御
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureTimeController()
{
    if (m_mw.m_timeController) return;

    m_mw.m_timeController = new TimeControlController(&m_mw);
    m_mw.m_timeController->setTimeDisplayPresenter(m_mw.m_timePresenter);
    m_mw.m_timeController->ensureClock();
}

// ---------------------------------------------------------------------------
// リプレイ
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureReplayController()
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
// MatchCoordinator 配線
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureMatchCoordinatorWiring()
{
    const bool firstTime = !m_mw.m_matchWiring;
    if (!m_mw.m_matchWiring) {
        m_mw.m_matchWiring = std::make_unique<MatchCoordinatorWiring>();
    }

    m_registry.ensureEvaluationGraphController();
    m_registry.ensurePlayerInfoWiring();

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
        m_registry.ensureCoreInitCoordinator();
        ad.initializeNewGame = std::bind(&MainWindowCoreInitCoordinator::initializeNewGame,
                                         m_mw.m_coreInit.get(), std::placeholders::_1);
        ad.ensureAndGetPlayerInfoWiring = [this]() -> PlayerInfoWiring* {
            m_registry.ensurePlayerInfoWiring();
            return m_mw.m_playerInfoWiring;
        };
        ad.ensureAndGetDialogCoordinator = [this]() -> DialogCoordinator* {
            m_registry.ensureDialogCoordinator();
            return m_mw.m_dialogCoordinator;
        };
        ad.ensureAndGetTurnStateSync = [this]() -> TurnStateSyncService* {
            m_registry.ensureTurnStateSyncService();
            return m_mw.m_turnStateSync.get();
        };
        m_mw.m_matchAdapter->updateDeps(ad);
    }

    auto deps = MainWindowWiringAssembler::buildMatchWiringDeps(m_mw);
    m_mw.m_matchWiring->updateDeps(deps);

    // 初回のみ: 転送シグナルを GameSessionOrchestrator / 各コントローラに接続
    if (firstTime) {
        ensureGameSessionOrchestrator();
        m_mw.ensurePlayerInfoWiring();
        m_mw.ensureKifuNavigationCoordinator();
        m_mw.ensureBranchNavigationWiring();

        MatchCoordinatorWiring::ForwardingTargets targets;
        targets.appearance = m_mw.m_appearanceController.get();
        targets.playerInfo = m_mw.m_playerInfoWiring;
        targets.kifuNav = m_mw.m_kifuNavCoordinator.get();
        targets.branchNav = m_mw.m_branchNavWiring.get();
        m_mw.m_matchWiring->wireForwardingSignals(targets, m_mw.m_gameSessionOrchestrator);
    }
}

// ---------------------------------------------------------------------------
// ゲーム状態コントローラ
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureGameStateController()
{
    if (m_mw.m_gameStateController) return;

    m_registry.ensureUiStatePolicyManager();

    MainWindowDepsFactory::GameStateControllerCallbacks cbs;
    cbs.enableArrowButtons = std::bind(&UiStatePolicyManager::transitionToIdle, m_mw.m_uiStatePolicy);
    cbs.setReplayMode = std::bind(&MainWindow::setReplayMode, &m_mw, std::placeholders::_1);
    cbs.refreshBranchTree = [this]() { m_mw.m_kifu.currentSelectedPly = 0; };
    cbs.updatePlyState = [this](int ap, int sp, int mi) {
        m_mw.m_kifu.activePly = ap;
        m_mw.m_kifu.currentSelectedPly = sp;
        m_mw.m_state.currentMoveIndex = mi;
        m_registry.ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->syncNavStateToPly(sp);
    };

    m_mw.m_compositionRoot->ensureGameStateController(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_gameStateController);
}

// ---------------------------------------------------------------------------
// 対局開始コーディネーター
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureGameStartCoordinator()
{
    if (m_mw.m_gameStart) return;

    ensureMatchCoordinatorWiring();
    m_mw.m_matchWiring->ensureMenuGameStartCoordinator();
    m_mw.m_gameStart = m_mw.m_matchWiring->menuGameStartCoordinator();
}

// ---------------------------------------------------------------------------
// CSA通信対局配線
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureCsaGameWiring()
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
    m_registry.ensureUiStatePolicyManager();
    m_registry.ensureGameRecordUpdateService();
    m_registry.ensureUiNotificationService();
    m_mw.m_csaGameWiring->wireExternalSignals(m_mw.m_uiStatePolicy,
                                               m_mw.m_gameRecordUpdateService.get(),
                                               m_mw.m_notificationService);

    qCDebug(lcApp).noquote() << "ensureCsaGameWiring_: created and connected";
}

// ---------------------------------------------------------------------------
// 開始前クリーンアップ
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensurePreStartCleanupHandler()
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
    deps.evalGraphController = m_mw.m_evalGraphController.get();
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
// ターン同期ブリッジ
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureTurnSyncBridge()
{
    auto* gc = m_mw.m_gameController;
    auto* tm = m_mw.findChild<TurnManager*>("TurnManager");
    if (!gc || !tm) return;

    TurnSyncBridge::wire(gc, tm, &m_mw);
}

// ---------------------------------------------------------------------------
// 手番同期サービス
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureTurnStateSyncService()
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
    deps.updateTurnStatus = std::bind(&MainWindowGameRegistry::updateTurnStatus, this, std::placeholders::_1);
    deps.onTurnManagerCreated = [this](TurnManager* tm) {
        connect(tm, &TurnManager::changed,
                &m_mw, &MainWindow::onTurnManagerChanged,
                Qt::UniqueConnection);
    };
    m_mw.m_turnStateSync->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// ライブセッション開始
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureLiveGameSessionStarted()
{
    ensureLiveGameSessionUpdater();
    m_mw.m_liveGameSessionUpdater->ensureSessionStarted();
}

// ---------------------------------------------------------------------------
// ライブセッション更新
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureLiveGameSessionUpdater()
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

// ---------------------------------------------------------------------------
// 手戻しフロー
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureUndoFlowService()
{
    if (!m_mw.m_undoFlowService) {
        m_mw.m_undoFlowService = std::make_unique<UndoFlowService>();
    }

    UndoFlowService::Deps deps;
    deps.match = m_mw.m_match;
    deps.evalGraphController = m_mw.m_evalGraphController.get();
    deps.playMode = &m_mw.m_state.playMode;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    m_mw.m_undoFlowService->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 持将棋コントローラ
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureJishogiController()
{
    if (m_mw.m_jishogiController) return;
    m_mw.m_jishogiController = std::make_unique<JishogiScoreDialogController>();
}

// ---------------------------------------------------------------------------
// 入玉宣言ハンドラ
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureNyugyokuHandler()
{
    if (m_mw.m_nyugyokuHandler) return;
    m_mw.m_nyugyokuHandler = std::make_unique<NyugyokuDeclarationHandler>();
}

// ---------------------------------------------------------------------------
// 連続対局コントローラ
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureConsecutiveGamesController()
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

// ---------------------------------------------------------------------------
// 対局ライフサイクル
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureGameSessionOrchestrator()
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
    deps.ensureGameStateController = std::bind(&MainWindowGameRegistry::ensureGameStateController, this);
    deps.ensureSessionLifecycleCoordinator = std::bind(&MainWindowGameRegistry::ensureSessionLifecycleCoordinator, this);
    deps.ensureConsecutiveGamesController = std::bind(&MainWindowGameRegistry::ensureConsecutiveGamesController, this);
    deps.ensureGameStartCoordinator = std::bind(&MainWindowGameRegistry::ensureGameStartCoordinator, this);
    deps.ensurePreStartCleanupHandler = std::bind(&MainWindowGameRegistry::ensurePreStartCleanupHandler, this);
    deps.ensureDialogCoordinator = [this]() { m_registry.ensureDialogCoordinator(); };
    deps.ensureReplayController = std::bind(&MainWindowGameRegistry::ensureReplayController, this);

    // === Action callbacks ===
    deps.initMatchCoordinator = std::bind(&MainWindowGameRegistry::initMatchCoordinator, this);
    deps.clearSessionDependentUi = [this]() { m_registry.clearSessionDependentUi(); };
    deps.updateJosekiWindow = [this]() { m_registry.updateJosekiWindow(); };
    deps.sfenRecord = [this]() -> QStringList* { return m_mw.m_queryService->sfenRecord(); };

    m_mw.m_gameSessionOrchestrator->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// コア初期化（MainWindow から移譲）
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureCoreInitCoordinator()
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
    deps.setupBoardInteractionController = [this]() { m_registry.setupBoardInteractionController(); };
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    deps.ensureTurnSyncBridge = std::bind(&MainWindowGameRegistry::ensureTurnSyncBridge, this);
    deps.ensurePlayerInfoWiringAndApply = [this]() {
        m_registry.ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->applyPlayersNamesForMode();
            m_mw.m_playerInfoWiring->applyEngineNamesToLogModels();
            m_mw.m_playerInfoWiring->applySecondEngineVisibility();
        }
    };
    m_mw.m_coreInit->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// セッションライフサイクル
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::ensureSessionLifecycleCoordinator()
{
    if (!m_mw.m_sessionLifecycle) {
        m_mw.m_sessionLifecycle = new SessionLifecycleCoordinator(&m_mw);
    }

    SessionLifecycleDepsFactory::Callbacks callbacks;
    callbacks.clearGameStateFields = std::bind(&MainWindowGameRegistry::clearGameStateFields, this);
    callbacks.resetEngineState = std::bind(&MainWindowGameRegistry::resetEngineState, this);
    ensureGameSessionOrchestrator();
    callbacks.onPreStartCleanupRequested = std::bind(&GameSessionOrchestrator::onPreStartCleanupRequested,
                                                     m_mw.m_gameSessionOrchestrator);
    callbacks.resetModels = [this](const QString& sfen) { m_registry.resetModels(sfen); };
    callbacks.resetUiState = [this](const QString& sfen) { m_registry.resetUiState(sfen); };
    callbacks.clearEvalState = [this]() { m_registry.clearEvalState(); };
    callbacks.unlockGameOverStyle = [this]() { m_registry.unlockGameOverStyle(); };
    callbacks.startGame = std::bind(&GameSessionOrchestrator::invokeStartGame,
                                    m_mw.m_gameSessionOrchestrator);
    callbacks.updateEndTime = [this](const QDateTime& endTime) {
        m_registry.ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->updateGameInfoWithEndTime(endTime);
        }
    };
    callbacks.startNextConsecutiveGame = std::bind(&GameSessionOrchestrator::startNextConsecutiveGame,
                                                    m_mw.m_gameSessionOrchestrator);
    callbacks.lastTimeControl = &m_mw.m_lastTimeControl;
    callbacks.updateGameInfoWithTimeControl = [this](bool enabled, qint64 baseMs, qint64 byoyomiMs, qint64 incMs) {
        m_registry.ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->updateGameInfoWithTimeControl(enabled, baseMs, byoyomiMs, incMs);
        }
    };

    m_mw.m_sessionLifecycle->updateDeps(
        SessionLifecycleDepsFactory::createDeps(m_mw.buildRuntimeRefs(), callbacks));
}

// ---------------------------------------------------------------------------
// ゲーム状態フィールドクリア
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::clearGameStateFields()
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

// ---------------------------------------------------------------------------
// エンジン状態リセット
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::resetEngineState()
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

// ---------------------------------------------------------------------------
// 手番表示更新
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::updateTurnStatus(int currentPlayer)
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

// ---------------------------------------------------------------------------
// MatchCoordinator初期化
// ---------------------------------------------------------------------------

void MainWindowGameRegistry::initMatchCoordinator()
{
    if (!m_mw.m_gameController || !m_mw.m_shogiView) return;

    ensureMatchCoordinatorWiring();

    if (!m_mw.m_matchWiring->initializeSession(
            std::bind(&MainWindowGameRegistry::ensureMatchCoordinatorWiring, this))) {
        return;
    }

    m_mw.m_match = m_mw.m_matchWiring->match();
}
