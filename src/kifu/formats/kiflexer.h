#ifndef KIFLEXER_H
#define KIFLEXER_H

/// @file kiflexer.h
/// @brief KIF形式固有の字句解析・トークン化・BOD盤面解析

#include <QChar>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <optional>

#include "kifdisplayitem.h"
#include "shogitypes.h"

namespace KifLexer {

// === 行の種類判定 ===

/// 行頭が手数（半/全角数字）で始まるか判定。BOD「手数＝」等は除外。
/// 成功時は先頭数字の桁数を返す。
[[nodiscard]] std::optional<int> startsWithMoveNumber(const QString& line);

// === 正規表現アクセサ ===

/// KIF時間正規表現: ( m:ss / H:MM:SS )
const QRegularExpression& kifTimeRe();

/// 変化行検出（キャプチャなし）: "変化：12手" など
const QRegularExpression& variationHeaderRe();

/// 変化行検出（キャプチャあり）: 手数をキャプチャ
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

// === 終局語 ===

/// 終局語の表示アイテムを生成
KifDisplayItem buildTerminalItem(int moveIndex, const QString& term,
                                 const QString& timeText, const QRegularExpressionMatch& tm);

// === 指し手解析 ===

/// 目的地（「同」対応）を読む
bool findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev);

/// 駒種（漢字）→ USIドロップ文字（'P','L',...）
QChar pieceKanjiToUsiUpper(const QString& s);

/// Promoted表記が含まれているか（"成" かつ "不成" でない）
bool isPromotionMoveText(const QString& line);

/// 1行の指し手をUSIに変換（prevTo参照；成功で更新）
bool convertMoveLine(const QString& moveText, QString& usi,
                     int& prevToFile, int& prevToRank);

// === BOD盤面解析 ===

/// BOD持駒行を解析して駒数マップに追加
void parseBodHandsLine(const QString& line, QMap<Piece, int>& outCounts, bool isBlack);

/// 持駒マップからSFEN形式の持駒文字列を生成
QString buildHandsSfen(const QMap<Piece, int>& black, const QMap<Piece, int>& white);

/// BOD行を収集して rowByRank マップに格納
bool collectBodRows(const QStringList& lines, QMap<QChar, QString>& rowByRank,
                    QString* detectedLabel = nullptr);

/// BODから手番と手数を解析
void parseBodTurnAndMoveNumber(const QStringList& lines, QChar& turn, int& moveNumber);

} // namespace KifLexer

#endif // KIFLEXER_H
