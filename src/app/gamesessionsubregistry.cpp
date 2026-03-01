/// @file gamesessionsubregistry.cpp
/// @brief Game系セッション管理（クリーンアップ・ライブセッション・ライフサイクル・コア初期化）の ensure* 実装

#include "gamesessionsubregistry.h"
#include "gamesubregistry.h"
#include "gamewiringsubregistry.h"
#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowcoreinitcoordinator.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowserviceregistry.h"

#include "boardinteractioncontroller.h"
#include "consecutivegamescontroller.h"
#include "csagamecoordinator.h"
#include "gamesessionorchestrator.h"
#include "kifuloadcoordinator.h"
#include "livegamesessionupdater.h"
#include "matchruntimequeryservice.h"
#include "playerinfowiring.h"
#include "prestartcleanuphandler.h"
#include "sessionlifecyclecoordinator.h"
#include "sessionlifecycledepsfactory.h"
#include "logcategories.h"

#include <functional>

GameSessionSubRegistry::GameSessionSubRegistry(MainWindow& mw,
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
// 開始前クリーンアップ
// ---------------------------------------------------------------------------

void GameSessionSubRegistry::ensurePreStartCleanupHandler()
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

    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_preStartCleanupHandler = new PreStartCleanupHandler(deps, &m_mw);

    qCDebug(lcApp).noquote() << "ensurePreStartCleanupHandler_: created and connected";
}

// ---------------------------------------------------------------------------
// ライブセッション開始
// ---------------------------------------------------------------------------

void GameSessionSubRegistry::ensureLiveGameSessionStarted()
{
    ensureLiveGameSessionUpdater();
    m_mw.m_liveGameSessionUpdater->ensureSessionStarted();
}

// ---------------------------------------------------------------------------
// ライブセッション更新
// ---------------------------------------------------------------------------

void GameSessionSubRegistry::ensureLiveGameSessionUpdater()
{
    if (!m_mw.m_liveGameSessionUpdater) {
        createLiveGameSessionUpdater();
    }
    refreshLiveGameSessionUpdaterDeps();
}

void GameSessionSubRegistry::createLiveGameSessionUpdater()
{
    m_mw.m_liveGameSessionUpdater = std::make_unique<LiveGameSessionUpdater>();
}

void GameSessionSubRegistry::refreshLiveGameSessionUpdaterDeps()
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

void GameSessionSubRegistry::ensureGameSessionOrchestrator()
{
    if (!m_mw.m_gameSessionOrchestrator) {
        createGameSessionOrchestrator();
    }
    refreshGameSessionOrchestratorDeps();
}

void GameSessionSubRegistry::createGameSessionOrchestrator()
{
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_gameSessionOrchestrator = new GameSessionOrchestrator(&m_mw);
}

void GameSessionSubRegistry::refreshGameSessionOrchestratorDeps()
{
    if (!m_mw.m_gameSessionOrchestrator) return;

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
    deps.ensureGameStateController = std::bind(&GameSubRegistry::ensureGameStateController, m_gameReg);
    deps.ensureSessionLifecycleCoordinator = std::bind(&GameSessionSubRegistry::ensureSessionLifecycleCoordinator, this);
    deps.ensureConsecutiveGamesController = std::bind(&GameWiringSubRegistry::ensureConsecutiveGamesController,
                                                      m_gameReg->wiring());
    deps.ensureGameStartCoordinator = std::bind(&GameSubRegistry::ensureGameStartCoordinator, m_gameReg);
    deps.ensurePreStartCleanupHandler = std::bind(&GameSessionSubRegistry::ensurePreStartCleanupHandler, this);
    deps.ensureDialogCoordinator = [this]() { m_registry->ensureDialogCoordinator(); };
    deps.ensureReplayController = std::bind(&GameSubRegistry::ensureReplayController, m_gameReg);

    // === Action callbacks ===
    deps.initMatchCoordinator = std::bind(&GameWiringSubRegistry::initMatchCoordinator,
                                          m_gameReg->wiring());
    deps.clearSessionDependentUi = [this]() { m_registry->clearSessionDependentUi(); };
    deps.updateJosekiWindow = [this]() { m_registry->kifu()->updateJosekiWindow(); };
    deps.sfenRecord = [this]() -> QStringList* { return m_mw.m_queryService->sfenRecord(); };

    m_mw.m_gameSessionOrchestrator->updateDeps(deps);
}

// ---------------------------------------------------------------------------
// コア初期化（MainWindow から移譲）
// ---------------------------------------------------------------------------

void GameSessionSubRegistry::ensureCoreInitCoordinator()
{
    if (!m_mw.m_coreInit) {
        createCoreInitCoordinator();
    }
    refreshCoreInitDeps();
}

void GameSessionSubRegistry::createCoreInitCoordinator()
{
    m_mw.m_coreInit = std::make_unique<MainWindowCoreInitCoordinator>();
}

void GameSessionSubRegistry::refreshCoreInitDeps()
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
    deps.setupBoardInteractionController = [this]() { m_registry->setupBoardInteractionController(); };
    deps.setCurrentTurn = std::bind(&MainWindow::setCurrentTurn, &m_mw);
    deps.ensureTurnSyncBridge = std::bind(&GameWiringSubRegistry::ensureTurnSyncBridge,
                                         m_gameReg->wiring());
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

void GameSessionSubRegistry::ensureSessionLifecycleCoordinator()
{
    if (!m_mw.m_sessionLifecycle) {
        createSessionLifecycleCoordinator();
    }
    refreshSessionLifecycleDeps();
}

void GameSessionSubRegistry::createSessionLifecycleCoordinator()
{
    // Lifetime: owned by MainWindow (QObject parent=&m_mw)
    // Created: once on first use, never recreated
    m_mw.m_sessionLifecycle = new SessionLifecycleCoordinator(&m_mw);
}

void GameSessionSubRegistry::refreshSessionLifecycleDeps()
{
    if (!m_mw.m_sessionLifecycle) return;

    SessionLifecycleDepsFactory::Callbacks callbacks;
    callbacks.clearGameStateFields = std::bind(&GameSessionSubRegistry::clearGameStateFields, this);
    callbacks.resetEngineState = std::bind(&GameSessionSubRegistry::resetEngineState, this);
    ensureGameSessionOrchestrator();
    callbacks.onPreStartCleanupRequested = std::bind(&GameSessionOrchestrator::onPreStartCleanupRequested,
                                                     m_mw.m_gameSessionOrchestrator);
    callbacks.resetModels = [this](const QString& sfen) { m_registry->resetModels(sfen); };
    callbacks.resetUiState = [this](const QString& sfen) { m_registry->resetUiState(sfen); };
    callbacks.clearEvalState = [this]() { m_registry->clearEvalState(); };
    callbacks.unlockGameOverStyle = [this]() { m_registry->unlockGameOverStyle(); };
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

void GameSessionSubRegistry::clearGameStateFields()
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

void GameSessionSubRegistry::resetEngineState()
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
