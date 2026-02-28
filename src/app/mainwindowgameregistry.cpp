/// @file mainwindowgameregistry.cpp
/// @brief Game系（対局・ゲーム進行・ターン管理・セッションライフサイクル）の ensure* 実装
///
/// MainWindowServiceRegistry のメソッドを実装する分割ファイル。
/// 共通基盤メソッドは MainWindowFoundationRegistry に移動済み。

#include "mainwindowserviceregistry.h"
#include "mainwindow.h"
#include "mainwindowcompositionroot.h"
#include "mainwindowdepsfactory.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowmatchadapter.h"
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
#include "kifunavigationcoordinator.h"
#include "livegamesessionupdater.h"
#include "matchcoordinatorwiring.h"
#include "matchruntimequeryservice.h"
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
#include "uistatepolicymanager.h"
#include "undoflowservice.h"
#include "logcategories.h"

#include <functional>

// ---------------------------------------------------------------------------
// 時間制御
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureTimeController()
{
    if (m_mw.m_timeController) return;

    m_mw.m_timeController = new TimeControlController(&m_mw);
    m_mw.m_timeController->setTimeDisplayPresenter(m_mw.m_timePresenter);
    m_mw.m_timeController->ensureClock();
}

// ---------------------------------------------------------------------------
// リプレイ
// ---------------------------------------------------------------------------

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
// MatchCoordinator 配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureMatchCoordinatorWiring()
{
    const bool firstTime = !m_mw.m_matchWiring;
    if (!m_mw.m_matchWiring) {
        m_mw.m_matchWiring = std::make_unique<MatchCoordinatorWiring>();
    }

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
        ensureCoreInitCoordinator();
        ad.initializeNewGame = std::bind(&MainWindowCoreInitCoordinator::initializeNewGame,
                                         m_mw.m_coreInit.get(), std::placeholders::_1);
        ad.ensureAndGetPlayerInfoWiring = [this]() -> PlayerInfoWiring* {
            m_foundation->ensurePlayerInfoWiring();
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

    auto deps = buildMatchWiringDeps();
    m_mw.m_matchWiring->updateDeps(deps);

    // 初回のみ: 転送シグナルを GameSessionOrchestrator / 各コントローラに接続
    if (firstTime) {
        ensureGameSessionOrchestrator();
        m_foundation->ensurePlayerInfoWiring();
        m_foundation->ensureKifuNavigationCoordinator();
        ensureBranchNavigationWiring();

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

void MainWindowServiceRegistry::ensureGameStateController()
{
    if (m_mw.m_gameStateController) return;

    m_foundation->ensureUiStatePolicyManager();

    MainWindowDepsFactory::GameStateControllerCallbacks cbs;
    cbs.enableArrowButtons = std::bind(&UiStatePolicyManager::transitionToIdle, m_mw.m_uiStatePolicy);
    cbs.setReplayMode = std::bind(&MainWindow::setReplayMode, &m_mw, std::placeholders::_1);
    cbs.refreshBranchTree = [this]() { m_mw.m_kifu.currentSelectedPly = 0; };
    cbs.updatePlyState = [this](int ap, int sp, int mi) {
        m_mw.m_kifu.activePly = ap;
        m_mw.m_kifu.currentSelectedPly = sp;
        m_mw.m_state.currentMoveIndex = mi;
        m_foundation->ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->syncNavStateToPly(sp);
    };

    m_mw.m_compositionRoot->ensureGameStateController(m_mw.buildRuntimeRefs(), cbs, &m_mw, m_mw.m_gameStateController);
}

// ---------------------------------------------------------------------------
// 対局開始コーディネーター
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureGameStartCoordinator()
{
    if (m_mw.m_gameStart) return;

    ensureMatchCoordinatorWiring();
    m_mw.m_matchWiring->ensureMenuGameStartCoordinator();
    m_mw.m_gameStart = m_mw.m_matchWiring->menuGameStartCoordinator();
}

// ---------------------------------------------------------------------------
// CSA通信対局配線
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

    m_mw.m_csaGameWiring = std::make_unique<CsaGameWiring>(deps);

    // 外部シグナル接続を CsaGameWiring に委譲
    m_foundation->ensureUiStatePolicyManager();
    ensureGameRecordUpdateService();
    m_foundation->ensureUiNotificationService();
    m_mw.m_csaGameWiring->wireExternalSignals(m_mw.m_uiStatePolicy,
                                               m_mw.m_gameRecordUpdateService.get(),
                                               m_mw.m_notificationService);

    qCDebug(lcApp).noquote() << "ensureCsaGameWiring_: created and connected";
}

// ---------------------------------------------------------------------------
// 開始前クリーンアップ
// ---------------------------------------------------------------------------

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

void MainWindowServiceRegistry::ensureTurnSyncBridge()
{
    auto* gc = m_mw.m_gameController;
    auto* tm = m_mw.findChild<TurnManager*>("TurnManager");
    if (!gc || !tm) return;

    TurnSyncBridge::wire(gc, tm, &m_mw, &MainWindow::onTurnManagerChanged);
}

// ---------------------------------------------------------------------------
// 手番同期サービス
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// ライブセッション開始
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureLiveGameSessionStarted()
{
    ensureLiveGameSessionUpdater();
    m_mw.m_liveGameSessionUpdater->ensureSessionStarted();
}

// ---------------------------------------------------------------------------
// ライブセッション更新
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// 手戻しフロー
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureUndoFlowService()
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
// 連続対局コントローラ
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// 対局ライフサイクル
// ---------------------------------------------------------------------------

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
    deps.ensureDialogCoordinator = [this]() { ensureDialogCoordinator(); };
    deps.ensureReplayController = std::bind(&MainWindowServiceRegistry::ensureReplayController, this);

    // === Action callbacks ===
    deps.initMatchCoordinator = std::bind(&MainWindowServiceRegistry::initMatchCoordinator, this);
    deps.clearSessionDependentUi = [this]() { clearSessionDependentUi(); };
    deps.updateJosekiWindow = [this]() { updateJosekiWindow(); };
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
    deps.setupBoardInteractionController = [this]() { setupBoardInteractionController(); };
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    deps.ensureTurnSyncBridge = std::bind(&MainWindowServiceRegistry::ensureTurnSyncBridge, this);
    deps.ensurePlayerInfoWiringAndApply = [this]() {
        m_foundation->ensurePlayerInfoWiring();
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
    callbacks.resetModels = [this](const QString& sfen) { resetModels(sfen); };
    callbacks.resetUiState = [this](const QString& sfen) { resetUiState(sfen); };
    callbacks.clearEvalState = [this]() { clearEvalState(); };
    callbacks.unlockGameOverStyle = [this]() { unlockGameOverStyle(); };
    callbacks.startGame = std::bind(&GameSessionOrchestrator::invokeStartGame,
                                    m_mw.m_gameSessionOrchestrator);
    callbacks.updateEndTime = [this](const QDateTime& endTime) {
        m_foundation->ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->updateGameInfoWithEndTime(endTime);
        }
    };
    callbacks.startNextConsecutiveGame = std::bind(&GameSessionOrchestrator::startNextConsecutiveGame,
                                                    m_mw.m_gameSessionOrchestrator);
    callbacks.lastTimeControl = &m_mw.m_lastTimeControl;
    callbacks.updateGameInfoWithTimeControl = [this](bool enabled, qint64 baseMs, qint64 byoyomiMs, qint64 incMs) {
        m_foundation->ensurePlayerInfoWiring();
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

// ---------------------------------------------------------------------------
// エンジン状態リセット
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// 手番表示更新
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// MatchCoordinator初期化
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::initMatchCoordinator()
{
    if (!m_mw.m_gameController || !m_mw.m_shogiView) return;

    ensureMatchCoordinatorWiring();

    // 旧 MC が wireConnections() 内で破棄されると m_match がダングリングポインタに
    // なり、m_queryService->sfenRecord() 経由で解放済みメモリにアクセスするクラッシュの
    // 原因となる。再初期化前に nullptr にしてガードで安全に弾けるようにする。
    m_mw.m_match = nullptr;

    if (!m_mw.m_matchWiring->initializeSession(
            std::bind(&MainWindowServiceRegistry::ensureMatchCoordinatorWiring, this))) {
        return;
    }

    m_mw.m_match = m_mw.m_matchWiring->match();
}
