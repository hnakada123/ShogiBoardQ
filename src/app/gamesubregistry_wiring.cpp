/// @file gamesubregistry_wiring.cpp
/// @brief Game系配線管理（MC配線・CSA配線・連続対局・ターン同期ブリッジ・MC初期化）の ensure* 実装

/// MainWindowServiceRegistry の Game 配線系メソッドを実装する分割ファイル。

#include "mainwindowserviceregistry.h"
#include "evaluationgraphcontroller.h"

#include "kifusubregistry.h"
#include "mainwindow.h"
#include "mainwindowfoundationregistry.h"
#include "mainwindowmatchadapter.h"
#include "mainwindowsignalrouter.h"
#include "mainwindowcoreinitcoordinator.h"
#include "ui_mainwindow.h"

#include "boardinteractioncontroller.h"
#include "consecutivegamescontroller.h"
#include "considerationwiring.h"
#include "csagamewiring.h"
#include "gamerecordupdateservice.h"
#include "gameinfopanecontroller.h"
#include "gamesessionorchestrator.h"
#include "kifuexportmetadata.h"
#include "kifufilecontroller.h"
#include "matchcoordinator.h"
#include "shogiview.h"
#include "kifunavigationcoordinator.h"
#include "playmodepolicyservice.h"
#include "piecesoundplayer.h"
#include "playerinfowiring.h"
#include "sessionlifecyclecoordinator.h"
#include "timecontrolcontroller.h"
#include "turnmanager.h"
#include "turnsyncbridge.h"
#include "logcategories.h"

#include <QDateTime>

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

MatchCoordinator::Hooks MainWindowServiceRegistry::buildMatchHooks()
{
    // 司令塔は一度だけ作るので、フックは呼ばれた時点の部品を読む（作成時のポインタを写さない）
    MatchCoordinator::Hooks hooks;
    auto* adapter = m_mw.m_matchAdapter.get();

    hooks.ui.updateTurnDisplay = [this](MatchCoordinator::Player cur) {
        m_mw.onTurnManagerChanged(cur == MatchCoordinator::P2 ? Turn::White : Turn::Black);
    };
    hooks.ui.setPlayersNames = [this](const QString& p1, const QString& p2) {
        m_mw.m_playerInfoWiring->onSetPlayersNames(p1, p2);
    };
    hooks.ui.setEngineNames = [this](const QString& e1, const QString& e2) {
        m_mw.m_playerInfoWiring->onSetEngineNames(e1, e2);
    };
    hooks.ui.renderBoardFromGc = [adapter]() {
        adapter->renderBoardFromGc();
    };
    hooks.ui.showGameOverDialog = [adapter](const QString& title, const QString& message) {
        adapter->showGameOverMessageBox(title, message);
    };
    hooks.ui.usesHandicapNames = [this]() -> bool {
        const auto* gameInfo = m_mw.m_playerInfoWiring ? m_mw.m_playerInfoWiring->gameInfoController() : nullptr;
        return KifuExportMetadataBuilder::usesHandicapNames(
            gameInfo ? gameInfo->gameInfo() : QList<KifGameInfoItem>(), m_mw.m_state.startSfenStr);
    };
    hooks.ui.showMoveHighlights = [adapter](const QPoint& from, const QPoint& to) {
        adapter->showMoveHighlights(from, to);
    };

    hooks.game.initializeNewGame = [adapter](const QString& sfen) {
        adapter->initializeNewGameHook(sfen);
    };
    hooks.game.appendKifuLine = [this](const QString& text, const QString& elapsed) {
        m_kifu->ensureGameRecordUpdateService();
        if (m_mw.m_gameRecordUpdateService) {
            m_mw.m_gameRecordUpdateService->appendKifuLine(text, elapsed);
        }
    };
    hooks.game.appendEvalP1 = [this]() {
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine1Graph(-1);
    };
    hooks.game.appendEvalP2 = [this]() {
        if (m_mw.m_evalGraphController) m_mw.m_evalGraphController->redrawEngine2Graph(-1);
    };
    hooks.game.autoSaveKifu = [this](const QString& saveDir) {
        m_kifu->ensureKifuFileController();
        m_mw.m_kifuFileController->autoSaveKifuToFile(saveDir);
    };
    return hooks;
}

