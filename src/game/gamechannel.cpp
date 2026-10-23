/// @file gamechannel.cpp
/// @brief ローカル対局の窓口の実装

#include "gamechannel.h"

#include "matchcoordinator.h"

LocalGameChannel::LocalGameChannel(MatchCoordinator* match)
    : m_match(match)
{
    Q_ASSERT(m_match);
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
    // 持将棋ルールが「なし」なら宣言の前に止めている（NyugyokuDeclarationHandler）。届いたら27点法で判定する
    return m_match->declareNyugyoku(m_match->sideToMove(),
                                    NyugyokuJudgement::configuredRule().value_or(JishogiCalculator::Rule::Points27));
}
