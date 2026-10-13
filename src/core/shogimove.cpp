/// @file shogimove.cpp
/// @brief 将棋の指し手データ構造体の実装

#include "shogimove.h"
#include "boardcoordinates.h"

ShogiMove::ShogiMove() = default;

ShogiMove::ShogiMove(const QPoint& from, const QPoint& to, Piece moving, Piece captured, bool promotion)
    : fromSquare(from), toSquare(to), movingPiece(moving), capturedPiece(captured), isPromotion(promotion) {}

bool ShogiMove::operator==(const ShogiMove& other) const {
    return fromSquare == other.fromSquare
            && toSquare == other.toSquare
            && movingPiece == other.movingPiece
            && capturedPiece == other.capturedPiece
            && isPromotion == other.isPromotion;
}

ShogiMove ShogiMove::fromUsiMove(const UsiMove& move, Turn mover, Piece moving, Piece captured)
{
    const QPoint from = BoardCoordinates::fromSquare(move, mover);
    const QPoint to = BoardCoordinates::toSquare(move);
    return ShogiMove(QPoint(from.x() - 1, from.y() - 1), QPoint(to.x() - 1, to.y() - 1), moving, captured,
                     move.promote);
}

UsiMove ShogiMove::toUsiMove() const
{
    UsiMove move;
    move.to = BoardSquare{toSquare.x() + 1, toSquare.y() + 1};
    if (fromSquare.x() >= 9) {
        move.drop = toBlack(demote(movingPiece));
    } else {
        move.from = BoardSquare{fromSquare.x() + 1, fromSquare.y() + 1};
        move.promote = isPromotion;
    }
    return move;
}

QDebug operator<<(QDebug dbg, const ShogiMove& move) {
    QDebugStateSaver saver(dbg);
    auto disp = [](int v) -> int {
        // 盤上(0..8)は1-indexed表示、駒台(9,10)はそのまま
        return (0 <= v && v <= 8) ? (v + 1) : v;
    };

    dbg.nospace()
        << "From:(" << disp(move.fromSquare.x()) << "," << disp(move.fromSquare.y()) << ") "
        << "To:("   << disp(move.toSquare.x())   << "," << disp(move.toSquare.y())   << ") "
        << "Moving:"   << pieceToChar(move.movingPiece)
        << " Captured:" << pieceToChar(move.capturedPiece)
        << " Promotion:" << (move.isPromotion ? "true" : "false");
    return dbg;
}
