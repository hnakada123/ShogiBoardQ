/// @file gamesubregistry_session.cpp
/// @brief Game系セッション管理（クリーンアップ・ライブセッション・ライフサイクル・コア初期化）の ensure* 実装

/// MainWindowServiceRegistry の Game セッション系メソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"

#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowresetservice.h"
#include "mainwindowcoreinitcoordinator.h"
#include "mainwindowfoundationregistry.h"

#include "boardinteractioncontroller.h"
#include "consecutivegamescontroller.h"
#include "csagamecoordinator.h"
#include "gamesessionorchestrator.h"
#include "kifuloadcoordinator.h"
#include "livegamesession.h"
#include "livegamesessionupdater.h"
#include "playmodepolicyservice.h"
#include "playerinfowiring.h"
#include "prestartcleanuphandler.h"
#include "sessionlifecyclecoordinator.h"
#include "logcategories.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "uistatepolicymanager.h"
#include "gamerecordmodel.h"

#include <functional>

// ---------------------------------------------------------------------------
// 開始前クリーンアップ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensurePreStartCleanupHandler()
{
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
    deps.liveGameSession = m_mw.m_branchNav.liveGameSession;
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    if (!m_mw.m_registryParts.preStartCleanupHandler) {
        m_mw.m_registryParts.preStartCleanupHandler = new PreStartCleanupHandler(deps, &m_mw);
    } else {
        m_mw.m_registryParts.preStartCleanupHandler->updateDependencies(deps);
    }

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
    deps.sfenRecord = &m_mw.m_kifu.sfenRecord;
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
    deps.confirmDiscardUnsavedKifu = [this]() { return confirmDiscardUnsavedKifu(); };

    // === Controllers（使うときに取得。遅延生成のものはここで作る） ===
    deps.gameStateController = [this]() -> GameStateController* {
        ensureGameStateController();
        return m_mw.m_gameStateController;
    };
    deps.gameStart = [this]() -> GameStartCoordinator* {
        ensureGameStartCoordinator();
        return m_mw.m_gameStart;
    };
    deps.dialogCoordinator = [this]() -> DialogCoordinator* {
        ensureDialogCoordinator();
        return m_mw.m_dialogCoordinator;
    };
    deps.csaGameCoordinator = [this]() -> CsaGameCoordinator* { return m_mw.m_csaGameCoordinator; };
    deps.match = [this]() -> MatchCoordinator* { return m_mw.m_match; };

    // === Core objects ===
    deps.replayController = m_mw.m_replayController;
    deps.gameController = m_mw.m_gameController;
    deps.kifuModel = m_mw.m_models.kifuRecord;

    // === State pointers ===
    deps.playMode = &m_mw.m_state.playMode;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.bottomSide = &m_mw.m_player.bottomSide;

    // === Branch navigation ===
    deps.branchTree = m_mw.m_branchNav.branchTree;
    deps.navState = m_mw.m_branchNav.navState;

    deps.liveSession = m_mw.m_branchNav.liveGameSession;
    deps.setResumeAvailable = [this](bool available) {
        m_foundation->ensureUiStatePolicyManager();
        m_mw.m_uiStatePolicy->setResumeAvailable(available);
    };
    deps.prepareResumeUi = [this]() {
        unlockGameOverStyle();
        if (m_mw.m_shogiView) m_mw.m_shogiView->setMouseClickMode(true);
        if (m_mw.m_timeController) m_mw.m_timeController->clearGameEndTime();
        if (m_mw.m_playerInfoWiring) m_mw.m_playerInfoWiring->updateGameInfoWithEndTime(QDateTime());
        if (m_mw.m_models.gameRecord) m_mw.m_models.gameRecord->markDirty();
    };

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
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
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

    SessionLifecycleCoordinator::Deps deps;
    deps.replayController = m_mw.m_replayController;
    deps.gameController = m_mw.m_gameController;
    deps.gameStateController = m_mw.m_gameStateController;
    deps.sfenRecord = &m_mw.m_kifu.sfenRecord;
    deps.playMode = &m_mw.m_state.playMode;
    deps.startSfenStr = &m_mw.m_state.startSfenStr;
    deps.currentSfenStr = &m_mw.m_state.currentSfenStr;
    deps.timeController = m_mw.m_timeController;
    deps.consecutiveGamesController = m_mw.m_consecutiveGamesController;
    deps.match = m_mw.m_match;
    deps.shogiView = m_mw.m_shogiView;

    deps.clearGameStateFields = [this]() {
        MainWindowResetService().clearGameStateFields(m_mw.m_state, m_mw.m_player, m_mw.m_kifu);
    };
    deps.resetEngineState = [this]() {
        MainWindowResetService().resetEngineState(m_mw.m_match, m_mw.m_csaGameCoordinator,
                                                 m_mw.m_consecutiveGamesController);
    };
    deps.performPreStartCleanup = [this]() {
        ensurePreStartCleanupHandler();
        if (m_mw.m_registryParts.preStartCleanupHandler) {
            m_mw.m_registryParts.preStartCleanupHandler->performCleanup();
        }
    };
    deps.clearSessionDependentUi = [this]() { clearSessionDependentUi(); };
    deps.resetModels = [this](const QString& sfen) { resetModels(sfen); };
    deps.resetUiState = [this](const QString& sfen) { resetUiState(sfen); };
    deps.clearEvalState = [this]() { clearEvalState(); };
    deps.unlockGameOverStyle = [this]() { unlockGameOverStyle(); };
    deps.startGame = [this]() {
        startGameSession();
    };
    deps.updateJosekiWindow = [this]() { m_kifu->updateJosekiWindow(); };
    deps.commitLiveGameSessionIfActive = [this]() {
        auto* session = m_mw.m_branchNav.liveGameSession;
        if (session != nullptr && session->isActive()) {
            qCDebug(lcApp) << "commitLiveGameSessionIfActive: committing live game session";
            session->commit();
        }
    };
    deps.updateEndTime = [this](const QDateTime& endTime) {
        m_foundation->ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->updateGameInfoWithEndTime(endTime);
        }
    };
    deps.startNextConsecutiveGame = [this]() {
        ensureConsecutiveGamesController();
        if (m_mw.m_consecutiveGamesController) {
            m_mw.m_consecutiveGamesController->startNextGame();
        }
    };
    deps.lastTimeControl = &m_mw.m_lastTimeControl;
    deps.updateGameInfoWithTimeControl = [this](const GameStartCoordinator::TimeSettings& tc) {
        m_foundation->ensurePlayerInfoWiring();
        if (m_mw.m_playerInfoWiring) {
            m_mw.m_playerInfoWiring->updateGameInfoWithTimeControl(tc.enabled ? tc.rules : TimeControl{});
        }
    };

    m_mw.m_sessionLifecycle->updateDeps(deps);
}
