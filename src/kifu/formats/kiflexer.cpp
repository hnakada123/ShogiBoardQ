/// @file kiflexer.cpp
/// @brief KIF形式固有の字句解析・トークン化・BOD盤面解析の実装

#include "kiflexer.h"
#include "notationutils.h"
#include "parsecommon.h"

#include <QRegularExpression>

namespace {

const QRegularExpression& nextMoveNumberRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("\\s+[0-9０-９]"));
        return &r;
    }();
    return re;
}

bool isSpaceLike(QChar ch)
{
    return ch.isSpace() || ch == QChar(0x3000);
}

int bodKanjiDigit(QChar c)
{
    switch (c.unicode()) {
    case u'〇': case u'零': return 0;
    case u'一': return 1; case u'二': return 2; case u'三': return 3;
    case u'四': return 4; case u'五': return 5; case u'六': return 6;
    case u'七': return 7; case u'八': return 8; case u'九': return 9;
    default: return -1;
    }
}

int parseKanjiNumberString(const QString& s)
{
    if (s.isEmpty()) return -1;
    qsizetype idx = s.indexOf(QChar(u'十'));
    if (idx >= 0) {
        int tens = 1;
        if (idx > 0) {
            int d = bodKanjiDigit(s.at(0));
            if (d <= 0) return -1;
            tens = d;
        }
        int ones = 0;
        if (idx + 1 < s.size()) {
            int d = bodKanjiDigit(s.at(idx + 1));
            if (d < 0) return -1;
            ones = d;
        }
        return tens * 10 + ones;
    }
    if (s.size() == 1) {
        int d = bodKanjiDigit(s.at(0));
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

QString rowTokensToSfen(const QStringList& tokens)
{
    QString row;
    int empty = 0;
    auto flushEmpty = [&](){
        if (empty > 0) { row += QString::number(empty); empty = 0; }
    };

    int used = 0;
    for (const QString& tok : std::as_const(tokens)) {
        if (used >= 9) break;

        if (tok == QStringLiteral("・")) {
            ++empty; ++used;
            continue;
        }

        const bool gote = tok.startsWith(QLatin1Char('v'));
        const QString body = gote ? tok.mid(1) : tok;

        Piece base; bool promoted = false;
        if (!KifuParseCommon::mapKanjiPiece(body, base, promoted)) {
            ++empty; ++used;
            continue;
        }

        flushEmpty();
        if (promoted) row += QLatin1Char('+');
        row += pieceToChar(gote ? toWhite(base) : base);
        ++used;
    }
    while (used < 9) { ++empty; ++used; }
    flushEmpty();
    return row;
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
            QStringLiteral("\\(\\s*([0-9０-９]{1,2})[:：]([0-9０-９]{2})(?:\\s*[／/]\\s*([0-9０-９]{1,3})[:：]([0-9０-９]{2})[:：]([0-9０-９]{2}))?\\s*\\)")
            );
        return &r;
    }();
    return re;
}

const QRegularExpression& variationHeaderRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*変化[:：]\\s*[0-9０-９]+\\s*手"));
        return &r;
    }();
    return re;
}

const QRegularExpression& variationHeaderCaptureRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*変化[:：]\\s*([0-9０-９]+)手"));
        return &r;
    }();
    return re;
}

// === 時間解析 ===

QString normalizeTimeMatch(const QRegularExpressionMatch& m)
{
    const int mm = KifuParseCommon::flexDigitsToIntNoDetach(m.captured(1));
    const int ss = KifuParseCommon::flexDigitsToIntNoDetach(m.captured(2));
    const bool hasCum = m.lastCapturedIndex() >= 5 && m.captured(3).size();
    const int HH = hasCum ? KifuParseCommon::flexDigitsToIntNoDetach(m.captured(3)) : 0;
    const int MM = hasCum ? KifuParseCommon::flexDigitsToIntNoDetach(m.captured(4)) : 0;
    const int SS = hasCum ? KifuParseCommon::flexDigitsToIntNoDetach(m.captured(5)) : 0;
    auto z2 = [](int x){ return QStringLiteral("%1").arg(x, 2, 10, QLatin1Char('0')); };
    return z2(mm) + QLatin1Char(':') + z2(ss) + QLatin1Char('/') +
           z2(HH) + QLatin1Char(':') + z2(MM) + QLatin1Char(':') + z2(SS);
}

