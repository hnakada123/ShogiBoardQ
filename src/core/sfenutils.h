#ifndef SFENUTILS_H
#define SFENUTILS_H

/// @file sfenutils.h
/// @brief SFEN文字列操作ユーティリティの定義

#include <QString>
#include <QStringList>

#include "shogiposition.h"
#include "shogitypes.h"
#include "usiposition.h"

/// SFEN文字列の正規化・変換ユーティリティ
namespace SfenUtils {

/// 平手初期配置（盤面部分・C文字列）
inline constexpr const char* kHirateBoardSfen =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL";

/// 平手初期配置（手番・持ち駒・手数を含む完全SFEN・C文字列）
inline constexpr const char* kHirateSfen =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

/// 平手初期配置（盤面部分のみ）
inline QString hirateBoardSfen()
{
    return QString::fromLatin1(kHirateBoardSfen);
}

/// 平手初期配置（手番・持ち駒・手数を含む完全SFEN）
inline QString hirateSfen()
{
    return QString::fromLatin1(kHirateSfen);
}

/// SFEN の手番（"w" なら後手、それ以外は先手）
inline Turn sideToMove(const QString& sfen)
{
    return sfen.section(QLatin1Char(' '), 1, 1, QString::SectionSkipEmpty) == QLatin1String("w") ? Turn::White : Turn::Black;
}

/**
 * @brief 比較用キー（`board turn stand`。手数を含めない）へ正規化する
 *
 * 局面として読めれば Position::positionKey()（持ち駒は標準の並び）。以前のこのアプリが書いた、
 * 先後を駒の種類ごとに交互に書いた持ち駒（例 "BbPp"）も同じキーになる。
 * 局面として読めない文字列は、前後の空白を除いた最初の3つの欄を返す。
 */
inline QString normalizeSfenKey(const QString& sfenLike)
{
    if (const auto position = Position::fromSfenLike(sfenLike)) return position->positionKey();
    return UsiPosition::sfenOf(sfenLike).section(QLatin1Char(' '), 0, 2, QString::SectionSkipEmpty);
}

} // namespace SfenUtils

#endif // SFENUTILS_H
