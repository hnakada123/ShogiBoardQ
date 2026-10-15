/// @file mainwindowcomposer_game.cpp
/// @brief 対局系（司令塔・開始・終局・手番・検討・CSA 通信対局）の依存の注入とシグナル接続

#include "mainwindowcomposer.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "analysissession.h"
#include "boardinteractioncontroller.h"
#include "boardsyncpresenter.h"
#include "candidatearrowcontroller.h"
#include "consecutivegamescontroller.h"
#include "csagamewiring.h"
#include "dialogcoordinator.h"
#include "enginelifecyclemanager.h"
#include "evaluationgraphcontroller.h"
#include "gamesession.h"
#include "gamestartcoordinator.h"
#include "josekiwindowcontroller.h"
#include "kifufilecontroller.h"
#include "kifunavigationcoordinator.h"
#include "mainwindowactions.h"
#include "mainwindowappearancecontroller.h"
#include "playerinfopresenter.h"
#include "humanturnpolicy.h"
#include "prestartcleanuphandler.h"
#include "replaycontroller.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "timecontrolcontroller.h"
#include "timedisplaypresenter.h"
#include "turnstatesyncservice.h"
#include "uinotificationservice.h"
#include "uistatepolicymanager.h"
#include "usicommandcontroller.h"

#include <functional>

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
    deps.analysisTab = m_c.analysisTab;
    deps.boardSetupController = m_c.boardSetupController;
    deps.usiCommLog = m_c.commLog1;
    deps.engineThinking = m_c.thinking1;
    deps.timeController = m_c.timeController;
    deps.activity = &m_state.activity;
    deps.parentWidget = &m_window;
    deps.prepareRecord = std::bind(&GameSession::beginCsaGameRecord, m_c.gameSession,
                                   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    deps.recordGameEnd = std::bind(&GameSession::recordGameEndTime, m_c.gameSession);
    deps.saveKifuToPath = std::bind(&KifuFileController::saveKifuToPath, m_c.kifuFileController,
                                    std::placeholders::_1, std::placeholders::_2);
    deps.setBoardFlipped = [this](bool flipped) {
        // 通常対局と同じ経路で反転し、評価値グラフや名前表示の向きも揃える
        if (m_c.shogiView->flipMode() != flipped) m_c.appearanceController->flipBoard();
    };

    m_c.csaGameWiring = std::make_unique<CsaGameWiring>(deps);
    // 司令塔は最初の通信対局で作られる。作られたとき、棋譜行・エラーはこれらへ直接つながる
    m_c.csaGameWiring->wireExternalSignals(m_c.uiStatePolicy, m_c.gameSession, m_c.notificationService);
}

// ---------------------------------------------------------------------------
// 対局の状態・開始・終局・手番・手戻し・検討
// ---------------------------------------------------------------------------

