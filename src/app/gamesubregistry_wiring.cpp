/// @file gamesubregistry_wiring.cpp
/// @brief Game系配線管理（MC配線・CSA配線・連続対局・ターン同期ブリッジ・MC初期化）の ensure* 実装

/// MainWindowServiceRegistry の Game 配線系メソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"

#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowmatchadapter.h"
#include "mainwindowmatchwiringdepsservice.h"
#include "mainwindowsignalrouter.h"
#include "mainwindowcoreinitcoordinator.h"
#include "ui_mainwindow.h"

#include "boardinteractioncontroller.h"
#include "consecutivegamescontroller.h"
#include "considerationwiring.h"
#include "csagamewiring.h"
#include "gamerecordupdateservice.h"
#include "gamesessionorchestrator.h"
#include "kifufilecontroller.h"
#include "kifunavigationcoordinator.h"
#include "matchruntimequeryservice.h"
#include "playerinfowiring.h"
#include "sessionlifecyclecoordinator.h"
#include "timecontrolcontroller.h"
#include "turnmanager.h"
#include "turnsyncbridge.h"
#include "logcategories.h"

// ---------------------------------------------------------------------------
// MatchCoordinator 配線
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureMatchCoordinatorWiring()
{
    if (!m_mw.m_matchWiring) {
        createMatchCoordinatorWiring();
    }
    refreshMatchWiringDeps();
}

void MainWindowServiceRegistry::createMatchCoordinatorWiring()
{
    m_mw.m_matchWiring = std::make_unique<MatchCoordinatorWiring>();

    // 初回のみ: 転送シグナルを各責務クラスに接続
    ensureGameSessionOrchestrator();
    ensureGameStateController();
    ensureConsecutiveGamesController();
    ensureSessionLifecycleCoordinator();
    m_foundation->ensurePlayerInfoWiring();
    m_foundation->ensureKifuNavigationCoordinator();
    m_kifu->ensureBranchNavigationWiring();

    MatchCoordinatorWiring::ForwardingTargets targets;
    targets.gameSession = m_mw.m_gameSessionOrchestrator;
    targets.sessionLifecycle = m_mw.m_sessionLifecycle;
    targets.gameStateController = m_mw.m_gameStateController;
    targets.consecutiveGamesController = m_mw.m_consecutiveGamesController;
    targets.appearance = m_mw.m_appearanceController.get();
    targets.playerInfo = m_mw.m_playerInfoWiring;
    targets.kifuNav = m_mw.m_kifuNavCoordinator.get();
    targets.branchNav = m_mw.m_branchNavWiring.get();
    m_mw.m_matchWiring->wireForwardingSignals(targets);
}

