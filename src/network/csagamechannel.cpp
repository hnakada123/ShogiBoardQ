/// @file csagamechannel.cpp
/// @brief 通信対局の窓口の実装

#include "csagamechannel.h"

#include "csagamecoordinator.h"

CsaGameChannel::CsaGameChannel(CsaGameCoordinator* coordinator)
    : m_coordinator(coordinator)
{
    Q_ASSERT(m_coordinator);
}

void CsaGameChannel::resign()
{
    m_coordinator->onResign();
}

void CsaGameChannel::breakOff()
{
    m_coordinator->requestChudan();
}

void CsaGameChannel::moveNow()
{
    m_coordinator->forceImmediateMove();
}

bool CsaGameChannel::isHumanToMove() const
{
    return m_coordinator->isHumanPlayer() && m_coordinator->isMyTurn();
}

Turn CsaGameChannel::sideToMove() const
{
    const bool myTurn = m_coordinator->isMyTurn();
    const bool black = m_coordinator->isBlackSide() ? myTurn : !myTurn;
    return black ? Turn::Black : Turn::White;
}

std::optional<NyugyokuJudgement::Result> CsaGameChannel::declareNyugyoku()
{
    m_coordinator->declareWin();
    return std::nullopt;
}

MatchPlayers CsaGameChannel::players() const
{
    MatchPlayers players;
    players.blackHumanName = m_coordinator->gameSummary().blackName;
    players.whiteHumanName = m_coordinator->gameSummary().whiteName;
    return players;
}

TimeControl CsaGameChannel::timeControl() const
{
    return m_coordinator->gameSummary().timeControl();
}
