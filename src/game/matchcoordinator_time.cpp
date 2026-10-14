/// @file matchcoordinator_time.cpp
/// @brief MatchCoordinator 時間管理転送メソッド

#include "matchcoordinator.h"
#include "matchtimekeeper.h"
#include "shogiclock.h"

// --- 時間管理転送 ---

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
