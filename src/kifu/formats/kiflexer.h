#ifndef KIFLEXER_H
#define KIFLEXER_H

/// @file kiflexer.h
/// @brief KIF形式固有の字句解析・トークン化・BOD盤面解析

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <optional>

#include "shogiposition.h"
#include "shogitypes.h"

namespace KifLexer {

// === 行の種類判定 ===

/// 行頭が手数（半/全角数字）で始まるか判定。BOD「手数＝」等は除外。
/// 成功時は先頭数字の桁数を返す。
[[nodiscard]] std::optional<int> startsWithMoveNumber(const QString& line);

// === 正規表現アクセサ ===

/// KIF時間正規表現: ( m:ss / H:MM:SS )
const QRegularExpression& kifTimeRe();

/// 変化の行（"変化：12手" など。KIF と KI2 で同じ）。1番目のキャプチャが手数
const QRegularExpression& variationHeaderCaptureRe();

// === 時間解析 ===

/// 時間正規表現マッチから "mm:ss/HH:MM:SS" を正規化
QString normalizeTimeMatch(const QRegularExpressionMatch& m);

/// 時間マッチから累計時間部分 "HH:MM:SS" を抽出
QString extractCumTimeFromMatch(const QRegularExpressionMatch& tm);

// === 行内トークン抽出 ===

/// rest から時間・次手番号を抽出し、rest を時間/次手を除いた部分に置換
void stripTimeAndNextMove(QString& rest, const QString& lineStr, int skipOffset,
                          QRegularExpressionMatch& tm, QString& timeText, int& nextMoveStartIdx);

/// rest からインラインコメント（*／＊）を除去し commentBuf に蓄積
void stripInlineComment(QString& rest, QString& commentBuf);

/// lineStr を次の指し手位置まで進める
void advanceToNextMove(QString& lineStr, int skipOffset,
                       const QRegularExpressionMatch& tm, int nextMoveStartIdx);

// === BOD盤面解析 ===

/**
 * @brief 局面図（BOD・「配置：」）を局面にする
 *
 * 盤の9段・両者の持駒・手番（「後手番」「上手番」）・手数（「手数＝N」の次の手）を読む。
 * 9段がそろわなければ false。detectedLabel には "BOD" か "配置" を入れる
 */
bool readBodPosition(const QStringList& lines, Position& out, QString* detectedLabel = nullptr);

} // namespace KifLexer

#endif // KIFLEXER_H
