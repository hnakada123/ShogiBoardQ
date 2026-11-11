/// @file p2pgamechannel.cpp
/// @brief P2P対局の窓口の実装

#include "p2pgamechannel.h"

#include "p2pgamecoordinator.h"

P2pGameChannel::P2pGameChannel(P2pGameCoordinator* coordinator)
    : m_coordinator(coordinator)
{
    Q_ASSERT(m_coordinator);
}

GameChannel::MoveResult P2pGameChannel::playMove(const UsiMove& move)
{
    return m_coordinator->playMove(move);
}

void P2pGameChannel::resign()
{
    m_coordinator->resign();
}

void P2pGameChannel::breakOff()
{
    m_coordinator->offerAbort();
}

bool P2pGameChannel::isInProgress() const
{
    return m_coordinator->isInProgress();
}

bool P2pGameChannel::isHumanToMove() const
{
    return m_coordinator->isHumanToMove();
}

bool P2pGameChannel::canResign() const
{
    return m_coordinator->canResign();
}

Turn P2pGameChannel::sideToMove() const
{
    return m_coordinator->sideToMove();
}
