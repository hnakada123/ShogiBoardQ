/// @file gamechannel.cpp
/// @brief ローカル対局の窓口の実装

#include "gamechannel.h"

#include "matchcoordinator.h"
#include "shogiclock.h"

LocalGameChannel::LocalGameChannel(MatchCoordinator* match, const ShogiClock* clock)
    : m_match(match)
    , m_clock(clock)
{
    Q_ASSERT(m_match && m_clock);
}

void LocalGameChannel::resign()
{
    m_match->handleResign();
}

void LocalGameChannel::breakOff()
{
    m_match->handleBreakOff();
}

void LocalGameChannel::moveNow()
{
    m_match->forceImmediateMove();
}

bool LocalGameChannel::isHumanToMove() const
{
    return !m_match->isGameOver() && m_match->isHumanSide(m_match->sideToMove());
}

Turn LocalGameChannel::sideToMove() const
{
    return m_match->sideToMove();
}

std::optional<NyugyokuJudgement::Result> LocalGameChannel::declareNyugyoku()
{
    return m_match->declareNyugyoku(m_match->sideToMove(), NyugyokuJudgement::configuredRule());
}

MatchPlayers LocalGameChannel::players() const
{
    return m_match->players();
}

TimeControl LocalGameChannel::timeControl() const
{
    return m_clock->timeControl();
}
