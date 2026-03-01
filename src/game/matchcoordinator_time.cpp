/// @file matchcoordinator_time.cpp
/// @brief MatchCoordinator 時間管理転送メソッド

#include "matchcoordinator.h"
#include "matchtimekeeper.h"
#include "shogiclock.h"

// --- 時間管理転送 ---

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

MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes() const {
    return m_timekeeper ? m_timekeeper->computeGoTimes() : GoTimes{};
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

void MatchCoordinator::recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const
{
    if (m_timekeeper) { m_timekeeper->recomputeClockSnapshot(turnText, p1, p2); return; }
    turnText.clear(); p1.clear(); p2.clear();
}
