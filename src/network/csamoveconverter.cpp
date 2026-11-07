/// @file csamoveconverter.cpp
/// @brief CSA の指し手と USI の指し手の変換の実装

#include "csamoveconverter.h"

#include "csalexer.h"

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
    // 書式と成りの読み方は棋譜の CSA の読み込みと同じ（CsaLexer::parseMoveLine）
    const std::optional<CsaLexer::CsaMove> parsed = CsaLexer::parseMoveLine(csaMove.left(7), board, nullptr);
    if (!parsed || parsed->written.side != board.sideToMove()) return std::nullopt;
    const UsiMove& move = parsed->move;
    if (move.isDrop() && board.handCount(move.dropPieceFor(board.sideToMove())) <= 0) return std::nullopt;
    // 駒種・持ち駒が盤と合うかは、同じ指し手を CSA に戻して確かめる
    if (!move.isValid() || toCsa(move, board.sideToMove(), board) != csaMove.left(7)) return std::nullopt;
    return move;
}

} // namespace CsaMoveConverter
