/// @file ki2lexer.cpp
/// @brief KI2形式固有の字句解析・曖昧指し手解決の実装

#include "ki2lexer.h"
#include "notationutils.h"
#include "parsecommon.h"

#include <QRegularExpression>
#include "logcategories.h"

namespace {

bool getPieceTypeAndPromoted(const QString& moveText, Piece& pieceUpper, bool& isPromoted)
{
    isPromoted = false;
    pieceUpper = Piece::None;
    Piece base = Piece::None;
    if (!KifuParseCommon::mapKanjiPiece(moveText, base, isPromoted)) return false;
    pieceUpper = base;
    return true;
}

bool isGoldLikeMove(int df, int dr, int forward)
{
    if (qAbs(df) > 1 || qAbs(dr) > 1) return false;
    if (df == 0 && dr == -forward) return false;
    if (qAbs(df) == 1 && dr == -forward) return false;
    return true;
}

bool isPathClear(int fromFile, int fromRank, int toFile, int toRank,
                 const QString boardState[9][9])
{
    const int df = (toFile > fromFile) ? 1 : (toFile < fromFile) ? -1 : 0;
    const int dr = (toRank > fromRank) ? 1 : (toRank < fromRank) ? -1 : 0;
    int f = fromFile + df;
    int r = fromRank + dr;
    while (f != toFile || r != toRank) {
        const int col = 9 - f;
        const int row = r - 1;
        if (col < 0 || col > 8 || row < 0 || row > 8) break;
        if (!boardState[row][col].isEmpty()) return false;
        f += df;
        r += dr;
    }
    return true;
}

} // namespace

namespace Ki2Lexer {

// === 正規表現アクセサ ===

const QRegularExpression& resultPatternCaptureRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*まで([0-9０-９]+)手"));
        return &r;
    }();
    return re;
}

const QRegularExpression& resultPatternRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^\\s*まで[0-9０-９]+手"));
        return &r;
    }();
    return re;
}

const QRegularExpression& afterColonRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("^.*[:：]"));
        return &r;
    }();
    return re;
}

// === 行の種類判定 ===

bool isKi2MoveLine(const QString& line)
{
    const QString t = line.trimmed();
    if (t.isEmpty()) return false;
    const QChar first = t.at(0);
    return (first == QChar(u'▲') || first == QChar(u'△') || first == QChar(u'▽') ||
            first == QChar(u'☗') || first == QChar(u'☖'));
}

bool isResultLine(const QString& line)
{
    return resultPatternRe().match(line).hasMatch();
}

// === 結果行解析 ===

bool parseResultLine(const QString& line, QString& terminalWord, int& moveCount)
{
    const QRegularExpressionMatch m = resultPatternCaptureRe().match(line);
    if (!m.hasMatch()) return false;
    moveCount = KifuParseCommon::flexDigitsToIntNoDetach(m.captured(1));

    static const std::pair<QString, QString> table[] = {
        {QStringLiteral("千日手"), QStringLiteral("千日手")},
        {QStringLiteral("持将棋"), QStringLiteral("持将棋")},
        {QStringLiteral("中断"), QStringLiteral("中断")},
        {QStringLiteral("切れ負け"), QStringLiteral("切れ負け")},
        {QStringLiteral("反則勝ち"), QStringLiteral("反則勝ち")},
        {QStringLiteral("反則負け"), QStringLiteral("反則負け")},
        {QStringLiteral("入玉勝ち"), QStringLiteral("入玉勝ち")},
        {QStringLiteral("不戦勝"), QStringLiteral("不戦勝")},
        {QStringLiteral("不戦敗"), QStringLiteral("不戦敗")},
        {QStringLiteral("不詰"), QStringLiteral("不詰")},
        {QStringLiteral("詰み"), QStringLiteral("詰み")},
        {QStringLiteral("詰"), QStringLiteral("詰み")},
        {QStringLiteral("勝ち"), QStringLiteral("投了")},
        {QStringLiteral("勝"), QStringLiteral("投了")},
    };
    for (const auto& [search, result] : table) {
        if (line.contains(search)) {
            terminalWord = result;
            return true;
        }
    }
    return false;
}

// === 指し手テキスト解析 ===

