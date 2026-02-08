#ifndef SFENUTILS_H
#define SFENUTILS_H

/// @file sfenutils.h
/// @brief SFEN文字列操作ユーティリティの定義
/// @todo remove コメントスタイルガイド適用済み

#include <QString>

/// SFEN文字列の正規化・変換ユーティリティ
/// @todo remove コメントスタイルガイド適用済み
namespace SfenUtils {

/// "startpos" を標準初期配置のSFEN文字列に展開する（既にSFENならそのまま返す）
inline QString normalizeStart(const QString& startPositionStr)
{
    if (startPositionStr == QStringLiteral("startpos")) {
        return QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    }
    return startPositionStr;
}

} // namespace SfenUtils

#endif // SFENUTILS_H
