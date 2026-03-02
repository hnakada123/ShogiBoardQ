/// @file ki2tosfenconverter.cpp
/// @brief KI2形式棋譜コンバータクラスの実装

#include "ki2tosfenconverter.h"
#include "ki2lexer.h"
#include "kiftosfenconverter.h"
#include "kifreader.h"
#include "parsecommon.h"
#include "notationutils.h"

#include <QRegularExpression>
#include "logcategories.h"

namespace {

void parseSfenBoardField(const QString& boardStr, QString boardState[9][9])
{
    const QStringList ranks = boardStr.split(QLatin1Char('/'));
    for (qsizetype r = 0; r < qMin(qsizetype(9), ranks.size()); ++r) {
        const QString& row = ranks[r];
        int f = 0;
        for (qsizetype i = 0; i < row.size() && f < 9; ++i) {
            const QChar ch = row.at(i);
            if (ch.isDigit()) {
                int n = ch.digitValue();
                while (n-- > 0 && f < 9) { boardState[r][f++].clear(); }
            } else if (ch == QLatin1Char('+')) {
                if (i + 1 < row.size()) {
                    const QChar p = row.at(++i);
                    boardState[r][f++] = QString(QLatin1Char('+')) + p;
                }
            } else {
                boardState[r][f++] = QString(ch);
            }
        }
    }
}

void parseSfenHandsField(const QString& handsStr,
                          QMap<Piece, int>& blackHands, QMap<Piece, int>& whiteHands)
{
    if (handsStr == QStringLiteral("-")) return;
    int num = 0;
    for (qsizetype i = 0; i < handsStr.size(); ++i) {
        const QChar ch = handsStr.at(i);
        if (ch.isDigit()) {
            num = num * 10 + ch.digitValue();
        } else {
            const bool black = ch.isUpper();
            const int n = (num > 0) ? num : 1;
            num = 0;
            const Piece basePiece = toBlack(charToPiece(ch));
            if (black) blackHands[basePiece] += n;
            else       whiteHands[basePiece] += n;
        }
    }
}

bool parseBoardToken(const QString& token, Piece& pieceUpper, bool& isPromoted, bool& isBlack)
{
    if (token.isEmpty()) return false;
    isPromoted = (token.startsWith(QLatin1Char('+')));
    const QChar ch = isPromoted ? token.at(1) : token.at(0);
    isBlack = ch.isUpper();
    pieceUpper = toBlack(charToPiece(ch));
    return true;
}

void applyDropToBoard(const QString& usi, QString boardState[9][9],
                      QMap<Piece, int>& hands, bool blackToMove)
{
    const qsizetype star = usi.indexOf('*');
    if (star != 1 || usi.size() < 4) return;
    const Piece up = toBlack(charToPiece(usi.at(0)));
    const int file = usi.at(2).toLatin1() - '0';
    const int rankIdx = usi.at(3).toLatin1() - 'a';
    if (file < 1 || file > 9 || rankIdx < 0 || rankIdx > 8) return;
    const int colIdx = 9 - file;
    if (hands.value(up, 0) > 0) hands[up]--;
    const QChar pieceChar = pieceToChar(blackToMove ? up : toWhite(up));
    boardState[rankIdx][colIdx] = QString(pieceChar);
}

void applyMoveOnBoard(const QString& usi, QString boardState[9][9],
                      QMap<Piece, int>& capHands, bool blackToMove)
{
    if (usi.size() < 4) return;
    const int fileFrom = usi.at(0).toLatin1() - '0';
    const int rankFromIdx = usi.at(1).toLatin1() - 'a';
    const int fileTo = usi.at(2).toLatin1() - '0';
    const int rankToIdx = usi.at(3).toLatin1() - 'a';
    const bool promote = (usi.size() >= 5 && usi.at(4) == QLatin1Char('+'));

    if (fileFrom < 1 || fileFrom > 9 || rankFromIdx < 0 || rankFromIdx > 8) return;
    if (fileTo < 1 || fileTo > 9 || rankToIdx < 0 || rankToIdx > 8) return;

    const int colFrom = 9 - fileFrom;
    const int colTo = 9 - fileTo;
    const QString fromToken = boardState[rankFromIdx][colFrom];
    if (fromToken.isEmpty()) return;

    const QString toToken = boardState[rankToIdx][colTo];
    if (!toToken.isEmpty()) {
        Piece capPiece = Piece::None;
        bool capPromoted, capBlack;
        if (parseBoardToken(toToken, capPiece, capPromoted, capBlack)) {
            capHands[capPiece]++;
        }
    }

    Piece movingPiece = Piece::None;
    bool wasPromoted, isBlack;
    if (parseBoardToken(fromToken, movingPiece, wasPromoted, isBlack)) {
        const bool nowPromoted = promote || wasPromoted;
        const QChar outChar = pieceToChar(blackToMove ? movingPiece : toWhite(movingPiece));
        if (nowPromoted) {
            boardState[rankToIdx][colTo] = QString(QLatin1Char('+')) + outChar;
        } else {
            boardState[rankToIdx][colTo] = QString(outChar);
        }
    }
    boardState[rankFromIdx][colFrom].clear();
}

// KI2の指し手テキストとUSI結果からKIF形式 prettyMove を構築
QString buildKi2PrettyMove(const QString& move, const QString& usi, const QString& teban)
{
    if (usi.isEmpty()) {
        QString prettyMove = move;
        if (!prettyMove.startsWith(QChar(u'▲')) && !prettyMove.startsWith(QChar(u'△')) &&
            !prettyMove.startsWith(QChar(u'▽')) &&
            !prettyMove.startsWith(QChar(u'☗')) && !prettyMove.startsWith(QChar(u'☖'))) {
            prettyMove = teban + prettyMove;
        }
        return prettyMove;
    }

    static const QRegularExpression tebanMarkRe(QStringLiteral("[▲△▽☗☖]"));
    QString moveBody = move;
    moveBody.remove(tebanMarkRe);
    moveBody = moveBody.trimmed();

    if (usi.contains(QLatin1Char('*'))) {
        return moveBody.contains(QChar(u'打'))
            ? (teban + moveBody)
            : (teban + moveBody + QStringLiteral("打"));
    }

    if (usi.size() >= 4) {
        const int fromFile = usi.at(0).toLatin1() - '0';
        const int fromRank = usi.at(1).toLatin1() - 'a' + 1;
        return teban + moveBody + QStringLiteral("(%1%2)").arg(fromFile).arg(fromRank);
    }
    return teban + moveBody;
}

// 終局語のKI2 DisplayItemを生成
void handleKi2TerminalWord(const QString& term, int& moveIndex,
                           QString& commentBuf, QList<KifDisplayItem>& out,
                           bool& firstMoveFound, bool& gameEnded,
                           const QString& openingCommentBuf,
                           const QString& openingBookmarkBuf)
{
    if (!firstMoveFound) {
        firstMoveFound = true;
        out.push_back(KifuParseCommon::createOpeningDisplayItem(openingCommentBuf, openingBookmarkBuf));
    }
    KifuParseCommon::flushCommentToLastItem(commentBuf, out);

    ++moveIndex;
    out.push_back(KifuParseCommon::createTerminalDisplayItem(moveIndex, term));
    gameEnded = true;
}

} // anonymous namespace

