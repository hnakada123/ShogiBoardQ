/// @file parsemoveformat.cpp
/// @brief USI/USEN共通の指し手フォーマット変換関数の実装

#include "parsemoveformat.h"
#include "parsecommon.h"
#include "kifmovetext.h"
#include "sfenpositiontracer.h"
#include "usimove.h"

namespace KifuParseCommon {

int buildUsiMoveDisplayItems(const QStringList& usiMoves,
                             const QString& baseSfen,
                             int startPly,
                             QList<KifDisplayItem>& outDisp,
                             const QString& previousUsi)
{
    SfenPositionTracer tracer;
    if (!tracer.setFromSfen(baseSfen)) {
        tracer.resetToStartpos();
    }

    // 分岐の1手目でも、直前の手と同じ地点なら「同」と表記する
    BoardSquare previousTo;
    if (const auto previous = UsiMove::parse(previousUsi)) previousTo = previous->to;

    int plyNumber = startPly - 1;
    bool blackToMove = !baseSfen.contains(QStringLiteral(" w "));

    for (const QString& usi : std::as_const(usiMoves)) {
        ++plyNumber;
        const Turn mover = blackToMove ? Turn::Black : Turn::White;
        const QString teban = tebanMark(blackToMove ? 1 : 2);
        QString pretty;
        if (usi.startsWith(QChar('?'))) {
            // プレースホルダー指し手 (USEN でデコードできなかった指し手)
            pretty = teban + QStringLiteral("?(") + usi.mid(1) + QStringLiteral("手目)");
        } else if (const auto move = UsiMove::parse(usi)) {
            const Piece moving = move->isDrop() ? move->dropPieceFor(mover)
                                                : tracer.pieceAt(move->from.file, move->from.rank);
            pretty = KifMoveText::format(mover, moving, *move, previousTo);
            previousTo = move->to;
        } else {
            pretty = teban + QStringLiteral("?");
        }
        outDisp.push_back(createMoveDisplayItem(plyNumber, pretty));
        (void)tracer.applyUsiMove(usi);
        blackToMove = !blackToMove;
    }

    return plyNumber;
}

KifDisplayItem createUsiTerminalDisplayItem(int ply, const QString& term,
                                           const QString& baseSfen, qsizetype moveCount)
{
    const bool blackToMove = (moveCount % 2 == 0) != baseSfen.contains(QStringLiteral(" w "));
    auto item = createTerminalDisplayItem(ply, term);
    item.prettyMove = (blackToMove ? QStringLiteral("▲") : QStringLiteral("△")) + term;
    return item;
}

} // namespace KifuParseCommon
