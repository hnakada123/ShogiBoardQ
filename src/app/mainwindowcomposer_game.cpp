/// @file mainwindowcomposer_game.cpp
/// @brief 対局系（司令塔・開始・終局・手番・CSA 通信対局）の依存の注入

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "boardinteractioncontroller.h"
#include "boardsetupcontroller.h"
#include "consecutivegamescontroller.h"
#include "considerationwiring.h"
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "dialogcoordinator.h"
#include "engineanalysistab.h"
#include "evaluationgraphcontroller.h"
#include "gameinfopanecontroller.h"
#include "gamerecordmodel.h"
#include "gamerecordupdateservice.h"
#include "gamesessionorchestrator.h"
#include "gamestatecontroller.h"
#include "josekiwindowwiring.h"
#include "kifuexportmetadata.h"
#include "kifufilecontroller.h"
#include "kifuloadcoordinator.h"
#include "kifunavigationcoordinator.h"
#include "livegamesessionupdater.h"
#include "mainwindowactions.h"
#include "mainwindowappearancecontroller.h"
#include "matchcoordinatorwiring.h"
#include "piecesoundplayer.h"
#include "playerinfowiring.h"
#include "playmodepolicyservice.h"
#include "prestartcleanuphandler.h"
#include "recordpane.h"
#include "replaycontroller.h"
#include "sessionlifecyclecoordinator.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "turnstatesyncservice.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#include "undoflowservice.h"
#include "usicommandcontroller.h"

#include <QDateTime>
#include <functional>
#include <QTableView>

// ---------------------------------------------------------------------------
// 司令塔（MatchCoordinator）と対局開始
// ---------------------------------------------------------------------------

MatchCoordinator::Hooks MainWindowComposer::buildMatchHooks()
{
    // 司令塔は一度だけ作るので、フックは呼ばれた時点の部品を読む
    MatchCoordinator::Hooks hooks;

    hooks.ui.updateTurnDisplay = [this](Turn cur) {
        m_c.turnStateSync->onTurnManagerChanged(cur);
    };
    hooks.ui.setPlayersNames = [this](const QString& p1, const QString& p2) {
        m_c.playerInfoWiring->onSetPlayersNames(p1, p2);
    };
    hooks.ui.setEngineNames = [this](const QString& e1, const QString& e2) {
        m_c.playerInfoWiring->onSetEngineNames(e1, e2);
    };
    hooks.ui.renderBoardFromGc = [this]() {
        if (m_c.gameController->board()) m_c.shogiView->applyBoardAndRender(m_c.gameController->board());
    };
    hooks.ui.showGameOverDialog = [this](const QString& title, const QString& message) {
        if (m_c.dialogCoordinator) m_c.dialogCoordinator->showGameOverMessage(title, message);
    };
    hooks.ui.usesHandicapNames = [this]() -> bool {
        const auto* gameInfo = m_c.playerInfoWiring->gameInfoController();
        return KifuExportMetadataBuilder::usesHandicapNames(
            gameInfo ? gameInfo->gameInfo() : QList<KifGameInfoItem>(), m_state.startSfenStr);
    };
    hooks.ui.showMoveHighlights = [this](const QPoint& from, const QPoint& to) {
        if (m_c.boardController) m_c.boardController->showMoveHighlights(from, to);
    };

    hooks.game.initializeNewGame = [this](const QString& sfen) {
        // 盤モデルを開始局面にして描き直し、対局者名・ログの見出しをモードに合わせる
        QString start = sfen;
        m_c.gameController->newGame(start);
        if (m_c.gameController->board()) m_c.shogiView->applyBoardAndRender(m_c.gameController->board());
        m_c.playerInfoWiring->applyPlayersNamesForMode();
        m_c.playerInfoWiring->applyEngineNamesToLogModels();
        m_c.playerInfoWiring->applySecondEngineVisibility();
    };
    hooks.game.appendRecord = [this](const RecordEntry& entry) {
        if (m_c.gameRecordUpdateService) m_c.gameRecordUpdateService->appendRecord(entry);
    };
    hooks.game.appendEval = [this](Turn side) {
        if (side == Turn::Black) m_c.evalGraphController->redrawEngine1Graph(-1);
        else m_c.evalGraphController->redrawEngine2Graph(-1);
    };
    hooks.game.autoSaveKifu = [this](const QString& saveDir) {
        m_c.kifuFileController->autoSaveKifuToFile(saveDir);
    };
    return hooks;
}

