/// @file matchcoordinator.cpp
/// @brief 対局進行コーディネータ（司令塔）クラスの実装

#include "matchcoordinator.h"
#include "matchtimekeeper.h"
#include "matchundohandler.h"
#include "gameendhandler.h"
#include "gamestartorchestrator.h"
#include "analysissessionhandler.h"
#include "strategycontext.h"
#include "gamemodestrategy.h"
#include "humanvshumanstrategy.h"
#include "humanvsenginestrategy.h"
#include "enginevsenginestrategy.h"
#include "shogiclock.h"
#include "usi.h"
#include "shogiview.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "logcategories.h"

#include <QObject>
#include <QDebug>
#include <QDateTime>

using P = MatchCoordinator::Player;

// --- 初期化・破棄 ---

MatchCoordinator::MatchCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_view(d.view)
    , m_hooks(d.hooks)
{
    // 既定値
    m_cur = P1;

    m_sfenHistory = d.sfenRecord ? d.sfenRecord : &m_sharedSfenRecord;
    m_externalGameMoves = d.gameMoves;

    // デバッグ：どのリストを使うか明示
    qCDebug(lcGame).noquote()
        << "shared sfenRecord*=" << static_cast<const void*>(m_sfenHistory);

    // 共有ポインタが未指定の場合は内部履歴を使用
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

    m_strategyCtx = std::make_unique<StrategyContext>(*this);
}

MatchCoordinator::~MatchCoordinator() = default;

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

    // timeUpdated シグナルを MC 経由で転送
    connect(m_timekeeper, &MatchTimekeeper::timeUpdated,
            this,         &MatchCoordinator::timeUpdated);
}

EngineLifecycleManager* MatchCoordinator::engineManager()
{
    ensureEngineManager();
    return m_engineManager;
}

bool MatchCoordinator::isEngineShutdownInProgress() const
{
    return m_engineManager ? m_engineManager->isShutdownInProgress() : false;
}

void MatchCoordinator::ensureAnalysisSession()
{
    if (m_analysisSession) return;
    m_analysisSession = std::make_unique<AnalysisSessionHandler>(this);

    // ハンドラのシグナルをMatchCoordinatorのシグナルに中継する
    connect(m_analysisSession.get(), &AnalysisSessionHandler::considerationModeEnded,
            this,                    &MatchCoordinator::considerationModeEnded);
    connect(m_analysisSession.get(), &AnalysisSessionHandler::tsumeSearchModeEnded,
            this,                    &MatchCoordinator::tsumeSearchModeEnded);
    connect(m_analysisSession.get(), &AnalysisSessionHandler::considerationWaitingStarted,
            this,                    &MatchCoordinator::considerationWaitingStarted);

    // ハンドラ用コールバック設定
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

    // Refs 設定
    GameEndHandler::Refs refs;
    refs.gc        = m_gc;
    refs.clock     = m_timekeeper ? m_timekeeper->clock() : nullptr;
    refs.usi1Provider = [this]() -> Usi* { return m_engineManager ? m_engineManager->usi1() : nullptr; };
    refs.usi2Provider = [this]() -> Usi* { return m_engineManager ? m_engineManager->usi2() : nullptr; };
    refs.playMode  = &m_playMode;
    refs.gameOver  = &m_gameOver;
    refs.strategyProvider = [this]() -> GameModeStrategy* { return m_strategy.get(); };
    refs.sfenHistory = m_sfenHistory;
    m_gameEndHandler->setRefs(refs);

    // Hooks 設定
    GameEndHandler::Hooks hooks;
    hooks.disarmHumanTimerIfNeeded = [this]() { disarmHumanTimerIfNeeded(); };
    hooks.primaryEngine     = [this]() -> Usi* { return primaryEngine(); };
    hooks.turnEpochFor      = [this](Player p) -> qint64 { return turnEpochFor(p); };
    hooks.setGameOver       = [this](const GameEndInfo& info, bool loserIsP1, bool append) {
        setGameOver(info, loserIsP1, append);
    };
    hooks.markGameOverMoveAppended = [this]() { markGameOverMoveAppended(); };
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

    // Refs 設定
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

    // Hooks 設定
    GameStartOrchestrator::Hooks hooks;
    hooks.initializeNewGame = m_hooks.game.initializeNewGame;
    hooks.setPlayersNames   = m_hooks.ui.setPlayersNames;
    hooks.setEngineNames    = m_hooks.ui.setEngineNames;
    hooks.renderBoardFromGc = m_hooks.ui.renderBoardFromGc;
    hooks.clearGameOverState = [this]() { clearGameOverState(); };
    hooks.updateTurnDisplay  = [this](Player p) { updateTurnDisplay(p); };
    hooks.initializePositionStringsForStart = [this](const QString& s) {
        initPositionStringsFromSfen(s);
    };
    hooks.createAndStartModeStrategy = [this](const StartOptions& opt) {
        createAndStartModeStrategy(opt);
    };
    hooks.flipBoard = [this]() { flipBoard(); };
    hooks.startInitialEngineMoveIfNeeded = [this]() { startInitialEngineMoveIfNeeded(); };
    m_gameStartOrchestrator->setHooks(hooks);
}

