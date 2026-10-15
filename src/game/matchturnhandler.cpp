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


MatchTurnHandler::MatchTurnHandler(std::unique_ptr<MatchStrategyContext> context)
    : m_strategyCtx(std::move(context))
{
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
    switch (m_refs.session->playMode()) {
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine: {
        if (m_hooks.initEnginesForEvE)
            m_hooks.initEnginesForEvE(opt.engineName1, opt.engineName2);
        if (m_hooks.initAndStartEngine) {
            m_hooks.initAndStartEngine(Turn::Black, opt.enginePath1, opt.engineName1);
            m_hooks.initAndStartEngine(Turn::White, opt.enginePath2, opt.engineName2);
        }
        m_strategy = std::make_unique<EngineVsEngineStrategy>(
            *m_strategyCtx, m_refs.mcAsParent);
        m_strategy->start();
        break;
    }

    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, Turn::Black, opt.enginePath1, opt.engineName1);
        m_strategy->start();
        break;

    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        m_strategy = std::make_unique<HumanVsEngineStrategy>(
            *m_strategyCtx, Turn::White, opt.enginePath2, opt.engineName2);
        m_strategy->start();
        break;

    case PlayMode::HumanVsHuman:
        m_strategy = std::make_unique<HumanVsHumanStrategy>(*m_strategyCtx);
        m_strategy->start();
        break;

    default:
        qCWarning(lcGame).noquote()
            << "configureAndStart: unexpected playMode="
            << static_cast<int>(m_refs.session->playMode());
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

void MatchTurnHandler::finishTurnTimerAndSetConsiderationFor(Turn mover)
{
    if (m_strategy) m_strategy->finishTurnTimerAndSetConsideration(mover);
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

void MatchTurnHandler::updateTurnDisplay(Turn p)
{
    if (m_hooks.updateTurnDisplayCb) m_hooks.updateTurnDisplayCb(p);
}

// --- 対局進行 ---

void MatchTurnHandler::forceImmediateMove()
{
    if (!m_refs.gc || !m_refs.session || !m_hooks.sendStopToEngine) return;
    if (m_refs.session && m_refs.session->gameOver().isOver) return;

    const Turn player = m_refs.gc->currentPlayer();
    // 人間の手番に「すぐ指させる」を押しても先読みは停止しない。
    switch (m_refs.session->playMode()) {
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::HandicapHumanVsEngine:
        if (player != Turn::White) return;
        break;
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::HandicapEngineVsHuman:
        if (player != Turn::Black) return;
        break;
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapEngineVsEngine:
        break;
    default:
        return;
    }

    const bool isEvE =
        (m_refs.session->playMode() == PlayMode::EvenEngineVsEngine) ||
        (m_refs.session->playMode() == PlayMode::HandicapEngineVsEngine);

    if (isEvE) {
        Usi* eng = (m_refs.gc->currentPlayer() == Turn::Black)
            ? (m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr)
            : (m_hooks.secondaryEngine ? m_hooks.secondaryEngine() : nullptr);
        if (eng) m_hooks.sendStopToEngine(eng);
    } else if (Usi* eng = m_hooks.primaryEngine ? m_hooks.primaryEngine() : nullptr) {
        m_hooks.sendStopToEngine(eng);
    }
}

void MatchTurnHandler::handlePlayerTimeOut(Turn player)
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

    if (!m_refs.session->gameOver().isOver) {
        switch (m_refs.session->playMode()) {
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
