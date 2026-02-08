/// @file shogimove.cpp
/// @brief 将棋の指し手データ構造体の実装
/// @todo remove コメントスタイルガイド適用済み

#include "shogimove.h"

/// @todo remove コメントスタイルガイド適用済み
ShogiMove::ShogiMove()
    : fromSquare(0, 0), toSquare(0, 0), movingPiece(' '), capturedPiece(' '), isPromotion(false) {}

/// @todo remove コメントスタイルガイド適用済み
ShogiMove::ShogiMove(const QPoint& from, const QPoint& to, QChar moving, QChar captured, bool promotion)
    : fromSquare(from), toSquare(to), movingPiece(moving), capturedPiece(captured), isPromotion(promotion) {}

/// @todo remove コメントスタイルガイド適用済み
bool ShogiMove::operator==(const ShogiMove& other) const {
    return fromSquare == other.fromSquare
            && toSquare == other.toSquare
            && movingPiece == other.movingPiece
            && capturedPiece == other.capturedPiece
            && isPromotion == other.isPromotion;
}

/// @todo remove コメントスタイルガイド適用済み
std::ostream& operator<<(std::ostream& os, const ShogiMove& move) {
    os << "From: (" << move.fromSquare.x() + 1 << ", " << move.fromSquare.y() + 1 << ')';
    os << " To: (" << move.toSquare.x() + 1 << ", " << move.toSquare.y() + 1 << ')';
    os << " Moving Piece: " << move.movingPiece.toLatin1();
    os << " Captured Piece: " << move.capturedPiece.toLatin1();
    os << " Promotion: " << (move.isPromotion ? "true" : "false");
    return os;
}

/// @todo remove コメントスタイルガイド適用済み
QDebug operator<<(QDebug dbg, const ShogiMove& move) {
    QDebugStateSaver saver(dbg);
    auto disp = [](int v) -> int {
        // 盤上(0..8)は1-indexed表示、駒台(9,10)はそのまま
        return (0 <= v && v <= 8) ? (v + 1) : v;
    };

    dbg.nospace()
        << "From:(" << disp(move.fromSquare.x()) << "," << disp(move.fromSquare.y()) << ") "
        << "To:("   << disp(move.toSquare.x())   << "," << disp(move.toSquare.y())   << ") "
        << "Moving:"   << move.movingPiece
        << " Captured:" << move.capturedPiece
        << " Promotion:" << (move.isPromotion ? "true" : "false");
    return dbg;
}
