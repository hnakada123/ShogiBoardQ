/// @file gamesubregistry_session.cpp
/// @brief Game系セッション管理（クリーンアップ・ライブセッション・ライフサイクル・コア初期化）の ensure* 実装

/// MainWindowServiceRegistry の Game セッション系メソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"

#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowcoreinitcoordinator.h"
#include "mainwindowfoundationregistry.h"

#include "boardinteractioncontroller.h"
#include "consecutivegamescontroller.h"
#include "csagamecoordinator.h"
#include "gamesessionorchestrator.h"
#include "kifuloadcoordinator.h"
#include "livegamesession.h"
#include "livegamesessionupdater.h"
#include "matchruntimequeryservice.h"
#include "playerinfowiring.h"
#include "prestartcleanuphandler.h"
#include "sessionlifecyclecoordinator.h"
#include "sessionlifecycledepsfactory.h"
#include "logcategories.h"

#include <functional>

// ---------------------------------------------------------------------------
// 開始前クリーンアップ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensurePreStartCleanupHandler()
{
    if (m_mw.m_registryParts.preStartCleanupHandler) return;

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

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_registryParts.preStartCleanupHandler = new PreStartCleanupHandler(deps, &m_mw);

    qCDebug(lcApp).noquote() << "ensurePreStartCleanupHandler_: created and connected";
}

// ---------------------------------------------------------------------------
// ライブセッション開始
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::startLiveGameSessionIfNeeded()
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
        createLiveGameSessionUpdater();
    }
    refreshLiveGameSessionUpdaterDeps();
}

void MainWindowServiceRegistry::createLiveGameSessionUpdater()
{
    m_mw.m_liveGameSessionUpdater = std::make_unique<LiveGameSessionUpdater>();
}

void MainWindowServiceRegistry::refreshLiveGameSessionUpdaterDeps()
{
    if (!m_mw.m_liveGameSessionUpdater) return;

    LiveGameSessionUpdater::Deps deps;
    deps.liveSession = m_mw.m_branchNav.liveGameSession;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.gameController = m_mw.m_gameController;
    deps.sfenRecord = m_mw.m_queryService->sfenRecord();
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    m_mw.m_liveGameSessionUpdater->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// 対局ライフサイクル
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureGameSessionOrchestrator()
{
    if (!m_mw.m_gameSessionOrchestrator) {
        createGameSessionOrchestrator();
    }
    refreshGameSessionOrchestratorDeps();
}

void MainWindowServiceRegistry::createGameSessionOrchestrator()
{
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_gameSessionOrchestrator = new GameSessionOrchestrator(&m_mw);
}

void MainWindowServiceRegistry::refreshGameSessionOrchestratorDeps()
{
    if (!m_mw.m_gameSessionOrchestrator) return;

    GameSessionOrchestrator::Deps deps;

    // === Controllers（ダブルポインタ） ===
    deps.gameStateController = &m_mw.m_gameStateController;
    deps.gameStart = &m_mw.m_gameStart;
    deps.dialogCoordinator = &m_mw.m_dialogCoordinator;
    deps.csaGameCoordinator = &m_mw.m_csaGameCoordinator;
    deps.match = &m_mw.m_match;

    // === Core objects ===
    deps.replayController = m_mw.m_replayController;
    deps.gameController = m_mw.m_gameController;
    deps.kifuLoadCoordinator = m_mw.m_kifuLoadCoordinator;
    deps.kifuModel = m_mw.m_models.kifuRecord;

    // === State pointers ===
    deps.playMode = &m_mw.m_state.playMode;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.currentSelectedPly = &m_mw.m_kifu.currentSelectedPly;
    deps.bottomIsP1 = &m_mw.m_player.bottomIsP1;

    // === Branch navigation ===
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;

    // === Lazy-init callbacks ===
    deps.ensureGameStateController = [this]() {
        ensureGameStateController();
    };
    deps.ensureGameStartCoordinator = [this]() {
        ensureGameStartCoordinator();
    };
    deps.ensureDialogCoordinator = [this]() { ensureDialogCoordinator(); };

    m_mw.m_gameSessionOrchestrator->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// コア初期化（MainWindow から移譲）
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureCoreInitCoordinator()
{
    if (!m_mw.m_coreInit) {
        createCoreInitCoordinator();
    }
    refreshCoreInitDeps();
}

void MainWindowServiceRegistry::createCoreInitCoordinator()
{
    m_mw.m_coreInit = std::make_unique<MainWindowCoreInitCoordinator>();
}

void MainWindowServiceRegistry::refreshCoreInitDeps()
{
    if (!m_mw.m_coreInit) return;

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
    deps.setCurrentTurn = [this]() {
        m_mw.setCurrentTurn();
    };
    deps.prepareTurnSyncBridge = [this]() {
        prepareTurnSyncBridge();
    };
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
        createSessionLifecycleCoordinator();
    }
    refreshSessionLifecycleDeps();
}

void MainWindowServiceRegistry::createSessionLifecycleCoordinator()
{
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_sessionLifecycle = new SessionLifecycleCoordinator(&m_mw);
}

void MainWindowServiceRegistry::refreshSessionLifecycleDeps()
{
    if (!m_mw.m_sessionLifecycle) return;

    SessionLifecycleDepsFactory::Callbacks callbacks;
    callbacks.clearGameStateFields = [this]() {
        clearGameStateFields();
    };
    callbacks.resetEngineState = [this]() {
        resetEngineState();
    };
    callbacks.performPreStartCleanup = [this]() {
        ensurePreStartCleanupHandler();
        if (m_mw.m_registryParts.preStartCleanupHandler) {
            m_mw.m_registryParts.preStartCleanupHandler->performCleanup();
        }
    };
    callbacks.clearSessionDependentUi = [this]() { clearSessionDependentUi(); };
    callbacks.resetModels = [this](const QString& sfen) { resetModels(sfen); };
    callbacks.resetUiState = [this](const QString& sfen) { resetUiState(sfen); };
    callbacks.clearEvalState = [this]() { clearEvalState(); };
    callbacks.unlockGameOverStyle = [this]() { unlockGameOverStyle(); };
    callbacks.startGame = [this]() {
        startGameSession();
    };
    callbacks.updateJosekiWindow = [this]() { m_kifu->updateJosekiWindow(); };
    callbacks.commitLiveGameSessionIfActive = [this]() {
        auto* session = m_mw.m_branchNav.liveGameSession;
        if (session != nullptr && session->isActive()) {
            qCDebug(lcApp) << "commitLiveGameSessionIfActive: committing live game session";
            session->commit();
        }
    };
    callbacks.updateEndTime = [this](const QDateTime& endTime) {
        m_foundation->ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->updateGameInfoWithEndTime(endTime);
        }
    };
    callbacks.startNextConsecutiveGame = [this]() {
        ensureConsecutiveGamesController();
        if (m_mw.m_consecutiveGamesController) {
            m_mw.m_consecutiveGamesController->startNextGame();
        }
    };
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
