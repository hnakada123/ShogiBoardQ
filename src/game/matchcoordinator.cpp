/// @file matchcoordinator.cpp
/// @brief 対局進行コーディネータ（司令塔）クラスの実装

#include "matchcoordinator.h"
#include "matchturnhandler.h"
#include "matchtimekeeper.h"
#include "matchundohandler.h"
#include "gameendhandler.h"
#include "gamestartorchestrator.h"
#include "analysissessionhandler.h"
#include "shogiclock.h"
#include "usi.h"
#include "shogiview.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

#include <QObject>
#include <QDebug>

using P = MatchCoordinator::Player;

// --- 初期化・破棄 ---

MatchCoordinator::MatchCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_view(d.view)
    , m_hooks(d.hooks)
    , m_externalGameMoves(d.gameMoves)
{
    m_sfenHistory = d.sfenRecord ? d.sfenRecord : &m_sharedSfenRecord;

    qCDebug(lcGame).noquote()
        << "shared sfenRecord*=" << static_cast<const void*>(m_sfenHistory);

    if (!d.sfenRecord) {
        qCWarning(lcGame) << "Deps.sfenRecord is null. Using internal sfen history buffer.";
    }

    // エンジンマネージャの初期化
    ensureEngineManager();
    m_engineManager->updateUsiPtrs(d.usi1, d.usi2);
    m_engineManager->setModelPtrs(d.comm1, d.think1, d.comm2, d.think2);

    // 時計マネージャの初期化
    ensureTimekeeper();
    m_timekeeper->setClock(d.clock);
}

MatchCoordinator::~MatchCoordinator() = default;

// --- ハンドラ遅延生成 ---

void MatchCoordinator::ensureEngineManager()
{
    if (m_engineManager) return;
    m_engineManager = new EngineLifecycleManager(this);

    EngineLifecycleManager::Refs refs;
    refs.gc       = m_gc;
    refs.playMode = &m_playMode;
    m_engineManager->setRefs(refs);

    EngineLifecycleManager::Hooks hooks;
    hooks.renderBoardFromGc = m_hooks.ui.renderBoardFromGc;
    hooks.appendEvalP1      = m_hooks.game.appendEvalP1;
    hooks.appendEvalP2      = m_hooks.game.appendEvalP2;
    hooks.onEngineResign    = [this](int idx) { handleEngineResign(idx); };
    hooks.onEngineWin       = [this](int idx) { handleEngineWin(idx); };
    hooks.computeGoTimes    = [this]() -> EngineLifecycleManager::GoTimes {
        ensureTimekeeper();
        const auto t = m_timekeeper->computeGoTimes();
        return { t.btime, t.wtime, t.byoyomi, t.binc, t.winc };
    };
    m_engineManager->setHooks(hooks);
}

void MatchCoordinator::ensureTimekeeper()
{
    if (m_timekeeper) return;
    m_timekeeper = new MatchTimekeeper(this);

    MatchTimekeeper::Refs refs;
    refs.gc = m_gc;
    m_timekeeper->setRefs(refs);

    MatchTimekeeper::Hooks hooks;
    hooks.remainingMsFor = [this](int p) -> qint64 {
        return m_hooks.time.remainingMsFor ? m_hooks.time.remainingMsFor(static_cast<Player>(p)) : 0;
    };
    hooks.incrementMsFor = [this](int p) -> qint64 {
        return m_hooks.time.incrementMsFor ? m_hooks.time.incrementMsFor(static_cast<Player>(p)) : 0;
    };
    hooks.byoyomiMs = [this]() -> qint64 {
        return m_hooks.time.byoyomiMs ? m_hooks.time.byoyomiMs() : 0;
    };
    m_timekeeper->setHooks(hooks);

    connect(m_timekeeper, &MatchTimekeeper::timeUpdated,
            this,         &MatchCoordinator::timeUpdated);
}

