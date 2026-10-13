#ifndef KIFMOVETEXT_H
#define KIFMOVETEXT_H

/// @file kifmovetext.h
/// @brief KIF 形式の指し手の文字列（「▲７六歩(77)」「△同　銀(31)」「▲５五角打」）

#include <QString>

#include "shoginotation.h"
#include "usimove.h"

namespace KifMoveText {

/**
 * @brief KIF 形式の指し手の文字列を作る
 * @param mover 指す側（手番の印 ▲△ を決める）
 * @param moving 動かす駒（成る前の駒。駒打ちは打つ駒）
 * @param move 指し手
 * @param previousTo 直前の手の移動先。同じマスなら移動先を「同　」と書く（無効なマスなら毎回マスを書く）
 *
 * 対局の棋譜・棋譜の読み込み・通信対局・定跡・詰将棋の読み筋で同じ書式を使う。
 * 成香・成桂・成銀は2文字で書く。
 */
inline QString format(Turn mover, Piece moving, const UsiMove& move, BoardSquare previousTo = {})
{
    QString text = (mover == Turn::Black) ? QStringLiteral("▲") : QStringLiteral("△");
    if (previousTo.isValid() && previousTo == move.to) {
        text += QStringLiteral("同　");
    } else {
        text += ShogiNotation::fileZenkaku(move.to.file);
        text += ShogiNotation::rankKanji(move.to.rank);
    }
    text += ShogiNotation::movePieceName(moving);
    if (move.isDrop()) return text + QStringLiteral("打");
    if (move.promote) text += QStringLiteral("成");
    return text + QStringLiteral("(%1%2)").arg(move.from.file).arg(move.from.rank);
}

} // namespace KifMoveText

#endif // KIFMOVETEXT_H