bool findDestination(const QString& moveText, int& toFile, int& toRank, bool& isSameAsPrev)
{
    isSameAsPrev = false;

    if (moveText.contains(QStringLiteral("同"))) {
        toFile = toRank = 0;
        isSameAsPrev = true;
        return true;
    }

    QString line = moveText;
    line.remove(QChar(u'▲'));
    line.remove(QChar(u'△'));
    line.remove(QChar(u'▽'));
    line.remove(QChar(u'☗'));
    line.remove(QChar(u'☖'));

    static const QRegularExpression s_digitKanji(QStringLiteral("([1-9１-９])([一二三四五六七八九])"));
    static const QRegularExpression s_digitDigit(QStringLiteral("([1-9１-９])([1-9１-９])"));

    QRegularExpressionMatch m = s_digitKanji.match(line);
    if (!m.hasMatch()) m = s_digitDigit.match(line);
    if (!m.hasMatch()) return false;

    const QChar fch = m.capturedView(1).at(0);
    const QChar rch = m.capturedView(2).at(0);

    toFile = KifuParseCommon::flexDigitToIntNoDetach(fch);
    int r  = NotationUtils::kanjiDigitToInt(rch);
    if (r == 0) r = KifuParseCommon::flexDigitToIntNoDetach(rch);
    toRank = r;

    return (toFile >= 1 && toFile <= 9 && toRank >= 1 && toRank <= 9);
}

Piece pieceKanjiToUsiUpper(const QString& s)
{
    Piece base = Piece::None; bool promoted = false;
    if (!KifuParseCommon::mapKanjiPiece(s, base, promoted)) return Piece::None;
    return base;
}

bool isPromotionMoveText(const QString& moveText)
{
    if (moveText.contains(QStringLiteral("不成"))) return false;

    QString head = moveText;
    qsizetype u = head.indexOf(QChar(u'打'));
    if (u >= 0) head = head.left(u);

    static const QRegularExpression kDirectionRe(QStringLiteral("[右左上下引寄直行]+"));
    head.remove(kDirectionRe);
    head.remove(QChar(u'▲'));
    head.remove(QChar(u'△'));
    head.remove(QChar(u'▽'));
    head.remove(QChar(u'☗'));
    head.remove(QChar(u'☖'));
    head = head.trimmed();

    static const QRegularExpression kPromoteSuffix(QStringLiteral("(歩|香|桂|銀|角|飛)成$"));
    return kPromoteSuffix.match(head).hasMatch();
}

QString extractMoveModifier(const QString& moveText)
{
    QString result;
    static const QStringList modifiers = {
        QStringLiteral("右"), QStringLiteral("左"), QStringLiteral("上"),
        QStringLiteral("引"), QStringLiteral("寄"), QStringLiteral("直"),
        QStringLiteral("行")
    };
    for (const QString& mod : modifiers) {
        if (moveText.contains(mod)) result += mod;
    }
    return result;
}

QStringList extractMovesFromLine(const QString& line)
{
    QStringList moves;
    const QString t = line.trimmed();
    static const QRegularExpression moveRe(QStringLiteral("([▲△▽☗☖][^▲△▽☗☖]+)"));
    QRegularExpressionMatchIterator it = moveRe.globalMatch(t);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        QString move = m.captured(1).trimmed();
        qsizetype commentIdx = move.indexOf(QChar(u'*'));
        if (commentIdx < 0) commentIdx = move.indexOf(QChar(u'＊'));
        if (commentIdx >= 0) move = move.left(commentIdx).trimmed();
        if (!move.isEmpty()) moves << move;
    }
    return moves;
}

// === 駒の移動判定・候補収集 ===

