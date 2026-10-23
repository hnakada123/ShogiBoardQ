/// @file kiflexer.cpp
/// @brief KIF形式固有の字句解析・トークン化の実装・BOD盤面解析

#include "kiflexer.h"

#include <QRegularExpression>
#include "shoginotation.h"

namespace {

const QRegularExpression& nextMoveNumberRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("\\s+[0-9０-９]"));
        return &r;
    }();
    return re;
}

} // namespace

namespace KifLexer {

std::optional<int> startsWithMoveNumber(QStringView line)
{
    int i = 0, digits = 0;
    while (i < line.size()) {
        const QChar ch = line.at(i);
        const ushort u = ch.unicode();
        const bool ascii = (u >= '0' && u <= '9');
        const bool zenk  = QStringLiteral("０１２３４５６７８９").contains(ch);
        if (ascii || zenk) { ++i; ++digits; }
        else break;
    }
    if (digits == 0) return std::nullopt;
    if (i < line.size()) {
        QChar next = line.at(i);
        if (next == QChar(u'手') || next == QChar(u'＝')) return std::nullopt;
    }
    return digits;
}

namespace {

/// 消費時間（"( m:ss/H:MM:SS)"。分と時間は何桁でもよい）
const QRegularExpression& kifTimeRe()
{
    static const QRegularExpression re(
        QStringLiteral("\\(\\s*([0-9０-９]+)[:：]([0-9０-９]{2})(?:\\s*[／/]\\s*([0-9０-９]+)[:：]([0-9０-９]{2})[:：]([0-9０-９]{2}))?\\s*\\)"));
    return re;
}

/// 消費時間の欄を値にする（秒・分が60以上なら読めない表記として残す）
RecordTime timeOf(const QRegularExpressionMatch& m)
{
    const auto number = [&m](int index) { return static_cast<qint64>(ShogiNotation::numberFromDigits(m.captured(index))); };
    const bool hasTotal = m.lastCapturedIndex() >= 5 && !m.captured(3).isEmpty();
    const qint64 minutes = number(1), seconds = number(2);
    const qint64 hours = hasTotal ? number(3) : 0;
    const qint64 totalMinutes = hasTotal ? number(4) : 0;
    const qint64 totalSeconds = hasTotal ? number(5) : 0;
    if (seconds >= 60 || totalMinutes >= 60 || totalSeconds >= 60) {
        RecordTime unreadable;
        unreadable.importedText = m.captured(0);
        return unreadable;
    }
    return RecordTime::fromMilliseconds((minutes * 60 + seconds) * 1000,
                                        ((hours * 60 + totalMinutes) * 60 + totalSeconds) * 1000);
}

/// 行内のコメントの始まり（* か ＊。なければ -1）
qsizetype commentStart(const QString& text)
{
    const qsizetype ascii = text.indexOf(QChar(u'*'));
    const qsizetype wide = text.indexOf(QChar(u'＊'));
    if (ascii < 0) return wide;
    return wide < 0 ? ascii : qMin(ascii, wide);
}

} // namespace

const QRegularExpression& variationHeaderCaptureRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*変化[:：]\\s*([0-9０-９]+)\\s*手"));
        return &r;
    }();
    return re;
}

