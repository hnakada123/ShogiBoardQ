/// @file p2psignedline.cpp
/// @brief 双方が署名した手順と、その棋譜から決まる終局の実装

#include "p2psignedline.h"

namespace P2p {

const GameState& SignedLine::last() const
{
    static const GameState kNoState;
    return m_states.empty() ? kNoState : m_states.back().state;
}

void SignedLine::append(const SignedState& state)
{
    m_states.push_back(state);
    m_gameStates.push_back(state.state);
    m_terminal = m_judge.append(state.state);
}

FinalResult SignedLine::resultFor(Outcome outcome, Cause cause) const
{
    FinalResult r;
    r.gameId = m_gameId;
    r.finalStateHash = last().hash();
    r.transcriptHash = transcriptHash(m_gameId, m_gameStates);
    r.ply = last().ply;
    r.outcome = outcome;
    r.cause = cause;
    return r;
}

bool SignedLine::isCurrent(const FinalResult& result) const
{
    // 手数と最新の状態を先に比べる（手順の要約は、そろったときだけ求める）
    return result.gameId == m_gameId && result.ply == last().ply && result.finalStateHash == last().hash()
        && result == resultFor(result.outcome, result.cause);
}

} // namespace P2p
