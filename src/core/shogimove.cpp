#include "shogimove.h"

ShogiMove::ShogiMove()
    : fromSquare(0, 0), toSquare(0, 0), movingPiece(' '), capturedPiece(' '), isPromotion(false) {}

ShogiMove::ShogiMove(const QPoint& from, const QPoint& to, QChar moving, QChar captured, bool promotion)
    : fromSquare(from), toSquare(to), movingPiece(moving), capturedPiece(captured), isPromotion(promotion) {}

// 構造体ShogiMoveの比較演算子定義
// これにより直接"=="で指し手を比較できる。
bool ShogiMove::operator==(const ShogiMove& other) const {
    return fromSquare == other.fromSquare
            && toSquare == other.toSquare
            && movingPiece == other.movingPiece
            && capturedPiece == other.capturedPiece
            && isPromotion == other.isPromotion;
}

// 構造体ShogiMoveのデバッグプリント用演算子"<<"の定義
std::ostream& operator<<(std::ostream& os, const ShogiMove& move) {
    os << "From: (" << move.fromSquare.x() + 1 << ", " << move.fromSquare.y() + 1 << ')';
    os << " To: (" << move.toSquare.x() + 1 << ", " << move.toSquare.y() + 1 << ')';
    os << " Moving Piece: " << move.movingPiece.toLatin1();
    os << " Captured Piece: " << move.capturedPiece.toLatin1();
    os << " Promotion: " << (move.isPromotion ? "true" : "false");
    return os;
}

// ...既存コードの下に追加
QDebug operator<<(QDebug dbg, const ShogiMove& move) {
    QDebugStateSaver saver(dbg);
    auto disp = [](int v) -> int {
        // 盤上(0..8)は +1 表示、駒台(9,10など)はそのまま
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
