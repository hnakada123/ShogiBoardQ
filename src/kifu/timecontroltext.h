#ifndef TIMECONTROLTEXT_H
#define TIMECONTROLTEXT_H

/// @file timecontroltext.h
/// @brief 対局情報の持ち時間の書式（書き出し・CSA への変換・表示で同じ規則を使う）

#include <QString>
#include <optional>

#include "timecontrol.h"

/**
 * @brief 対局情報の「持ち時間」の値の読み書き
 *
 * 片方の書式は「mm:ss」（切れ負け）、「mm:ss+秒」（秒読み）、「mm:ss+秒秒加算」（フィッシャー加算）。
 * 先後で違うときは「先手 mm:ss+秒 / 後手 mm:ss+秒」（駒落ちは下手・上手）。
 * このアプリが書いた値を読むためのもので、ほかのソフトの書式は読まない（呼び出し側が扱う）。
 */
namespace TimeControlText {

/// 片方の持ち時間の値
QString format(const TimeControlSide& side);
/// 先後の持ち時間の値（同じなら1つ、違えば先後を書く。handicapNames なら下手・上手）
QString format(const TimeControl& timeControl, bool handicapNames);

/// 読んだ持ち時間
struct Parsed {
    TimeControl timeControl;
    bool perSide = false;        ///< 先後を分けて書いていた
    bool handicapNames = false;  ///< 先後を下手・上手と書いていた
};

/// 片方の持ち時間の値を読む（書式が違えば空）
std::optional<TimeControlSide> parseSide(const QString& text);
/// 持ち時間の値を読む（書式が違えば空）
std::optional<Parsed> parse(const QString& text);

} // namespace TimeControlText

#endif // TIMECONTROLTEXT_H
