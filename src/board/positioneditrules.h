#ifndef POSITIONEDITRULES_H
#define POSITIONEDITRULES_H

/// @file positioneditrules.h
/// @brief 局面編集で駒を動かす規則（盤・駒台・駒箱の間の移動）

#include <QPoint>

#include "shogitypes.h"

class ShogiBoard;

/**
 * @brief 局面編集で駒を盤・駒台・駒箱の間で動かす
 *
 * 座標は盤の表示と同じ（盤は筋 1〜9、先手の駒台は筋 10、後手の駒台は筋 11、駒箱は筋 12）。
 * 指し手の合法性は見ず、置けない形（玉を持ち駒にする・二歩・味方の駒の上・駒台に無い駒・玉を取る）だけを
 * 断る。行き所のない段に置いた歩・香・桂は ShogiBoard が成駒にする（PieceRules::isDeadSquare）。
 * 編集中の駒配置は手番を変えない。
 */
namespace PositionEditRules {

/// from の駒を to へ動かす。駒箱の駒は pieceBoxSide の駒として置く。動かせなければ盤を変えず false
bool apply(ShogiBoard& board, const QPoint& from, const QPoint& to, Turn pieceBoxSide = Turn::Black);

/// source（移動元の駒）を dest（移動先の駒。駒台なら駒台の駒種）の上へ動かせるか
bool canMove(const ShogiBoard& board, Piece source, Piece dest, int fileFrom, int fileTo);

} // namespace PositionEditRules

#endif // POSITIONEDITRULES_H
