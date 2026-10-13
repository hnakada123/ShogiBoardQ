/// @file matchcoordinator_time.cpp
/// @brief MatchCoordinator 時間管理転送メソッド

#include "matchcoordinator.h"
#include "matchtimekeeper.h"
#include "shogiclock.h"

// --- 時間管理転送 ---

void MatchCoordinator::markTurnEpochNowFor(Turn side, qint64 nowMs) {
    ensureTimekeeper();
    m_timekeeper->markTurnEpochNowFor(side, nowMs);
}

qint64 MatchCoordinator::turnEpochFor(Turn side) const {
    return m_timekeeper ? m_timekeeper->turnEpochFor(side) : -1;
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

void MatchCoordinator::recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const
{
    if (m_timekeeper) { m_timekeeper->recomputeClockSnapshot(turnText, p1, p2); return; }
    turnText.clear(); p1.clear(); p2.clear();
}
