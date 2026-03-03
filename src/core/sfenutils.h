#ifndef SFENUTILS_H
#define SFENUTILS_H

/// @file sfenutils.h
/// @brief SFEN文字列操作ユーティリティの定義

#include <QString>

/// SFEN文字列の正規化・変換ユーティリティ
namespace SfenUtils {

/// 平手初期配置（盤面部分のみ）
inline QString hirateBoardSfen()
{
    return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");
}

/// 平手初期配置（手番・持ち駒・手数を含む完全SFEN）
inline QString hirateSfen()
{
    return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

/// "startpos" を標準初期配置のSFEN文字列に展開する（既にSFENならそのまま返す）
inline QString normalizeStart(const QString& startPositionStr)
{
    if (startPositionStr == QStringLiteral("startpos")) {
        return hirateSfen();
    }
    return startPositionStr;
}

/// 盤面部分が平手初期配置かを判定する
inline bool isHirateBoardSfen(const QString& boardSfen)
{
    return boardSfen == hirateBoardSfen();
}

} // namespace SfenUtils

#endif // SFENUTILS_H