void MainWindowServiceRegistry::refreshMatchWiringDeps()
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
        ensureCoreInitCoordinator();
        ad.initializeNewGame = [this](QString& startSfenStr) {
            m_mw.m_coreInit->initializeNewGame(startSfenStr);
        };
        ad.ensureAndGetPlayerInfoWiring = [this]() -> PlayerInfoWiring* {
            m_foundation->ensurePlayerInfoWiring();
            return m_mw.m_playerInfoWiring;
        };
        ad.ensureAndGetDialogCoordinator = [this]() -> DialogCoordinator* {
            ensureDialogCoordinator();
            return m_mw.m_dialogCoordinator;
        };
        ad.ensureAndGetTurnStateSync = [this]() -> TurnStateSyncService* {
            prepareTurnStateSyncService();
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

MatchCoordinatorWiring::Deps MainWindowServiceRegistry::buildMatchWiringDeps()
{
    MainWindowMatchWiringDepsService::Inputs in;
    in.evalGraphController = m_mw.m_evalGraphController.get();
    in.onTurnChanged = [this](ShogiGameController::Player player) {
        m_mw.onTurnManagerChanged(player);
    };
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
    in.initializeNewGame = [adapter](const QString& sfen) {
        adapter->initializeNewGameHook(sfen);
    };
    in.renderBoard = [adapter]() {
        adapter->renderBoardFromGc();
    };
    in.showMoveHighlights = [adapter](const QPoint& from, const QPoint& to) {
        adapter->showMoveHighlights(from, to);
    };
    in.appendKifuLine = [this](const QString& text, const QString& elapsed) {
        m_kifu->ensureGameRecordUpdateService();
        if (m_mw.m_gameRecordUpdateService) {
            m_mw.m_gameRecordUpdateService->appendKifuLine(text, elapsed);
        }
    };
    in.showGameOverDialog = [adapter](const QString& title, const QString& message) {
        adapter->showGameOverMessageBox(title, message);
    };
    in.remainingMsFor = [this](MatchCoordinator::Player player) -> qint64 {
        return queryRemainingMsFor(player);
    };
    in.incrementMsFor = [this](MatchCoordinator::Player player) -> qint64 {
        return queryIncrementMsFor(player);
    };
    in.byoyomiMs = [this]() -> qint64 {
        return queryByoyomiMs();
    };
    in.setPlayersNames = [this](const QString& p1, const QString& p2) {
        m_mw.m_playerInfoWiring->onSetPlayersNames(p1, p2);
    };
    in.setEngineNames = [this](const QString& e1, const QString& e2) {
        m_mw.m_playerInfoWiring->onSetEngineNames(e1, e2);
    };
    m_kifu->ensureKifuFileController();
    in.autoSaveKifu = [this](const QString& gameResultText,
                             PlayMode mode,
                             const QString& gameModeText,
                             const QString& dateTimeText,
                             const QString& blackText,
                             const QString& whiteText) {
        m_mw.m_kifuFileController->autoSaveKifuToFile(gameResultText, mode, gameModeText,
                                                      dateTimeText, blackText, whiteText);
    };

    m_foundation->ensureKifuNavigationCoordinator();
    in.updateHighlightsForPly = [this](int ply) {
        m_mw.m_kifuNavCoordinator->syncBoardAndHighlightsAtRow(ply);
    };
    in.updateTurnAndTimekeepingDisplay = [adapter]() {
        adapter->updateTurnAndTimekeepingDisplay();
    };
    in.isHumanSide = [this](ShogiGameController::Player player) {
        return queryIsHumanSide(player);
    };
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

    in.ensureTimeController = [this]() {
        ensureTimeController();
    };
    in.ensureEvaluationGraphController = [this]() {
        m_foundation->ensureEvaluationGraphController();
    };
    in.ensurePlayerInfoWiring = [this]() {
        m_foundation->ensurePlayerInfoWiring();
    };
    in.ensureUsiCommandController = [this]() {
        ensureUsiCommandController();
    };
    in.ensureUiStatePolicyManager = [this]() {
        m_foundation->ensureUiStatePolicyManager();
    };
    in.connectBoardClicks = [this]() {
        m_mw.m_signalRouter->connectBoardClicks();
    };
    in.connectMoveRequested = [this]() {
        m_mw.m_signalRouter->connectMoveRequested();
    };

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
    m_kifu->ensureGameRecordUpdateService();
    m_foundation->ensureUiNotificationService();
    m_mw.m_csaGameWiring->wireExternalSignals(m_mw.m_uiStatePolicy,
                                              m_mw.m_gameRecordUpdateService.get(),
                                              m_mw.m_notificationService);

    qCDebug(lcApp).noquote() << "ensureCsaGameWiring_: created and connected";
}

// ---------------------------------------------------------------------------
// 連続対局コントローラ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::ensureConsecutiveGamesController()
{
    if (!m_mw.m_consecutiveGamesController) {
        // Lifetime: owned by MainWindow (QObject parent=&m_mw)
        // Created: once on first use, never recreated
        m_mw.m_consecutiveGamesController = new ConsecutiveGamesController(&m_mw);
    }

    m_mw.m_consecutiveGamesController->setTimeController(m_mw.m_timeController);
    m_mw.m_consecutiveGamesController->setGameStartCoordinator(m_mw.m_gameStart);
    m_mw.m_consecutiveGamesController->setPerformPreStartCleanup([this]() {
        ensureSessionLifecycleCoordinator();
        if (m_mw.m_sessionLifecycle) {
            m_mw.m_sessionLifecycle->performPreStartCleanup();
        }
    });
}

// ---------------------------------------------------------------------------
// ターン同期ブリッジ
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::prepareTurnSyncBridge()
{
    auto* gc = m_mw.m_gameController;
    auto* tm = m_mw.findChild<TurnManager*>("TurnManager");
    if (!gc || !tm) return;

    TurnSyncBridge::wire(gc, tm, &m_mw, &MainWindow::onTurnManagerChanged);
}

// ---------------------------------------------------------------------------
// MatchRuntimeQueryService 中継メソッド（shutdown 安全性確保）
// ---------------------------------------------------------------------------

qint64 MainWindowServiceRegistry::queryRemainingMsFor(MatchCoordinator::Player player)
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->remainingMsFor(player);
    return 0;
}

qint64 MainWindowServiceRegistry::queryIncrementMsFor(MatchCoordinator::Player player)
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->incrementMsFor(player);
    return 0;
}

qint64 MainWindowServiceRegistry::queryByoyomiMs()
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->byoyomiMs();
    return 0;
}

bool MainWindowServiceRegistry::queryIsHumanSide(ShogiGameController::Player player)
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->isHumanSide(player);
    return false;
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

    if (!m_mw.m_matchWiring->initializeSession([this]() {
            ensureMatchCoordinatorWiring();
        })) {
        return;
    }

    m_mw.m_match = m_mw.m_matchWiring->match();
    m_mw.refreshPlayModePolicyDeps();
    ensureReplayController();
    ensureGameStateController();
    ensureSessionLifecycleCoordinator();

    // ConsiderationWiring が保持する旧 MatchCoordinator ポインタを更新
    if (m_mw.m_considerationWiring) {
        m_mw.m_considerationWiring->setMatchCoordinator(m_mw.m_match);
    }
}