void MatchCoordinator::ensureMatchTurnHandler()
{
    if (m_turnHandler) return;
    m_turnHandler = std::make_unique<MatchTurnHandler>(*this);

    MatchTurnHandler::Refs refs;
    refs.gc              = m_gc;
    refs.currentTurn     = &m_cur;
    refs.playMode        = &m_playMode;
    refs.positionStr1    = &m_positionStr1;
    refs.positionPonder1 = &m_positionPonder1;
    refs.positionStrHistory = &m_positionStrHistory;
    refs.gameOver        = &m_gameOver;
    refs.mcAsParent      = this;
    m_turnHandler->setRefs(refs);

    MatchTurnHandler::Hooks hooks;
    hooks.renderBoardFromGc  = m_hooks.ui.renderBoardFromGc;
    hooks.updateTurnDisplayCb = m_hooks.ui.updateTurnDisplay;
    hooks.primaryEngine      = [this]() -> Usi* { return primaryEngine(); };
    hooks.secondaryEngine    = [this]() -> Usi* { return secondaryEngine(); };
    hooks.sendStopToEngine   = m_hooks.engine.sendStopToEngine;
    hooks.clockProvider      = [this]() -> ShogiClock* { return clock(); };
    hooks.initAndStartEngine = [this](Player p, const QString& path, const QString& name) {
        initializeAndStartEngineFor(p, path, name);
    };
    hooks.initEnginesForEvE  = [this](const QString& n1, const QString& n2) {
        initEnginesForEvE(n1, n2);
    };
    hooks.emitBoardFlipped   = [this](bool f) { emit boardFlipped(f); };
    hooks.handleBreakOff     = [this]() { handleBreakOff(); };
    hooks.handleEngineError  = [this](const QString& msg) -> bool {
        ensureAnalysisSession();
        return m_analysisSession->handleEngineError(msg);
    };
    hooks.showGameOverDialog = m_hooks.ui.showGameOverDialog;
    m_turnHandler->setHooks(hooks);
}

void MatchCoordinator::ensureAnalysisSession()
{
    if (m_analysisSession) return;
    m_analysisSession = std::make_unique<AnalysisSessionHandler>(this);

    connect(m_analysisSession.get(), &AnalysisSessionHandler::considerationModeEnded,
            this,                    &MatchCoordinator::considerationModeEnded);
    connect(m_analysisSession.get(), &AnalysisSessionHandler::tsumeSearchModeEnded,
            this,                    &MatchCoordinator::tsumeSearchModeEnded);
    connect(m_analysisSession.get(), &AnalysisSessionHandler::considerationWaitingStarted,
            this,                    &MatchCoordinator::considerationWaitingStarted);

    AnalysisSessionHandler::Hooks hooks;
    hooks.showGameOverDialog = m_hooks.ui.showGameOverDialog;
    hooks.destroyEnginesKeepModels = [this]() { destroyEngines(false); };
    hooks.isShutdownInProgress = [this]() -> bool {
        return m_engineManager && m_engineManager->isShutdownInProgress();
    };
    hooks.setPlayMode = [this](PlayMode mode) { setPlayMode(mode); };
    hooks.destroyEnginesAll = [this]() { destroyEngines(); };
    hooks.createAnalysisEngine = [this](const AnalysisOptions& opt) -> Usi* {
        ensureEngineManager();
        auto [comm, think] = m_engineManager->ensureEngineModels(1);
        comm->setEngineName(opt.engineName);
        Usi* usi = new Usi(comm, think, m_gc, m_playMode, this);
        m_engineManager->setUsi1(usi);
        connect(usi, &Usi::errorOccurred,
                this, &MatchCoordinator::onUsiError, Qt::UniqueConnection);
        m_engineManager->wireResignToArbiter(usi, true);
        m_engineManager->wireWinToArbiter(usi, true);
        usi->setLogIdentity(QStringLiteral("[E1]"), QStringLiteral("P1"), opt.engineName);
        usi->setSquelchResignLogging(false);
        return usi;
    };
    hooks.initAndStartEngine = [this](int, const QString& path, const QString& name) {
        initializeAndStartEngineFor(P1, path, name);
    };
    hooks.setEngineNames = [this](const QString& n1, const QString& n2) {
        if (m_hooks.ui.setEngineNames) m_hooks.ui.setEngineNames(n1, n2);
    };
    hooks.setShutdownInProgress = [this](bool val) {
        ensureEngineManager();
        m_engineManager->setShutdownInProgress(val);
    };
    m_analysisSession->setHooks(hooks);
}

