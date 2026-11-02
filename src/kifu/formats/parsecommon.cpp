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

    // 持駒見出し（駒落ちは下手・上手）。行頭だけを見る（指し手の行のコメントに書いた語で、指し手の行を読み飛ばさない）
    if (isBodHandsLine(line.trimmed())) return true;

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
        // 局面図の筋の見出し「９ ８ ７ …」は手数で始まる指し手の行に見えるので、先に読み飛ばす。
        // 局面図の後ろに対局情報を書くソフト（ShogiGUI）がある
        if (isBoardHeaderOrFrame(t)) continue;
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
