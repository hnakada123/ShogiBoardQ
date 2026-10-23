/// @file kiftextparser.cpp
/// @brief KIF・KI2 のテキストを読む処理の実装

#include "kiftextparser.h"

#include "ki2lexer.h"
#include "kiflexer.h"
#include "kiflinebuilder.h"
#include "kifmovegrammar.h"
#include "kifreader.h"
#include "logcategories.h"
#include "notationutils.h"
#include "parsecommon.h"
#include "shoginotation.h"
#include "shogiposition.h"
#include "terminaltype.h"

#include <QRegularExpression>

namespace {

using KifTextParser::Dialect;

QString tagOf(Dialect dialect)
{
    return dialect == Dialect::Kif ? QStringLiteral("KIF") : QStringLiteral("KI2");
}

/// 開始局面（局面図、なければ手合割と手番の行）。kind にはその書き方を入れる
Position initialPosition(const QStringList& lines, StartKind* kind)
{
    Position bod;
    if (KifLexer::readBodPosition(lines, bod)) {
        *kind = StartKind::Diagram;
        qCDebug(lcKifu).noquote() << "BOD detected. sfen =" << bod.toSfen();
        return bod;
    }

    QString found;
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.contains(QStringLiteral("手合割")) || line.contains(QStringLiteral("手合"))) {
            found = line;
            break;
        }
    }
    static const QRegularExpression s_afterColon(QStringLiteral("^.*[:：]"));
    QString name = found;
    name.remove(s_afterColon);
    name = name.trimmed();
    if (name.isEmpty()) name = QStringLiteral("平手");
    *kind = found.isEmpty() ? StartKind::Default : StartKind::Handicap;
    Position initial = Position::fromSfen(NotationUtils::mapHandicapToSfen(name)).value_or(Position::hirate());
    // 局面図がなくても、明示された手番を優先する（駒落ちは上手番・下手番と書く）。
    for (const QString& raw : lines) {
        const QString t = raw.trimmed();
        if (t == QStringLiteral("後手番") || t == QStringLiteral("上手番")) {
            initial.setSideToMove(Turn::White);
            break;
        }
        if (t == QStringLiteral("先手番") || t == QStringLiteral("下手番")) {
            initial.setSideToMove(Turn::Black);
            break;
        }
    }
    return initial;
}

/// 1つの手順を読む（本譜か、「変化：N手」の次の行から次の変化の前まで）
class BlockReader
{
public:
    BlockReader(Dialect dialect, ParseReport& report, KifLineBuilder& builder)
        : m_dialect(dialect), m_tag(tagOf(dialect)), m_report(report), m_builder(builder)
    {
    }

    /// lines[begin, end) を読む。棋譜全体を読めなかったことにしたら false
    bool read(const QStringList& lines, qsizetype begin, qsizetype end)
    {
        for (qsizetype n = begin; n < end && !m_report.failed(); ++n) {
            const QString line = lines.at(n).trimmed();
            if (KifuParseCommon::isKifCommentLine(line)) {
                // 空の「*」行も段落の区切りとして残す
                m_builder.appendComment(line.mid(1).trimmed());
                continue;
            }
            if (KifuParseCommon::isBookmarkLine(line)) {
                const QString name = line.mid(1).trimmed();
                if (!name.isEmpty()) m_builder.appendBookmark(name);
                continue;
            }
            if (m_builder.ended() || line.isEmpty()) continue;
            if (m_dialect == Dialect::Ki2 && readResultLine(line)) continue;
            if (KifuParseCommon::isKifSkippableHeaderLine(line) || KifuParseCommon::isBoardHeaderOrFrame(line)) continue;
            if (KifLexer::startsWithMoveNumber(line)) readNumberedLine(line);
            else if (Ki2Lexer::isKi2MoveLine(line)) readMarkedLine(line);
        }
        return !m_report.failed();
    }

private:
    /// 「まで77手で先手の勝ち」（KI2 の終局）
    bool readResultLine(const QString& line)
    {
        const std::optional<Ki2Lexer::ResultLine> result = Ki2Lexer::parseResultLine(line);
        if (!result) return false;
        m_builder.addTerminal(result->terminal, result->endingSide(m_builder.sideToMove()));
        return true;
    }

    /// 手数で始まる行（KIF。"1 ７六歩(77) ( 0:01/00:00:01)"。1行に複数の手が並ぶこともある）
    void readNumberedLine(const QString& line)
    {
        const QList<KifLexer::NumberedMove> moves = KifLexer::readNumberedMoves(line);
        for (const KifLexer::NumberedMove& move : moves) {
            if (m_builder.ended()) return;
            const RecordTime time = move.time.value_or(KifLineBuilder::zeroTime());
            if (const TerminalType terminal = TerminalTypes::fromWord(move.text); terminal != TerminalType::None) {
                // 千日手の行は、消費時間を数えずに累計だけを残す
                m_builder.addTerminal(terminal, m_builder.sideToMove(),
                                      terminal == TerminalType::Repetition ? RecordTime::fromMilliseconds(0, time.totalMs) : time);
            } else if (!readMove(move.text, time, line)) {
                return;
            }
            if (!move.comment.isEmpty()) m_builder.appendComment(move.comment);
        }
    }

