#ifndef KI2LEXER_H
#define KI2LEXER_H

/// @file ki2lexer.h
/// @brief KI2形式固有の字句解析（行の種類・結果行・1行に並んだ指し手の切り出し）

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <optional>

#include "shogitypes.h"
#include "terminaltype.h"

namespace Ki2Lexer {

// === 正規表現アクセサ ===

/// 結果行検出（キャプチャあり）: "まで○手で..." の手数をキャプチャ
const QRegularExpression& resultPatternCaptureRe();

// === 行の種類判定 ===

/// KI2行が指し手行かどうか判定（▲または△で始まる）
bool isKi2MoveLine(const QString& line);

// === 結果行解析 ===

/// 結果行（"まで77手で先手の勝ち"）を読んだ値
struct ResultLine {
    TerminalType terminal = TerminalType::None;
    int moveCount = 0;
    QString terminalText; ///< 独自の結果行の終局理由
    /// 行が名指しした側（"先手の勝ち" なら先手、"後手の反則負け" なら後手）。名指しがなければ空
    std::optional<Turn> named;

    /// 終局の行を記録する側（RecordEntry::endingSide と同じ規則。書き出しの buildEndingLine の逆）
    Turn endingSide(Turn sideToMove) const;
};

/// 結果行を読む。結果行でなければ空
std::optional<ResultLine> parseResultLine(const QString& line);

// === 指し手テキスト解析 ===

/// KI2形式の1行から指し手を抽出（複数手が1行に含まれる場合あり）
QStringList extractMovesFromLine(const QString& line);

} // namespace Ki2Lexer

#endif // KI2LEXER_H