void MainWindowComposer::wireGame()
{
    // 対局の棋譜の行は GameSession がライブ対局のセッション（分岐ツリー）に書く
    m_c.match->setRecordSink(m_c.gameSession);

    // 盤を操作できるか・対局が進行中かの判定
    HumanTurnPolicy::Deps pd;
    pd.activity = &m_state.activity;
    pd.gameController = m_c.gameController;
    pd.match = m_c.match;
    pd.csaGameWiring = m_c.csaGameWiring.get();
    pd.canRecordVariationMove = [this]() { return m_c.actions && m_c.actions->canRecordVariationMove(); };
    m_c.humanTurnPolicy->updateDeps(pd);
    m_c.boardController->setIsHumanTurnCallback([this]() -> bool {
        return m_c.humanTurnPolicy && m_c.humanTurnPolicy->isHumanTurnNow();
    });
    m_c.josekiController->setHumanTurnCheck(std::bind(&HumanTurnPolicy::isHumanTurnNow, m_c.humanTurnPolicy.get()));

    // 手番同期（TurnManager を作り、GC と表示をつなぐ）
    TurnStateSyncService::Deps td;
    td.gameController = m_c.gameController;
    td.shogiView = m_c.shogiView;
    td.clock = m_c.timeController->clock();
    td.timePresenter = m_c.timePresenter;
    td.isGameActivelyInProgress = std::bind(&HumanTurnPolicy::isGameActivelyInProgress,
                                            m_c.humanTurnPolicy.get());
    m_c.turnStateSync->updateDeps(td);

    // リプレイ・時間切れ
    m_c.replayController->setClock(m_c.timeController->clock());
    m_c.replayController->setShogiView(m_c.shogiView);
    m_c.replayController->setGameController(m_c.gameController);
    m_c.replayController->setMatchCoordinator(m_c.match);
    m_c.timeController->setMatchCoordinator(m_c.match);

    // エンジンの枠を読む部品（候補手の矢印・評価値グラフ・USI コマンドの手動送信）
    CandidateArrowController::Sources arrows;
    arrows.match = m_c.match;
    arrows.engines = m_c.engines;
    arrows.activity = &m_state.activity;
    arrows.analysis = m_c.analysisSession;
    CandidateArrowController::forView(m_c.shogiView)->setSources(arrows);
    m_c.evalGraphController->setEngines(m_c.engines);
    m_c.usiCommandController->setEngines(m_c.engines);

    // 対局の流れの UI 側
    GameSession::Deps gd;
    gd.state = &m_state;
    gd.match = m_c.match;
    gd.gameStart = m_c.gameStart;
    gd.csa = m_c.csaGameWiring.get();
    gd.analysis = m_c.analysisSession;
    gd.consecutive = m_c.consecutiveGamesController;
    gd.preStartCleanup = m_c.preStartCleanupHandler;
    gd.gameController = m_c.gameController;
    gd.shogiView = m_c.shogiView;
    gd.boardController = m_c.boardController;
    gd.boardSync = m_c.boardSync;
    gd.turnSync = m_c.turnStateSync.get();
    gd.timeController = m_c.timeController;
    gd.replayController = m_c.replayController;
    gd.uiStatePolicy = m_c.uiStatePolicy;
    gd.playerInfo = m_c.playerInfo;
    gd.josekiController = m_c.josekiController.get();
    gd.dialogCoordinator = m_c.dialogCoordinator;
    gd.kifuFileController = m_c.kifuFileController;
    gd.branchTree = m_c.branchTree;
    gd.navState = m_c.navState;
    gd.kifuNav = m_c.kifuNavCoordinator.get();
    gd.liveSession = m_c.liveGameSession;
    gd.gameRecord = m_c.gameRecord;
    gd.kifuRecordModel = m_c.kifuRecord;
    gd.recordPane = m_c.recordPane;
    gd.evalGraph = m_c.evalGraphController.get();
    gd.sessionUi.commLog1 = m_c.commLog1;
    gd.sessionUi.commLog2 = m_c.commLog2;
    gd.sessionUi.thinking1 = m_c.thinking1;
    gd.sessionUi.thinking2 = m_c.thinking2;
    gd.sessionUi.consideration = m_c.consideration;
    gd.sessionUi.analysisTab = m_c.analysisTab;
    gd.sessionUi.analysisModel = m_c.analysis;
    gd.sessionUi.evalGraphController = m_c.evalGraphController.get();
    gd.sessionUi.commentCoordinator = m_c.commentCoordinator;
    gd.models.navState = m_c.navState;
    gd.models.displayCoordinator = m_c.displayCoordinator;
    gd.models.boardController = m_c.boardController;
    gd.models.gameRecord = m_c.gameRecord;
    gd.models.evalGraphController = m_c.evalGraphController.get();
    gd.models.timeController = m_c.timeController;
    gd.models.gameInfoController = m_c.gameInfoController;
    gd.models.kifuLoadCoordinator = m_c.kifuLoadCoordinator;
    gd.models.branchTree = m_c.branchTree;
    gd.models.liveGameSession = m_c.liveGameSession;
    m_c.gameSession->setDeps(gd);

    connectMatchSignals();
    connectAnalysisSignals();
}

