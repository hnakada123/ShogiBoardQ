#include "parsecommon.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include "kifmovetext.h"

#include <QRegularExpression>
#include <QStringView>

namespace KifuParseCommon {

bool KifLineDraft::resolve(KifLine& out, QString* error) const
{
    const auto replay = SfenReplay::replay(baseSfen, usiMoves);
    if (replay.failedPly == 0) {
        if (error) *error += replay.error + QLatin1Char('\n');
        return false;
    }
    // 指せない手があれば、その手の手前までを読む（読めない手と同じ扱い）
    const bool truncated = !replay.success();
    if (truncated && error) {
        const int ply = startPly + replay.failedPly - 1;
        *error += QStringLiteral("Cannot restore move %1: %2\n").arg(ply).arg(replay.error)
                  + truncationNotice(ply);
    }
    KifLine result;
    result.startPly = startPly;
    result.baseSfen = replay.initialSfen;
    int first = 0;
    if (!disp.isEmpty() && disp.first().ply == 0) {
        result.opening = disp.first();
        first = 1;
    }
    if (!truncated && disp.size() - first != usiMoves.size() + (endsWithTerminal ? 1 : 0)) {
        if (error) *error += QStringLiteral("Move and display counts differ at ply %1\n").arg(startPly);
        return false;
    }
    BoardSquare previousTo;
    if (const auto previous = UsiMove::parse(previousUsi)) previousTo = previous->to;
    for (qsizetype i = 0; i < replay.moves.size(); ++i) {
        const auto& step = replay.moves.at(i);
        const auto& item = disp.at(first + i);
        const Turn mover = step.beforeSfen.contains(QStringLiteral(" w ")) ? Turn::White : Turn::Black;
        const auto& usi = step.move.move;
        const QString display = item.prettyMove.isEmpty()
            ? KifMoveText::format(mover, step.move.movingPiece, usi, previousTo) : item.prettyMove;
        RecordEntry record = RecordEntry::played(step.move, display, step.sfen, RecordTime::fromText(item.timeText));
        record.mover = mover;
        record.comment = item.comment;
        record.bookmark = item.bookmark;
        result.entries.append(record);
        previousTo = usi.to;
    }
    if (endsWithTerminal && !truncated) {
        const auto& item = disp.last();
        TerminalType type = item.terminalType;
        if (type == TerminalType::None) {
            if (error) *error += QStringLiteral("Unknown terminal record: %1\n").arg(item.prettyMove);
            type = TerminalType::Error;
        }
        RecordEntry record = RecordEntry::ending(type, item.terminalSide, RecordTime::fromText(item.timeText));
        record.sfen = result.sfenAt(result.moveCount());
        record.comment = item.comment;
        record.bookmark = item.bookmark;
        result.entries.append(record);
    }
    out = std::move(result);
    return true;
}

QString truncationNotice(int ply)
{
    return ply > 1 ? QStringLiteral("The record was read up to move %1.\n").arg(ply - 1)
                   : QStringLiteral("No moves were read from this line.\n");
}

QString passNotice(int ply)
{
    return QStringLiteral("Move %1 is a pass, which is not supported.\n").arg(ply) + truncationNotice(ply);
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

QString formatTimeMS(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 tot = ms / 1000;
    const qint64 mm = tot / 60;
    const qint64 ss = tot % 60;
    return QStringLiteral("%1:%2")
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'));
}

QString formatTimeHMS(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 tot = ms / 1000;
    const qint64 hh = tot / 3600;
    const qint64 mm = (tot % 3600) / 60;
    const qint64 ss = tot % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hh, 2, 10, QLatin1Char('0'))
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'));
}

QString formatTimeText(qint64 moveMs, qint64 cumMs)
{
    return formatTimeMS(moveMs) + QStringLiteral("/") + formatTimeHMS(cumMs);
}

QMap<QString, QString> toGameInfoMap(const QList<KifGameInfoItem>& items)
{
    QMap<QString, QString> m;
    for (const auto& it : items) {
        m.insert(it.key, it.value);
    }
    return m;
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

bool tryHandleCommentLine(const QString& line, bool firstMoveFound,
                          QString& commentBuf, QString& openingCommentBuf)
{
    if (!isKifCommentLine(line)) return false;
    const QString c = line.mid(1).trimmed();
    // 空の「*」行も段落の区切りとして保持する。
    appendLine(firstMoveFound ? commentBuf : openingCommentBuf, c);
    return true;
}

bool tryHandleBookmarkLine(const QString& line, bool firstMoveFound,
                           QList<KifDisplayItem>& items, QString& openingBookmarkBuf)
{
    if (!isBookmarkLine(line)) return false;
    const QString name = line.mid(1).trimmed();
    if (!name.isEmpty()) {
        if (firstMoveFound && items.size() > 1) {
            appendLine(items.last().bookmark, name);
        } else {
            appendLine(openingBookmarkBuf, name);
        }
    }
    return true;
}

KifDisplayItem createTerminalDisplayItem(int ply, TerminalType type, Turn side,
                                         const QString& timeText)
{
    const RecordEntry entry = RecordEntry::ending(type, side);
    KifDisplayItem item;
    item.prettyMove = entry.displayText;
    item.timeText = timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText;
    item.ply = ply;
    item.terminal = true;
    item.terminalType = type;
    item.terminalSide = side;
    return item;
}

KifDisplayItem createMoveDisplayItem(int ply, const QString& prettyMove,
                                     const QString& timeText)
{
    KifDisplayItem item;
    item.prettyMove = prettyMove;
    item.timeText = timeText.isEmpty() ? QStringLiteral("00:00/00:00:00") : timeText;
    item.ply = ply;
    return item;
}

void finalizeDisplayItems(QString& commentBuf, QList<KifDisplayItem>& items,
                          const QString& openingCommentBuf, const QString& openingBookmarkBuf)
{
    if (!commentBuf.isEmpty() && !items.isEmpty()) {
        appendLine(items.last().comment, commentBuf);
        commentBuf.clear();
    }
    if (items.isEmpty()) {
        items.push_back(createOpeningDisplayItem(openingCommentBuf, openingBookmarkBuf));
    }
}

QString buildHandsSfen(const QMap<Piece, int>& black, const QMap<Piece, int>& white)
{
    static const Piece order[] = {Piece::BlackRook, Piece::BlackBishop, Piece::BlackGold,
                                  Piece::BlackSilver, Piece::BlackKnight, Piece::BlackLance, Piece::BlackPawn};
    QString s;
    for (const bool gote : {false, true}) {
        const QMap<Piece, int>& counts = gote ? white : black;
        for (const Piece p : order) {
            const int cnt = counts.value(p, 0);
            if (cnt <= 0) continue;
            if (cnt > 1) s += QString::number(cnt);
            s += pieceToChar(gote ? toWhite(p) : p);
        }
    }
    return s.isEmpty() ? QStringLiteral("-") : s;
}

void mergeOpeningIntoFirstMove(KifLine& line)
{
    if (!line.opening) return;
    const auto opening = *line.opening;
    line.opening.reset();
    if (line.entries.isEmpty()) return;
    auto& first = line.entries.first();
    QString comment = opening.comment;
    QString bookmark = opening.bookmark;
    if (!first.comment.isEmpty()) appendLine(comment, first.comment);
    if (!first.bookmark.isEmpty()) appendLine(bookmark, first.bookmark);
    first.comment = comment;
    first.bookmark = bookmark;
}

} // namespace KifuParseCommon
