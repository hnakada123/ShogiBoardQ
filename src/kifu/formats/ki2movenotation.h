#ifndef KI2MOVENOTATION_H
#define KI2MOVENOTATION_H

/// @file ki2movenotation.h
/// @brief KI2 の指し手の文字列と、局面の上の指し手（USI）との対応

#include <QList>
#include <QString>
#include <optional>

#include "kifmovegrammar.h"
#include "shogitypes.h"
#include "usimove.h"

class Position;

/**
 * @brief KI2 の指し手の文字列（"▲５二金右"）と USI の指し手の対応
 *
 * 移動元の書かれていない KI2 では、同じ駒が同じマスへ動ける場合に修飾語（右・左・直・上・引・寄）を、
 * 盤上の駒も動ける場合に「打」を付ける（日本将棋連盟の棋譜の表記方法）。
 * 「動ける駒」は局面の合法手で決めるので、ピンされた駒は数えない。
 *
 * 反則手（二歩・王手放置など）を書いた棋譜も読み書きできるよう、合法手で決まらない手は
 * 駒の動きだけで決める。
 */
namespace Ki2MoveNotation {

/**
 * @brief 局面の上で、KI2 の指し手の文字列が表す指し手を1つに決める
 * @param text 指し手の文字列を分けたもの（KifMoveGrammar::parseMoveText）
 * @param board 指す前の局面
 * @param mover 指す側
 * @param previousTo 直前の手の移動先（「同」で使う）
 * @return 決まった指し手。候補がない・修飾語で1つに絞れないときは nullopt
 */
[[nodiscard]] std::optional<UsiMove> resolve(const KifMoveGrammar::MoveText& text, const Position& board,
                                             Turn mover, BoardSquare previousTo);

/// 局面の上の指し手に、KI2 で付ける語
struct Annotation {
    QString modifiers;        ///< 修飾語（右・左・直・上・引・寄。「左上」のように2字のこともある）。盤上の同じ駒も動ける駒打ちは「打」
    bool canPromote = false;  ///< 成ることもできる手（成らない手は「不成」と書く）
    /// 同じ駒で同じマスへ動ける盤上の駒の位置（指した駒を含む。2つ以上なら移動元に紛れがある。
    /// 画面の英語表記が移動元を付けるかもこれで決める）
    QList<BoardSquare> sources;
};

/**
 * @brief 局面と指し手から、KI2 で付ける修飾語・「打」と、成れる手かどうかを決める
 * @param board 指す前の局面
 * @param mover 指す側
 * @param move 指し手
 *
 * 修飾語・「打」は、同じ駒で同じマスへ動ける駒がほかにあるときだけ付ける。
 * JKF の relative も、この修飾語を記号にしたもの。
 */
Annotation annotate(const Position& board, Turn mover, const UsiMove& move);

/**
 * @brief 局面と指し手から KI2 の指し手の文字列を作る（"▲５二金右"・"△同　銀成"・"▲５五角打"）
 * @param board 指す前の局面
 * @param mover 指す側（手番の印を決める）
 * @param move 指し手
 * @param previousTo 直前の手の移動先。同じマスなら「同　」と書く
 *
 * 修飾語・「打」・「不成」は必要なときだけ付ける。
 */
QString format(const Position& board, Turn mover, const UsiMove& move, BoardSquare previousTo);

} // namespace Ki2MoveNotation

#endif // KI2MOVENOTATION_H
