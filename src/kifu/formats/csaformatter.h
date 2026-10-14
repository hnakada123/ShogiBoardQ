#ifndef CSAFORMATTER_H
#define CSAFORMATTER_H

/// @file csaformatter.h
/// @brief CSA形式の文字列生成ヘルパ関数の定義

#include <QChar>
#include <QString>
#include <QStringList>

/**
 * @brief CSAエクスポート用フォーマット変換ヘルパ関数群
 */
namespace CsaFormatter {

QString extractCsaTimeToken(const QString& timeText);
QString csaResultCode(const QString& terminalMove);
QString convertToCsaDateTime(const QString& dateTimeStr);
QString convertToCsaTime(const QString& timeStr);
/// 対局情報の持ち時間を CSA の行にする。先後で違う持ち時間は $TIME+ と $TIME- の2行になる
QStringList convertToCsaTimeLines(const QString& timeStr);

} // namespace CsaFormatter

#endif // CSAFORMATTER_H