bool canPieceMoveTo(Piece pieceUpper, bool isPromoted,
                    int fromFile, int fromRank,
                    int toFile, int toRank,
                    bool blackToMove)
{
    const int df = toFile - fromFile;
    const int dr = toRank - fromRank;
    const int forward = blackToMove ? -1 : 1;

    switch (pieceUpper) {
    case Piece::BlackPawn:
        return isPromoted ? isGoldLikeMove(df, dr, forward) : (df == 0 && dr == forward);
    case Piece::BlackLance:
        return isPromoted ? isGoldLikeMove(df, dr, forward) : (df == 0 && dr * forward > 0);
    case Piece::BlackKnight:
        return isPromoted ? isGoldLikeMove(df, dr, forward) : (qAbs(df) == 1 && dr == 2 * forward);
    case Piece::BlackSilver:
        if (isPromoted) return isGoldLikeMove(df, dr, forward);
        if (qAbs(df) <= 1 && qAbs(dr) <= 1) {
            if (df == 0 && dr == -forward) return false;
            if (df == 0 && dr == 0) return false;
            if (qAbs(df) == 1 && dr == 0) return false;
            return true;
        }
        return false;
    case Piece::BlackGold:
        return isGoldLikeMove(df, dr, forward);
    case Piece::BlackBishop:
        if (isPromoted) {
            if (qAbs(df) == qAbs(dr) && df != 0) return true;
            return (qAbs(df) <= 1 && qAbs(dr) <= 1 && (df != 0 || dr != 0));
        }
        return (qAbs(df) == qAbs(dr) && df != 0);
    case Piece::BlackRook:
        if (isPromoted) {
            if ((df == 0 && dr != 0) || (df != 0 && dr == 0)) return true;
            return (qAbs(df) <= 1 && qAbs(dr) <= 1 && qAbs(df) == qAbs(dr) && df != 0);
        }
        return ((df == 0 && dr != 0) || (df != 0 && dr == 0));
    case Piece::BlackKing:
        return (qAbs(df) <= 1 && qAbs(dr) <= 1 && (df != 0 || dr != 0));
    default:
        return false;
    }
}

QVector<Candidate> collectCandidates(Piece pieceUpper, bool moveIsPromoted,
                                     int toFile, int toRank,
                                     bool blackToMove,
                                     const QString boardState[9][9])
{
    QVector<Candidate> candidates;
    for (int r = 0; r < 9; ++r) {
        for (int f = 0; f < 9; ++f) {
            const QString& token = boardState[r][f];
            if (token.isEmpty()) continue;

            // parseToken inline
            bool tokenPromoted = token.startsWith(QLatin1Char('+'));
            const QChar ch = tokenPromoted ? token.at(1) : token.at(0);
            bool tokenBlack = ch.isUpper();
            Piece tokenPiece = toBlack(charToPiece(ch));

            if (tokenBlack != blackToMove) continue;
            if (tokenPiece != pieceUpper) continue;
            if (tokenPromoted != moveIsPromoted) continue;

            const int candFile = 9 - f;
            const int candRank = r + 1;

            if (!canPieceMoveTo(pieceUpper, tokenPromoted, candFile, candRank,
                                toFile, toRank, blackToMove))
                continue;

            if (pieceUpper == Piece::BlackRook || pieceUpper == Piece::BlackBishop ||
                pieceUpper == Piece::BlackLance) {
                if (!isPathClear(candFile, candRank, toFile, toRank, boardState))
                    continue;
            }
            candidates.push_back({candFile, candRank});
        }
    }
    return candidates;
}

QVector<Candidate> filterByDirection(const QVector<Candidate>& candidates,
                                     const QString& modifier,
                                     bool blackToMove,
                                     int toFile, int toRank)
{
    if (modifier.isEmpty()) return candidates;
    const int forward = blackToMove ? -1 : 1;
    QVector<Candidate> filtered = candidates;

    if (modifier.contains(QChar(u'右'))) {
        int targetFile = blackToMove ? 9 : 0;
        for (const auto& c : std::as_const(filtered)) {
            if (blackToMove) { if (targetFile == 9 || c.file < targetFile) targetFile = c.file; }
            else { if (targetFile == 0 || c.file > targetFile) targetFile = c.file; }
        }
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered))
            if (c.file == targetFile) tmp.push_back(c);
        if (!tmp.isEmpty()) filtered = tmp;
    }

    if (modifier.contains(QChar(u'左'))) {
        int targetFile = blackToMove ? 0 : 9;
        for (const auto& c : std::as_const(filtered)) {
            if (blackToMove) { if (targetFile == 0 || c.file > targetFile) targetFile = c.file; }
            else { if (targetFile == 9 || c.file < targetFile) targetFile = c.file; }
        }
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered))
            if (c.file == targetFile) tmp.push_back(c);
        if (!tmp.isEmpty()) filtered = tmp;
    }

    if (modifier.contains(QChar(u'上')) || modifier.contains(QChar(u'行'))) {
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered))
            if ((toRank - c.rank) * forward > 0) tmp.push_back(c);
        if (!tmp.isEmpty()) filtered = tmp;
    }
    if (modifier.contains(QChar(u'引'))) {
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered))
            if ((toRank - c.rank) * forward < 0) tmp.push_back(c);
        if (!tmp.isEmpty()) filtered = tmp;
    }
    if (modifier.contains(QChar(u'寄'))) {
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered))
            if (c.rank == toRank) tmp.push_back(c);
        if (!tmp.isEmpty()) filtered = tmp;
    }
    if (modifier.contains(QChar(u'直'))) {
        QVector<Candidate> tmp;
        for (const auto& c : std::as_const(filtered))
            if (c.file == toFile) tmp.push_back(c);
        if (!tmp.isEmpty()) filtered = tmp;
    }
    return filtered;
}

