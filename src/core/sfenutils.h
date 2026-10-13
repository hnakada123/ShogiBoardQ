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
    QString trimmed = startPositionStr.trimmed();
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

/**
 * @brief SFEN の持ち駒欄を標準の並びに直す
 *
 * 標準は先手の駒をすべて書いてから後手の駒を書き、どちらも飛・角・金・銀・桂・香・歩の順。
 * 以前のこのアプリは先後を駒の種類ごとに交互に書いていた（例 "BbPp"）ので、比較の前にそろえる。
 * 読めない欄はそのまま返す。
 */
inline QString canonicalHands(const QString& hands)
{
    if (hands == QStringLiteral("-")) return hands;
    static const QString order = QStringLiteral("RBGSNLP");
    int counts[2][7] = {};
    int number = 0;
    for (const QChar ch : hands) {
        if (ch.isDigit()) {
            number = number * 10 + ch.digitValue();
            continue;
        }
        const qsizetype kind = order.indexOf(ch.toUpper());
        if (kind < 0) return hands;
        counts[ch.isUpper() ? 0 : 1][kind] += (number > 0) ? number : 1;
        number = 0;
    }
    if (number > 0) return hands;
    QString out;
    for (int side = 0; side < 2; ++side) {
        for (int kind = 0; kind < 7; ++kind) {
            const int n = counts[side][kind];
            if (n <= 0) continue;
            if (n > 1) out += QString::number(n);
            out += side == 0 ? order.at(kind) : order.at(kind).toLower();
        }
    }
    return out.isEmpty() ? QStringLiteral("-") : out;
}

/// 比較用キー（`board turn stand`、持ち駒は標準の並び）へ正規化する
inline QString normalizeSfenKey(const QString& sfenLike)
{
    QString normalized = normalizePositionLikeSfen(sfenLike);
    QStringList parts = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 3) {
        parts[2] = canonicalHands(parts.at(2));
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