void MatchCoordinator::ensureGameEndHandler()
{
    if (m_gameEndHandler) return;
    m_gameEndHandler = new GameEndHandler(this);

    // シグナル転送（GameEndHandler → MatchCoordinator）
    connect(m_gameEndHandler, &GameEndHandler::gameEnded,
            this,             &MatchCoordinator::gameEnded);
    connect(m_gameEndHandler, &GameEndHandler::gameOverStateChanged,
            this,             &MatchCoordinator::gameOverStateChanged);
    connect(m_gameEndHandler, &GameEndHandler::requestAppendGameOverMove,
            this,             &MatchCoordinator::requestAppendGameOverMove);

    // Refs 設定
    GameEndHandler::Refs refs;
    refs.gc        = m_gc;
    refs.clock     = m_timekeeper ? m_timekeeper->clock() : nullptr;
    refs.usi1Provider = [this]() -> Usi* { return m_engineManager ? m_engineManager->usi1() : nullptr; };
    refs.usi2Provider = [this]() -> Usi* { return m_engineManager ? m_engineManager->usi2() : nullptr; };
    refs.playMode  = &m_playMode;
    refs.gameOver  = &m_gameOver;
    refs.strategyProvider = [this]() -> GameModeStrategy* {
        ensureMatchTurnHandler();
        return m_turnHandler->strategy();
    };
    refs.sfenHistory = m_sfenHistory;
    m_gameEndHandler->setRefs(refs);

    // Hooks 設定
    GameEndHandler::Hooks hooks;
    hooks.disarmHumanTimerIfNeeded = [this]() { disarmHumanTimerIfNeeded(); };
    hooks.primaryEngine     = [this]() -> Usi* { return primaryEngine(); };
    hooks.turnEpochFor      = [this](Player p) -> qint64 { return turnEpochFor(p); };
    hooks.appendKifuLine    = m_hooks.game.appendKifuLine;
    hooks.showGameOverDialog = m_hooks.ui.showGameOverDialog;
    hooks.autoSaveKifuIfEnabled = [this]() {
        if (m_autoSaveKifu && !m_kifuSaveDir.isEmpty() && m_hooks.game.autoSaveKifu) {
            qCInfo(lcGame) << "Calling autoSaveKifu hook: dir=" << m_kifuSaveDir;
            m_hooks.game.autoSaveKifu(m_kifuSaveDir, m_playMode,
                                 m_humanName1, m_humanName2,
                                 m_engineNameForSave1, m_engineNameForSave2);
        }
    };
    m_gameEndHandler->setHooks(hooks);
}