MatchCoordinator::StrategyContext& MatchCoordinator::strategyCtx()
{
    return *m_strategyCtx;
}

void MatchCoordinator::updateUsiPtrs(Usi* e1, Usi* e2) {
    ensureEngineManager();
    m_engineManager->updateUsiPtrs(e1, e2);
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

void MatchCoordinator::flipBoard() {
    // 実際の反転は GUI 側で実施（レイアウト/ラベル入替等を考慮）
    if (m_hooks.ui.renderBoardFromGc) m_hooks.ui.renderBoardFromGc();
    emit boardFlipped(true);
}

void MatchCoordinator::updateTurnDisplay(Player p) {
    m_cur = p;
    if (m_hooks.ui.updateTurnDisplay) m_hooks.ui.updateTurnDisplay(p);
}

void MatchCoordinator::initializeAndStartEngineFor(Player side,
                                                   const QString& enginePathIn,
                                                   const QString& engineNameIn)
{
    ensureEngineManager();
    m_engineManager->initializeAndStartEngineFor(
        static_cast<EngineLifecycleManager::Player>(side), enginePathIn, engineNameIn);
}

void MatchCoordinator::destroyEngine(int idx, bool clearThinking)
{
    ensureEngineManager();
    m_engineManager->destroyEngine(idx, clearThinking);
}

void MatchCoordinator::destroyEngines(bool clearModels)
{
    ensureEngineManager();
    m_engineManager->destroyEngines(clearModels);
}

void MatchCoordinator::setPlayMode(PlayMode m)
{
    m_playMode = m;
}

void MatchCoordinator::initEnginesForEvE(const QString& engineName1,
                                         const QString& engineName2)
{
    ensureEngineManager();
    m_engineManager->initEnginesForEvE(engineName1, engineName2);
}

bool MatchCoordinator::engineThinkApplyMove(Usi* engine,
                                            QString& positionStr,
                                            QString& ponderStr,
                                            QPoint* outFrom,
                                            QPoint* outTo)
{
    ensureEngineManager();
    return m_engineManager->engineThinkApplyMove(engine, positionStr, ponderStr, outFrom, outTo);
}

bool MatchCoordinator::engineMoveOnce(Usi* eng,
                                      QString& positionStr,
                                      QString& ponderStr,
                                      bool useSelectedField2,
                                      int engineIndex,
                                      QPoint* outTo)
{
    ensureEngineManager();
    return m_engineManager->engineMoveOnce(eng, positionStr, ponderStr, useSelectedField2, engineIndex, outTo);
}

// --- 対局開始フロー（GameStartOrchestrator へ委譲） ---

void MatchCoordinator::configureAndStart(const StartOptions& opt)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->configureAndStart(opt);
}

void MatchCoordinator::createAndStartModeStrategy(const StartOptions& opt)
{
    m_strategy.reset();
    switch (m_playMode) {
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine: {
        // エンジン同士は両方起動してから開始
        initEnginesForEvE(opt.engineName1, opt.engineName2);
        initializeAndStartEngineFor(P1, opt.enginePath1, opt.engineName1);
        initializeAndStartEngineFor(P2, opt.enginePath2, opt.engineName2);
        m_strategy = std::make_unique<EngineVsEngineStrategy>(*m_strategyCtx, opt, this);
        m_strategy->start();
        break;
    }

    // エンジンが先手（P1）
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, true, opt.enginePath1, opt.engineName1);
        m_strategy->start();
        break;

    // エンジンが後手（P2）
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, false, opt.enginePath2, opt.engineName2);
        m_strategy->start();
        break;

    case PlayMode::HumanVsHuman:
        m_strategy = std::make_unique<HumanVsHumanStrategy>(*m_strategyCtx);
        m_strategy->start();
        break;

    default:
        qCWarning(lcGame).noquote()
            << "configureAndStart: unexpected playMode="
            << static_cast<int>(m_playMode);
        break;
    }
}

