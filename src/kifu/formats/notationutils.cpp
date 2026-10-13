/// @file notationutils.cpp
/// @brief 棋譜変換器間で共有される座標変換・数字変換・手合マップユーティリティの実装

#include "notationutils.h"
#include "sfenutils.h"
#include "shoginotation.h"
#include "usimove.h"

#include <QLatin1Char>
#include <QStringLiteral>

namespace NotationUtils {

int kanjiDigitToInt(QChar c)
{
    return ShogiNotation::rankFromKanji(c);
}

QString intToZenkakuDigit(int d)
{
    const QString text = ShogiNotation::fileZenkaku(d);
    return text.isEmpty() ? QString(QChar(u'？')) : text;
}

QString intToKanjiDigit(int d)
{
    const QString text = ShogiNotation::rankKanji(d);
    return text.isEmpty() ? QString(QChar(u'？')) : text;
}

QString formatSfenMove(int fromFile, int fromRank, int toFile, int toRank, bool promote)
{
    UsiMove move;
    move.from = BoardSquare{fromFile, fromRank};
    move.to = BoardSquare{toFile, toRank};
    move.promote = promote;
    return move.toString();
}

QString formatSfenDrop(QChar piece, int toFile, int toRank)
{
    UsiMove move;
    move.drop = UsiNotation::dropPieceFromChar(piece);
    move.to = BoardSquare{toFile, toRank};
    return move.toString();
}

namespace {
struct HandicapPreset { const char* key; const char* sfen; };

const HandicapPreset* handicapPresets(size_t& count)
{
    static const HandicapPreset tbl[] = {
        {"平手",       SfenUtils::kHirateSfen},
        {"香落ち",     "lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"右香落ち",   "1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"角落ち",     "lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"飛車落ち",   "lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"飛香落ち",   "lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"二枚落ち",   "lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"三枚落ち",   "lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"四枚落ち",   "1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"五枚落ち",   "2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"左五枚落ち", "1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"六枚落ち",   "2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"左七枚落ち", "2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"右七枚落ち", "3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"八枚落ち",   "3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
        {"十枚落ち",   "4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"},
    };
    count = sizeof(tbl) / sizeof(tbl[0]);
    return tbl;
}
} // namespace

QString mapHandicapToSfen(const QString& label)
{
    size_t count = 0;
    const HandicapPreset* tbl = handicapPresets(count);
    const QString normalizedLabel = label.trimmed();
    for (size_t i = 0; i < count; ++i) {
        if (normalizedLabel == QString::fromUtf8(tbl[i].key)) return QString::fromUtf8(tbl[i].sfen);
    }
    return SfenUtils::hirateSfen();
}

QString handicapLabelForSfen(const QString& sfen)
{
    const QString board = sfen.trimmed().section(QLatin1Char(' '), 0, 0);
    if (board.isEmpty()) return {};
    size_t count = 0;
    const HandicapPreset* tbl = handicapPresets(count);
    for (size_t i = 0; i < count; ++i) {
        if (board == QString::fromUtf8(tbl[i].sfen).section(QLatin1Char(' '), 0, 0)) return QString::fromUtf8(tbl[i].key);
    }
    return {};
}

} // namespace NotationUtils
