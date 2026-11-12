#ifndef P2PGAMERULES_H
#define P2PGAMERULES_H

/// @file p2pgamerules.h
/// @brief P2P対局の将棋の規則（次の状態の作成と検証・時計の算術・棋譜から決まる終局。実装仕様 7.1節・8章）

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

#include "p2pprotocol.h"
#include "shogitypes.h"
#include "usimove.h"

class Position;
struct MatchGameEndInfo;

namespace P2p {

namespace GameRules {

/// 状態の後の手番（ply が偶数なら先手）
inline Turn sideToMove(quint32 ply) { return ply % 2 == 0 ? Turn::Black : Turn::White; }

/// 局面の SFEN（空白1個区切り・持駒は RBGSNLPrbgsnlp の順・手数の欄は ply + 1）
QByteArray sfenFor(const Position& position, quint32 ply);

/// 対局の最初の状態（State0）
GameState initialState(const QByteArray& gameId, const TimeControlTerms& terms);

/// 着手できる時間か（手番開始時の基本持ち時間 R・秒読み B・消費時間 E で、E < R + B）
bool canMoveWithin(quint64 remainingMainMs, quint64 byoyomiMs, quint64 spentMs);

/**
 * @brief 前の状態から、手番の側が move を spentMs で指した次の状態を作る
 *
 * 合法手でない・時間切れ（E >= R + B）・512手を超える手なら nullopt（error に理由）。
 */
std::optional<GameState> nextState(const GameState& previous, const UsiMove& move, quint64 spentMs,
                                   const TimeControlTerms& terms, QString* error = nullptr);

/// 受け取った次の状態の検査の結果
enum class StateCheck {
    Ok,
    WrongGame,      ///< gameId が違う
    WrongPly,       ///< 手数が前の状態の次でない
    WrongPrevious,  ///< 前状態ハッシュが違う
    IllegalMove,    ///< 指し手が読めない・合法手でない・512手を超える
    SfenMismatch,   ///< 指した後の SFEN が合わない
    ClockMismatch,  ///< 残り時間の計算が合わない、または時間切れの後の手
};

/// 受け取った next が previous の次の状態として正しいか（合法手・SFEN・時計の算術）
StateCheck checkNext(const GameState& previous, const GameState& next, const TimeControlTerms& terms);

/// 棋譜から決まる終局（詰み・合法手なし・連続王手の千日手・千日手・512手）
struct Terminal {
    Outcome outcome = Outcome::Draw;
    Cause cause = Cause::MaxPly;
};

/**
 * @brief 手順の最後の状態が規則上の終局か
 *
 * 詰み（合法手のない手番側の負け）、連続王手の千日手、千日手、512手到達の順で判定する（実装仕様 8章）。
 * states は State0 から連続していること（検査済み）。
 */
std::optional<Terminal> judge(const std::vector<GameState>& states);

/// 勝った側の outcome
inline Outcome winnerOutcome(Turn winner) { return winner == Turn::Black ? Outcome::BlackWins : Outcome::WhiteWins; }

/**
 * @brief 合意した結果を、ローカル対局・通信対局と同じ終局の原因（MatchCause）にする
 *
 * 棋譜の終局の行は、この原因から MatchEnding::endingOf（matchending.h）で決める（最大手数は持将棋と書く）。
 * 合意の中断は中断（BreakOff）。勝ち負けのない終局では loser を使わない。
 */
MatchGameEndInfo endInfo(const FinalResult& result);

} // namespace GameRules

} // namespace P2p

#endif // P2PGAMERULES_H
