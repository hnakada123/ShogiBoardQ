#ifndef MATESEARCHPOSITION_H
#define MATESEARCHPOSITION_H

/// @file matesearchposition.h
/// @brief 詰み探索（go mate）に送る局面（途中局面は手順で、開始局面は攻方の手番に合わせて）

#include <optional>

#include "recordline.h"
#include "shogiposition.h"
#include "usiposition.h"

namespace MateSearchPosition {

/**
 * @brief 1つの局面（詰将棋の問題など）を、玉の有無から決めた攻方の手番・手数1にする
 *
 * 玉が片方だけなら、その玉のない側を攻方として手番にする（詰将棋の慣例で攻方の玉は置かない）。
 * 両方あるか両方ないときは手番のまま。
 */
inline Position attackerToMove(Position position)
{
    const bool blackKing = position.countOnBoard(makePiece(Turn::Black, PieceKind::King)) > 0;
    const bool whiteKing = position.countOnBoard(makePiece(Turn::White, PieceKind::King)) > 0;
    if (blackKing != whiteKing) position.setSideToMove(whiteKing ? Turn::Black : Turn::White);
    position.setMoveNumber(1);
    return position;
}

/**
 * @brief 表示中の手順の局面（line.currentPly）を詰み探索に送る値
 *
 * 途中局面は開始局面からの手順で送る（エンジンに千日手の判断材料を渡す。RecordLine::usiPosition）。
 * 開始局面は attackerToMove で手番を合わせる。手順が無ければ standaloneSfen（表示中の局面）を使う。
 * 局面を読めなければ nullopt。
 */
inline std::optional<UsiPosition> forLine(const RecordLine& line, const QString& standaloneSfen)
{
    if (!line.isEmpty() && line.currentPly > 0) return line.usiPosition(line.currentPly);
    const std::optional<Position> start = Position::fromSfen(line.isEmpty() ? standaloneSfen : line.startSfen);
    if (!start) return std::nullopt;
    return UsiPosition(attackerToMove(*start));
}

} // namespace MateSearchPosition

#endif // MATESEARCHPOSITION_H
