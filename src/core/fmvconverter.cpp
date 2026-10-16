/// @file fmvconverter.cpp
/// @brief Qt型 ←→ エンジン内部型の境界変換

#include "fmvconverter.h"

#include "shogiposition.h"

namespace fmv {

bool Converter::toEnginePosition(EnginePosition& out, const Position& position)
{
    out.clear();

    // 盤は同じ符号・同じ並び（[段 * 9 + 筋]、どちらも0始まり）
    out.board = position.squares();

    // 持ち駒
    for (const Color color : {Color::Black, Color::White}) {
        for (int ht = 0; ht < static_cast<int>(HandType::HandTypeNb); ++ht) {
            const Piece piece = pieceOf(color, pieceTypeOf(static_cast<HandType>(ht)));
            out.hand[static_cast<int>(color)][static_cast<std::size_t>(ht)] = position.handCount(piece);
        }
    }

    // ビットボード構築
    out.rebuildBitboards();
    return true;
}

bool Converter::toEngineMove(ConvertedMove& out,
                             Turn turn,
                             const EnginePosition& pos,
                             const UsiMove& move)
{
    out = {};
    if (!move.to.isValid()) {
        return false;
    }
    const Square toSq = toSquare(move.to.file - 1, move.to.rank - 1);

    const Color sideColor = toColor(turn);
    if (move.isDrop()) {
        const PieceType pt = typeOf(move.drop);
        const HandType ht = handTypeOf(pt);
        if (ht == HandType::HandTypeNb) {
            return false;
        }
        if (pos.hand[static_cast<std::size_t>(sideColor)][static_cast<std::size_t>(ht)] <= 0) {
            return false;
        }
        out.nonPromote.kind = MoveKind::Drop;
        out.nonPromote.from = kInvalidSquare;
        out.nonPromote.to = toSq;
        out.nonPromote.piece = pt;
        out.nonPromote.promote = false;
        out.hasPromoteVariant = false;
        return true;
    }

    // 盤上移動
    if (!move.from.isValid()) {
        return false;
    }
    const Square fromSq = toSquare(move.from.file - 1, move.from.rank - 1);

    // 盤上の駒から実体を取得
    const Piece boardPiece = pos.board[fromSq];
    if (boardPiece == Piece::None || colorOf(boardPiece) != sideColor) {
        return false;
    }
    const PieceType pieceType = typeOf(boardPiece);

    out.nonPromote.kind = MoveKind::Board;
    out.nonPromote.from = fromSq;
    out.nonPromote.to = toSq;
    out.nonPromote.piece = pieceType;
    out.nonPromote.promote = false;

    // 成りバリアント
    if (isPromotableType(pieceType)) {
        const int fromRank = squareRank(fromSq);
        const int toRank = squareRank(toSq);
        const bool inPromotionZone = (sideColor == Color::Black)
            ? (fromRank <= 2 || toRank <= 2)
            : (fromRank >= 6 || toRank >= 6);
        if (inPromotionZone) {
            out.promote = out.nonPromote;
            out.promote.promote = true;
            // piece は元の駒種のまま（成りフラグで区別）
            out.hasPromoteVariant = true;
        }
    }

    return true;
}

Color Converter::toColor(Turn t) noexcept
{
    return (t == Turn::Black) ? Color::Black : Color::White;
}

UsiMove Converter::toUsiMove(const Move& m)
{
    UsiMove move;
    move.to = BoardSquare{squareFile(m.to) + 1, squareRank(m.to) + 1};
    if (m.kind == MoveKind::Drop) {
        move.drop = pieceOf(Color::Black, m.piece);
        return move;
    }
    move.from = BoardSquare{squareFile(m.from) + 1, squareRank(m.from) + 1};
    move.promote = m.promote;
    return move;
}

} // namespace fmv