void MainWindowComposer::connectMatchSignals()
{
    MatchCoordinator* match = m_c.match;
    GameSession* session = m_c.gameSession;

    // --- 盤・手番・時計・評価値 ---
    connect(match, &MatchCoordinator::gamePrepared, session, &GameSession::onGamePrepared);
    connect(match, &MatchCoordinator::boardChanged, session, &GameSession::renderBoard);
    connect(match, &MatchCoordinator::turnChanged,
            m_c.turnStateSync.get(), &TurnStateSyncService::onTurnManagerChanged);
    connect(match, &MatchCoordinator::moveHighlightRequested,
            m_c.boardController, &BoardInteractionController::showMoveHighlights);
    connect(match, &MatchCoordinator::timeUpdated, m_c.timePresenter, &TimeDisplayPresenter::onMatchTimeUpdated);
    connect(match, &MatchCoordinator::engineMovePlayed,
            m_c.evalGraphController.get(), &EvaluationGraphController::plotEngineScore);
    connect(match, &MatchCoordinator::movesUndone, session, &GameSession::onMovesUndone);

    // --- 終局・中断 ---
    connect(match, &MatchCoordinator::gameOverStateChanged, session, &GameSession::onGameOverStateChanged);
    connect(match, &MatchCoordinator::gameEnded, session, &GameSession::onGameEnded);
    connect(match, &MatchCoordinator::gameOverMessage, m_c.dialogCoordinator, &DialogCoordinator::showInformation);
    connect(match, &MatchCoordinator::autoSaveRequested,
            m_c.kifuFileController, &KifuFileController::autoSaveKifuToFile);
    connect(match, &MatchCoordinator::gameEndProcessed, session, &GameSession::onGameEndProcessed);
    connect(match, &MatchCoordinator::gameInterrupted, session, &GameSession::onGameInterrupted);

    // --- 対局の開始 ---
    GameStartCoordinator* gameStart = m_c.gameStart;
    connect(gameStart, &GameStartCoordinator::requestPreStartCleanup, session, &GameSession::prepareStart);
    connect(gameStart, &GameStartCoordinator::requestApplyTimeControl, session, &GameSession::applyTimeControl);
    connect(gameStart, &GameStartCoordinator::started,
            m_c.uiStatePolicy, &UiStatePolicyManager::transitionToDuringGame);
    connect(gameStart, &GameStartCoordinator::started, session, &GameSession::onGameStarted);
    connect(gameStart, &GameStartCoordinator::playerNamesResolved,
            m_c.playerInfo, &PlayerInfoPresenter::onMatchPlayersResolved);
    connect(gameStart, &GameStartCoordinator::consecutiveGamesConfigured,
            m_c.consecutiveGamesController, &ConsecutiveGamesController::configure);
    connect(gameStart, &GameStartCoordinator::requestSelectKifuRow,
            m_c.kifuNavCoordinator.get(), &KifuNavigationCoordinator::selectKifuRow);
    connect(gameStart, &GameStartCoordinator::boardFlipRequested,
            m_c.appearanceController.get(), &MainWindowAppearanceController::flipBoard);

    // --- 連続対局 ---
    connect(m_c.consecutiveGamesController, &ConsecutiveGamesController::nextGameReady,
            session, &GameSession::startNextGame);
}

void MainWindowComposer::connectAnalysisSignals()
{
    AnalysisSession* analysis = m_c.analysisSession;
    connect(analysis, &AnalysisSession::considerationModeEnded,
            m_c.uiStatePolicy, &UiStatePolicyManager::transitionToIdle);
    connect(analysis, &AnalysisSession::tsumeSearchModeEnded,
            m_c.uiStatePolicy, &UiStatePolicyManager::transitionToIdle);
    connect(analysis, &AnalysisSession::engineNamesChanged,
            m_c.playerInfo, &PlayerInfoPresenter::onAnalysisEngineNamesChanged);
    connect(analysis, &AnalysisSession::messageRequested,
            m_c.dialogCoordinator, &DialogCoordinator::showInformation);
}