bool inferSourceSquare(Piece pieceUpper, bool moveIsPromoted,
                       int toFile, int toRank,
                       const QString& modifier, bool blackToMove,
                       const QString boardState[9][9],
                       int& outFromFile, int& outFromRank)
{
    const auto candidates = collectCandidates(pieceUpper, moveIsPromoted,
                                               toFile, toRank, blackToMove, boardState);
    if (candidates.isEmpty()) return false;

    if (candidates.size() == 1) {
        outFromFile = candidates[0].file;
        outFromRank = candidates[0].rank;
        return true;
    }

    const auto filtered = filterByDirection(candidates, modifier, blackToMove, toFile, toRank);
    outFromFile = filtered[0].file;
    outFromRank = filtered[0].rank;
    return true;
}

// === KI2→USI変換 ===

QString convertKi2MoveToUsi(const QString& moveText,
                            QString boardState[9][9],
                            QMap<Piece, int>& blackHands,
                            QMap<Piece, int>& whiteHands,
                            bool blackToMove,
                            int& prevToFile, int& prevToRank)
{
    if (moveText.contains(QStringLiteral("パス"))) return QStringLiteral("pass");

    QString term;
    if (KifuParseCommon::isTerminalWordContains(moveText, &term)) return {};

    int toFile = 0, toRank = 0;
    bool isSame = false;
    if (!findDestination(moveText, toFile, toRank, isSame)) return {};
    if (isSame) { toFile = prevToFile; toRank = prevToRank; }
    if (toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) return {};

    Piece pieceUpper = Piece::None;
    bool moveIsPromoted = false;
    if (!getPieceTypeAndPromoted(moveText, pieceUpper, moveIsPromoted)) return {};

    bool explicitDrop = moveText.contains(QChar(u'打'));
    const QString modifier = extractMoveModifier(moveText);
    int fromFile = 0, fromRank = 0;
    bool canMoveFromBoard = inferSourceSquare(pieceUpper, moveIsPromoted, toFile, toRank,
                                              modifier, blackToMove, boardState, fromFile, fromRank);

    QMap<Piece, int>& hands = blackToMove ? blackHands : whiteHands;
    bool hasInHand = !moveIsPromoted && (hands.value(pieceUpper, 0) > 0);
    bool isDrop = explicitDrop;
    if (!isDrop && !canMoveFromBoard && hasInHand) isDrop = true;

    QString usi;
    if (isDrop) {
        if (!hasInHand) {
            qCWarning(lcKifu) << "No piece in hand for drop:" << pieceToChar(pieceUpper) << "move:" << moveText;
            return {};
        }
        usi = NotationUtils::formatSfenDrop(pieceToChar(pieceUpper), toFile, toRank);
    } else {
        if (!canMoveFromBoard) {
            qCWarning(lcKifu) << "Cannot infer source square for:" << moveText;
            return {};
        }
        usi = NotationUtils::formatSfenMove(fromFile, fromRank, toFile, toRank, isPromotionMoveText(moveText));
    }

    prevToFile = toFile;
    prevToRank = toRank;
    return usi;
}

} // namespace Ki2Lexer
