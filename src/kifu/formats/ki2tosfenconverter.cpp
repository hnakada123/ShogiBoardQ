/// @file ki2tosfenconverter.cpp
/// @brief KI2形式棋譜コンバータクラスの実装

#include "ki2tosfenconverter.h"
#include "ki2lexer.h"
#include "ki2movenotation.h"
#include "kiftosfenconverter.h"
#include "kiflexer.h"
#include "kifreader.h"
#include "parsecommon.h"
#include "notationutils.h"
#include "shoginotation.h"
#include "sfenreplay.h"
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
void handleKi2TerminalWord(TerminalType terminal, Turn side, int& moveIndex,
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
    out.push_back(KifuParseCommon::createTerminalDisplayItem(moveIndex, terminal, side));
    gameEnded = true;
}

} // anonymous namespace

QString Ki2ToSfenConverter::mapHandicapToSfen(const QString& label)
{
    return NotationUtils::mapHandicapToSfen(label);
}

namespace {
bool parseKi2Block(const QStringList& lines, const QString& baseSfen, int startPly,
                   BoardSquare previousTo, KifLine& outLine, QString* error)
{
    ShogiBoard board;
    if (!board.trySetSfen(baseSfen)) {
        if (error) *error += QStringLiteral("KI2: invalid start position: %1\n").arg(baseSfen);
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
        TerminalType resultTerminal = TerminalType::None;
        int resultCount = 0;
        QStringList moves;
        if (Ki2Lexer::parseResultLine(text, resultTerminal, resultCount)) moves.append(TerminalTypes::label(resultTerminal));
        else if (Ki2Lexer::isKi2MoveLine(text)) moves = Ki2Lexer::extractMovesFromLine(text);
        for (const QString& move : std::as_const(moves)) {
            if (const TerminalType terminal = TerminalTypes::findWord(move); terminal != TerminalType::None) {
                handleKi2TerminalWord(terminal, mover, ply, comment, line.disp, firstMoveFound,
                                      gameEnded, openingComment, openingBookmark);
                line.endsWithTerminal = true;
                break;
            }
            // パス・読めない手・局面に合わない手は、その手の手前で手順を打ち切る（KIF と同じ）
            if (move.contains(QStringLiteral("パス"))) {
                if (error) *error += QStringLiteral("KI2: ") + KifuParseCommon::passNotice(ply + 1);
                gameEnded = true;
                break;
            }
            const auto parsed = Ki2Lexer::parseMoveText(move);
            const std::optional<UsiMove> usi =
                parsed ? Ki2MoveNotation::resolve(*parsed, board, mover, previousTo) : std::nullopt;
            if (!usi || !board.applyUsiMove(*usi, mover)) {
                if (error) {
                    *error += QStringLiteral("KI2: cannot parse move %1: %2\n").arg(ply + 1).arg(move)
                              + KifuParseCommon::truncationNotice(ply + 1);
                }
                gameEnded = true;
                break;
            }
            previousTo = usi->to;
            if (!firstMoveFound) {
                firstMoveFound = true;
                line.disp.append(KifuParseCommon::createOpeningDisplayItem(openingComment, openingBookmark));
            }
            KifuParseCommon::flushCommentToLastItem(comment, line.disp);
            line.usiMoves.append(usi->toString());
            line.disp.append(KifuParseCommon::createMoveDisplayItem(++ply, buildKi2PrettyMove(move, usi, mover == Turn::Black ? QStringLiteral("▲") : QStringLiteral("△"))));
            mover = (mover == Turn::Black) ? Turn::White : Turn::Black;
        }
    }
    KifuParseCommon::finalizeDisplayItems(comment, line.disp, openingComment, openingBookmark);
    return line.resolve(outLine, error);
}
} // namespace

bool Ki2ToSfenConverter::parseText(const QString& text, KifParseResult& out, QString* errorMessage)
{
    return parseLines(KifReader::splitLines(text), out, errorMessage);
}

bool Ki2ToSfenConverter::parseLines(const QStringList& lines, KifParseResult& out, QString* errorMessage,
                                    QString* detectedLabel)
{
    out = KifParseResult{};
    const QString initial = KifToSfenConverter::detectInitialSfenFromLines(lines, detectedLabel);
    QList<qsizetype> boundaries;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (KifLexer::variationHeaderCaptureRe().match(lines[i]).hasMatch()) boundaries.append(i);
    }
    boundaries.append(lines.size());
    KifParseResult parsed;
    if (!parseKi2Block(lines.mid(0, boundaries[0]), initial, 1, BoardSquare{}, parsed.mainline, errorMessage)) {
        return false;
    }
    for (qsizetype block = 0; block + 1 < boundaries.size(); ++block) {
        const auto match = KifLexer::variationHeaderCaptureRe().match(lines[boundaries[block]]);
        const int start = ShogiNotation::numberFromDigits(match.captured(1));
        const auto base = parsed.latestBranchPoint(start);
        if (!base) {
            if (errorMessage) *errorMessage += QStringLiteral("KI2: invalid variation start: %1\n").arg(start);
            continue;
        }
        // 変化の最初の手の「同」は、分岐する前の手の移動先
        BoardSquare previousTo;
        if (const auto previous = UsiMove::parse(base->previousUsi)) previousTo = previous->to;
        KifLine line;
        const auto blockLines = lines.mid(boundaries[block] + 1, boundaries[block + 1] - boundaries[block] - 1);
        if (!parseKi2Block(blockLines, base->sfen, start, previousTo, line, errorMessage)) return false;
        KifuParseCommon::mergeOpeningIntoFirstMove(line);
        parsed.addVariation(*base, std::move(line));
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

bool Ki2ToSfenConverter::buildInitialSfenFromBod(const QStringList& lines,
                                                  QString& outSfen,
                                                  QString* detectedLabel,
                                                  QString* warn)
{
    return KifToSfenConverter::buildInitialSfenFromBod(lines, outSfen, detectedLabel, warn);
}