Usi* MatchCoordinator::primaryEngine() const
{
    return m_engineManager ? m_engineManager->usi1() : nullptr;
}

Usi* MatchCoordinator::secondaryEngine() const
{
    return m_engineManager ? m_engineManager->usi2() : nullptr;
}

void MatchCoordinator::setTimeControlConfig(bool useByoyomi,
                                            int byoyomiMs1, int byoyomiMs2,
                                            int incMs1,     int incMs2,
                                            bool loseOnTimeout)
{
    ensureTimekeeper();
    m_timekeeper->setTimeControlConfig(useByoyomi, byoyomiMs1, byoyomiMs2,
                                       incMs1, incMs2, loseOnTimeout);
}

const MatchCoordinator::TimeControl& MatchCoordinator::timeControl() const {
    return m_timekeeper->timeControl();
}

void MatchCoordinator::markTurnEpochNowFor(Player side, qint64 nowMs) {
    ensureTimekeeper();
    m_timekeeper->markTurnEpochNowFor(side, nowMs);
}

qint64 MatchCoordinator::turnEpochFor(Player side) const {
    return m_timekeeper ? m_timekeeper->turnEpochFor(side) : -1;
}

void MatchCoordinator::armTurnTimerIfNeeded() {
    if (m_strategy) m_strategy->armTurnTimerIfNeeded();
}

void MatchCoordinator::finishTurnTimerAndSetConsiderationFor(Player mover) {
    if (m_strategy) m_strategy->finishTurnTimerAndSetConsideration(static_cast<int>(mover));
}

void MatchCoordinator::disarmHumanTimerIfNeeded() {
    if (m_strategy) m_strategy->disarmTurnTimer();
}

MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes() const {
    return m_timekeeper ? m_timekeeper->computeGoTimes() : GoTimes{};
}

void MatchCoordinator::computeGoTimesForUSI(qint64& outB, qint64& outW) const {
    if (m_timekeeper) { m_timekeeper->computeGoTimesForUSI(outB, outW); return; }
    outB = outW = 0;
}

void MatchCoordinator::refreshGoTimes() {
    if (m_timekeeper) m_timekeeper->refreshGoTimes();
}

void MatchCoordinator::setClock(ShogiClock* clock)
{
    ensureTimekeeper();
    m_timekeeper->setClock(clock);
}

void MatchCoordinator::pokeTimeUpdateNow()
{
    if (m_timekeeper) m_timekeeper->pokeTimeUpdateNow();
}

ShogiClock* MatchCoordinator::clock()
{
    return m_timekeeper ? m_timekeeper->clock() : nullptr;
}

const ShogiClock* MatchCoordinator::clock() const
{
    return m_timekeeper ? m_timekeeper->clockConst() : nullptr;
}

void MatchCoordinator::clearGameOverState()
{
    const bool wasOver = m_gameOver.isOver;
    m_gameOver = GameOverState{}; // 全クリア
    if (wasOver) {
        emit gameOverStateChanged(m_gameOver);
        qCDebug(lcGame) << "clearGameOverState()";
    }
}

// 司令塔が終局を確定させる唯一の入口
void MatchCoordinator::setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce)
{
    if (m_gameOver.isOver) {
        qCDebug(lcGame) << "setGameOver() ignored: already over";
        return;
    }

    qCDebug(lcGame).nospace()
        << "setGameOver cause="
        << ((info.cause==Cause::Timeout)?"Timeout":"Resign")
        << " loser=" << ((info.loser==P1)?"P1":"P2")
        << " appendMoveOnce=" << appendMoveOnce;

    m_gameOver.isOver        = true;
    m_gameOver.hasLast       = true;
    m_gameOver.lastLoserIsP1 = loserIsP1;
    m_gameOver.lastInfo      = info;
    m_gameOver.when          = QDateTime::currentDateTime();

    emit gameOverStateChanged(m_gameOver);
    emit gameEnded(info);

    if (appendMoveOnce && !m_gameOver.moveAppended) {
        qCDebug(lcGame) << "emit requestAppendGameOverMove";
        emit requestAppendGameOverMove(info);
    }
}

void MatchCoordinator::markGameOverMoveAppended()
{
    if (!m_gameOver.isOver) return;
    if (m_gameOver.moveAppended) return;

    m_gameOver.moveAppended = true;
    emit gameOverStateChanged(m_gameOver);
    qCDebug(lcGame) << "markGameOverMoveAppended()";
}

