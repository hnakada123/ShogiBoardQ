#ifndef TIMECONTROLTEXT_H
#define TIMECONTROLTEXT_H

/// @file timecontroltext.h
/// @brief 対局情報の持ち時間の書式（書き出し・CSA への変換・表示で同じ規則を使う）

#include <QString>
#include <QtGlobal>
#include <optional>

#include "timecontrol.h"

/**
 * @brief 対局情報の「持ち時間」の値の読み書き
 *
 * 片方の書式は「mm:ss」（切れ負け）、「mm:ss+秒」（秒読み）、「mm:ss+秒秒加算」（フィッシャー加算）。
 * 先後で違うときは「先手 mm:ss+秒 / 後手 mm:ss+秒」（駒落ちは下手・上手）。時間制限のない対局は「無制限」
 * （未開始の「未設定」は空の値）。
 * このアプリが書いた値を読むためのもので、ほかのソフトの書式は読まない（呼び出し側が扱う）。
 */
namespace TimeControlText {

/// 時間制限のない対局の値（ファイルに書く語。表示は KifuPresentation::infoValue が訳す）
inline const QString kUnlimited = QStringLiteral(QT_TRANSLATE_NOOP("KifuPresentation", "無制限"));

/// 片方の持ち時間の値
QString format(const TimeControlSide& side);
/// 先後の持ち時間の値（同じなら1つ、違えば先後を書く。handicapNames なら下手・上手。制限がなければ「無制限」）
QString format(const TimeControl& timeControl, bool handicapNames);

/// 読んだ持ち時間
struct Parsed {
    TimeControl timeControl;
    bool unlimited = false;      ///< 「無制限」（timeControl は制限なし）
    bool perSide = false;        ///< 先後を分けて書いていた
    bool handicapNames = false;  ///< 先後を下手・上手と書いていた
};

/// 片方の持ち時間の値を読む（書式が違えば空）
std::optional<TimeControlSide> parseSide(const QString& text);
/// 持ち時間の値を読む（書式が違えば空）
std::optional<Parsed> parse(const QString& text);

} // namespace TimeControlText

#endif // TIMECONTROLTEXT_H
