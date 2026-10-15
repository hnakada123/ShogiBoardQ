/// @file kiftosfenconverter.cpp
/// @brief KIF形式棋譜コンバータクラスの実装

#include "kiftosfenconverter.h"
#include "shoginotation.h"
#include "kiflexer.h"
#include "kifreader.h"
#include "parsecommon.h"
#include "notationutils.h"

#include <QRegularExpression>
#include "logcategories.h"
#include <QMap>

namespace {

QString turnMark(bool blackToMove)
{
    return blackToMove ? QStringLiteral("▲") : QStringLiteral("△");
}

bool readKifText(const QString& path, QString& text, QString* warn)
{
    QString usedEnc;
    return KifReader::readAllTextAuto(path, text, &usedEnc, warn);
}

/// KIF の1つの手順（本譜または1つの変化）を lines[begin, end) から読む。
/// line.disp の先頭は開始局面の項目（ply 0）。表示用の指し手と USI の指し手は同じ走査で作る。
/// 読めない指し手があれば、その手の前で手順を打ち切り warn に記録する。
bool parseKifBlock(const QStringList& lines, qsizetype begin, qsizetype end,
                   const QString& baseSfen, int startPly, const QString& prevUsi,
                   KifLine& outLine, QString* warn)
{
    using namespace KifuParseCommon;

    // 手順の1手目が「同」のときは、分岐元に至る直前の指し手の移動先を使う
    int prevToFile = 0, prevToRank = 0;
    KifLexer::usiDestination(prevUsi, prevToFile, prevToRank);
    bool blackToMove = !baseSfen.contains(QStringLiteral(" w "));
    int moveIndex = startPly - 1;
    bool firstMoveFound = false;
    bool ended = false;
    QString openingComment, openingBookmark, comment;
    KifuParseCommon::KifLineDraft line;
    line.baseSfen = baseSfen;
    line.startPly = startPly;

    for (qsizetype n = begin; n < end; ++n) {
        QString lineStr = lines.at(n).trimmed();

        if (tryHandleCommentLine(lineStr, firstMoveFound, comment, openingComment)) continue;
        if (tryHandleBookmarkLine(lineStr, firstMoveFound, line.disp, openingBookmark)) continue;
        if (ended || lineStr.isEmpty() || isKifSkippableHeaderLine(lineStr) || isBoardHeaderOrFrame(lineStr))
            continue;

        // 1行に複数の指し手が書かれていることがある
        while (!lineStr.isEmpty()) {
            const auto optDigits = KifLexer::startsWithMoveNumber(lineStr);
            if (!optDigits) break;
            int i = *optDigits;
            while (i < lineStr.size() && lineStr.at(i).isSpace()) ++i;
            if (i >= lineStr.size()) break;

            QString rest = lineStr.mid(i).trimmed();
            QRegularExpressionMatch tm;
            QString timeText;
            int nextMoveStartIdx = -1;
            QString inlineComment;
            KifLexer::stripTimeAndNextMove(rest, lineStr, i, tm, timeText, nextMoveStartIdx);
            KifLexer::stripInlineComment(rest, inlineComment);
            KifLexer::advanceToNextMove(lineStr, i, tm, nextMoveStartIdx);

            if (!firstMoveFound) {
                firstMoveFound = true;
                line.disp.push_back(createOpeningDisplayItem(openingComment, openingBookmark));
            }
            if (rest.isEmpty()) continue;

            if (const TerminalType terminal = TerminalTypes::fromWord(rest); terminal != TerminalType::None) {
                flushCommentToLastItem(comment, line.disp);
                line.disp.push_back(KifLexer::buildTerminalItem(++moveIndex, terminal,
                                                                blackToMove ? Turn::Black : Turn::White, timeText, tm));
                if (!inlineComment.isEmpty()) appendLine(comment, inlineComment);
                line.endsWithTerminal = true;
                ended = true;
                break;
            }

            QString usi;
            if (!KifLexer::convertMoveLine(rest, usi, prevToFile, prevToRank)) {
                if (warn) {
                    *warn += QStringLiteral("KIF: cannot parse move %1: %2\n")
                                 .arg(moveIndex + 1).arg(lines.at(n).trimmed());
                }
                ended = true;
                break;
            }
            flushCommentToLastItem(comment, line.disp);
            line.disp.push_back(createMoveDisplayItem(++moveIndex, turnMark(blackToMove) + rest, timeText));
            line.usiMoves << usi;
            if (!inlineComment.isEmpty()) appendLine(comment, inlineComment);
            blackToMove = !blackToMove;
        }
    }

    finalizeDisplayItems(comment, line.disp, openingComment, openingBookmark);
    return line.resolve(outLine, warn);
}

} // namespace