QList<NumberedMove> readNumberedMoves(const QString& source)
{
    static const QRegularExpression s_branchMark(QStringLiteral("\\s*[+＋]$"));
    QList<NumberedMove> moves;
    // 行の残りは複製せずに位置で進め、コメントの始まりと消費時間の欄は行の先頭から1度ずつしか探さない
    // （手ごとに残り全体を複製して探し直すと、1行に多くの手を並べた行で二乗の時間になる。セキュリティ監査 S6）
    const QStringView all(source);
    const qsizetype commentAt = commentStart(source);
    // 正規表現は既定では照合のたびに文字列全体の UTF-16 を確かめるので、行を1度確かめて省く
    const QRegularExpression::MatchOptions options = all.isValidUtf16()
        ? QRegularExpression::DontCheckSubjectStringMatchOption : QRegularExpression::NoMatchOption;
    const auto find = [options](const QRegularExpression& re, QStringView subject, qsizetype offset = 0) {
        return re.matchView(subject, offset, QRegularExpression::NormalMatch, options);
    };
    QRegularExpressionMatch nextTime = find(kifTimeRe(), all);   // まだ使っていない最初の消費時間の欄
    QStringView line = all;
    while (!line.isEmpty()) {
        const std::optional<int> digits = startsWithMoveNumber(line);
        if (!digits) break;
        qsizetype i = *digits;
        while (i < line.size() && line.at(i).isSpace()) ++i;
        if (i >= line.size()) break;

        NumberedMove move;
        QStringView rest = line.mid(i).trimmed();
        const qsizetype restStart = rest.data() - all.data();
        // コメントは行末まで（コメントの中の数字を次の手と見ない）
        if (commentAt >= restStart && commentAt < restStart + rest.size()) {
            const qsizetype comment = commentAt - restStart;
            move.comment = rest.mid(comment + 1).trimmed().toString();
            rest = rest.left(comment).trimmed();
        }
        // 消費時間の欄（手の表記の中で最初のもの）。行の先頭から探した欄がこの手より前なら、ここから探し直す
        if (nextTime.hasMatch() && nextTime.capturedStart(0) < restStart) nextTime = find(kifTimeRe(), all, restStart);
        qsizetype timeStart = -1, timeEnd = -1;
        QRegularExpressionMatch time;
        if (nextTime.hasMatch() && nextTime.capturedEnd(0) <= restStart + rest.size()) {
            time = nextTime;
            timeStart = time.capturedStart(0) - restStart;
            timeEnd = time.capturedEnd(0) - restStart;
        } else if (nextTime.hasMatch() && nextTime.capturedStart(0) < restStart + rest.size()) {
            // 欄がコメントの始まりをまたぐ（まれ）。手の表記の中だけで探す
            time = find(kifTimeRe(), rest);
            timeStart = time.capturedStart(0);
            timeEnd = time.capturedEnd(0);
        }
        // 消費時間の欄、なければ次の手の手数で、この手の表記が終わる
        QStringView next;
        if (time.hasMatch()) {
            move.time = timeOf(time);
            next = rest.mid(timeEnd).trimmed();
            rest = rest.left(timeStart).trimmed();
        } else if (const QRegularExpressionMatch number = find(nextMoveNumberRe(), rest); number.hasMatch()) {
            next = rest.mid(number.capturedStart()).trimmed();
            rest = rest.left(number.capturedStart()).trimmed();
        }
        // 消費時間欄がない行では、分岐ありを示す行末の「+」が指し手に残るので取り除く
        move.text = rest.toString();
        move.text.remove(s_branchMark);
        if (!move.text.isEmpty()) moves.append(move);
        line = next;
    }
    return moves;
}

} // namespace KifLexer

// ============================================================
// KIF形式のBOD盤面解析の実装
// ============================================================