    /// 手番の印で始まる行（KI2。"▲７六歩    △３四歩"）
    void readMarkedLine(const QString& line)
    {
        const QStringList moves = Ki2Lexer::extractMovesFromLine(line);
        for (const QString& move : moves) {
            if (m_builder.ended()) return;
            if (const TerminalType terminal = TerminalTypes::findWord(move); terminal != TerminalType::None) {
                m_builder.addTerminal(terminal, m_builder.sideToMove());
                return;
            }
            if (!readMove(move, KifLineBuilder::zeroTime(), move)) return;
        }
    }

    /// 指し手の文字列を読んで指す。読めない・指せない手なら手順を打ち切って false
    bool readMove(const QString& text, const RecordTime& time, const QString& source)
    {
        const int ply = m_builder.nextPly();
        if (text.contains(QStringLiteral("パス"))) {
            m_report.rejectPass(m_tag, ply);
            m_builder.stop();
            return false;
        }
        const auto parsed = KifMoveGrammar::parseMoveText(text);
        const Turn mover = m_builder.sideToMove();
        const std::optional<UsiMove> move =
            parsed ? KifMoveGrammar::resolve(*parsed, m_builder.position(), mover, m_builder.previousTo()) : std::nullopt;
        KifMoveText::Notes notes;
        if (parsed) {
            notes.modifier = parsed->modifiers;
            notes.declinedPromotion = parsed->promote == false;
        }
        // 読めない手・指せない手で、手順はここで打ち切る（棋譜全体を読めなかったことにするかは、report が ParseOptions で決める）
        if (!move) {
            m_report.rejectMove(ply, QStringLiteral("%1: cannot parse move %2: %3").arg(m_tag).arg(ply).arg(source));
            m_builder.stop();
            return false;
        }
        if (!m_builder.addMove(*move, time, notes)) {
            m_report.rejectMove(ply, m_builder.cannotRestore(m_tag, ply));
            m_builder.stop();
            return false;
        }
        return true;
    }

    Dialect m_dialect;
    QString m_tag;
    ParseReport& m_report;
    KifLineBuilder& m_builder;
};

QList<KifGameInfoItem> gameInfoOf(const QStringList& lines, Dialect dialect)
{
    if (dialect == Dialect::Ki2) {
        return KifuParseCommon::extractHeaderGameInfo(lines, [](const QString& t) { return Ki2Lexer::isKi2MoveLine(t); });
    }
    static const QRegularExpression kMoveNumber(QStringLiteral("^\\s*[0-9０-９]+\\s"));
    return KifuParseCommon::extractHeaderGameInfo(lines, [](const QString& t) { return kMoveNumber.match(t).hasMatch(); });
}

} // namespace

namespace KifTextParser {

KifuParseOutcome parse(QStringView text, Dialect dialect, const ParseOptions& options)
{
    KifuParseOutcome outcome;
    ParseReport report(options);
    const QStringList lines = KifReader::splitLines(text.toString());
    const QString tag = tagOf(dialect);

    // 「変化：N手」の行で手順を区切る（最初の区切りまでが本譜）
    QList<qsizetype> boundaries;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (KifLexer::variationHeaderCaptureRe().match(lines.at(i)).hasMatch()) boundaries.append(i);
    }
    boundaries.append(lines.size());

    KifParseResult& record = outcome.record;
    {
        KifLineBuilder main(initialPosition(lines, &outcome.startKind));
        if (!BlockReader(dialect, report, main).read(lines, 0, boundaries.first())) {
            outcome.warning = report.warnings();
            return outcome;
        }
        record.mainline = main.take();
    }

    for (qsizetype b = 0; b + 1 < boundaries.size(); ++b) {
        const auto match = KifLexer::variationHeaderCaptureRe().match(lines.at(boundaries.at(b)));
        const int startPly = ShogiNotation::numberFromDigits(match.captured(1));
        const auto base = record.latestBranchPoint(startPly);
        const std::optional<Position> start = base ? Position::fromSfen(base->sfen) : std::nullopt;
        if (!start) {
            report.warn(QStringLiteral("%1: invalid variation start: %2").arg(tag).arg(startPly));
            continue;
        }
        KifLineBuilder line(*start, startPly, base->previousMove, KifLineBuilder::LineKind::Variation);
        if (!BlockReader(dialect, report, line).read(lines, boundaries.at(b) + 1, boundaries.at(b + 1))) {
            outcome.warning = report.warnings();
            outcome.record = {};
            return outcome;
        }
        record.addVariation(*base, line.take());
    }

    qCDebug(lcKifu).noquote() << tag << "parsed: mainline moves =" << record.mainline.moveCount()
                              << "variations =" << record.variations.size();
    outcome.success = true;
    outcome.gameInfo = gameInfoOf(lines, dialect);
    outcome.warning = report.warnings();
    return outcome;
}

KifuParseOutcome parseBod(QStringView text)
{
    KifuParseOutcome outcome;
    Position position;
    if (!KifLexer::readBodPosition(text.toString().split(QLatin1Char('\n')), position)) {
        outcome.warning = QStringLiteral("BOD: the board diagram could not be read.\n");
        return outcome;
    }
    outcome.success = true;
    outcome.startKind = StartKind::Diagram;
    outcome.positionOnly = true;
    outcome.record.mainline.startSfen = position.toSfen();
    return outcome;
}

} // namespace KifTextParser