QString Ki2ToSfenConverter::mapHandicapToSfen(const QString& label)
{
    return NotationUtils::mapHandicapToSfen(label);
}

QString Ki2ToSfenConverter::detectInitialSfenFromFile(const QString& ki2Path, QString* detectedLabel)
{
    return KifToSfenConverter::detectInitialSfenFromFile(ki2Path, detectedLabel);
}

void Ki2ToSfenConverter::initBoardFromSfen(const QString& sfen,
                                            QString boardState[9][9],
                                            QMap<Piece, int>& blackHands,
                                            QMap<Piece, int>& whiteHands)
{
    for (int r = 0; r < 9; ++r)
        for (int f = 0; f < 9; ++f)
            boardState[r][f].clear();
    blackHands.clear();
    whiteHands.clear();

    const QStringList parts = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) return;
    parseSfenBoardField(parts[0], boardState);
    if (parts.size() >= 3) parseSfenHandsField(parts[2], blackHands, whiteHands);
}

void Ki2ToSfenConverter::applyMoveToBoard(const QString& usi,
                                           QString boardState[9][9],
                                           QMap<Piece, int>& blackHands,
                                           QMap<Piece, int>& whiteHands,
                                           bool blackToMove)
{
    if (usi.isEmpty()) return;
    if (usi.contains(QLatin1Char('*'))) {
        QMap<Piece, int>& hands = blackToMove ? blackHands : whiteHands;
        applyDropToBoard(usi, boardState, hands, blackToMove);
    } else {
        QMap<Piece, int>& hands = blackToMove ? blackHands : whiteHands;
        applyMoveOnBoard(usi, boardState, hands, blackToMove);
    }
}

