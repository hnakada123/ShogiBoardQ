/// @file humanvshumanstrategy.cpp
/// @brief 人間 vs 人間モードの Strategy 実装

#include "humanvshumanstrategy.h"

#include "logcategories.h"

HumanVsHumanStrategy::HumanVsHumanStrategy(MatchStrategyHost& host)
    : m_host(host)
{
}

void HumanVsHumanStrategy::start()
{
    qCDebug(lcGame) << "[Match] Start HvH";
}

void HumanVsHumanStrategy::onHumanMove(const RecordEntry& played)
{
    finishTurnTimer(played.mover);
    if (m_host.completeHumanMove(played)) armTurnTimerIfNeeded();
}

void HumanVsHumanStrategy::armTurnTimerIfNeeded()
{
    if (m_turnTimerArmed) return;
    m_turnTimer.start();
    m_turnTimerArmed = true;
}

void HumanVsHumanStrategy::disarmTurnTimer()
{
    // 「待った」や終局で計測を打ち切る。次の armTurnTimerIfNeeded() で測り直す。
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}

void HumanVsHumanStrategy::finishTurnTimer(Turn mover)
{
    if (!m_turnTimerArmed) return;
    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;
    m_host.recordConsiderationTime(mover, ms);
    disarmTurnTimer();
}
