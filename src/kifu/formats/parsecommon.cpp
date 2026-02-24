#include "parsecommon.h"

#include <QRegularExpression>

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

bool mapKanjiPiece(const QString& text, Piece& base, bool& promoted)
{
    promoted = false;

    if (text.contains(QChar(u'歩')) || text.contains(QChar(u'と'))) {
        base = Piece::BlackPawn;
        promoted = text.contains(QChar(u'と'));
        return true;
    }
    if (text.contains(QChar(u'香')) || text.contains(QChar(u'杏'))) {
        base = Piece::BlackLance;
        promoted = text.contains(QChar(u'杏'));
        return true;
    }
    if (text.contains(QChar(u'桂')) || text.contains(QChar(u'圭'))) {
        base = Piece::BlackKnight;
        promoted = text.contains(QChar(u'圭'));
        return true;
    }
    if (text.contains(QChar(u'銀')) || text.contains(QChar(u'全'))) {
        base = Piece::BlackSilver;
        promoted = text.contains(QChar(u'全'));
        return true;
    }
    if (text.contains(QChar(u'金'))) {
        base = Piece::BlackGold;
        return true;
    }
    if (text.contains(QChar(u'角')) || text.contains(QChar(u'馬'))) {
        base = Piece::BlackBishop;
        promoted = text.contains(QChar(u'馬'));
        return true;
    }
    if (text.contains(QChar(u'飛')) || text.contains(QChar(u'龍')) || text.contains(QChar(u'竜'))) {
        base = Piece::BlackRook;
        promoted = (text.contains(QChar(u'龍')) || text.contains(QChar(u'竜')));
        return true;
    }
    if (text.contains(QChar(u'玉')) || text.contains(QChar(u'王'))) {
        base = Piece::BlackKing;
        return true;
    }

    return false;
}

std::optional<int> parseFileChar(QChar ch)
{
    const ushort u = ch.unicode();
    if (u >= '1' && u <= '9')
        return u - '0';
    return std::nullopt;
}

std::optional<int> parseRankChar(QChar ch)
{
    const ushort u = ch.unicode();
    if (u >= 'a' && u <= 'i')
        return u - 'a' + 1;
    return std::nullopt;
}

std::optional<int> parseDigit(QChar ch)
{
    const ushort u = ch.unicode();
    if (u >= '0' && u <= '9')
        return u - '0';
    return std::nullopt;
}

void appendLine(QString& buf, const QString& text)
{
    if (!buf.isEmpty()) buf += QLatin1Char('\n');
    buf += text;
}

void flushCommentToLastItem(QString& buf, QList<KifDisplayItem>& items, int minSize)
{
    if (buf.isEmpty()) return;
    if (items.size() <= minSize) { buf.clear(); return; }
    QString& dst = items.last().comment;
    if (!dst.isEmpty()) dst += QLatin1Char('\n');
    dst += buf;
    buf.clear();
}

KifDisplayItem createOpeningDisplayItem(const QString& comment, const QString& bookmark)
{
    KifDisplayItem item;
    item.prettyMove = QString();
    item.timeText   = QStringLiteral("00:00/00:00:00");
    item.comment    = comment;
    item.bookmark   = bookmark;
    item.ply        = 0;
    return item;
}

bool isBoardHeaderOrFrame(const QString& line)
{
    if (line.trimmed().isEmpty()) return false;

    static const QString kDigitsZ   = QStringLiteral("１２３４５６７８９");
    static const QString kKanjiRow  = QStringLiteral("一二三四五六七八九");
    static const QString kBoxChars  = QStringLiteral("┌┬┐┏┳┓└┴┘┗┻┛│┃─━┼");

    // 先頭「９ ８ ７ … １」（半角/全角/混在・空白区切りを許容）
    {
        int digitCount = 0;
        bool onlyDigitsAndSpace = true;
        for (qsizetype i = 0; i < line.size(); ++i) {
            const QChar ch = line.at(i);
            if (ch.isSpace()) continue;
            const ushort u = ch.unicode();
            const bool ascii19 = (u >= QChar(u'1').unicode() && u <= QChar(u'9').unicode());
            const bool zenk19  = kDigitsZ.contains(ch);
            if (ascii19 || zenk19) { ++digitCount; continue; }
            onlyDigitsAndSpace = false; break;
        }
        if (onlyDigitsAndSpace && digitCount >= 5) return true;
    }

    // 罫線：ASCII（+,-,|）/ Unicode
    {
        const QString s = line.trimmed();
        if ((s.startsWith(QLatin1Char('+')) && s.endsWith(QLatin1Char('+'))) ||
            (s.startsWith(QChar(u'|')) && s.endsWith(QChar(u'|')))) {
            return true;
        }
        int boxCount = 0;
        for (qsizetype i = 0; i < s.size(); ++i) if (kBoxChars.contains(s.at(i))) ++boxCount;
        if (boxCount >= qMax(3, static_cast<int>(s.size()) / 2)) return true;
    }

    // 持駒見出し
    if (line.contains(QStringLiteral("先手の持駒")) ||
        line.contains(QStringLiteral("後手の持駒")))
        return true;

    // 下部見出し（"一二三…九"）だけの行
    {
        const QString t = line.trimmed();
        if (!t.isEmpty()) {
            bool ok = true; int kanjiCount = 0;
            for (qsizetype i = 0; i < t.size(); ++i) {
                const QChar ch = t.at(i);
                if (kKanjiRow.contains(ch)) { ++kanjiCount; continue; }
                if (ch.isSpace()) continue;
                ok = false; break;
            }
            if (ok && kanjiCount >= 5) return true;
        }
    }
    return false;
}

bool containsAnyTerminal(const QString& s, QString* matched)
{
    for (const QString& w : terminalWords()) {
        if (s.contains(w)) { if (matched) *matched = w; return true; }
    }
    return false;
}

bool isKifSkippableHeaderLine(const QString& line)
{
    if (line.isEmpty()) return true;
    if (line.startsWith(QLatin1Char('#'))) return true;

    static const QStringList keys = {
        QStringLiteral("開始日時"), QStringLiteral("終了日時"), QStringLiteral("対局日"),
        QStringLiteral("棋戦"), QStringLiteral("戦型"), QStringLiteral("持ち時間"),
        QStringLiteral("秒読み"), QStringLiteral("消費時間"), QStringLiteral("場所"),
        QStringLiteral("表題"),   QStringLiteral("掲載"),   QStringLiteral("記録係"),
        QStringLiteral("備考"),   QStringLiteral("図"),     QStringLiteral("振り駒"),
        QStringLiteral("先手省略名"), QStringLiteral("後手省略名"),
        QStringLiteral("手数----指手---------消費時間--"),
        QStringLiteral("手数――指手――――――――消費時間――"),
        QStringLiteral("先手："), QStringLiteral("後手："),
        QStringLiteral("先手番"), QStringLiteral("後手番"),
        QStringLiteral("手合割"), QStringLiteral("手合"),
        QStringLiteral("手数＝")
    };
    for (const auto& k : keys) {
        if (line.contains(k)) return true;
    }

    if (line.startsWith(QLatin1Char('&'))) return true;

    static const QRegularExpression s_made(QStringLiteral("^\\s*まで[0-9０-９]+手"));
    if (s_made.match(line).hasMatch()) return true;

    return false;
}

} // namespace KifuParseCommon
