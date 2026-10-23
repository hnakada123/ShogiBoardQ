#ifndef KIFLEXER_H
#define KIFLEXER_H

/// @file kiflexer.h
/// @brief KIF形式固有の字句解析・トークン化・BOD盤面解析

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <optional>

#include "recordtime.h"
#include "shogiposition.h"
#include "shogitypes.h"

namespace KifLexer {

// === 行の種類判定 ===

/// 行頭が手数（半/全角数字）で始まるか判定。BOD「手数＝」等は除外。
/// 成功時は先頭数字の桁数を返す。
[[nodiscard]] std::optional<int> startsWithMoveNumber(QStringView line);

// === 正規表現アクセサ ===

/// 変化の行（"変化：12手" など。KIF と KI2 で同じ）。1番目のキャプチャが手数
const QRegularExpression& variationHeaderCaptureRe();

// === 手数で始まる行 ===

/// 手数で始まる行の1手（"1 ７六歩(77)   ( 0:01/00:00:01) *コメント"）
struct NumberedMove {
    QString text;                    ///< 指し手・終局語の表記（手数・消費時間・コメント・分岐の印を除く）
    std::optional<RecordTime> time;  ///< 消費時間と累計（書かれていなければ空。読めない値は表記のまま残す）
    QString comment;                 ///< 行内のコメント（* の後ろ。行末まで）
};

/// 手数で始まる行を読む。1行に複数の手が並ぶこともある（表記のない手は読み飛ばす）
QList<NumberedMove> readNumberedMoves(const QString& line);

// === BOD盤面解析 ===

/**
 * @brief 局面図（BOD・「配置：」）を局面にする
 *
 * 盤の9段・両者の持駒・手番（「後手番」「上手番」）・手数（「手数＝N」の次の手）を読む。
 * 9段がそろわなければ false
 */
bool readBodPosition(const QStringList& lines, Position& out);

} // namespace KifLexer

#endif // KIFLEXER_H
