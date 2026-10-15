/// @file ki2tosfenconverter.cpp
/// @brief KI2形式棋譜コンバータクラスの実装

#include "ki2tosfenconverter.h"
#include "ki2lexer.h"
#include "ki2movenotation.h"
#include "kiftosfenconverter.h"
#include "kifreader.h"
#include "parsecommon.h"
#include "notationutils.h"
#include "sfenpositiontracer.h"
#include "shogiboard.h"

#include <QRegularExpression>
#include <optional>
#include <utility>
#include "logcategories.h"

namespace {

// KI2の指し手テキストと決まった指し手からKIF形式 prettyMove を構築（移動元を括弧で足す）
QString buildKi2PrettyMove(const QString& move, const std::optional<UsiMove>& usi, const QString& teban)
{
    static const QRegularExpression tebanMarkRe(QStringLiteral("[▲△▽☗☖]"));
    QString moveBody = move;
    moveBody.remove(tebanMarkRe);
    moveBody = moveBody.trimmed();

    if (!usi) return teban + moveBody;
    if (usi->isDrop()) {
        return moveBody.contains(QChar(u'打'))
            ? (teban + moveBody)
            : (teban + moveBody + QStringLiteral("打"));
    }
    return teban + moveBody + QStringLiteral("(%1%2)").arg(usi->from.file).arg(usi->from.rank);
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

namespace {
bool parseKi2Block(const QStringList& lines, const QString& baseSfen, int startPly,
                   BoardSquare previousTo, KifLine& outLine, QString* error)
{
    ShogiBoard board;
    if (!board.trySetSfen(baseSfen)) {
        if (error) *error = QStringLiteral("KI2: invalid start position: %1").arg(baseSfen);
        return false;
    }
    Turn mover = board.currentPlayer();
    int ply = startPly - 1;
    bool firstMoveFound = false, gameEnded = false;
    QString openingComment, openingBookmark, comment;
    KifuParseCommon::KifLineDraft line;
    line.baseSfen = baseSfen;
    line.startPly = startPly;
    for (const QString& raw : lines) {
        const QString text = raw.trimmed();
        if (KifuParseCommon::tryHandleCommentLine(text, firstMoveFound, comment, openingComment)) continue;
        if (KifuParseCommon::tryHandleBookmarkLine(text, firstMoveFound, line.disp, openingBookmark)) continue;
        if (gameEnded) continue;
        QString resultTerm;
        int resultCount = 0;
        QStringList moves;
        if (Ki2Lexer::parseResultLine(text, resultTerm, resultCount)) moves.append(resultTerm);
        else if (Ki2Lexer::isKi2MoveLine(text)) moves = Ki2Lexer::extractMovesFromLine(text);
        for (const QString& move : std::as_const(moves)) {
            const QString mark = (mover == Turn::Black) ? QStringLiteral("▲") : QStringLiteral("△");
            QString term;
            if (KifuParseCommon::isTerminalWordContains(move, &term)) {
                handleKi2TerminalWord(term, ply, comment, line.disp, firstMoveFound,
                                      gameEnded, openingComment, openingBookmark);
                line.disp.last().prettyMove = mark + term;
                line.endsWithTerminal = true;
                break;
            }
            std::optional<UsiMove> usi;
            if (!move.contains(QStringLiteral("パス"))) {
                const auto parsed = Ki2Lexer::parseMoveText(move);
                usi = parsed ? Ki2MoveNotation::resolve(*parsed, board, mover, previousTo) : std::nullopt;
                if (!usi || !board.applyUsiMove(*usi, mover)) {
                    if (error) *error = QStringLiteral("KI2: cannot parse move %1: %2").arg(ply + 1).arg(move);
                    return false;
                }
                previousTo = usi->to;
            }
            if (!firstMoveFound) {
                firstMoveFound = true;
                line.disp.append(KifuParseCommon::createOpeningDisplayItem(openingComment, openingBookmark));
            }
            KifuParseCommon::flushCommentToLastItem(comment, line.disp);
            line.usiMoves.append(usi ? usi->toString() : QStringLiteral("pass"));
            line.disp.append(KifuParseCommon::createMoveDisplayItem(++ply, buildKi2PrettyMove(move, usi, mark)));
            mover = (mover == Turn::Black) ? Turn::White : Turn::Black;
        }
    }
    KifuParseCommon::finalizeDisplayItems(comment, line.disp, openingComment, openingBookmark);
    return line.resolve(outLine, error);
}
} // namespace

QStringList Ki2ToSfenConverter::convertFile(const QString& path, QString* errorMessage)
{
    KifParseResult result;
    return parseWithVariations(path, result, errorMessage) ? result.mainline.usiMoves() : QStringList();
}

QList<KifDisplayItem> Ki2ToSfenConverter::extractMovesWithTimes(const QString& path, QString* errorMessage)
{
    KifParseResult result;
    return parseWithVariations(path, result, errorMessage) ? result.mainline.displayItems() : QList<KifDisplayItem>();
}

bool Ki2ToSfenConverter::parseWithVariations(const QString& path, KifParseResult& out, QString* errorMessage)
{
    out = KifParseResult{};
    QString text;
    QString encoding;
    if (!KifReader::readAllTextAuto(path, text, &encoding, errorMessage)) return false;
    return parseText(text, out, errorMessage);
}

bool Ki2ToSfenConverter::parseText(const QString& text, KifParseResult& out, QString* errorMessage)
{
    return parseLines(KifReader::splitLines(text), out, errorMessage);
}

bool Ki2ToSfenConverter::parseLines(const QStringList& lines, KifParseResult& out, QString* errorMessage)
{
    out = KifParseResult{};
    const QString initial = KifToSfenConverter::detectInitialSfenFromLines(lines);
    static const QRegularExpression variationRe(QStringLiteral("^変化[:：]\\s*([0-9０-９]+)\\s*手"));
    QList<qsizetype> boundaries;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (variationRe.match(lines[i].trimmed()).hasMatch()) boundaries.append(i);
    }
    boundaries.append(lines.size());
    KifParseResult parsed;
    if (!parseKi2Block(lines.mid(0, boundaries[0]), initial, 1, BoardSquare{}, parsed.mainline, errorMessage)) {
        return false;
    }
    for (qsizetype block = 0; block + 1 < boundaries.size(); ++block) {
        const auto match = variationRe.match(lines[boundaries[block]].trimmed());
        const int start = KifuParseCommon::flexDigitsToIntNoDetach(match.captured(1));
        const auto base = KifuParseCommon::findVariationBase(parsed.mainline, parsed.variations, start);
        if (!base) {
            if (errorMessage) *errorMessage = QStringLiteral("KI2: invalid variation start: %1").arg(start);
            return false;
        }
        // 変化の最初の手の「同」は、分岐する前の手の移動先
        BoardSquare previousTo;
        if (const auto previous = UsiMove::parse(base->prevUsi)) previousTo = previous->to;
        KifVariation variation;
        variation.startPly = start;
        variation.parentVariation = base->parentVariation;
        const auto blockLines = lines.mid(boundaries[block] + 1, boundaries[block + 1] - boundaries[block] - 1);
        if (!parseKi2Block(blockLines, base->sfen, start, previousTo, variation.line, errorMessage)) return false;
        KifuParseCommon::mergeOpeningIntoFirstMove(variation.line);
        parsed.variations.append(variation);
    }
    out = std::move(parsed);
    return true;
}

QString Ki2ToSfenConverter::detectInitialSfenFromText(const QString& text, QString* detectedLabel)
{
    return KifToSfenConverter::detectInitialSfenFromText(text, detectedLabel);
}

QList<KifGameInfoItem> Ki2ToSfenConverter::extractGameInfoFromText(const QString& text)
{
    return extractGameInfoFromLines(KifReader::splitLines(text));
}

QList<KifGameInfoItem> Ki2ToSfenConverter::extractGameInfoFromLines(const QStringList& lines)
{
    return KifuParseCommon::extractHeaderGameInfo(lines, [](const QString& t) {
        return Ki2Lexer::isKi2MoveLine(t);
    });
}

QList<KifGameInfoItem> Ki2ToSfenConverter::extractGameInfo(const QString& filePath)
{
    if (filePath.isEmpty()) return {};

    QString usedEnc, warn, text;
    if (!KifReader::readAllTextAuto(filePath, text, &usedEnc, &warn)) {
        qCWarning(lcKifu).noquote() << "read failed:" << filePath << "warn:" << warn;
        return {};
    }
    return extractGameInfoFromText(text);
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