QString extractCumTimeFromMatch(const QRegularExpressionMatch& tm)
{
    auto z2 = [](int x){ return QStringLiteral("%1").arg(x, 2, 10, QLatin1Char('0')); };
    const bool hasCum = tm.lastCapturedIndex() >= 5 && tm.captured(3).size();
    if (!hasCum) return QStringLiteral("00:00:00");
    const int HH = KifuParseCommon::flexDigitsToIntNoDetach(tm.captured(3));
    const int MM = KifuParseCommon::flexDigitsToIntNoDetach(tm.captured(4));
    const int SS = KifuParseCommon::flexDigitsToIntNoDetach(tm.captured(5));
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

// === 終局語 ===

KifDisplayItem buildTerminalItem(int moveIndex, const QString& term,
                                 const QString& timeText, const QRegularExpressionMatch& tm)
{
    const QString teban = KifuParseCommon::tebanMark(moveIndex);
    const QString cum = tm.hasMatch() ? extractCumTimeFromMatch(tm) : QStringLiteral("00:00:00");

    KifDisplayItem item;
    item.prettyMove = teban + term;
    item.ply = moveIndex;
    if (term == QStringLiteral("千日手")) {
        item.timeText = QStringLiteral("00:00/") + cum;
    } else {
        item.timeText = timeText.isEmpty() ? (QStringLiteral("00:00/") + cum) : timeText;
    }
    item.comment = QString();
    return item;
}

// === 指し手解析 ===

bool findDestination(const QString& line, int& toFile, int& toRank, bool& isSameAsPrev)
{
    isSameAsPrev = false;

    if (line.contains(QStringLiteral("同"))) {
        toFile = toRank = 0;
        isSameAsPrev = true;
        return true;
    }

    static const auto& s_paren = *[]() {
        static const QRegularExpression r(QStringLiteral("[\\(（]"));
        return &r;
    }();
    const int paren = static_cast<int>(line.indexOf(s_paren));
    const QString head = (paren >= 0) ? line.left(paren) : line;

    static const auto& s_digitKanji = *[]() {
        static const QRegularExpression r(QStringLiteral("([1-9１-９])([一二三四五六七八九])"));
        return &r;
    }();
    static const auto& s_digitDigit = *[]() {
        static const QRegularExpression r(QStringLiteral("([1-9１-９])([1-9１-９])"));
        return &r;
    }();

    QRegularExpressionMatch m = s_digitKanji.match(head);
    if (!m.hasMatch()) m = s_digitDigit.match(head);
    if (!m.hasMatch()) return false;

    const QChar fch = m.capturedView(1).at(0);
    const QChar rch = m.capturedView(2).at(0);

    toFile = KifuParseCommon::flexDigitToIntNoDetach(fch);
    int r  = NotationUtils::kanjiDigitToInt(rch);
    if (r == 0) r = KifuParseCommon::flexDigitToIntNoDetach(rch);
    toRank = r;

    return (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9);
}

QChar pieceKanjiToUsiUpper(const QString& s)
{
    Piece base = Piece::None; bool promoted = false;
    if (!KifuParseCommon::mapKanjiPiece(s, base, promoted)) return QChar();
    return pieceToChar(base);
}

bool isPromotionMoveText(const QString& line)
{
    if (line.contains(QStringLiteral("不成"))) return false;

    QString head = line;
    static const auto& kParenRe = *[]() {
        static const QRegularExpression r(QStringLiteral("[\\(（]"));
        return &r;
    }();
    int p = static_cast<int>(head.indexOf(kParenRe));
    if (p >= 0) head = head.left(p);
    int u = static_cast<int>(head.indexOf(QChar(u'打')));
    if (u >= 0) head = head.left(u);

    static const auto& kModsRe = *[]() {
        static const QRegularExpression r(QStringLiteral("[右左上下引寄直行]+"));
        return &r;
    }();
    head.remove(kModsRe);
    head.remove(QChar(u'▲'));
    head.remove(QChar(u'△'));
    head = head.trimmed();

    static const auto& kPromoteSuffix = *[]() {
        static const QRegularExpression r(QStringLiteral("(歩|香|桂|銀|角|飛)成$"));
        return &r;
    }();
    return kPromoteSuffix.match(head).hasMatch();
}

bool convertMoveLine(const QString& moveText, QString& usi,
                     int& prevToFile, int& prevToRank)
{
    if (moveText == QStringLiteral("パス")) {
        usi = QStringLiteral("pass");
        return true;
    }

    static const auto& kMods = *[]() {
        static const QRegularExpression r(QStringLiteral("[右左上下引寄直行]+"));
        return &r;
    }();
    QString line = moveText;
    line.remove(kMods);
    line = line.trimmed();

    int toF = 0, toR = 0; bool same = false;
    if (!findDestination(line, toF, toR, same)) return false;
    if (same) { toF = prevToFile; toR = prevToRank; }
    if (!(toF >= 1 && toF <= 9 && toR >= 1 && toR <= 9)) return false;

    const bool isDrop = line.contains(QStringLiteral("打"));

    int fromF = 0, fromR = 0;
    {
        static const auto& kParenAscii = *[]() {
            static const QRegularExpression r(QStringLiteral("\\(([0-9０-９])([0-9０-９])\\)"));
            return &r;
        }();
        static const auto& kParenZenkaku = *[]() {
            static const QRegularExpression r(QStringLiteral("（([0-9０-９])([0-9０-９])）"));
            return &r;
        }();
        QRegularExpressionMatch m = kParenAscii.match(line);
        if (!m.hasMatch()) m = kParenZenkaku.match(line);
        if (m.hasMatch() && m.lastCapturedIndex() >= 2) {
            const QChar a = m.capturedView(1).at(0);
            const QChar b = m.capturedView(2).at(0);
            fromF = KifuParseCommon::flexDigitToIntNoDetach(a);
            fromR = KifuParseCommon::flexDigitToIntNoDetach(b);
        }
    }

    if (isDrop) {
        const QChar usiPiece = pieceKanjiToUsiUpper(line);
        if (usiPiece.isNull()) return false;
        usi = NotationUtils::formatSfenDrop(usiPiece, toF, toR);
    } else {
        if (!(fromF >= 1 && fromF <= 9 && fromR >= 1 && fromR <= 9)) return false;
        usi = NotationUtils::formatSfenMove(fromF, fromR, toF, toR, isPromotionMoveText(line));
    }

    prevToFile = toF;
    prevToRank = toR;
    return true;
}

// === BOD盤面解析 ===

void parseBodHandsLine(const QString& line, QMap<Piece, int>& outCounts, bool isBlack)
{
    static const QString prefixB = QStringLiteral("先手の持駒");
    static const QString prefixW = QStringLiteral("後手の持駒");
    QString t = line.trimmed();
    if (!t.startsWith(prefixB) && !t.startsWith(prefixW)) return;

    const bool sideBlack = t.startsWith(prefixB);
    if (sideBlack != isBlack) return;

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
        Piece base; bool promoted = false;
        if (!KifuParseCommon::mapKanjiPiece(QString(head), base, promoted)) continue;
        if (base == Piece::BlackKing) continue;

        int cnt = parseCountSuffixFlexible(tok);
        outCounts[base] += cnt;
    }
}