namespace {

bool isSpaceLike(QChar ch)
{
    return ch.isSpace() || ch == QChar(0x3000);
}

int parseKanjiNumberString(const QString& s)
{
    if (s.isEmpty()) return -1;
    qsizetype idx = s.indexOf(QChar(u'十'));
    if (idx >= 0) {
        int tens = 1;
        if (idx > 0) {
            int d = ShogiNotation::kanjiNumeralValue(s.at(0));
            if (d <= 0) return -1;
            tens = d;
        }
        int ones = 0;
        if (idx + 1 < s.size()) {
            int d = ShogiNotation::kanjiNumeralValue(s.at(idx + 1));
            if (d < 0) return -1;
            ones = d;
        }
        return tens * 10 + ones;
    }
    if (s.size() == 1) {
        int d = ShogiNotation::kanjiNumeralValue(s.at(0));
        return (d >= 0) ? d : -1;
    }
    return -1;
}

int parseCountSuffixFlexible(const QString& token)
{
    if (token.size() <= 1) return 1;
    QString rest = token.mid(1).trimmed();
    if (rest.isEmpty()) return 1;
    static const QRegularExpression reAscii(QStringLiteral("^(?:[×x*]?)(\\d+)$"));
    QRegularExpressionMatch m = reAscii.match(rest);
    if (m.hasMatch()) return m.captured(1).toInt();
    int n = parseKanjiNumberString(rest);
    return (n > 0) ? n : 1;
}

QString zenk2ascii(QString s)
{
    static const QString z = QStringLiteral("０１２３４５６７８９");
    for (qsizetype i = 0; i < s.size(); ++i) {
        qsizetype idx = z.indexOf(s.at(i));
        if (idx >= 0) s[i] = QChar('0' + static_cast<int>(idx));
    }
    return s;
}

bool extractFramedContent(const QString& line, QString& inner, QChar& rankKanji)
{
    static const QRegularExpression rowRe(QStringLiteral("^\\s*\\|(.+)\\|\\s*([一二三四五六七八九])\\s*$"));
    QRegularExpressionMatch m = rowRe.match(line);
    if (!m.hasMatch()) return false;
    inner = m.captured(1);
    rankKanji = m.captured(2).at(0);
    return true;
}

QStringList tokenizeBodRowContent(const QString& inner)
{
    static const QString promotedSingles = QStringLiteral("と杏圭全馬龍竜");
    static const QString baseSingles = QStringLiteral("歩香桂銀金角飛玉王") + promotedSingles;

    QStringList tokens;
    qsizetype i = 0, n = inner.size();
    while (i < n && tokens.size() < 9) {
        while (i < n && isSpaceLike(inner.at(i))) ++i;
        if (i >= n) break;

        QChar ch = inner.at(i);

        if (ch == QChar(0xFF65)) { tokens << QStringLiteral("・"); ++i; continue; }
        if (ch == QChar(0xFF56)) { ch = QLatin1Char('v'); }

        if (ch == QChar(0x30FB)) {
            tokens << QStringLiteral("・");
            ++i;
            continue;
        }

        if (ch == QLatin1Char('v')) {
            ++i;
            while (i < n && isSpaceLike(inner.at(i))) ++i;
            if (i < n && baseSingles.contains(inner.at(i))) {
                tokens << (QStringLiteral("v") + inner.at(i));
                ++i;
            } else {
                tokens << QStringLiteral("・");
            }
            continue;
        }

        if (baseSingles.contains(ch)) {
            tokens << QString(ch);
            ++i;
            continue;
        }

        ++i;
    }

    while (tokens.size() < 9) tokens << QStringLiteral("・");
    return tokens;
}

bool parseBodRow(const QString& line, QStringList& outTokens, QChar& outRankKanji, bool hasFrame)
{
    QString inner;
    if (hasFrame) {
        if (!extractFramedContent(line, inner, outRankKanji)) return false;
    } else {
        inner = line.trimmed();
        outRankKanji = QChar();
    }
    outTokens = tokenizeBodRowContent(inner);
    return true;
}

/// 局面図の1段（9筋から1筋の順の9つの駒の名前。"v" は後手、"・" は空き）を局面に置く
void placeBodRow(const QStringList& tokens, int rank, Position& out)
{
    for (qsizetype i = 0; i < tokens.size() && i < 9; ++i) {
        const QString& token = tokens.at(i);
        const bool gote = token.startsWith(QLatin1Char('v'));
        const Piece piece = ShogiNotation::pieceFromName(gote ? QStringView(token).mid(1) : QStringView(token));
        out.setPieceAt({9 - static_cast<int>(i), rank}, piece == Piece::None ? piece : (gote ? toWhite(piece) : piece));
    }
}

/// 持駒の行（「先手の持駒：飛　角　歩三」。駒落ちは下手・上手）を局面の持ち駒に足す
void addBodHands(const QString& line, Position& out)
{
    const QString t = line.trimmed();
    const bool sideBlack = t.startsWith(QStringLiteral("先手の持駒")) || t.startsWith(QStringLiteral("下手の持駒"));
    const bool sideWhite = t.startsWith(QStringLiteral("後手の持駒")) || t.startsWith(QStringLiteral("上手の持駒"));
    if (!sideBlack && !sideWhite) return;

    qsizetype idx = t.indexOf(QChar(u'：')); if (idx < 0) idx = t.indexOf(QLatin1Char(':'));
    if (idx < 0) return;

    QString rhs = t.mid(idx+1).trimmed();
    if (rhs == QStringLiteral("なし")) return;

    rhs.replace(QChar(u'、'), QLatin1Char(' '));
    rhs.replace(QChar(0x3000), QLatin1Char(' '));
    static const QRegularExpression kWhitespaceRe(QStringLiteral("\\s+"));
    const QStringList toks = rhs.split(kWhitespaceRe, Qt::SkipEmptyParts);

    for (QString tok : toks) {
        tok = tok.trimmed();
        if (tok.isEmpty()) continue;

        QChar head = tok.at(0);
        const Piece piece = demote(ShogiNotation::pieceFromName(QStringView(&head, 1)));
        if (piece == Piece::None || piece == Piece::BlackKing) continue;
        out.addToHand(sideBlack ? piece : toWhite(piece), parseCountSuffixFlexible(tok));
    }
}

} // namespace

