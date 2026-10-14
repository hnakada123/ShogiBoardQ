/// @file humanvshumanstrategy.cpp
/// @brief 人間 vs 人間モードの Strategy 実装

#include "humanvshumanstrategy.h"
#include "strategycontext.h"
#include "shogigamecontroller.h"
#include "shogiclock.h"
#include "logcategories.h"

HumanVsHumanStrategy::HumanVsHumanStrategy(MatchCoordinator::StrategyContext& ctx)
    : m_ctx(ctx)
{
}

void HumanVsHumanStrategy::start()
{
    qCDebug(lcGame) << "[Match] Start HvH";

    // --- 手番の単一ソースを確立：GC → TurnManager → m_cur → 表示
    const Turn side = m_ctx.gc() ? m_ctx.gc()->currentPlayer() : Turn::Black;

    m_ctx.setCurrentTurn(side);

    // 盤描画は手番反映のあと（ハイライト/時計のズレ防止）
    if (m_ctx.hooks().ui.renderBoardFromGc)
        m_ctx.hooks().ui.renderBoardFromGc();
    m_ctx.updateTurnDisplay(m_ctx.currentTurn());
}

void HumanVsHumanStrategy::onHumanMove(const QPoint& from, const QPoint& to,
                                        const QString& prettyMove)
{
    const Turn after = m_ctx.gc() ? m_ctx.gc()->currentPlayer() : Turn::White;
    const Turn mover = after == Turn::Black ? Turn::White : Turn::Black;
    finishTurnTimerAndSetConsideration(mover);
    if (m_ctx.completeHumanMove(mover, prettyMove, from, to)) armTurnTimerIfNeeded();
}

void HumanVsHumanStrategy::armTurnTimerIfNeeded()
{
    if (!m_turnTimerArmed) {
        m_turnTimer.start();
        m_turnTimerArmed = true;
    }
}

void HumanVsHumanStrategy::disarmTurnTimer()
{
    // 「待った」や終局で計測を打ち切る。次の armTurnTimerIfNeeded() で測り直す。
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}

void HumanVsHumanStrategy::finishTurnTimerAndSetConsideration(Turn mover)
{
    if (!m_turnTimerArmed) return;

    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;

    ShogiClock* clock = m_ctx.clock();
    if (clock) clock->setMeasuredConsiderationTime(mover, ms);
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}
