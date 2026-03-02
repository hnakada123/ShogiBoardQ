/// @file kiflexer.cpp
/// @brief KIF形式固有の字句解析・トークン化の実装（BOD盤面解析は kiflexer_bod.cpp）

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

} // namespace KifLexer
