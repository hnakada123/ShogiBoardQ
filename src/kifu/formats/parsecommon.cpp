#include "parsecommon.h"

#include <QRegularExpression>
#include <QStringView>

namespace KifuParseCommon {

void appendLine(QString& buf, const QString& text)
{
    if (!buf.isEmpty()) buf += QLatin1Char('\n');
    buf += text;
}

bool isBoardHeaderOrFrame(QStringView line)
{
    if (line.trimmed().isEmpty()) return false;

    static const QString kDigitsZ   = QStringLiteral("１２３４５６７８９");
    static const QString kKanjiRow  = QStringLiteral("一二三四五六七八九");
    static const QString kBoxChars  = QStringLiteral("┌┬┐┏┳┓└┴┘┗┻┛│┃─━┼");

    // 先頭「９ ８ ７ … １」（半角/全角/混在・空白区切りを許容）
    {
        int digitCount = 0;
        bool onlyDigitsAndSpace = true;
        for (const QChar ch : line) {
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
        const QStringView s = line.trimmed();
        if ((s.startsWith(QLatin1Char('+')) && s.endsWith(QLatin1Char('+'))) ||
            (s.startsWith(QChar(u'|')) && s.endsWith(QChar(u'|')))) {
            return true;
        }
        int boxCount = 0;
        for (const QChar ch : s) if (kBoxChars.contains(ch)) ++boxCount;
        if (boxCount >= qMax(3, static_cast<int>(s.size()) / 2)) return true;
    }

    // 持駒見出し（駒落ちは下手・上手）
    if (line.contains(QStringLiteral("先手の持駒")) ||
        line.contains(QStringLiteral("後手の持駒")) ||
        line.contains(QStringLiteral("下手の持駒")) ||
        line.contains(QStringLiteral("上手の持駒")))
        return true;

    // 下部見出し（"一二三…九"）だけの行
    {
        const QStringView t = line.trimmed();
        if (!t.isEmpty()) {
            bool ok = true; int kanjiCount = 0;
            for (const QChar ch : t) {
                if (kKanjiRow.contains(ch)) { ++kanjiCount; continue; }
                if (ch.isSpace()) continue;
                ok = false; break;
            }
            if (ok && kanjiCount >= 5) return true;
        }
    }
    return false;
}

bool isKifSkippableHeaderLine(QStringView line)
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
        QStringLiteral("下手："), QStringLiteral("上手："),
        QStringLiteral("先手番"), QStringLiteral("後手番"),
        QStringLiteral("下手番"), QStringLiteral("上手番"),
        QStringLiteral("手合割"), QStringLiteral("手合"),
        QStringLiteral("手数＝")
    };
    for (const auto& k : std::as_const(keys)) {
        if (line.contains(k)) return true;
    }

    if (line.startsWith(QLatin1Char('&'))) return true;

    static const QRegularExpression s_made(QStringLiteral("^\\s*まで[0-9０-９]+手"));
    if (s_made.matchView(line).hasMatch()) return true;

    return false;
}

bool isKifCommentLine(QStringView s)
{
    if (s.isEmpty()) return false;
    const QChar ch = s.front();
    return (ch == QChar(u'*') || ch == QChar(u'＊'));
}

bool isBookmarkLine(QStringView s)
{
    return s.startsWith(QLatin1Char('&'));
}

bool isBodHandsLine(QStringView line)
{
    // 駒落ちの棋譜は先手側を下手、後手側を上手と書く
    static const QString kHeads[] = {
        QStringLiteral("先手の持駒"), QStringLiteral("後手の持駒"),
        QStringLiteral("先手の持ち駒"), QStringLiteral("後手の持ち駒"),
        QStringLiteral("下手の持駒"), QStringLiteral("上手の持駒"),
        QStringLiteral("下手の持ち駒"), QStringLiteral("上手の持ち駒"),
    };
    for (const QString& head : kHeads) {
        if (line.startsWith(head)) return true;
    }
    return false;
}

QList<KifGameInfoItem> extractHeaderGameInfo(
    const QStringList& lines,
    const std::function<bool(const QString&)>& isMoveLine)
{
    QList<KifGameInfoItem> ordered;

    static const QRegularExpression kHeaderLine(
        QStringLiteral("^\\s*([^：:]+?)\\s*[：:]\\s*(.*?)\\s*$")
    );
    static const QRegularExpression kLineIsComment(
        QStringLiteral("^\\s*[#＃\\*\\＊]")
    );

    for (const QString& rawLine : std::as_const(lines)) {
        const QString t = rawLine.trimmed();
        if (t.isEmpty()) continue;
        if (kLineIsComment.match(t).hasMatch()) continue;
        if (isMoveLine(t)) break;
        if (isBodHandsLine(t)) continue;

        QRegularExpressionMatch m = kHeaderLine.match(rawLine);
        if (!m.hasMatch()) continue;

        QString key = m.captured(1).trimmed();
        if (key.endsWith(u'：') || key.endsWith(u':')) key.chop(1);
        key = key.trimmed();

        QString val = m.captured(2).trimmed();
        val.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        ordered.push_back({ key, val });
    }

    return ordered;
}

} // namespace KifuParseCommon
