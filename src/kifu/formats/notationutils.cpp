/// @file notationutils.cpp
/// @brief 棋譜変換器間で共有される座標変換・数字変換・手合マップユーティリティの実装

#include "notationutils.h"

#include <QLatin1Char>
#include <QStringLiteral>

namespace NotationUtils {

QChar rankNumToLetter(int r)
{
    if (r < 1 || r > 9) return QChar();
    return QChar(QLatin1Char(static_cast<char>('a' + (r - 1))));
}

int rankLetterToNum(QChar ch)
{
    char c = ch.toLatin1();
    if (c >= 'a' && c <= 'i') return c - 'a' + 1;
    return 0;
}

int kanjiDigitToInt(QChar c)
{
    switch (c.unicode()) {
    case u'一': return 1;
    case u'二': return 2;
    case u'三': return 3;
    case u'四': return 4;
    case u'五': return 5;
    case u'六': return 6;
    case u'七': return 7;
    case u'八': return 8;
    case u'九': return 9;
    default: return 0;
    }
}

QString intToZenkakuDigit(int d)
{
    static const QChar map[] = { QChar(u'０'), QChar(u'１'), QChar(u'２'), QChar(u'３'), QChar(u'４'),
                                 QChar(u'５'), QChar(u'６'), QChar(u'７'), QChar(u'８'), QChar(u'９') };
    if (d >= 1 && d <= 9) return QString(map[d]);
    return QString(QChar(u'？'));
}

QString intToKanjiDigit(int d)
{
    static const QChar map[10] = { QChar(), QChar(u'一'), QChar(u'二'), QChar(u'三'), QChar(u'四'),
                                   QChar(u'五'), QChar(u'六'), QChar(u'七'), QChar(u'八'), QChar(u'九') };
    if (d >= 1 && d <= 9) return QString(map[d]);
    return QString(QChar(u'？'));
}

QString formatSfenMove(int fromFile, int fromRank, int toFile, int toRank, bool promote)
{
    QString usi = QStringLiteral("%1%2%3%4")
        .arg(fromFile)
        .arg(rankNumToLetter(fromRank))
        .arg(toFile)
        .arg(rankNumToLetter(toRank));
    if (promote) usi += QLatin1Char('+');
    return usi;
}

QString formatSfenDrop(QChar piece, int toFile, int toRank)
{
    return QStringLiteral("%1*%2%3").arg(piece).arg(toFile).arg(rankNumToLetter(toRank));
}

QString mapHandicapToSfen(const QString& label)
{
    static const char* kEven = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
    struct Pair { const char* key; const char* sfen; };
    static const Pair tbl[] = {
        {"平手",       kEven},
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
    for (const auto& p : tbl) {
        if (label.contains(QString::fromUtf8(p.key))) return QString::fromUtf8(p.sfen);
    }
    return QString::fromUtf8(kEven);
}

} // namespace NotationUtils