// ===== テキスト =====

QString KifToSfenConverter::mapHandicapToSfen(const QString& label) {
    return NotationUtils::mapHandicapToSfen(label);
}

QString KifToSfenConverter::detectInitialSfenFromText(const QString& text, QString* detectedLabel)
{
    return detectInitialSfenFromLines(KifReader::splitLines(text), detectedLabel);
}

bool KifToSfenConverter::parseText(const QString& text, KifParseResult& out, QString* errorMessage)
{
    return parseLines(KifReader::splitLines(text), out, errorMessage);
}

QList<KifGameInfoItem> KifToSfenConverter::extractGameInfoFromText(const QString& text)
{
    return extractGameInfoFromLines(KifReader::splitLines(text));
}

QString KifToSfenConverter::detectInitialSfenFromLines(const QStringList& lines, QString* detectedLabel)
{
    // 先に BOD を試す
    QString bodSfen;
    QString warn;
    if (buildInitialSfenFromBod(lines, bodSfen, detectedLabel, &warn)) {
        qCDebug(lcKifu).noquote() << "BOD detected. sfen =" << bodSfen;
        return bodSfen;
    }

    // 従来の「手合割」ベース
    QString found;
    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        const QString line = it->trimmed();
        if (line.contains(QStringLiteral("手合割")) || line.contains(QStringLiteral("手合"))) {
            found = line; break;
        }
    }
    static const QRegularExpression s_afterColon(QStringLiteral("^.*[:：]"));
    QString label = found;
    label.remove(s_afterColon);
    label = label.trimmed();
    if (label.isEmpty()) label = QStringLiteral("平手");
    if (detectedLabel) *detectedLabel = found.isEmpty() ? QStringLiteral("平手(既定)") : label;
    QString initial = NotationUtils::mapHandicapToSfen(label);
    // 局面図がなくても、明示された手番を優先する（駒落ちは上手番・下手番と書く）。
    for (const QString& raw : std::as_const(lines)) {
        const QString t = raw.trimmed();
        if (t == QStringLiteral("後手番") || t == QStringLiteral("上手番")) {
            initial.replace(QStringLiteral(" b "), QStringLiteral(" w "));
            break;
        }
        if (t == QStringLiteral("先手番") || t == QStringLiteral("下手番")) {
            initial.replace(QStringLiteral(" w "), QStringLiteral(" b "));
            break;
        }
    }
    return initial;
}

bool KifToSfenConverter::parseLines(const QStringList& lines, KifParseResult& out, QString* errorMessage)
{
    out = KifParseResult{};

    // 「変化：N手」の行で手順を区切る（最初の区切りまでが本譜）
    QList<qsizetype> boundaries;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (KifLexer::variationHeaderCaptureRe().match(lines.at(i)).hasMatch()) boundaries.append(i);
    }
    boundaries.append(lines.size());

    if (!parseKifBlock(lines, 0, boundaries.first(), detectInitialSfenFromLines(lines), 1, QString(),
                  out.mainline, errorMessage)) return false;

    for (qsizetype b = 0; b + 1 < boundaries.size(); ++b) {
        const auto match = KifLexer::variationHeaderCaptureRe().match(lines.at(boundaries.at(b)));
        const int startPly = KifuParseCommon::flexDigitsToIntNoDetach(match.captured(1));
        const auto base = KifuParseCommon::findVariationBase(out.mainline, out.variations, startPly);
        if (!base) {
            if (errorMessage) *errorMessage += QStringLiteral("KIF: invalid variation start: %1\n").arg(startPly);
            continue;
        }
        KifVariation variation;
        variation.startPly = startPly;
        variation.parentVariation = base->parentVariation;
        if (!parseKifBlock(lines, boundaries.at(b) + 1, boundaries.at(b + 1), base->sfen, startPly, base->prevUsi,
                      variation.line, errorMessage)) return false;
        KifuParseCommon::mergeOpeningIntoFirstMove(variation.line);
        out.variations.append(variation);
    }

    qCDebug(lcKifu).noquote() << "parsed: mainline moves =" << out.mainline.usiMoves().size()
                              << "variations =" << out.variations.size();
    return true;
}

