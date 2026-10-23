#ifndef KIFMOVETEXT_H
#define KIFMOVETEXT_H

/// @file kifmovetext.h
/// @brief KIF 形式の指し手の文字列（「▲７六歩(77)」「△同　銀(31)」「▲５五角打」）

#include <QString>

#include "piecerules.h"
#include "shoginotation.h"
#include "usimove.h"

namespace KifMoveText {

/// 移動先が直前の手と同じマスのときの「同」の書き方
enum class SameSquare {
    Spaced,   ///< 「同　銀」（棋譜の1手）
    Compact,  ///< 「同銀」（読み筋のように手を続けて書くとき）
};

/// 棋譜に書かれていた修飾（JKF の relative・promote を表記に残す）
struct Notes {
    QString modifier;                ///< 相対位置の修飾語（「右」「上」「打」など。KI2 と同じ語）
    bool declinedPromotion = false;  ///< 成れるのに成らなかった（「不成」と書く）
    bool withoutSource = false;      ///< 移動元「(77)」を書かない（KI2 の手）
    /// 駒打ちの「打」を修飾語にあるときだけ書く（KI2。盤上の同じ駒も動けるときだけ付く）。
    /// false なら駒打ちには必ず「打」を書く（KIF）
    bool dropMarkFromModifier = false;
};

/**
 * @brief 成ることもできたのに成らなかった手か（規則は PieceRules::couldPromote）
 * @param moving 動かした駒（成る前の駒。色で敵陣を決める）
 *
 * 成る手・駒打ちは false。true なら KIF・KI2 では「不成」、JKF では promote: false、英語表記では「=」と書く。
 */
inline bool couldPromote(Piece moving, const UsiMove& move)
{
    return !move.isDrop() && !move.promote && PieceRules::couldPromote(moving, move.from.rank, move.to.rank);
}

/**
 * @brief KIF 形式の指し手の文字列を out の後ろに足す
 *
 * 引数は format と同じ。読み筋のように手を続けて書くとき、手ごとに文字列を作らずに済む。
 */
inline void append(QString& out, Turn mover, Piece moving, const UsiMove& move, BoardSquare previousTo = {},
                   SameSquare same = SameSquare::Spaced, const Notes& notes = {})
{
    out += QChar(mover == Turn::Black ? u'▲' : u'△');
    if (previousTo.isValid() && previousTo == move.to) {
        out += QChar(u'同');
        if (same == SameSquare::Spaced) out += QChar(u'　');
    } else {
        out += ShogiNotation::fileZenkakuChar(move.to.file);
        out += ShogiNotation::rankKanjiChar(move.to.rank);
    }
    out += ShogiNotation::movePieceName(moving);
    out += notes.modifier;
    if (move.isDrop()) {
        if (!notes.dropMarkFromModifier && !notes.modifier.contains(u'打')) out += QChar(u'打');
        return;
    }
    if (move.promote) out += QChar(u'成');
    else if (notes.declinedPromotion) out += QStringLiteral("不成");
    if (notes.withoutSource) return;
    out += QLatin1Char('(');
    out += QChar(u'0' + move.from.file);
    out += QChar(u'0' + move.from.rank);
    out += QLatin1Char(')');
}

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
inline QString format(Turn mover, Piece moving, const UsiMove& move, BoardSquare previousTo = {},
                      const Notes& notes = {})
{
    QString text;
    append(text, mover, moving, move, previousTo, SameSquare::Spaced, notes);
    return text;
}

} // namespace KifMoveText

#endif // KIFMOVETEXT_H
