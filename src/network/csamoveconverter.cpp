/// @file csamoveconverter.cpp
/// @brief CSA の指し手と USI の指し手の変換の実装

#include "csamoveconverter.h"

#include "shoginotation.h"
#include "shogiposition.h"

namespace {

QString squareText(BoardSquare square)
{
    return square.isValid() ? QString::number(square.file) + QString::number(square.rank) : QStringLiteral("00");
}

} // namespace

namespace CsaMoveConverter {

QString toCsa(const UsiMove& move, Turn mover, const Position& board)
{
    if (!move.isValid()) return {};
    Piece after = Piece::None;
    if (move.isDrop()) {
        after = move.drop;
    } else {
        const Piece source = board.pieceAt(move.from);
        if (source == Piece::None) return {};
        after = move.promote ? promote(source) : source;
    }
    const QString code = ShogiNotation::csaPieceCode(after);
    if (code.isEmpty()) return {};
    const QChar sign = mover == Turn::Black ? QLatin1Char('+') : QLatin1Char('-');
    return sign + squareText(move.isDrop() ? BoardSquare{} : move.from) + squareText(move.to) + code;
}

std::optional<UsiMove> toUsi(const QString& csaMove, const Position& board)
{
    if (csaMove.size() < 7) return std::nullopt;
    const QChar sign = csaMove.at(0);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return std::nullopt;
    const Turn mover = sign == QLatin1Char('+') ? Turn::Black : Turn::White;
    if (mover != board.sideToMove()) return std::nullopt;

    const BoardSquare from{csaMove.at(1).digitValue(), csaMove.at(2).digitValue()};
    const BoardSquare to{csaMove.at(3).digitValue(), csaMove.at(4).digitValue()};
    const Piece after = ShogiNotation::pieceFromCsaCode(QStringView(csaMove).mid(5, 2));
    if (!to.isValid() || after == Piece::None) return std::nullopt;

    UsiMove move;
    move.to = to;
    if (from.file == 0 && from.rank == 0) {
        // 打てるのは持ち駒にある玉以外の生駒
        if (isPromoted(after) || after == Piece::BlackKing) return std::nullopt;
        move.drop = after;
        if (board.handCount(move.dropPieceFor(mover)) <= 0) return std::nullopt;
    } else {
        // 成る手は、盤の生駒が CSA では成った駒になっている
        move.from = from;
        const Piece source = board.pieceAt(from);
        move.promote = isPromoted(after) && source != Piece::None && !isPromoted(source);
    }
    // 駒種・持ち駒が盤と合うかは、同じ指し手を CSA に戻して確かめる
    if (!move.isValid() || toCsa(move, mover, board) != csaMove.left(7)) return std::nullopt;
    return move;
}

} // namespace CsaMoveConverter
