#ifndef P2PSIGNEDLINE_H
#define P2PSIGNEDLINE_H

/// @file p2psignedline.h
/// @brief 双方が署名した手順と、その棋譜から決まる終局（実装仕様 6章・8章）

#include <QByteArray>

#include <optional>
#include <utility>
#include <vector>

#include "p2pevidence.h"
#include "p2pgamerules.h"

namespace P2p {

/**
 * @brief 両方の署名がそろった状態の列（開始局面 State0 から最新まで）
 *
 * 状態を足すたびに、棋譜から決まる終局（詰み・合法手なし・千日手・連続王手の千日手・512手到達）を
 * GameRules::TerminalJudge で1度だけ求めて持つ。
 * 結果の提案が今の手数を指しているかも、この列で決める。状態が規則どおりかは、足す前に呼ぶ側が確かめる。
 */
class SignedLine
{
public:
    explicit SignedLine(QByteArray gameId)
        : m_gameId(std::move(gameId))
    {
    }

    bool isEmpty() const { return m_states.empty(); }
    /// 状態の数（State0 を含む）
    std::size_t size() const { return m_states.size(); }
    const std::vector<SignedState>& states() const { return m_states; }
    /// ply 手目の状態（ply は size() より小さいこと）
    const SignedState& at(quint32 ply) const { return m_states[ply]; }
    /// 最新の状態。State0 の承認がそろう前は、手数 0 の空の状態
    const GameState& last() const;
    /// 最新の状態の手番
    Turn sideToMove() const { return GameRules::sideToMove(last().ply); }

    /// 状態を足し、棋譜から決まる終局を求め直す
    void append(const SignedState& state);
    /// 棋譜から決まる終局（決まっていなければ nullopt）
    const std::optional<GameRules::Terminal>& terminal() const { return m_terminal; }

    /// 最新の状態を指す結果
    FinalResult resultFor(Outcome outcome, Cause cause) const;
    /// result が、最新の状態と、そこまでの手順を指しているか
    bool isCurrent(const FinalResult& result) const;

private:
    QByteArray m_gameId;
    std::vector<SignedState> m_states;
    std::vector<GameState> m_gameStates;   ///< m_states の状態だけ（手順の要約を求めるのに使う）
    GameRules::TerminalJudge m_judge;
    std::optional<GameRules::Terminal> m_terminal;
};

} // namespace P2p

#endif // P2PSIGNEDLINE_H
