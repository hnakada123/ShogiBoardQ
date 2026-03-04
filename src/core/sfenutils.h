#ifndef SFENUTILS_H
#define SFENUTILS_H

/// @file sfenutils.h
/// @brief SFEN文字列操作ユーティリティの定義

#include <QString>
#include <QStringList>

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

/// "startpos" を標準初期配置のSFEN文字列に展開する（既にSFENならそのまま返す）
inline QString normalizeStart(const QString& startPositionStr)
{
    const QString trimmed = startPositionStr.trimmed();
    if (trimmed == QStringLiteral("startpos")) {
        return hirateSfen();
    }
    return trimmed;
}

/// "position ..." 接頭辞を除去し、純粋な SFEN/startpos 文字列に正規化する
inline QString normalizePositionLikeSfen(const QString& positionLike)
{
    QString sfen = positionLike.trimmed();
    if (sfen.startsWith(QStringLiteral("position sfen "))) {
        sfen = sfen.mid(14).trimmed();
    } else if (sfen.startsWith(QStringLiteral("position "))) {
        sfen = sfen.mid(9).trimmed();
    }
    return normalizeStart(sfen);
}

/// 比較用キー（`board turn stand`）へ正規化する
inline QString normalizeSfenKey(const QString& sfenLike)
{
    const QString normalized = normalizePositionLikeSfen(sfenLike);
    const QStringList parts = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 3) {
        return parts.mid(0, 3).join(QLatin1Char(' '));
    }
    return normalized;
}

/// 平手初期局面（`startpos` / 完全SFEN / `position ...`）かを判定する
inline bool isHirateStart(const QString& sfenLike)
{
    static const QString hirateKey = normalizeSfenKey(hirateSfen());
    return normalizeSfenKey(sfenLike) == hirateKey;
}

/// 盤面部分が平手初期配置かを判定する
inline bool isHirateBoardSfen(const QString& boardSfen)
{
    return boardSfen == hirateBoardSfen();
}

} // namespace SfenUtils

#endif // SFENUTILS_H
