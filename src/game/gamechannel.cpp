/// @file gamechannel.cpp
/// @brief ローカル対局の窓口の実装

#include "gamechannel.h"

#include "matchcoordinator.h"

LocalGameChannel::LocalGameChannel(MatchCoordinator* match)
    : m_match(match)
{
    Q_ASSERT(m_match);
}

GameChannel::MoveResult LocalGameChannel::playMove(const UsiMove& move)
{
    return m_match->playHumanMove(move) ? MoveResult::Played : MoveResult::Rejected;
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

bool LocalGameChannel::isInProgress() const
{
    return !m_match->isGameOver();
}

bool LocalGameChannel::isHumanToMove() const
{
    return m_match->canAcceptMove() && m_match->isHumanSide(m_match->sideToMove());
}

Turn LocalGameChannel::sideToMove() const
{
    return m_match->sideToMove();
}

bool LocalGameChannel::acceptsDeclaration() const
{
    return m_match->options().jishogiRule.has_value();
}

std::optional<NyugyokuJudgement::Result> LocalGameChannel::declareNyugyoku()
{
    // 持将棋ルールが「なし」なら宣言の前に止めている（NyugyokuDeclarationHandler が acceptsDeclaration で確かめる）
    return m_match->declareNyugyoku(m_match->sideToMove());
}