// 投了と同様に"対局の実体"として中断を一元処理
void MatchCoordinator::handleBreakOff()
{
    // エンジンの投了シグナルを切断（レースコンディション防止）
    Usi* u1 = primaryEngine();
    Usi* u2 = secondaryEngine();
    if (u1) QObject::disconnect(u1, &Usi::bestMoveResignReceived, nullptr, nullptr);
    if (u2) QObject::disconnect(u2, &Usi::bestMoveResignReceived, nullptr, nullptr);

    ensureGameEndHandler();
    m_gameEndHandler->handleBreakOff();
}

void MatchCoordinator::startAnalysis(const AnalysisOptions& opt)
{
    ensureAnalysisSession();
    m_analysisSession->startFullAnalysis(opt);
}

void MatchCoordinator::stopAnalysisEngine()
{
    ensureAnalysisSession();
    m_analysisSession->stopFullAnalysis();
}

void MatchCoordinator::updateConsiderationMultiPV(int multiPV)
{
    ensureAnalysisSession();
    m_analysisSession->updateMultiPV(primaryEngine(), multiPV);
}

bool MatchCoordinator::updateConsiderationPosition(const QString& newPositionStr,
                                                   int previousFileTo, int previousRankTo,
                                                   const QString& lastUsiMove)
{
    ensureAnalysisSession();
    return m_analysisSession->updatePosition(primaryEngine(), newPositionStr,
                                             previousFileTo, previousRankTo, lastUsiMove);
}

void MatchCoordinator::onUsiError(const QString& msg)
{
    qCWarning(lcGame).noquote() << "[USI-ERROR]" << msg;
    Usi* u1 = primaryEngine();
    Usi* u2 = secondaryEngine();
    if (u1) u1->cancelCurrentOperation();
    if (u2) u2->cancelCurrentOperation();

    // 詰み探索・検討モード中のエラー復旧（AnalysisSessionHandler へ委譲）
    ensureAnalysisSession();
    if (m_analysisSession->handleEngineError(msg)) {
        return;
    }

    // 対局中にエンジンがクラッシュした場合の復旧処理
    // （handleBreakOffは m_gameOver.isOver ガードがあるので重複呼び出しも安全）
    if (!m_gameOver.isOver) {
        switch (m_playMode) {
        case PlayMode::EvenHumanVsEngine:
        case PlayMode::EvenEngineVsHuman:
        case PlayMode::EvenEngineVsEngine:
        case PlayMode::HandicapHumanVsEngine:
        case PlayMode::HandicapEngineVsHuman:
        case PlayMode::HandicapEngineVsEngine:
            handleBreakOff();
            if (m_hooks.ui.showGameOverDialog) {
                m_hooks.ui.showGameOverDialog(tr("対局中断"), tr("エンジンエラー: %1").arg(msg));
            }
            return;
        default:
            break;
        }
    }
}

void MatchCoordinator::initPositionStringsFromSfen(const QString& sfenBase)
{
    m_positionStr1.clear();
    m_positionPonder1.clear();
    m_positionStrHistory.clear();

    if (sfenBase.isEmpty()) {
        m_positionStr1    = QStringLiteral("position startpos moves");
        m_positionPonder1 = m_positionStr1;
    } else if (sfenBase.startsWith(QLatin1String("position "))) {
        m_positionStr1    = sfenBase;
        m_positionPonder1 = sfenBase;
    } else if (MatchUndoHandler::isStandardStartposSfen(sfenBase)) {
        m_positionStr1    = QStringLiteral("position startpos");
        m_positionPonder1 = m_positionStr1;
    } else {
        m_positionStr1    = QStringLiteral("position sfen %1").arg(sfenBase);
        m_positionPonder1 = m_positionStr1;
    }
}

void MatchCoordinator::startInitialEngineMoveIfNeeded()
{
    if (m_strategy) m_strategy->startInitialMoveIfNeeded();
}

void MatchCoordinator::ensureUndoHandler()
{
    if (m_undoHandler) return;
    m_undoHandler = std::make_unique<MatchUndoHandler>();

    MatchUndoHandler::Refs refs;
    refs.gc                = m_gc;
    refs.view              = m_view;
    refs.sfenHistory       = m_sfenHistory;
    refs.positionStr1      = &m_positionStr1;
    refs.positionPonder1   = &m_positionPonder1;
    refs.positionStrHistory = &m_positionStrHistory;
    refs.gameMoves         = &gameMovesRef();
    m_undoHandler->setRefs(refs);
}