QStringList Ki2ToSfenConverter::convertFile(const QString& ki2Path, QString* errorMessage)
{
    QStringList out;
    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(ki2Path, lines, &usedEnc, errorMessage)) return out;

    qCDebug(lcKifu).noquote() << QStringLiteral("convertFile: encoding = %1 , lines = %2")
                                    .arg(usedEnc).arg(lines.size());

    const QString initialSfen = detectInitialSfenFromFile(ki2Path);
    QString boardState[9][9];
    QMap<Piece, int> blackHands, whiteHands;
    initBoardFromSfen(initialSfen, boardState, blackHands, whiteHands);

    bool blackToMove = true;
    {
        const QStringList parts = initialSfen.split(QLatin1Char(' '));
        if (parts.size() >= 2) blackToMove = (parts[1] == QStringLiteral("b"));
    }

    int prevToFile = 0, prevToRank = 0;
    bool gameEnded = false;

    for (const QString& raw : std::as_const(lines)) {
        const QString lineStr = raw.trimmed();
        if (gameEnded) break;
        if (KifuParseCommon::isKifCommentLine(lineStr) || KifuParseCommon::isBookmarkLine(lineStr)) continue;
        if (Ki2Lexer::isResultLine(lineStr)) break;
        if (lineStr.isEmpty() || KifuParseCommon::isKifSkippableHeaderLine(lineStr)
            || KifuParseCommon::isBoardHeaderOrFrame(lineStr)) continue;
        if (!Ki2Lexer::isKi2MoveLine(lineStr)) continue;
        if (KifuParseCommon::containsAnyTerminal(lineStr)) break;

        const QStringList moves = Ki2Lexer::extractMovesFromLine(lineStr);
        for (const QString& move : moves) {
            QString term;
            if (KifuParseCommon::isTerminalWordContains(move, &term)) {
                gameEnded = true;
                break;
            }
            const QString usi = Ki2Lexer::convertKi2MoveToUsi(move, boardState, blackHands, whiteHands,
                                                               blackToMove, prevToFile, prevToRank);
            if (!usi.isEmpty()) {
                out << usi;
                qCDebug(lcKifu).noquote() << QStringLiteral("USI [%1] %2").arg(out.size()).arg(usi);
                applyMoveToBoard(usi, boardState, blackHands, whiteHands, blackToMove);
                blackToMove = !blackToMove;
            } else {
                if (errorMessage) *errorMessage += QStringLiteral("[skip ?] %1\n").arg(move);
            }
        }
    }

    qCDebug(lcKifu).noquote() << QStringLiteral("convertFile: moves = %1").arg(out.size());
    return out;
}