QList<KifGameInfoItem> KifToSfenConverter::extractGameInfoFromLines(const QStringList& lines)
{
    static const QRegularExpression kLineLooksLikeMoveNo(
        QStringLiteral("^\\s*[0-9０-９]+\\s")
    );

    return KifuParseCommon::extractHeaderGameInfo(lines, [](const QString& t) {
        return kLineLooksLikeMoveNo.match(t).hasMatch();
    });
}

// ===== ファイル =====

QString KifToSfenConverter::detectInitialSfenFromFile(const QString& kifPath, QString* detectedLabel)
{
    QString text, warn;
    if (!readKifText(kifPath, text, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return NotationUtils::mapHandicapToSfen(QStringLiteral("平手"));
    }
    return detectInitialSfenFromText(text, detectedLabel);
}

QStringList KifToSfenConverter::convertFile(const QString& kifPath, QString* errorMessage)
{
    KifParseResult result;
    return parseWithVariations(kifPath, result, errorMessage) ? result.mainline.usiMoves() : QStringList();
}

QList<KifDisplayItem> KifToSfenConverter::extractMovesWithTimes(const QString& kifPath, QString* errorMessage)
{
    KifParseResult result;
    return parseWithVariations(kifPath, result, errorMessage) ? result.mainline.displayItems() : QList<KifDisplayItem>();
}

bool KifToSfenConverter::parseWithVariations(const QString& kifPath, KifParseResult& out, QString* errorMessage)
{
    out = KifParseResult{};
    QString text;
    if (!readKifText(kifPath, text, errorMessage)) return false;
    return parseText(text, out, errorMessage);
}

QList<KifGameInfoItem> KifToSfenConverter::extractGameInfo(const QString& filePath)
{
    if (filePath.isEmpty()) return {};
    QString text, warn;
    if (!readKifText(filePath, text, &warn)) {
        qCWarning(lcKifu).noquote() << "read failed:" << filePath << "warn:" << warn;
        return {};
    }
    return extractGameInfoFromText(text);
}

QMap<QString, QString> KifToSfenConverter::extractGameInfoMap(const QString& filePath)
{
    return KifuParseCommon::toGameInfoMap(extractGameInfo(filePath));
}

bool KifToSfenConverter::buildInitialSfenFromBod(const QStringList& lines,
                                                 QString& outSfen,
                                                 QString* detectedLabel,
                                                 QString* /*warn*/)
{
    // 1) 盤面情報の収集
    QMap<QChar, QString> rowByRank;
    if (!KifLexer::collectBodRows(lines, rowByRank, detectedLabel)) return false;

    // 2) 持駒
    QMap<Piece,int> handB, handW;
    for (const QString& line : std::as_const(lines)) {
        KifLexer::parseBodHandsLine(line, handB, true);
        KifLexer::parseBodHandsLine(line, handW, false);
    }

    // 3) 手番・手数
    QChar turn;
    int moveNumber;
    KifLexer::parseBodTurnAndMoveNumber(lines, turn, moveNumber);

    // 4) SFEN組み立て
    QStringList boardRows;
    for (int rank = 1; rank <= 9; ++rank) {
        const QChar rk = ShogiNotation::rankKanji(rank).at(0);
        boardRows << rowByRank.value(rk);
    }

    const QString board = boardRows.join(QLatin1Char('/'));
    const QString hands = KifuParseCommon::buildHandsSfen(handB, handW);
    outSfen = QStringLiteral("%1 %2 %3 %4")
                  .arg(board, QString(turn), hands, QString::number(moveNumber));

    return true;
}
