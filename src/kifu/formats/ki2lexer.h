#ifndef KI2LEXER_H
#define KI2LEXER_H

/// @file ki2lexer.h
/// @brief KI2形式固有の字句解析（行の種類・結果行・指し手の文字列）

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <optional>

#include "shogitypes.h"
#include "usimove.h"

namespace Ki2Lexer {

// === 正規表現アクセサ ===

/// 結果行検出（キャプチャあり）: "まで○手で..." の手数をキャプチャ
const QRegularExpression& resultPatternCaptureRe();

/// 結果行検出（キャプチャなし）: "まで○手で..." の判定のみ
const QRegularExpression& resultPatternRe();

// === 行の種類判定 ===

/// KI2行が指し手行かどうか判定（▲または△で始まる）
bool isKi2MoveLine(const QString& line);

/// 結果行かどうか判定
bool isResultLine(const QString& line);

// === 結果行解析 ===

/// 結果行を解析して終局語と手数を抽出
bool parseResultLine(const QString& line, QString& terminalWord, int& moveCount);

// === 指し手テキスト解析 ===

/// KI2形式の1行から指し手を抽出（複数手が1行に含まれる場合あり）
QStringList extractMovesFromLine(const QString& line);

/**
 * @brief KI2 の指し手の文字列を要素に分けたもの（"▲５二金右"・"△同　銀成"・"▲５五角打"）
 *
 * どの駒が動いたかは局面で決まるので、ここでは文字列に書いてあることだけを持つ。
 */
struct MoveText {
    BoardSquare to;               ///< 移動先（「同」だけのときは無効）
    bool sameAsPrevious = false;  ///< 「同」
    Piece piece = Piece::None;    ///< 動かす駒・打つ駒（先手の駒。成駒は成駒で表す）
    QString modifiers;            ///< 右・左・直・上・引・寄（「行」は「上」として読む）
    std::optional<bool> promote;  ///< 「成」なら true、「不成」「生」なら false、書いていなければ nullopt
    bool drop = false;            ///< 「打」
};

/// 指し手の文字列を読む（先頭の手番の印は飛ばす）。移動先と駒が読めなければ nullopt
[[nodiscard]] std::optional<MoveText> parseMoveText(QStringView text);

} // namespace Ki2Lexer

#endif // KI2LEXER_H
