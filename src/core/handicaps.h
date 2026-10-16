#ifndef HANDICAPS_H
#define HANDICAPS_H

/// @file handicaps.h
/// @brief 手合割（名前・JKF の preset・開始局面）の表。棋譜の読み書きと対局開始で同じ表を使う

#include <QString>
#include <QStringView>
#include <QtGlobal>

#include "sfenutils.h"

namespace Handicaps {

/// 手合割1つ
struct Handicap {
    const char* label;      ///< 名前（KIF の「手合割：」の値。UTF-8。表示は KifuPresentation が訳す）
    const char* jkfPreset;  ///< JKF の initial.preset
    const char* sfen;       ///< 開始局面（駒落ちは上手＝後手の手番）
};

inline constexpr Handicap kAll[] = {
    {QT_TRANSLATE_NOOP("KifuPresentation", "平手"), "HIRATE", SfenUtils::kHirateSfen},
    {QT_TRANSLATE_NOOP("KifuPresentation", "香落ち"), "KY", "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "右香落ち"), "KY_R", "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "角落ち"), "KA", "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "飛車落ち"), "HI", "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "飛香落ち"), "HIKY", "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "二枚落ち"), "2", "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "三枚落ち"), "3", "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "四枚落ち"), "4", "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "五枚落ち"), "5", "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "左五枚落ち"), "5_L", "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "六枚落ち"), "6", "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "左七枚落ち"), "7_L", "2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "右七枚落ち"), "7_R", "3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "八枚落ち"), "8", "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    {QT_TRANSLATE_NOOP("KifuPresentation", "十枚落ち"), "10", "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
};

/// 手合割の名前（KIF の「手合割：」の値）
inline QString labelOf(const Handicap& handicap) { return QString::fromUtf8(handicap.label); }

/// 名前の手合割（前後の空白は無視する）。なければ nullptr
inline const Handicap* byLabel(QStringView label)
{
    const QStringView key = label.trimmed();
    for (const Handicap& handicap : kAll) {
        if (key == QString::fromUtf8(handicap.label)) return &handicap;
    }
    return nullptr;
}

/// JKF の preset の手合割。なければ nullptr
inline const Handicap* byJkfPreset(QStringView preset)
{
    for (const Handicap& handicap : kAll) {
        if (preset == QLatin1String(handicap.jkfPreset)) return &handicap;
    }
    return nullptr;
}

/// 開始局面（盤・手番・持ち駒。手数は見ない）が同じ手合割。なければ nullptr
inline const Handicap* bySfen(const QString& sfen)
{
    const QString key = SfenUtils::normalizeSfenKey(sfen);
    for (const Handicap& handicap : kAll) {
        if (SfenUtils::normalizeSfenKey(QString::fromLatin1(handicap.sfen)) == key) return &handicap;
    }
    return nullptr;
}

/// 盤（駒の配置だけ）が同じ手合割。なければ nullptr
inline const Handicap* byBoard(const QString& sfen)
{
    const QString board = sfen.trimmed().section(QLatin1Char(' '), 0, 0);
    if (board.isEmpty()) return nullptr;
    for (const Handicap& handicap : kAll) {
        if (board == QString::fromLatin1(handicap.sfen).section(QLatin1Char(' '), 0, 0)) return &handicap;
    }
    return nullptr;
}

} // namespace Handicaps

#endif // HANDICAPS_H