void MatchCoordinator::ensureGameStartOrchestrator()
{
    if (m_gameStartOrchestrator) return;
    m_gameStartOrchestrator = std::make_unique<GameStartOrchestrator>();

    GameStartOrchestrator::Refs refs;
    refs.gc                 = m_gc;
    refs.playMode           = &m_playMode;
    refs.maxMoves           = &m_maxMoves;
    refs.currentTurn        = &m_cur;
    refs.currentMoveIndex   = &m_currentMoveIndex;
    refs.autoSaveKifu       = &m_autoSaveKifu;
    refs.kifuSaveDir        = &m_kifuSaveDir;
    refs.humanName1         = &m_humanName1;
    refs.humanName2         = &m_humanName2;
    refs.engineNameForSave1 = &m_engineNameForSave1;
    refs.engineNameForSave2 = &m_engineNameForSave2;
    refs.positionStr1       = &m_positionStr1;
    refs.positionPonder1    = &m_positionPonder1;
    refs.positionStrHistory = &m_positionStrHistory;
    refs.allGameHistories   = &m_allGameHistories;
    m_gameStartOrchestrator->setRefs(refs);

    GameStartOrchestrator::Hooks hooks;
    hooks.initializeNewGame = m_hooks.game.initializeNewGame;
    hooks.setPlayersNames   = m_hooks.ui.setPlayersNames;
    hooks.setEngineNames    = m_hooks.ui.setEngineNames;
    hooks.renderBoardFromGc = m_hooks.ui.renderBoardFromGc;
    hooks.clearGameOverState = [this]() { clearGameOverState(); };
    hooks.updateTurnDisplay  = [this](Player p) {
        ensureMatchTurnHandler();
        m_turnHandler->updateTurnDisplay(p);
    };
    hooks.initializePositionStringsForStart = [this](const QString& s) {
        ensureMatchTurnHandler();
        m_turnHandler->initPositionStringsFromSfen(s);
    };
    hooks.createAndStartModeStrategy = [this](const StartOptions& opt) {
        ensureMatchTurnHandler();
        m_turnHandler->createAndStartModeStrategy(opt);
    };
    hooks.flipBoard = [this]() { flipBoard(); };
    hooks.startInitialEngineMoveIfNeeded = [this]() { startInitialEngineMoveIfNeeded(); };
    m_gameStartOrchestrator->setHooks(hooks);
}

void MatchCoordinator::ensureUndoHandler()
{
    if (m_undoHandler) return;
    m_undoHandler = std::make_unique<MatchUndoHandler>();

    MatchUndoHandler::Refs refs;
    refs.gc                = m_gc;
    refs.sfenHistory       = m_sfenHistory;
    refs.positionStr1      = &m_positionStr1;
    refs.positionPonder1   = &m_positionPonder1;
    refs.positionStrHistory = &m_positionStrHistory;
    refs.gameMoves         = &gameMovesRef();
    m_undoHandler->setRefs(refs);
}

// --- StrategyContext アクセサ ---

MatchCoordinator::StrategyContext& MatchCoordinator::strategyCtx()
{
    ensureMatchTurnHandler();
    return m_turnHandler->strategyCtx();
}

// --- 投了・終局処理 ---

void MatchCoordinator::handleResign() {
    ensureGameEndHandler();
    m_gameEndHandler->handleResign();
}

void MatchCoordinator::handleEngineResign(int idx) {
    ensureGameEndHandler();
    m_gameEndHandler->handleEngineResign(idx);
}

void MatchCoordinator::handleEngineWin(int idx) {
    ensureGameEndHandler();
    m_gameEndHandler->handleEngineWin(idx);
}

void MatchCoordinator::handleNyugyokuDeclaration(Player declarer, bool success, bool isDraw)
{
    ensureGameEndHandler();
    m_gameEndHandler->handleNyugyokuDeclaration(declarer, success, isDraw);
}

void MatchCoordinator::handleBreakOff()
{
    Usi* u1 = primaryEngine();
    Usi* u2 = secondaryEngine();
    if (u1) QObject::disconnect(u1, &Usi::bestMoveResignReceived, nullptr, nullptr);
    if (u2) QObject::disconnect(u2, &Usi::bestMoveResignReceived, nullptr, nullptr);

    ensureGameEndHandler();
    m_gameEndHandler->handleBreakOff();
}

void MatchCoordinator::appendGameOverLineAndMark(Cause cause, Player loser)
{
    ensureGameEndHandler();
    m_gameEndHandler->appendGameOverLineAndMark(cause, loser);
}