void MatchCoordinator::setUndoBindings(const UndoRefs& refs, const UndoHooks& hooks) {
    ensureUndoHandler();
    m_undoHandler->setUndoBindings(refs, hooks);
}

bool MatchCoordinator::undoTwoPlies()
{
    ensureUndoHandler();
    return m_undoHandler->undoTwoPlies();
}

MatchCoordinator::StartOptions MatchCoordinator::buildStartOptions(
    PlayMode mode,
    const QString& startSfenStr,
    const QStringList* sfenRecord,
    const StartGameDialog* dlg) const
{
    return GameStartOrchestrator::buildStartOptions(mode, startSfenStr, sfenRecord, dlg);
}

void MatchCoordinator::ensureHumanAtBottomIfApplicable(const StartGameDialog* dlg, bool bottomIsP1)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->ensureHumanAtBottomIfApplicable(dlg, bottomIsP1);
}

void MatchCoordinator::prepareAndStartGame(PlayMode mode,
                                           const QString& startSfenStr,
                                           const QStringList* sfenRecord,
                                           const StartGameDialog* dlg,
                                           bool bottomIsP1)
{
    ensureGameStartOrchestrator();
    m_gameStartOrchestrator->prepareAndStartGame(mode, startSfenStr, sfenRecord, dlg, bottomIsP1);
}

void MatchCoordinator::startMatchTimingAndMaybeInitialGo()
{
    if (ShogiClock* clk = clock()) clk->startClock();
    startInitialEngineMoveIfNeeded();
}

void MatchCoordinator::handlePlayerTimeOut(int player)
{
    if (!m_gc) return;
    m_gc->applyTimeoutLossFor(player);
    m_gc->finalizeGameResult();
}

void MatchCoordinator::recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const
{
    if (m_timekeeper) { m_timekeeper->recomputeClockSnapshot(turnText, p1, p2); return; }
    turnText.clear(); p1.clear(); p2.clear();
}

void MatchCoordinator::appendGameOverLineAndMark(Cause cause, Player loser)
{
    ensureGameEndHandler();
    m_gameEndHandler->appendGameOverLineAndMark(cause, loser);
}

void MatchCoordinator::onHumanMove(const QPoint& from, const QPoint& to,
                                   const QString& prettyMove)
{
    if (m_strategy) m_strategy->onHumanMove(from, to, prettyMove);
}

void MatchCoordinator::forceImmediateMove()
{
    if (!m_gc || !m_hooks.engine.sendStopToEngine) return;

    const bool isEvE =
        (m_playMode == PlayMode::EvenEngineVsEngine) || (m_playMode == PlayMode::HandicapEngineVsEngine);
    if (isEvE) {
        const Player turn =
            (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2;
        if (Usi* eng = (turn == P1) ? primaryEngine() : secondaryEngine())
            m_hooks.engine.sendStopToEngine(eng);
    } else if (Usi* eng = primaryEngine()) {
        m_hooks.engine.sendStopToEngine(eng);
    }
}

void MatchCoordinator::sendGoToEngine(Usi* which, const GoTimes& t)
{
    ensureEngineManager();
    const EngineLifecycleManager::GoTimes et = { t.btime, t.wtime, t.byoyomi, t.binc, t.winc };
    m_engineManager->sendGoToEngine(which, et);
}

void MatchCoordinator::sendStopToEngine(Usi* which)
{
    ensureEngineManager();
    m_engineManager->sendStopToEngine(which);
}

void MatchCoordinator::sendRawToEngine(Usi* which, const QString& cmd)
{
    ensureEngineManager();
    m_engineManager->sendRawToEngine(which, cmd);
}

void MatchCoordinator::appendBreakOffLineAndMark()
{
    ensureGameEndHandler();
    m_gameEndHandler->appendBreakOffLineAndMark();
}

void MatchCoordinator::handleMaxMovesJishogi()
{
    ensureGameEndHandler();
    m_gameEndHandler->handleMaxMovesJishogi();
}

bool MatchCoordinator::checkAndHandleSennichite()
{
    ensureGameEndHandler();
    return m_gameEndHandler->checkAndHandleSennichite();
}

void MatchCoordinator::handleSennichite()
{
    ensureGameEndHandler();
    m_gameEndHandler->handleSennichite();
}

void MatchCoordinator::handleOuteSennichite(bool p1Loses)
{
    ensureGameEndHandler();
    m_gameEndHandler->handleOuteSennichite(p1Loses);
}