MatchCoordinator::UndoHooks MainWindowComposer::buildUndoHooks()
{
    MatchCoordinator::UndoHooks hooks;
    hooks.updateHighlightsForPly = [this](int ply) {
        m_c.kifuNavCoordinator->syncBoardAndHighlightsAtRow(ply);
    };
    hooks.updateTurnAndTimekeepingDisplay = [this]() {
        m_c.turnStateSync->updateTurnAndTimekeepingDisplay();
    };
    hooks.isHumanSide = [this](Turn player) {
        return m_c.playModePolicy && m_c.playModePolicy->isHumanSide(player);
    };
    hooks.setMouseClickMode = [this](bool mode) {
        m_c.shogiView->setMouseClickMode(mode);
    };
    return hooks;
}

void MainWindowComposer::wireMatchCoordinator()
{
    // 司令塔と GameStartCoordinator のシグナルの受け手。作るときに直接つなぐので先に登録する
    MatchCoordinatorWiring::SignalTargets targets;
    targets.gameSession = m_c.gameSessionOrchestrator;
    targets.sessionLifecycle = m_c.sessionLifecycle;
    targets.gameStateController = m_c.gameStateController;
    targets.consecutiveGamesController = m_c.consecutiveGamesController;
    targets.appearance = m_c.appearanceController.get();
    targets.playerInfo = m_c.playerInfoWiring;
    targets.kifuNav = m_c.kifuNavCoordinator.get();
    targets.branchNav = m_c.branchNavWiring.get();
    targets.uiStatePolicy = m_c.uiStatePolicy;
    m_c.matchWiring->setSignalTargets(targets);

    MatchCoordinatorWiring::Deps d;
    d.gc = m_c.gameController;
    d.view = m_c.shogiView;
    d.comm1 = m_c.commLog1;
    d.think1 = m_c.thinking1;
    d.comm2 = m_c.commLog2;
    d.think2 = m_c.thinking2;
    d.sfenRecord = &m_state.sfenRecord;
    d.gameMoves = &m_state.gameMoves;
    d.playMode = &m_state.playMode;
    d.timePresenter = m_c.timePresenter;
    d.boardController = m_c.boardController;
    d.kifuRecordModel = m_c.kifuRecord;
    d.clock = m_c.timeController->clock();
    d.timeController = m_c.timeController;
    d.evalGraphController = m_c.evalGraphController.get();
    d.usiCommandController = m_c.usiCommandController.get();
    d.hooks = buildMatchHooks();
    d.undoHooks = buildUndoHooks();
    m_c.matchWiring->updateDeps(d);

    // 司令塔は起動時に一度だけ作り、作り直さない
    m_c.matchWiring->initializeSession();
    m_c.match = m_c.matchWiring->match();
    m_c.matchWiring->createMenuGameStartCoordinator();
    m_c.gameStart = m_c.matchWiring->menuGameStartCoordinator();

    // 司令塔ができたので、司令塔を持つ部品に渡す
    MainWindowAppearanceController::Deps ad;
    ad.shogiView = m_c.shogiView;
    ad.timePresenter = m_c.timePresenter;
    ad.match = m_c.match;
    ad.bottomSide = &m_state.bottomSide;
    m_c.appearanceController->updateDeps(ad);

    // 棋譜追記（司令塔の終局状態を見るので、司令塔の後に作る）
    GameRecordUpdateService::Deps rd;
    rd.gameRecord = m_c.gameRecord;
    rd.liveSessionUpdater = m_c.liveGameSessionUpdater.get();
    rd.match = m_c.match;
    rd.liveGameSession = m_c.liveGameSession;
    rd.currentSfenStr = &m_state.currentSfenStr;
    rd.sfenRecord = &m_state.sfenRecord;
    m_c.gameRecordUpdateService = std::make_unique<GameRecordUpdateService>(rd);
}

