#include "parsecommon.h"

namespace KifuParseCommon {

namespace {

const QString& zenkakuDigitsStr()
{
    static const QString digits = QStringLiteral("０１２３４５６７８９");
    return digits;
}

} // namespace

int asciiDigitToInt(QChar c)
{
    const ushort u = c.unicode();
    return (u >= '0' && u <= '9') ? (u - '0') : 0;
}

int zenkakuDigitToInt(QChar c)
{
    const int idx = static_cast<int>(zenkakuDigitsStr().indexOf(c));
    return (idx >= 0) ? idx : 0;
}

int flexDigitToIntNoDetach(QChar c)
{
    int v = asciiDigitToInt(c);
    if (!v) {
        v = zenkakuDigitToInt(c);
    }
    return v;
}

int flexDigitsToIntNoDetach(const QString& text)
{
    int value = 0;
    const int n = static_cast<int>(text.size());
    for (int i = 0; i < n; ++i) {
        const QChar ch = text.at(i);
        int d = asciiDigitToInt(ch);
        if (!d) {
            d = zenkakuDigitToInt(ch);
        }
        if (!d && ch != QChar(u'0') && ch != QChar(u'０')) {
            continue;
        }
        if (ch == QChar(u'0') || ch == QChar(u'０')) {
            d = 0;
        }
        value = value * 10 + d;
    }
    return value;
}

const std::array<QString, 16>& terminalWords()
{
    static const std::array<QString, 16> words = {{
        QStringLiteral("中断"),
        QStringLiteral("投了"),
        QStringLiteral("持将棋"),
        QStringLiteral("千日手"),
        QStringLiteral("切れ負け"),
        QStringLiteral("時間切れ"),
        QStringLiteral("反則勝ち"),
        QStringLiteral("反則負け"),
        QStringLiteral("不戦勝"),
        QStringLiteral("不戦敗"),
        QStringLiteral("詰み"),
        QStringLiteral("詰"),
        QStringLiteral("不詰"),
        QStringLiteral("入玉勝ち"),
        QStringLiteral("宣言勝ち"),
        QStringLiteral("入玉宣言勝ち")
    }};
    return words;
}

bool isTerminalWordExact(const QString& text, QString* normalized)
{
    const QString t = text.trimmed();
    for (const QString& w : terminalWords()) {
        if (t == w) {
            if (normalized) {
                *normalized = w;
            }
            return true;
        }
    }
    return false;
}

bool isTerminalWordContains(const QString& text, QString* normalized)
{
    const QString t = text.trimmed();
    for (const QString& w : terminalWords()) {
        if (t == w || t.contains(w)) {
            if (normalized) {
                *normalized = w;
            }
            return true;
        }
    }
    return false;
}

bool mapKanjiPiece(const QString& text, QChar& base, bool& promoted)
{
    promoted = false;

    if (text.contains(QChar(u'歩')) || text.contains(QChar(u'と'))) {
        base = QLatin1Char('P');
        promoted = text.contains(QChar(u'と'));
        return true;
    }
    if (text.contains(QChar(u'香')) || text.contains(QChar(u'杏'))) {
        base = QLatin1Char('L');
        promoted = text.contains(QChar(u'杏'));
        return true;
    }
    if (text.contains(QChar(u'桂')) || text.contains(QChar(u'圭'))) {
        base = QLatin1Char('N');
        promoted = text.contains(QChar(u'圭'));
        return true;
    }
    if (text.contains(QChar(u'銀')) || text.contains(QChar(u'全'))) {
        base = QLatin1Char('S');
        promoted = text.contains(QChar(u'全'));
        return true;
    }
    if (text.contains(QChar(u'金'))) {
        base = QLatin1Char('G');
        return true;
    }
    if (text.contains(QChar(u'角')) || text.contains(QChar(u'馬'))) {
        base = QLatin1Char('B');
        promoted = text.contains(QChar(u'馬'));
        return true;
    }
    if (text.contains(QChar(u'飛')) || text.contains(QChar(u'龍')) || text.contains(QChar(u'竜'))) {
        base = QLatin1Char('R');
        promoted = (text.contains(QChar(u'龍')) || text.contains(QChar(u'竜')));
        return true;
    }
    if (text.contains(QChar(u'玉')) || text.contains(QChar(u'王'))) {
        base = QLatin1Char('K');
        return true;
    }

    return false;
}

} // namespace KifuParseCommon
