#ifndef KIFMOVEGRAMMAR_H
#define KIFMOVEGRAMMAR_H

/// @file kifmovegrammar.h
/// @brief KIF・KI2 の指し手の文字列の文法（"▲７六歩(77)"・"△同　銀成"・"▲５二金右"・"▲５五角打"）

#include <QString>
#include <QStringView>
#include <optional>

#include "shogitypes.h"
#include "usimove.h"

class Position;

/**
 * @brief KIF・KI2 の指し手の文字列
 *
 * KIF の手は「KI2 の手＋移動元の (xy)」なので、同じ文法で読む。移動元が書いてあればそれを使い、
 * 書いてなければ局面の合法手から決める（Ki2MoveNotation::resolve）。
 */
namespace KifMoveGrammar {

/// 指し手の文字列を要素に分けたもの。どの駒が動いたかは局面で決まるので、書いてあることだけを持つ
struct MoveText {
    BoardSquare to;               ///< 移動先（「同」だけのときは無効）
    bool sameAsPrevious = false;  ///< 「同」
    Piece piece = Piece::None;    ///< 動かす駒・打つ駒（先手の駒。成駒は成駒で表す）
    QString modifiers;            ///< 右・左・直・上・引・寄（「行」は「上」として読む）
    std::optional<bool> promote;  ///< 「成」なら true、「不成」「生」なら false、書いていなければ nullopt
    bool drop = false;            ///< 「打」
    std::optional<BoardSquare> from;  ///< 移動元（KIF の "(77)"。書いていなければ nullopt）
};

/// 指し手の文字列を読む（先頭の手番の印は飛ばす）。移動先と駒が読めなければ nullopt
[[nodiscard]] std::optional<MoveText> parseMoveText(QStringView text);

/**
 * @brief 局面の上で、指し手の文字列が表す手を1つに決める
 * @param board 指す前の局面
 * @param mover 指す側
 * @param previousTo 直前の手の移動先（「同」で使う）
 *
 * 移動元か「打」が書いてあればそのとおりの手（反則手を書いた棋譜も読めるよう、局面とは照らさない）。
 * どちらもなければ、合法手から修飾語で1つに決める（決まらなければ nullopt）。
 */
[[nodiscard]] std::optional<UsiMove> resolve(const MoveText& text, const Position& board, Turn mover,
                                             BoardSquare previousTo);

} // namespace KifMoveGrammar

#endif // KIFMOVEGRAMMAR_H
