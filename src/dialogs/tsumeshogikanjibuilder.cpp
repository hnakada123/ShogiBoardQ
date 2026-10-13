/// @file tsumeshogikanjibuilder.cpp
/// @brief 詰将棋の漢字PV構築ユーティリティの実装

#include "tsumeshogikanjibuilder.h"
#include "shogiboard.h"
#include "kifmovetext.h"
#include "shoginotation.h"
#include "usimove.h"

namespace TsumeshogiKanjiBuilder {

QString pieceCharToKanji(QChar piece)
{
    return ShogiNotation::movePieceName(charToPiece(piece));
}

QString buildKanjiPv(const QString& baseSfen, const QStringList& pvMoves)
{
    ShogiBoard board;
    board.setSfen(baseSfen);
    bool blackToMove = !baseSfen.contains(QStringLiteral(" w "));

    QString result;
    for (const QString& text : std::as_const(pvMoves)) {
        const auto move = UsiMove::parse(text);
        if (!move) continue;
        const Turn mover = blackToMove ? Turn::Black : Turn::White;
        const Piece moving = move->isDrop() ? move->dropPieceFor(mover)
                                            : board.pieceCharacter(move->from.file, move->from.rank);

        // 詰将棋の読み筋は「同」を使わず、移動先を毎手書く
        result += KifMoveText::format(mover, moving, *move);

        // 盤面に反映（読み筋の漢字化なので、駒台の枚数は確かめずに打つ）
        if (move->isDrop()) {
            if (board.pieceStandCount(moving) > 0) board.consumeStandPiece(moving);
            board.movePieceToSquare(moving, 0, 0, move->to.file, move->to.rank, false);
        } else {
            board.applyUsiMove(*move, mover);
        }
        blackToMove = !blackToMove;
    }
    return result;
}

} // namespace TsumeshogiKanjiBuilder