QList<KifDisplayItem> Ki2ToSfenConverter::extractMovesWithTimes(const QString& ki2Path,
                                                                 QString* errorMessage)
{
    QList<KifDisplayItem> out;
    QString usedEnc;
    QStringList lines;
    if (!KifReader::readLinesAuto(ki2Path, lines, &usedEnc, errorMessage)) return out;

    const QString initialSfen = detectInitialSfenFromFile(ki2Path);
    QString boardState[9][9];
    QMap<Piece, int> blackHands, whiteHands;
    initBoardFromSfen(initialSfen, boardState, blackHands, whiteHands);

    bool blackToMove = true;
    {
        const QStringList parts = initialSfen.split(QLatin1Char(' '));
        if (parts.size() >= 2) blackToMove = (parts[1] == QStringLiteral("b"));
    }

    QString openingCommentBuf, openingBookmarkBuf, commentBuf;
    int moveIndex = blackToMove ? 0 : 1;
    int prevToFile = 0, prevToRank = 0;
    bool gameEnded = false, firstMoveFound = false;

    for (const QString& raw : std::as_const(lines)) {
        const QString lineStr = raw.trimmed();
        if (gameEnded) break;

        if (KifuParseCommon::tryHandleCommentLine(lineStr, firstMoveFound, commentBuf, openingCommentBuf))
            continue;
        if (KifuParseCommon::tryHandleBookmarkLine(lineStr, firstMoveFound, out, openingBookmarkBuf))
            continue;

        if (Ki2Lexer::isResultLine(lineStr)) {
            QString terminalWord;
            int resultMoveCount = 0;
            if (Ki2Lexer::parseResultLine(lineStr, terminalWord, resultMoveCount)) {
                handleKi2TerminalWord(terminalWord, moveIndex, commentBuf, out,
                                       firstMoveFound, gameEnded, openingCommentBuf, openingBookmarkBuf);
            }
            continue;
        }

        if (lineStr.isEmpty() || KifuParseCommon::isKifSkippableHeaderLine(lineStr)
            || KifuParseCommon::isBoardHeaderOrFrame(lineStr)) continue;
        if (!Ki2Lexer::isKi2MoveLine(lineStr)) continue;

        const QStringList moves = Ki2Lexer::extractMovesFromLine(lineStr);
        for (const QString& move : moves) {
            QString term;
            if (KifuParseCommon::isTerminalWordContains(move, &term)) {
                handleKi2TerminalWord(term, moveIndex, commentBuf, out,
                                       firstMoveFound, gameEnded, openingCommentBuf, openingBookmarkBuf);
                break;
            }

            if (!firstMoveFound) {
                firstMoveFound = true;
                out.push_back(KifuParseCommon::createOpeningDisplayItem(openingCommentBuf, openingBookmarkBuf));
            }
            KifuParseCommon::flushCommentToLastItem(commentBuf, out);

            const QString usi = Ki2Lexer::convertKi2MoveToUsi(move, boardState, blackHands, whiteHands,
                                                               blackToMove, prevToFile, prevToRank);
            ++moveIndex;
            const QString teban = KifuParseCommon::tebanMark(moveIndex);
            const QString prettyMove = buildKi2PrettyMove(move, usi, teban);
            out.push_back(KifuParseCommon::createMoveDisplayItem(moveIndex, prettyMove));

            if (!usi.isEmpty())
                applyMoveToBoard(usi, boardState, blackHands, whiteHands, blackToMove);
            blackToMove = !blackToMove;
        }
    }

    KifuParseCommon::finalizeDisplayItems(commentBuf, out, openingCommentBuf, openingBookmarkBuf);

    return out;
}

bool Ki2ToSfenConverter::parseWithVariations(const QString& ki2Path,
                                              KifParseResult& out,
                                              QString* errorMessage)
{
    out = KifParseResult{};
    QString teai;
    out.mainline.baseSfen = detectInitialSfenFromFile(ki2Path, &teai);
    out.mainline.disp = extractMovesWithTimes(ki2Path, errorMessage);
    out.mainline.usiMoves = convertFile(ki2Path, errorMessage);
    return true;
}

QList<KifGameInfoItem> Ki2ToSfenConverter::extractGameInfo(const QString& filePath)
{
    if (filePath.isEmpty()) return {};

    QString usedEnc, warn;
    QStringList lines;
    if (!KifReader::readLinesAuto(filePath, lines, &usedEnc, &warn)) {
        qCWarning(lcKifu).noquote() << "read failed:" << filePath << "warn:" << warn;
        return {};
    }

    return KifuParseCommon::extractHeaderGameInfo(lines, [](const QString& t) {
        return Ki2Lexer::isKi2MoveLine(t);
    });
}

QMap<QString, QString> Ki2ToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    return KifuParseCommon::toGameInfoMap(extractGameInfo(filePath));
}

bool Ki2ToSfenConverter::buildInitialSfenFromBod(const QStringList& lines,
                                                  QString& outSfen,
                                                  QString* detectedLabel,
                                                  QString* warn)
{
    return KifToSfenConverter::buildInitialSfenFromBod(lines, outSfen, detectedLabel, warn);
}