namespace KifLexer {

// === BOD盤面解析 ===

bool readBodPosition(const QStringList& lines, Position& out)
{
    Position position;
    int rows = 0;
    int placementStart = -1;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines.at(i).contains(QStringLiteral("配置：")) || lines.at(i).contains(QStringLiteral("配置:"))) {
            placementStart = static_cast<int>(i);
            break;
        }
    }

    if (placementStart >= 0) {
        int currentRankIdx = 0;
        for (qsizetype i = placementStart + 1; i < lines.size() && currentRankIdx < 9; ++i) {
            const QString& raw = lines.at(i);
            if (raw.trimmed().isEmpty()) continue;
            QStringList toks; QChar dummyRank;
            if (parseBodRow(raw, toks, dummyRank, false)) {
                placeBodRow(toks, currentRankIdx + 1, position);
                currentRankIdx++;
            }
        }
        rows = currentRankIdx;
    } else {
        // 段の漢数字で段を決める（同じ段が2度あれば後の行）
        QList<int> seen;
        for (const QString& raw : std::as_const(lines)) {
            QStringList toks; QChar rankKanji;
            if (!parseBodRow(raw, toks, rankKanji, true)) continue;
            const int rank = ShogiNotation::rankFromKanji(rankKanji);
            if (rank < 1 || rank > 9) continue;
            placeBodRow(toks, rank, position);
            if (!seen.contains(rank)) seen.append(rank);
        }
        rows = static_cast<int>(seen.size());
    }
    if (rows != 9) return false;

    for (const QString& line : std::as_const(lines)) addBodHands(line, position);

    // 手番（駒落ちの棋譜は下手番（先手側）・上手番（後手側）と書く）と手数（「手数＝N」の次の手）
    for (const QString& l : std::as_const(lines)) {
        const QString t = l.trimmed();
        if (t.contains(QStringLiteral("先手番")) || t.contains(QStringLiteral("下手番"))) break;
        if (t.contains(QStringLiteral("後手番")) || t.contains(QStringLiteral("上手番"))) {
            position.setSideToMove(Turn::White);
            break;
        }
    }
    static const QRegularExpression reTeSu(QStringLiteral("手数\\s*[=＝]\\s*([0-9０-９]+)"));
    for (const QString& l : std::as_const(lines)) {
        auto mt = reTeSu.match(l);
        if (mt.hasMatch()) {
            // 「手数＝N」の次の手。大きすぎる数は手数の上限で止める（読めなければ 0 として 1 手目）
            const qint64 played = zenk2ascii(mt.captured(1)).toLongLong();
            position.setMoveNumber(static_cast<int>(qBound(qint64(1), played + 1, qint64(Position::kMaxMoveNumber))));
            break;
        }
    }
    out = position;
    return true;
}

} // namespace KifLexer