// ---------------------------------------------------------------------------
// CSA 通信対局
// ---------------------------------------------------------------------------

void MainWindowComposer::wireCsaGame()
{
    CsaGameWiring::Dependencies deps;
    deps.gameController = m_c.gameController;
    deps.shogiView = m_c.shogiView;
    deps.boardController = m_c.boardController;
    deps.statusBar = m_ui.statusbar;
    deps.sfenRecord = &m_state.sfenRecord;
    deps.analysisTab = m_c.analysisTab;
    deps.boardSetupController = m_c.boardSetupController;
    deps.usiCommLog = m_c.commLog1;
    deps.engineThinking = m_c.thinking1;
    deps.timeController = m_c.timeController;
    deps.gameMoves = &m_state.gameMoves;
    deps.playMode = &m_state.playMode;
    deps.parentWidget = &m_window;

    deps.prepareRecord = [this](const QString& startSfen, const QString& black, const QString& white) {
        // 局面列・指し手列は CsaGameCoordinator が再開局面まで用意済み。棋譜の作り直しで消さない
        const QStringList positions = m_state.sfenRecord;
        const QList<ShogiMove> moves = m_state.gameMoves;
        m_state.startSfenStr = startSfen;
        m_state.currentSfenStr = startSfen;
        m_state.humanName1 = black;
        m_state.humanName2 = white;
        m_state.engineName1.clear();
        m_state.engineName2.clear();
        m_c.match->clearGameOverState();
        auto* gameInfo = m_c.playerInfoWiring->gameInfoController();
        gameInfo->commitPendingEditor();
        const auto preparedInfo = gameInfo->gameInfo();
        m_c.sessionLifecycle->resetModels(startSfen);
        gameInfo->setGameInfo(preparedInfo);
        // 持ち時間と開始日時は CsaGameWiring がサーバーの対局条件で TimeControlController に設定済み
        const auto* tc = m_c.timeController;
        const QDateTime startTime = tc->gameStartDateTime().isValid()
                                        ? tc->gameStartDateTime() : QDateTime::currentDateTime();
        m_c.playerInfoWiring->setGameInfoForMatchStart(
            startTime, black, white, KifuExportMetadataBuilder::handicapLabel(startSfen), tc->timeControl());
        m_state.sfenRecord = positions;
        m_state.gameMoves = moves;
        m_c.liveGameSessionUpdater->startSessionIfNeeded();
    };
    deps.recordGameEnd = [this]() {
        m_c.timeController->recordGameEndTime();
        m_c.playerInfoWiring->updateGameInfoWithEndTime(m_c.timeController->gameEndDateTime());
    };
    deps.saveKifuToPath = [this](const QString& path, QString* error) {
        return m_c.kifuFileController->saveKifuToPath(path, error);
    };
    deps.setBoardFlipped = [this](bool flipped) {
        // 通常対局と同じ経路で反転し、評価値グラフや名前表示の向きも揃える
        if (m_c.shogiView->flipMode() != flipped) m_c.match->flipBoard();
    };

    m_c.csaGameWiring = std::make_unique<CsaGameWiring>(deps);
    // 司令塔は最初の通信対局で作られる。作られたとき、棋譜行・エラーはこれらへ直接つながる
    m_c.csaGameWiring->wireExternalSignals(m_c.uiStatePolicy,
                                           m_c.gameRecordUpdateService.get(),
                                           m_c.notificationService);
}

// ---------------------------------------------------------------------------
// 対局の状態・開始・終局・手番・手戻し
// ---------------------------------------------------------------------------