void MatchCoordinator::handleMaxMovesJishogi()
{
    ensureGameEndHandler();
    m_gameEndHandler->handleMaxMovesJishogi();
}

void MatchCoordinator::clearGameOverState()
{
    ensureGameEndHandler();
    m_gameEndHandler->clearGameOverState();
}

void MatchCoordinator::setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce)
{
    ensureGameEndHandler();
    m_gameEndHandler->setGameOver(info, loserIsP1, appendMoveOnce);
}

void MatchCoordinator::markGameOverMoveAppended()
{
    ensureGameEndHandler();
    m_gameEndHandler->markGameOverMoveAppended();
}

// --- 対局開始フロー ---

void MatchCoordinator::configureAndStart(const StartOptions& opt)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->configureAndStart(opt);
}

MatchCoordinator::StartOptions MatchCoordinator::buildStartOptions(
    PlayMode mode,
    const QString& startSfenStr,
    const QStringList* sfenRecord,
    const StartGameDialogData* dlg) const
{
    return GameStartOrchestrator::buildStartOptions(mode, startSfenStr, sfenRecord, dlg);
}

void MatchCoordinator::ensureHumanAtBottomIfApplicable(const StartGameDialogData* dlg, bool bottomIsP1)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->ensureHumanAtBottomIfApplicable(dlg, bottomIsP1);
}

void MatchCoordinator::prepareAndStartGame(PlayMode mode,
                                           const QString& startSfenStr,
                                           const QStringList* sfenRecord,
                                           const StartGameDialogData* dlg,
                                           bool bottomIsP1)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->prepareAndStartGame(mode, startSfenStr, sfenRecord, dlg, bottomIsP1);
}

// --- ターン進行（MatchTurnHandler へ委譲） ---

void MatchCoordinator::flipBoard() {
    ensureMatchTurnHandler();
    m_turnHandler->flipBoard();
}

void MatchCoordinator::onHumanMove(const QPoint& from, const QPoint& to,
                                   const QString& prettyMove)
{
    ensureMatchTurnHandler();
    m_turnHandler->onHumanMove(from, to, prettyMove);
}

void MatchCoordinator::forceImmediateMove() {
    ensureMatchTurnHandler();
    m_turnHandler->forceImmediateMove();
}

void MatchCoordinator::startInitialEngineMoveIfNeeded() {
    ensureMatchTurnHandler();
    m_turnHandler->startInitialEngineMoveIfNeeded();
}

void MatchCoordinator::armTurnTimerIfNeeded() {
    ensureMatchTurnHandler();
    m_turnHandler->armTurnTimerIfNeeded();
}

void MatchCoordinator::finishTurnTimerAndSetConsiderationFor(Player mover) {
    ensureMatchTurnHandler();
    m_turnHandler->finishTurnTimerAndSetConsiderationFor(mover);
}

void MatchCoordinator::disarmHumanTimerIfNeeded() {
    ensureMatchTurnHandler();
    m_turnHandler->disarmHumanTimerIfNeeded();
}

void MatchCoordinator::handlePlayerTimeOut(int player) {
    ensureMatchTurnHandler();
    m_turnHandler->handlePlayerTimeOut(player);
}

void MatchCoordinator::startMatchTimingAndMaybeInitialGo() {
    ensureMatchTurnHandler();
    m_turnHandler->startMatchTimingAndMaybeInitialGo();
}

void MatchCoordinator::onUsiError(const QString& msg) {
    ensureMatchTurnHandler();
    m_turnHandler->handleUsiError(msg);
}

// --- UNDO ---

void MatchCoordinator::setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks) {
    ensureUndoHandler();
    m_undoHandler->setUndoBindings(refs, hooks);
}

bool MatchCoordinator::undoTwoPlies()
{
    ensureUndoHandler();
    return m_undoHandler->undoTwoPlies();
}
