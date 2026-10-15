#ifndef SHOGIMOVE_H
#define SHOGIMOVE_H

/// @file shogimove.h
/// @brief 指した手の記録（USI の指し手と、動かした駒・取った駒）

#include <QDebug>
#include "shogitypes.h"
#include "usimove.h"

/**
 * @brief 指した1手の記録
 *
 * 指し手そのもの（移動元・移動先・成り・打つ駒）は UsiMove で持つ。筋・段は1始まり。
 * 指す前の局面で決まる「動かした駒」「取った駒」を添える。
 */
struct ShogiMove {
    UsiMove move;                       ///< 指し手（成り・駒打ちを含む）
    Piece movingPiece = Piece::None;    ///< 動かした駒（指す側の色の、成る前の駒）
    Piece capturedPiece = Piece::None;  ///< 取った駒（なければ None）

    ShogiMove() = default;
    ShogiMove(const UsiMove& usiMove, Piece moving, Piece captured = Piece::None)
        : move(usiMove), movingPiece(moving), capturedPiece(captured) {}

    bool isValid() const { return move.isValid() && movingPiece != Piece::None; }
    bool isDrop() const { return move.isDrop(); }
    bool isPromotion() const { return move.promote; }
    /// 指した側（動かした駒の色）
    Turn mover() const { return isWhitePiece(movingPiece) ? Turn::White : Turn::Black; }

    friend bool operator==(const ShogiMove& a, const ShogiMove& b)
    {
        return a.move == b.move && a.movingPiece == b.movingPiece && a.capturedPiece == b.capturedPiece;
    }
    friend bool operator!=(const ShogiMove& a, const ShogiMove& b) { return !(a == b); }
};

QDebug operator<<(QDebug dbg, const ShogiMove& move);

#endif // SHOGIMOVE_H
