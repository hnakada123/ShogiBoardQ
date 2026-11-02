#ifndef MATCHENDING_H
#define MATCHENDING_H

/// @file matchending.h
/// @brief 対局の終局の原因と、原因から棋譜の終局の行・エンジンへの gameover への対応（ローカル対局・通信対局で共通）

#include "recordentry.h"
#include "shogitypes.h"
#include "terminaltype.h"

/// 終局原因（MatchCoordinator::Cause）。通信対局はサーバーの結果をこの原因に直す（CsaGameProgress::endInfo）
enum class MatchCause : int {
    Resignation    = 0,  ///< 投了
    Timeout        = 1,  ///< 時間切れ
    BreakOff       = 2,  ///< 中断（通信対局はサーバーの中断・切断と、知らない終局の知らせ）
    Jishogi        = 3,  ///< 持将棋（入玉宣言の点数による引き分け）
    NyugyokuWin    = 4,  ///< 入玉宣言勝ち
    IllegalMove    = 5,  ///< 反則負け（入玉宣言失敗など）
    Sennichite     = 6,  ///< 千日手（引き分け）
    OuteSennichite = 7,  ///< 連続王手の千日手（反則負け）
    MaxMoves       = 8,  ///< 最大手数到達（棋譜には持将棋と書く）
};

/// 終局情報（MatchCoordinator::GameEndInfo）。対局者（先手・後手）は Turn で表す
struct MatchGameEndInfo {
    MatchCause  cause = MatchCause::Resignation;  ///< 終局原因
    Turn        loser = Turn::Black;              ///< 敗者（勝ち負けのない終局では使わない）
};

/**
 * @brief 終局の原因から、棋譜の終局の行とエンジンへの gameover を決める（この対応はここだけ）
 *
 * ローカル対局（MatchCoordinator）と通信対局（CsaGameCoordinator）が同じ関数を使い、同じ原因なら同じ終局語・
 * 同じ gameover になる。最大手数は持将棋と書き、中断も含めて勝ち負けのない終局はエンジンに draw と知らせる。
 */
namespace MatchEnding {

/// 勝ち負けのない終局か（中断・持将棋・最大手数・千日手）
bool isDraw(MatchCause cause);
/// 棋譜の終局語（最大手数は持将棋、連続王手の千日手は王手をかけていた側の反則負け）
TerminalType terminalOf(MatchCause cause);
/// 終局の行の終局語と側（終局の行を行った側の行を、勝敗を変えずに手番側の行にそろえる。RecordEntry::forSideToMove）
RecordEntry::Ending endingOf(const MatchGameEndInfo& info, Turn sideToMove);
/// side のエンジンに送る gameover（勝ち負けのない終局は draw）
GameOverResult engineResult(const MatchGameEndInfo& info, Turn side);

} // namespace MatchEnding

#endif // MATCHENDING_H
