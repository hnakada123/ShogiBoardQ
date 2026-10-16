#ifndef KI2LEXER_H
#define KI2LEXER_H

/// @file ki2lexer.h
/// @brief KI2形式固有の字句解析（行の種類・結果行・1行に並んだ指し手の切り出し）

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "shogitypes.h"
#include "terminaltype.h"

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
bool parseResultLine(const QString& line, TerminalType& terminal, int& moveCount);

// === 指し手テキスト解析 ===

/// KI2形式の1行から指し手を抽出（複数手が1行に含まれる場合あり）
QStringList extractMovesFromLine(const QString& line);

} // namespace Ki2Lexer

#endif // KI2LEXER_H
