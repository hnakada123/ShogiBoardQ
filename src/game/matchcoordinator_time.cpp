/// @file matchcoordinator_time.cpp
/// @brief MatchCoordinator 時間管理転送メソッド

#include "matchcoordinator.h"
#include "gameendhandler.h"
#include "matchtimekeeper.h"
#include "shogiclock.h"

// --- 時間管理転送 ---

void MatchCoordinator::setClock(ShogiClock* clock)
{
    m_timekeeper->setClock(clock);
    m_gameEndHandler->setClock(clock);
}

void MatchCoordinator::pokeTimeUpdateNow()
{
    m_timekeeper->pokeTimeUpdateNow();
}

ShogiClock* MatchCoordinator::clock()
{
    return m_timekeeper->clock();
}

const ShogiClock* MatchCoordinator::clock() const
{
    return m_timekeeper->clockConst();
}