QString buildHandsSfen(const QMap<Piece, int>& black, const QMap<Piece, int>& white)
{
    auto emitSide = [](const QMap<Piece, int>& m, bool gote) -> QString {
        const Piece order[] = {Piece::BlackRook, Piece::BlackBishop, Piece::BlackGold,
                               Piece::BlackSilver, Piece::BlackKnight, Piece::BlackLance, Piece::BlackPawn};
        QString s;
        for (Piece p : order) {
            int cnt = m.value(p, 0);
            if (cnt <= 0) continue;
            if (cnt > 1) s += QString::number(cnt);
            s += pieceToChar(gote ? toWhite(p) : p);
        }
        return s;
    };
    QString s = emitSide(black, false) + emitSide(white, true);
    if (s.isEmpty()) s = QStringLiteral("-");
    return s;
}

bool collectBodRows(const QStringList& lines, QMap<QChar, QString>& rowByRank,
                    QString* detectedLabel)
{
    static const QChar ranks_1to9[9] = {u'一',u'二',u'三',u'四',u'五',u'六',u'七',u'八',u'九'};

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
                rowByRank[ranks_1to9[currentRankIdx]] = rowTokensToSfen(toks);
                currentRankIdx++;
            }
        }
        if (detectedLabel) *detectedLabel = QStringLiteral("配置");
    } else {
        for (const QString& raw : std::as_const(lines)) {
            QStringList toks; QChar rank;
            if (parseBodRow(raw, toks, rank, true)) {
                rowByRank[rank] = rowTokensToSfen(toks);
            }
        }
        if (detectedLabel && !rowByRank.isEmpty()) *detectedLabel = QStringLiteral("BOD");
    }

    return (rowByRank.size() == 9);
}

void parseBodTurnAndMoveNumber(const QStringList& lines, QChar& turn, int& moveNumber)
{
    turn = QLatin1Char('b');
    for (const QString& l : std::as_const(lines)) {
        const QString t = l.trimmed();
        if (t.contains(QStringLiteral("先手番"))) { turn = QLatin1Char('b'); break; }
        if (t.contains(QStringLiteral("後手番"))) { turn = QLatin1Char('w'); break; }
    }

    moveNumber = 1;
    static const QRegularExpression reTeSu(QStringLiteral("手数\\s*[=＝]\\s*([0-9０-９]+)"));
    for (const QString& l : std::as_const(lines)) {
        auto mt = reTeSu.match(l);
        if (mt.hasMatch()) {
            moveNumber = zenk2ascii(mt.captured(1)).toInt() + 1;
            break;
        }
    }
}

} // namespace KifLexer