MatchCoordinator::UndoHooks MainWindowServiceRegistry::buildUndoHooks()
{
    MatchCoordinator::UndoHooks hooks;
    hooks.updateHighlightsForPly = [this](int ply) {
        m_foundation->ensureKifuNavigationCoordinator();
        m_mw.m_kifuNavCoordinator->syncBoardAndHighlightsAtRow(ply);
    };
    auto* adapter = m_mw.m_matchAdapter.get();
    hooks.updateTurnAndTimekeepingDisplay = [adapter]() {
        adapter->updateTurnAndTimekeepingDisplay();
    };
    hooks.isHumanSide = [this](Turn player) {
        return queryIsHumanSide(player);
    };
    hooks.setMouseClickMode = [this](bool mode) {
        if (m_mw.m_shogiView) m_mw.m_shogiView->setMouseClickMode(mode);
    };
    return hooks;
}

MatchCoordinatorWiring::Deps MainWindowServiceRegistry::buildMatchWiringDeps()
{
    MatchCoordinatorWiring::Deps d;
    d.gc     = m_mw.m_gameController;
    d.view   = m_mw.m_shogiView;
    d.comm1  = m_mw.m_models.commLog1;
    d.think1 = m_mw.m_models.thinking1;
    d.comm2  = m_mw.m_models.commLog2;
    d.think2 = m_mw.m_models.thinking2;
    d.sfenRecord = &m_mw.m_kifu.sfenRecord;
    d.gameMoves  = &m_mw.m_kifu.gameMoves;
    d.playMode   = &m_mw.m_state.playMode;
    d.timePresenter   = m_mw.m_timePresenter;
    d.boardController = m_mw.m_boardController;
    d.kifuRecordModel = m_mw.m_models.kifuRecord;
    d.hooks     = buildMatchHooks();
    d.undoHooks = buildUndoHooks();

    d.ensureTimeController = [this]() { ensureTimeController(); };
    d.ensureEvaluationGraphController = [this]() { m_foundation->ensureEvaluationGraphController(); };
    d.ensureUsiCommandController = [this]() { ensureUsiCommandController(); };
    d.ensureUiStatePolicyManager = [this]() { m_foundation->ensureUiStatePolicyManager(); };
    d.connectBoardClicks = [this]() { m_mw.m_signalRouter->connectBoardClicks(); };
    d.connectMoveRequested = [this]() { m_mw.m_signalRouter->connectMoveRequested(); };

    d.getClock = [this]() -> ShogiClock* {
        return m_mw.m_timeController ? m_mw.m_timeController->clock() : nullptr;
    };
    d.getTimeController = [this]() -> TimeControlController* { return m_mw.m_timeController; };
    d.getEvalGraphController = [this]() -> EvaluationGraphController* { return m_mw.m_evalGraphController.get(); };
    d.getUiStatePolicy = [this]() -> UiStatePolicyManager* { return m_mw.m_uiStatePolicy; };
    return d;
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
    deps.sfenRecord = &m_mw.m_kifu.sfenRecord;
    deps.analysisTab = m_mw.m_analysisTab;
    deps.boardSetupController = m_mw.m_boardSetupController;
    deps.usiCommLog = m_mw.m_models.commLog1;
    deps.engineThinking = m_mw.m_models.thinking1;
    deps.timeController = m_mw.m_timeController;
    deps.gameMoves = &m_mw.m_kifu.gameMoves;
    deps.playMode = &m_mw.m_state.playMode;
    deps.parentWidget = &m_mw;

    deps.prepareRecord = [this](const QString& startSfen, const QString& black, const QString& white) {
        m_mw.m_state.startSfenStr = startSfen;
        m_mw.m_state.currentSfenStr = startSfen;
        m_mw.m_player.humanName1 = black;
        m_mw.m_player.humanName2 = white;
        m_mw.m_player.engineName1.clear();
        m_mw.m_player.engineName2.clear();
        if (m_mw.m_match) m_mw.m_match->clearGameOverState();
        m_foundation->ensurePlayerInfoWiring();
        m_mw.m_playerInfoWiring->ensureGameInfoController();
        auto* gameInfo = m_mw.m_playerInfoWiring->gameInfoController();
        gameInfo->commitPendingEditor();
        const auto preparedInfo = gameInfo->gameInfo();
        resetModels(startSfen);
        gameInfo->setGameInfo(preparedInfo);
        // 持ち時間と開始日時は CsaGameWiring がサーバーの対局条件で TimeControlController に設定済み
        const auto* tc = m_mw.m_timeController;
        const QDateTime startTime = tc && tc->gameStartDateTime().isValid()
                                        ? tc->gameStartDateTime() : QDateTime::currentDateTime();
        m_mw.m_playerInfoWiring->setGameInfoForMatchStart(
            startTime, black, white, KifuExportMetadataBuilder::handicapLabel(startSfen),
            tc ? tc->timeControl() : TimeControl{});
        startLiveGameSessionIfNeeded();
        m_kifu->ensureGameRecordUpdateService();
    };
    deps.recordGameEnd = [this]() {
        if (!m_mw.m_timeController) return;
        m_mw.m_timeController->recordGameEndTime();
        if (m_mw.m_playerInfoWiring)
            m_mw.m_playerInfoWiring->updateGameInfoWithEndTime(m_mw.m_timeController->gameEndDateTime());
    };
    deps.saveKifuToPath = [this](const QString& path, QString* error) {
        m_kifu->ensureKifuFileController();
        return m_mw.m_kifuFileController && m_mw.m_kifuFileController->saveKifuToPath(path, error);
    };
    deps.setBoardFlipped = [this](bool flipped) {
        // 通常対局と同じ経路で反転し、評価値グラフや名前表示の向きも揃える
        if (m_mw.m_shogiView && m_mw.m_match && m_mw.m_shogiView->flipMode() != flipped) m_mw.m_match->flipBoard();
    };
    deps.syncPly = [this](int ply) {
        // 表示中の手数（分岐ツリーのカーソル）は LiveGameSession の着手通知で動く
        const auto* positions = &m_mw.m_kifu.sfenRecord;
        if (positions && ply >= 0 && ply < positions->size())
            m_mw.m_state.currentSfenStr = positions->at(ply);
    };

    m_mw.m_csaGameWiring = std::make_unique<CsaGameWiring>(deps);

    // 外部シグナル接続を CsaGameWiring に委譲
    m_foundation->ensureUiStatePolicyManager();
    m_kifu->ensureGameRecordUpdateService();
    m_foundation->ensureUiNotificationService();
    m_mw.m_csaGameWiring->wireExternalSignals(m_mw.m_uiStatePolicy,
                                              m_mw.m_gameRecordUpdateService.get(),
                                              m_mw.m_notificationService);

    // 通信対局の相手・自エンジンの着手で駒音を鳴らす
    m_foundation->ensurePieceSoundPlayer();
    connect(m_mw.m_csaGameWiring.get(), &CsaGameWiring::moveAppliedToBoard,
            m_mw.m_registryParts.pieceSoundPlayer, &PieceSoundPlayer::playMoveSound,
            Qt::UniqueConnection);

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
    m_mw.m_consecutiveGamesController->setPerformPreStartCleanup([this](const QString& startSfen) {
        ensureSessionLifecycleCoordinator();
        if (m_mw.m_sessionLifecycle) {
            m_mw.m_sessionLifecycle->prepareNextGame(startSfen);
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
// PlayModePolicyService 中継メソッド（終了処理中は既定値 false を返す）
// ---------------------------------------------------------------------------

bool MainWindowServiceRegistry::queryIsHumanSide(Turn player)
{
    return m_mw.m_playModePolicy && m_mw.m_playModePolicy->isHumanSide(player);
}

// ---------------------------------------------------------------------------
// MatchCoordinator初期化
// ---------------------------------------------------------------------------

void MainWindowServiceRegistry::initMatchCoordinator()
{
    if (!m_mw.m_gameController || !m_mw.m_shogiView) return;

    ensureMatchCoordinatorWiring();

    // 司令塔は初回だけ作られ、以後は同じものに依存を結び直す（MatchCoordinatorWiring::initializeSession）
    if (!m_mw.m_matchWiring->initializeSession()) {
        return;
    }

    m_mw.m_match = m_mw.m_matchWiring->match();
    m_mw.refreshPlayModePolicyDeps();
    refreshAppearanceDeps();
    ensureReplayController();
    ensureGameStateController();
    ensureSessionLifecycleCoordinator();

    // 司令塔より先に作られていた部品にも司令塔を渡す（2回目以降は同じものを渡すだけ）
    if (m_mw.m_considerationWiring) {
        m_mw.m_considerationWiring->setMatchCoordinator(m_mw.m_match);
    }
}
