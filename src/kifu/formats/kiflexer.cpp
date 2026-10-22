/// @file kiflexer.cpp
/// @brief KIF形式固有の字句解析・トークン化の実装・BOD盤面解析

#include "kiflexer.h"
#include "parsecommon.h"

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

std::optional<int> startsWithMoveNumber(const QString& line)
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

const QRegularExpression& kifTimeRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(
            QStringLiteral("\\(\\s*([0-9０-９]+)[:：]([0-9０-９]{2})(?:\\s*[／/]\\s*([0-9０-９]+)[:：]([0-9０-９]{2})[:：]([0-9０-９]{2}))?\\s*\\)")
            );
        return &r;
    }();
    return re;
}

const QRegularExpression& variationHeaderCaptureRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*変化[:：]\\s*([0-9０-９]+)\\s*手"));
        return &r;
    }();
    return re;
}

// === 時間解析 ===

QString normalizeTimeMatch(const QRegularExpressionMatch& m)
{
    const int mm = ShogiNotation::numberFromDigits(m.captured(1));
    const int ss = ShogiNotation::numberFromDigits(m.captured(2));
    const bool hasCum = m.lastCapturedIndex() >= 5 && m.captured(3).size();
    const int HH = hasCum ? ShogiNotation::numberFromDigits(m.captured(3)) : 0;
    const int MM = hasCum ? ShogiNotation::numberFromDigits(m.captured(4)) : 0;
    const int SS = hasCum ? ShogiNotation::numberFromDigits(m.captured(5)) : 0;
    auto z2 = [](int x){ return QStringLiteral("%1").arg(x, 2, 10, QLatin1Char('0')); };
    return z2(mm) + QLatin1Char(':') + z2(ss) + QLatin1Char('/') +
           z2(HH) + QLatin1Char(':') + z2(MM) + QLatin1Char(':') + z2(SS);
}

QString extractCumTimeFromMatch(const QRegularExpressionMatch& tm)
{
    auto z2 = [](int x){ return QStringLiteral("%1").arg(x, 2, 10, QLatin1Char('0')); };
    const bool hasCum = tm.lastCapturedIndex() >= 5 && tm.captured(3).size();
    if (!hasCum) return QStringLiteral("00:00:00");
    const int HH = ShogiNotation::numberFromDigits(tm.captured(3));
    const int MM = ShogiNotation::numberFromDigits(tm.captured(4));
    const int SS = ShogiNotation::numberFromDigits(tm.captured(5));
    return z2(HH) + QLatin1Char(':') + z2(MM) + QLatin1Char(':') + z2(SS);
}

// === 行内トークン抽出 ===

void stripTimeAndNextMove(QString& rest, const QString& lineStr, int skipOffset,
                          QRegularExpressionMatch& tm, QString& timeText, int& nextMoveStartIdx)
{
    tm = kifTimeRe().match(rest);
    if (tm.hasMatch()) {
        timeText = normalizeTimeMatch(tm);
        nextMoveStartIdx = static_cast<int>(tm.capturedEnd(0));
        rest = rest.left(tm.capturedStart(0)).trimmed();
    } else {
        QRegularExpressionMatch nextM = nextMoveNumberRe().match(rest);
        if (nextM.hasMatch()) {
            nextMoveStartIdx = static_cast<int>(nextM.capturedStart());
            rest = rest.left(nextMoveStartIdx).trimmed();
        } else {
            nextMoveStartIdx = -1;
        }
    }
    // 消費時間欄がない行では、分岐ありを示す行末の「+」が指し手に残るので取り除く
    static const QRegularExpression s_branchMark(QStringLiteral("\\s*[+＋]$"));
    rest.remove(s_branchMark);
    Q_UNUSED(lineStr);
    Q_UNUSED(skipOffset);
}

void stripInlineComment(QString& rest, QString& commentBuf)
{
    int commentIdx = static_cast<int>(rest.indexOf(QChar(u'*')));
    if (commentIdx < 0) commentIdx = static_cast<int>(rest.indexOf(QChar(u'＊')));
    if (commentIdx >= 0) {
        const QString inlineC = rest.mid(commentIdx + 1).trimmed();
        if (!inlineC.isEmpty()) {
            KifuParseCommon::appendLine(commentBuf, inlineC);
        }
        rest = rest.left(commentIdx).trimmed();
    }
}

void advanceToNextMove(QString& lineStr, int skipOffset,
                       const QRegularExpressionMatch& tm, int nextMoveStartIdx)
{
    if (nextMoveStartIdx != -1) {
        if (tm.hasMatch()) {
            int consumed = skipOffset + static_cast<int>(tm.capturedEnd(0));
            lineStr = lineStr.mid(consumed).trimmed();
        } else {
            QRegularExpressionMatch nm = nextMoveNumberRe().match(lineStr, skipOffset);
            if (nm.hasMatch()) {
                lineStr = lineStr.mid(nm.capturedStart()).trimmed();
            } else {
                lineStr.clear();
            }
        }
    } else {
        lineStr.clear();
    }
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

bool readBodPosition(const QStringList& lines, Position& out, QString* detectedLabel)
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
        if (detectedLabel) *detectedLabel = QStringLiteral("配置");
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
        if (detectedLabel && rows > 0) *detectedLabel = QStringLiteral("BOD");
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
            position.setMoveNumber(zenk2ascii(mt.captured(1)).toInt() + 1);
            break;
        }
    }
    out = position;
    return true;
}

} // namespace KifLexer
