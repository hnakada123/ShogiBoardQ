#ifndef POSITIONEDITRULES_H
#define POSITIONEDITRULES_H

/// @file positioneditrules.h
/// @brief 局面編集の規則（盤・駒台・駒箱の間の移動、右クリックの成・不成・先後の巡回、盤を空にする）

#include <QPoint>
#include <optional>

#include "shogiposition.h"
#include "shogitypes.h"

class ShogiBoard;

/**
 * @brief 局面編集の操作を、局面（Position）を変える関数として持つ
 *
 * 座標は盤の表示と同じ（盤は筋 1〜9、先手の駒台は筋 10、後手の駒台は筋 11、駒箱は筋 12。並びは
 * BoardCoordinates）。指し手の合法性は見ず、置けない形（玉を持ち駒にする・二歩・味方の駒の上・駒台に無い駒・
 * 玉を取る）だけを断る。行き所のない段に置いた歩・香・桂は成駒にする（PieceRules::isDeadSquare）。
 * 編集中の駒配置は手番を変えない。盤への反映は、変わった所だけを知らせる ShogiBoard::replacePosition で行う
 * （apply・cyclePiece・clear）。
 */
namespace PositionEditRules {

/// from の駒を to へ動かした局面。駒箱の駒は pieceBoxSide の駒として置く。動かせなければ nullopt
std::optional<Position> moved(const Position& position, const QPoint& from, const QPoint& to,
                              Turn pieceBoxSide = Turn::Black);

/// 盤上のマスの駒を、先手の生駒 → 成駒 → 後手の生駒 → 成駒の順に巡らせた局面（行き所のない段・二歩になる形は
/// 飛ばす）。駒が無い・盤外なら nullopt
std::optional<Position> cycled(const Position& position, BoardSquare square);

/// 盤上・両駒台を空にした局面（全駒が駒箱に戻る。手番と手数は残す）
Position cleared(const Position& position);

/// source（移動元の駒）を dest（移動先の駒。駒台なら駒台の駒種）の上へ動かせるか
bool canMove(const Position& position, Piece source, Piece dest, int fileFrom, int fileTo);

// --- 盤への反映 ---

/// moved の局面を盤に反映する。動かせなければ盤を変えず false
bool apply(ShogiBoard& board, const QPoint& from, const QPoint& to, Turn pieceBoxSide = Turn::Black);
/// cycled の局面を盤に反映する。巡らせられなければ盤を変えず false
bool cyclePiece(ShogiBoard& board, BoardSquare square);
/// 盤上・両駒台を空にする（全部の駒が動くので、局面を丸ごと置き換えて知らせる）
void clear(ShogiBoard& board);

} // namespace PositionEditRules

#endif // POSITIONEDITRULES_H
