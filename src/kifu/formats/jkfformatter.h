#ifndef JKFFORMATTER_H
#define JKFFORMATTER_H

/// @file jkfformatter.h
/// @brief JKF形式の変換ヘルパ関数の定義

#include <QChar>
#include <QJsonObject>
#include <QString>

struct KifDisplayItem;

/**
 * @brief JKFエクスポート用フォーマット変換ヘルパ関数群
 */
namespace JkfFormatter {

QString kanjiToCsaPiece(const QString& kanji);
int zenkakuToNumber(QChar c);
int kanjiToNumber(QChar c);
QString japaneseToJkfSpecial(const QString& japanese);
QJsonObject parseTimeToJkf(const QString& timeText);
QString sfenToJkfPreset(const QString& sfen);
QString sfenPieceToJkfKind(QChar c, bool promoted);
QJsonObject sfenToJkfData(const QString& sfen);
QJsonObject convertMoveToJkf(const KifDisplayItem& disp, int& prevToX, int& prevToY, int ply);

} // namespace JkfFormatter

#endif // JKFFORMATTER_H
