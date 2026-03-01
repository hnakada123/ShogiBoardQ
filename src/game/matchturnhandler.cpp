/// @file matchturnhandler.cpp
/// @brief ターン進行・ストラテジー管理ハンドラの実装

#include "matchturnhandler.h"
#include "strategycontext.h"
#include "gamemodestrategy.h"
#include "humanvshumanstrategy.h"
#include "humanvsenginestrategy.h"
#include "enginevsenginestrategy.h"
#include "shogiclock.h"
#include "usi.h"
#include "shogigamecontroller.h"
#include "matchundohandler.h"
#include "logcategories.h"

#include <QDebug>

using P = MatchCoordinator::Player;

MatchTurnHandler::MatchTurnHandler(MatchCoordinator& mc)
{
    m_strategyCtx = std::make_unique<MatchCoordinator::StrategyContext>(mc);
}

MatchTurnHandler::~MatchTurnHandler() = default;

void MatchTurnHandler::setRefs(const Refs& refs) { m_refs = refs; }
void MatchTurnHandler::setHooks(const Hooks& hooks) { m_hooks = hooks; }

MatchCoordinator::StrategyContext& MatchTurnHandler::strategyCtx()
{
    return *m_strategyCtx;
}

GameModeStrategy* MatchTurnHandler::strategy() const
{
    return m_strategy.get();
}

// --- ストラテジー管理 ---

void MatchTurnHandler::createAndStartModeStrategy(const StartOptions& opt)
{
    m_strategy.reset();
    switch (*m_refs.playMode) {
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine: {
        if (m_hooks.initEnginesForEvE)
            m_hooks.initEnginesForEvE(opt.engineName1, opt.engineName2);
        if (m_hooks.initAndStartEngine) {
            m_hooks.initAndStartEngine(P::P1, opt.enginePath1, opt.engineName1);
            m_hooks.initAndStartEngine(P::P2, opt.enginePath2, opt.engineName2);
        }
        m_strategy = std::make_unique<EngineVsEngineStrategy>(
            *m_strategyCtx, opt, m_refs.mcAsParent);
        m_strategy->start();
        break;
    }

    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, true, opt.enginePath1, opt.engineName1);
        m_strategy->start();
        break;

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
            << static_cast<int>(*m_refs.playMode);
        break;
    }
}

// --- ターン進行 ---

void MatchTurnHandler::onHumanMove(const QPoint& from, const QPoint& to,
                                   const QString& prettyMove)
{
    if (m_strategy) m_strategy->onHumanMove(from, to, prettyMove);
}

void MatchTurnHandler::startInitialEngineMoveIfNeeded()
{
    if (m_strategy) m_strategy->startInitialMoveIfNeeded();
}

void MatchTurnHandler::armTurnTimerIfNeeded()
{
    if (m_strategy) m_strategy->armTurnTimerIfNeeded();
}

void MatchTurnHandler::finishTurnTimerAndSetConsiderationFor(Player mover)
{
    if (m_strategy) m_strategy->finishTurnTimerAndSetConsideration(static_cast<int>(mover));
}

void MatchTurnHandler::disarmHumanTimerIfNeeded()
{
    if (m_strategy) m_strategy->disarmTurnTimer();
}

// --- 盤面・表示 ---

void MatchTurnHandler::flipBoard()
{
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    if (m_hooks.emitBoardFlipped) m_hooks.emitBoardFlipped(true);
}

void MatchTurnHandler::updateTurnDisplay(Player p)
{
    *m_refs.currentTurn = p;
    if (m_hooks.updateTurnDisplayCb) m_hooks.updateTurnDisplayCb(p);
}

void MatchTurnHandler::initPositionStringsFromSfen(const QString& sfenBase)
{
    m_refs.positionStr1->clear();
    m_refs.positionPonder1->clear();
    m_refs.positionStrHistory->clear();

    if (sfenBase.isEmpty()) {
        *m_refs.positionStr1    = QStringLiteral("position startpos moves");
        *m_refs.positionPonder1 = *m_refs.positionStr1;
    } else if (sfenBase.startsWith(QLatin1String("position "))) {
        *m_refs.positionStr1    = sfenBase;
        *m_refs.positionPonder1 = sfenBase;
    } else if (MatchUndoHandler::isStandardStartposSfen(sfenBase)) {
        *m_refs.positionStr1    = QStringLiteral("position startpos");
        *m_refs.positionPonder1 = *m_refs.positionStr1;
    } else {
        *m_refs.positionStr1    = QStringLiteral("position sfen %1").arg(sfenBase);
        *m_refs.positionPonder1 = *m_refs.positionStr1;
    }
}

// --- 対局進行 ---

void MatchTurnHandler::forceImmediateMove()
{
    if (!m_refs.gc || !m_hooks.sendStopToEngine) return;

    const bool isEvE =
        (*m_refs.playMode == PlayMode::EvenEngineVsEngine) ||
        (*m_refs.playMode == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        const Player turn =
            (m_refs.gc->currentPlayer() == ShogiGameController::Player1) ? P::P1 : P::P2;
        Usi* eng = (turn == P::P1)
            ? (m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr)
            : (m_hooks.secondaryEngine ? m_hooks.secondaryEngine() : nullptr);
        if (eng) m_hooks.sendStopToEngine(eng);
    } else if (Usi* eng = m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr) {
        m_hooks.sendStopToEngine(eng);
    }
}

void MatchTurnHandler::handlePlayerTimeOut(int player)
{
    if (!m_refs.gc) return;
    m_refs.gc->applyTimeoutLossFor(player);
    m_refs.gc->finalizeGameResult();
}

void MatchTurnHandler::startMatchTimingAndMaybeInitialGo()
{
    if (ShogiClock* clk = m_hooks.clockProvider ? m_hooks.clockProvider() : nullptr)
        clk->startClock();
    startInitialEngineMoveIfNeeded();
}

void MatchTurnHandler::handleUsiError(const QString& msg)
{
    qCWarning(lcGame).noquote() << "[USI-ERROR]" << msg;
    Usi* u1 = m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr;
    Usi* u2 = m_hooks.secondaryEngine ? m_hooks.secondaryEngine() : nullptr;
    if (u1) u1->cancelCurrentOperation();
    if (u2) u2->cancelCurrentOperation();

    if (m_hooks.handleEngineError && m_hooks.handleEngineError(msg)) {
        return;
    }

    if (!m_refs.gameOver->isOver) {
        switch (*m_refs.playMode) {
        case PlayMode::EvenHumanVsEngine:
        case PlayMode::EvenEngineVsHuman:
        case PlayMode::EvenEngineVsEngine:
        case PlayMode::HandicapHumanVsEngine:
        case PlayMode::HandicapEngineVsHuman:
        case PlayMode::HandicapEngineVsEngine:
            if (m_hooks.handleBreakOff) m_hooks.handleBreakOff();
            if (m_hooks.showGameOverDialog) {
                m_hooks.showGameOverDialog(
                    QObject::tr("対局中断"),
                    QObject::tr("エンジンエラー: %1").arg(msg));
            }
            return;
        default:
            break;
        }
    }
}