void MainWindowComposer::wireGame()
{
    // 対局モードの判定
    PlayModePolicyService::Deps pd;
    pd.playMode = &m_state.playMode;
    pd.gameController = m_c.gameController;
    pd.match = m_c.match;
    pd.csaGameWiring = m_c.csaGameWiring.get();
    pd.canRecordVariationMove = [this]() { return m_c.actions && m_c.actions->canRecordVariationMove(); };
    m_c.playModePolicy->updateDeps(pd);
    m_c.boardController->setIsHumanTurnCallback([this]() -> bool {
        return m_c.playModePolicy && m_c.playModePolicy->isHumanTurnNow();
    });
    m_c.josekiWiring->setPlayModePolicy(m_c.playModePolicy.get());

    // 手番同期（TurnManager を作り、GC と表示をつなぐ）
    TurnStateSyncService::Deps td;
    td.gameController = m_c.gameController;
    td.shogiView = m_c.shogiView;
    td.clock = m_c.timeController->clock();
    td.timePresenter = m_c.timePresenter;
    td.isGameActivelyInProgress = std::bind(&PlayModePolicyService::isGameActivelyInProgress,
                                            m_c.playModePolicy.get());
    m_c.turnStateSync->updateDeps(td);

    // リプレイ
    m_c.replayController->setClock(m_c.timeController->clock());
    m_c.replayController->setShogiView(m_c.shogiView);
    m_c.replayController->setGameController(m_c.gameController);
    m_c.replayController->setMatchCoordinator(m_c.match);

    // ゲーム状態（終局時の UI 更新）
    m_c.gameStateController->setMatchCoordinator(m_c.match);
    m_c.gameStateController->setTimeController(m_c.timeController);
    m_c.gameStateController->setKifuRecordModel(m_c.kifuRecord);
    GameStateController::Hooks gh;
    gh.enableArrowButtons = [this]() { m_c.uiStatePolicy->transitionToIdle(); };
    gh.setReplayMode = [this](bool on) { m_c.replayController->setReplayMode(on); };
    gh.syncCursorToPly = [this](int ply) { m_c.kifuNavCoordinator->syncNavStateToPly(ply); };
    gh.updateBoardView = [this]() { m_c.shogiView->update(); };
    gh.setGameOverStyleLock = [this](bool locked) { m_c.shogiView->setGameOverStyleLock(locked); };
    gh.setMouseClickMode = [this](bool mode) { m_c.shogiView->setMouseClickMode(mode); };
    gh.removeHighlightAllData = [this]() { m_c.shogiView->removeHighlightAllData(); };
    gh.setKifuViewSingleSelection = [this]() {
        if (m_c.recordPane->kifuView())
            m_c.recordPane->kifuView()->setSelectionMode(QAbstractItemView::SingleSelection);
    };
    m_c.gameStateController->setHooks(gh);

    // 手戻し
    UndoFlowService::Deps ud;
    ud.undoTwoPlies = [this]() {
        return !m_c.match->gameOverState().isOver && m_c.match->undoTwoPlies();
    };
    ud.markGameRecordDirty = std::bind(&GameRecordModel::markDirty, m_c.gameRecord);
    ud.liveSession = m_c.liveGameSession;
    ud.recordPresenter = m_c.recordPresenter;
    ud.evalGraphController = m_c.evalGraphController.get();
    ud.playMode = &m_state.playMode;
    ud.sfenRecord = &m_state.sfenRecord;
    ud.currentSfenStr = &m_state.currentSfenStr;
    m_c.undoFlowService->updateDeps(ud);

    // ライブセッション
    LiveGameSessionUpdater::Deps ld;
    ld.liveSession = m_c.liveGameSession;
    ld.branchTree = m_c.branchTree;
    ld.sfenRecord = &m_state.sfenRecord;
    ld.currentSfenStr = &m_state.currentSfenStr;
    m_c.liveGameSessionUpdater->updateDeps(ld);

    // 連続対局
    m_c.consecutiveGamesController->setTimeController(m_c.timeController);
    m_c.consecutiveGamesController->setGameStartCoordinator(m_c.gameStart);
    m_c.consecutiveGamesController->setPerformPreStartCleanup([this](const QString& startSfen) {
        m_c.sessionLifecycle->prepareNextGame(startSfen);
    });

    // UI 起点の対局操作（開始ダイアログ・投了・中断・再開）
    GameSessionOrchestrator::Deps gd;
    gd.gameStateController = m_c.gameStateController;
    gd.gameStart = m_c.gameStart;
    gd.dialogCoordinator = m_c.dialogCoordinator;
    gd.csaGameWiring = m_c.csaGameWiring.get();
    gd.match = m_c.match;
    gd.replayController = m_c.replayController;
    gd.gameController = m_c.gameController;
    gd.kifuModel = m_c.kifuRecord;
    gd.playMode = &m_state.playMode;
    gd.currentSfenStr = &m_state.currentSfenStr;
    gd.startSfenStr = &m_state.startSfenStr;
    gd.gameMoves = &m_state.gameMoves;
    gd.bottomSide = &m_state.bottomSide;
    gd.branchTree = m_c.branchTree;
    gd.navState = m_c.navState;
    gd.liveSession = m_c.liveGameSession;
    gd.uiStatePolicy = m_c.uiStatePolicy;
    gd.sessionLifecycle = m_c.sessionLifecycle;
    gd.kifuFileController = m_c.kifuFileController;
    gd.playerInfoWiring = m_c.playerInfoWiring;
    gd.timeController = m_c.timeController;
    gd.shogiView = m_c.shogiView;
    gd.gameRecord = m_c.gameRecord;
    m_c.gameSessionOrchestrator->updateDeps(gd);

    // セッションのライフサイクル（リセット・開始・終局）
    SessionLifecycleCoordinator::Deps sd;
    sd.state = &m_state;
    sd.match = m_c.match;
    sd.csaGameWiring = m_c.csaGameWiring.get();
    sd.replayController = m_c.replayController;
    sd.gameController = m_c.gameController;
    sd.gameStateController = m_c.gameStateController;
    sd.timeController = m_c.timeController;
    sd.consecutiveGamesController = m_c.consecutiveGamesController;
    sd.shogiView = m_c.shogiView;
    sd.preStartCleanupHandler = m_c.preStartCleanupHandler;
    sd.playerInfoWiring = m_c.playerInfoWiring;
    sd.josekiWiring = m_c.josekiWiring.get();
    sd.uiStatePolicy = m_c.uiStatePolicy;
    sd.sessionUi.commLog1 = m_c.commLog1;
    sd.sessionUi.commLog2 = m_c.commLog2;
    sd.sessionUi.thinking1 = m_c.thinking1;
    sd.sessionUi.thinking2 = m_c.thinking2;
    sd.sessionUi.consideration = m_c.consideration;
    sd.sessionUi.analysisTab = m_c.analysisTab;
    sd.sessionUi.analysisModel = m_c.analysis;
    sd.sessionUi.evalChart = m_c.evalChart;
    sd.sessionUi.evalGraphController = m_c.evalGraphController.get();
    sd.sessionUi.commentCoordinator = m_c.commentCoordinator;
    sd.models.navState = m_c.navState;
    sd.models.displayCoordinator = m_c.displayCoordinator;
    sd.models.boardController = m_c.boardController;
    sd.models.gameRecord = m_c.gameRecord;
    sd.models.evalGraphController = m_c.evalGraphController.get();
    sd.models.timeController = m_c.timeController;
    sd.models.sfenRecord = &m_state.sfenRecord;
    sd.models.gameInfoController = m_c.gameInfoController;
    sd.models.kifuLoadCoordinator = m_c.kifuLoadCoordinator;
    sd.models.branchTree = m_c.branchTree;
    sd.models.liveGameSession = m_c.liveGameSession;
    m_c.sessionLifecycle->updateDeps(sd);
}
