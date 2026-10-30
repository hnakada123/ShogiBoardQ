#ifndef CSAFORMATTER_H
#define CSAFORMATTER_H

/// @file csaformatter.h
/// @brief CSA形式の文字列生成ヘルパ関数の定義

#include <QChar>
#include <QString>
#include "terminaltype.h"
#include "recordtime.h"
#include "shogitypes.h"
#include <QStringList>

/**
 * @brief CSA 形式の文字列の変換（書き出しと、読み書きで共有する棋譜情報の語の対応）
 */
namespace CsaFormatter {

/// 一手の消費時間の行（T秒。ミリ秒があれば小数3桁）。時間が分からなければ T0
QString timeToken(const RecordTime& time);
QString convertToCsaDateTime(const QString& dateTimeStr);
QString convertToCsaTime(const QString& timeStr);
/// 対局情報の持ち時間を CSA の行にする。先後で違う持ち時間は $TIME+ と $TIME- の2行、「無制限」は行なし
QStringList convertToCsaTimeLines(const QString& timeStr);

/// CSA の棋譜情報の語（"$EVENT:" の EVENT）に対応する対局情報の項目名（別名も読む）。対応がなければ空
QString gameInfoKeyFromCsa(const QString& csaKey);
/// 対局情報の項目名に対応する CSA の棋譜情報の語。対応がなければ空
QString csaKeyFromGameInfo(const QString& gameInfoKey);
/// $NOTE の値（改行を \n、\ を \\ にする）
QString escapeNote(const QString& note);
QString unescapeNote(const QString& value);

} // namespace CsaFormatter

#endif // CSAFORMATTER_H
