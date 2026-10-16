/// @file notationutils.cpp
/// @brief 棋譜変換器で共有する USI 指し手の組み立てと手合割の名前の実装

#include "notationutils.h"

#include "handicaps.h"
#include "sfenutils.h"
#include "usimove.h"

#include <QLatin1Char>
#include <QStringLiteral>

namespace NotationUtils {

QString formatSfenMove(int fromFile, int fromRank, int toFile, int toRank, bool promote)
{
    UsiMove move;
    move.from = BoardSquare{fromFile, fromRank};
    move.to = BoardSquare{toFile, toRank};
    move.promote = promote;
    return move.toString();
}

QString formatSfenDrop(QChar piece, int toFile, int toRank)
{
    UsiMove move;
    move.drop = UsiNotation::dropPieceFromChar(piece);
    move.to = BoardSquare{toFile, toRank};
    return move.toString();
}

QString mapHandicapToSfen(const QString& label)
{
    const Handicaps::Handicap* handicap = Handicaps::byLabel(label);
    return handicap ? QString::fromLatin1(handicap->sfen) : SfenUtils::hirateSfen();
}

QString handicapLabelForSfen(const QString& sfen)
{
    const Handicaps::Handicap* handicap = Handicaps::byBoard(sfen);
    return handicap ? Handicaps::labelOf(*handicap) : QString();
}

} // namespace NotationUtils