QString Ki2ToSfenConverter::generateModifier(
    const QList<Ki2Lexer::Candidate>& candidates,
    int srcFile, int srcRank,
    int dstFile, int dstRank,
    bool blackToMove)
{
    if (candidates.size() <= 1) return {};

    const int forward = blackToMove ? -1 : 1;
    QStringList singleModifiers;

    {
        bool hasDifferentFile = false;
        for (const auto& c : std::as_const(candidates)) {
            if (c.file != srcFile || c.rank != srcRank) {
                if (c.file != srcFile) { hasDifferentFile = true; break; }
            }
        }
        if (hasDifferentFile)
            singleModifiers << QStringLiteral("右") << QStringLiteral("左");
    }

    const int dr = dstRank - srcRank;
    if (dr * forward > 0) {
        singleModifiers << QStringLiteral("上");
        if (srcFile == dstFile) singleModifiers << QStringLiteral("直");
    } else if (dr * forward < 0) {
        singleModifiers << QStringLiteral("引");
    } else {
        singleModifiers << QStringLiteral("寄");
    }

    for (const QString& mod : std::as_const(singleModifiers)) {
        const auto filtered = Ki2Lexer::filterByDirection(candidates, mod, blackToMove, dstFile, dstRank);
        if (filtered.size() == 1 && filtered[0].file == srcFile && filtered[0].rank == srcRank)
            return mod;
    }

    static const QStringList combos = {
        QStringLiteral("右上"), QStringLiteral("右引"),
        QStringLiteral("左上"), QStringLiteral("左引"),
        QStringLiteral("右寄"), QStringLiteral("左寄"),
    };
    for (const QString& combo : combos) {
        const auto filtered = Ki2Lexer::filterByDirection(candidates, combo, blackToMove, dstFile, dstRank);
        if (filtered.size() == 1 && filtered[0].file == srcFile && filtered[0].rank == srcRank)
            return combo;
    }
    return {};
}

QString Ki2ToSfenConverter::convertPrettyMoveToKi2(
    const QString& prettyMove,
    QString boardState[9][9],
    QMap<Piece, int>& blackHands,
    QMap<Piece, int>& whiteHands,
    bool blackToMove,
    int& prevToFile, int& prevToRank)
{
    static const QRegularExpression fromPosRe(QStringLiteral("\\(([0-9])([0-9])\\)$"));
    const QRegularExpressionMatch fromMatch = fromPosRe.match(prettyMove);

    int srcFile = 0, srcRank = 0;
    bool hasSource = false;
    if (fromMatch.hasMatch()) {
        srcFile = fromMatch.captured(1).at(0).toLatin1() - '0';
        srcRank = fromMatch.captured(2).at(0).toLatin1() - '0';
        hasSource = true;
    }

    int dstFile = 0, dstRank = 0;
    bool isSame = false;
    if (!Ki2Lexer::findDestination(prettyMove, dstFile, dstRank, isSame)) {
        QString result = prettyMove;
        result.remove(fromPosRe);
        return result;
    }
    if (isSame) { dstFile = prevToFile; dstRank = prevToRank; }

    Piece pieceUpper = Piece::None;
    bool isPromoted = false;
    if (!KifuParseCommon::mapKanjiPiece(prettyMove, pieceUpper, isPromoted)) {
        QString result = prettyMove;
        result.remove(fromPosRe);
        return result;
    }

    QString ki2Move = prettyMove;
    ki2Move.remove(fromPosRe);

    bool isDrop = prettyMove.contains(QChar(u'打'));
    if (!isDrop && !hasSource) isDrop = true;

    if (!isDrop && hasSource) {
        const auto candidates = Ki2Lexer::collectCandidates(pieceUpper, isPromoted,
                                                             dstFile, dstRank,
                                                             blackToMove, boardState);
        if (candidates.size() >= 2) {
            const QString modifier = generateModifier(candidates, srcFile, srcRank,
                                                       dstFile, dstRank, blackToMove);
            if (!modifier.isEmpty()) {
                static const QRegularExpression promoteSuffixRe(QStringLiteral("(成|不成)$"));
                const QRegularExpressionMatch pm = promoteSuffixRe.match(ki2Move);
                if (pm.hasMatch()) {
                    const qsizetype pos = pm.capturedStart(1);
                    ki2Move.insert(pos, modifier);
                } else {
                    ki2Move.append(modifier);
                }
            }
        }
    }

    if (isDrop) {
        const QString usi = NotationUtils::formatSfenDrop(pieceToChar(pieceUpper), dstFile, dstRank);
        applyMoveToBoard(usi, boardState, blackHands, whiteHands, blackToMove);
    } else if (hasSource) {
        const QString usi = NotationUtils::formatSfenMove(srcFile, srcRank, dstFile, dstRank,
                                                           Ki2Lexer::isPromotionMoveText(prettyMove));
        applyMoveToBoard(usi, boardState, blackHands, whiteHands, blackToMove);
    }

    prevToFile = dstFile;
    prevToRank = dstRank;
    return ki2Move;
}
