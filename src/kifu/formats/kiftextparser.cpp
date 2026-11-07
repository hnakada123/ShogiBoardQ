/// @file kiftextparser.cpp
/// @brief KIF・KI2 のテキストを読む処理の実装

#include "kiftextparser.h"

#include "ki2lexer.h"
#include "kiflexer.h"
#include "kiflinebuilder.h"
#include "kifmovegrammar.h"
#include "kifutextdecoder.h"
#include "logcategories.h"
#include "handicaps.h"
#include "parsecommon.h"
#include "recordentry.h"
#include "shoginotation.h"
#include "shogiposition.h"
#include "terminaltype.h"

#include <QRegularExpression>

#include <utility>

namespace {

using KifTextParser::Dialect;

QString tagOf(Dialect dialect)
{
    return dialect == Dialect::Kif ? QStringLiteral("KIF") : QStringLiteral("KI2");
}

bool isCustomTerminal(QStringView text)
{
    text = text.trimmed();
    if (!text.isEmpty() && ShogiNotation::isTurnMark(text.front())) text = text.mid(1).trimmed();
    // 指し手の形をした壊れた行やパスを、独自の終局理由に置き換えない。
    return !text.isEmpty() && !text.front().isDigit() && text.front() != QChar(u'同')
        && !text.contains(u"パス") && !KifMoveGrammar::parseMoveText(text);
}

/// 指し手の行か。局面図の筋の見出しは手数で始まる行に見えるので、呼ぶ前に除く
bool isMoveLine(const QString& line, Dialect dialect)
{
    if (dialect == Dialect::Ki2) return Ki2Lexer::isKi2MoveLine(line);
    static const QRegularExpression kMoveNumber(QStringLiteral("^\\s*[0-9０-９]+\\s"));
    return kMoveNumber.match(line).hasMatch();
}

/**
 * @brief 開始局面を読む行（最初の指し手の行より前の、コメントとしおりを除いた行）
 *
 * 全行を読むと、コメントに書いた語（「配置：」「後手番」「手数＝30」）や局面図で開始局面が変わる
 */
QStringList startPositionLines(const QStringList& lines, Dialect dialect)
{
    QStringList result;
    for (const QString& line : lines) {
        if (KifuParseCommon::isAnnotationLine(line)) continue;
        if (!KifuParseCommon::isBoardHeaderOrFrame(line) && isMoveLine(line, dialect)) break;
        result.append(line);
    }
    return result;
}

/**
 * @brief 開始局面（局面図、なければ手合割と手番の行）
 *
 * 開始局面が違うと以後の手がすべて意味を失うので、読めなければ report を失敗にする（戻り値は使わない）。
 * @param lines startPositionLines の行
 * @param kind 開始局面の書き方を入れる
 */
Position initialPosition(const QStringList& lines, StartKind* kind, const QString& tag, ParseReport& report)
{
    Position bod;
    switch (KifLexer::readBodPosition(lines, bod)) {
    case KifLexer::BodRead::Read:
        *kind = StartKind::Diagram;
        qCDebug(lcKifu).noquote() << "BOD detected. sfen =" << bod.toSfen();
        return bod;
    case KifLexer::BodRead::Unreadable:
        report.fail(QStringLiteral("%1: the board diagram could not be read.").arg(tag));
        return bod;
    case KifLexer::BodRead::NotFound:
        break;
    }

    // 手合割の行は「手合割：」「手合：」で始まる行だけ（コメントや対局者名の中の「手合割：」は読まない。
    // 行の途中を読むと、対局者名やコメントに書いた手合割で開始局面が変わる。セキュリティ監査 S9）
    static const QRegularExpression s_handicapKey(QStringLiteral("^手合割?[\\s　]*[:：]"));
    QString found;
    QString name;
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (const QRegularExpressionMatch key = s_handicapKey.match(line); key.hasMatch()) {
            found = line;
            name = line.mid(key.capturedEnd()).trimmed();
            break;
        }
    }
    *kind = found.isEmpty() ? StartKind::Default : StartKind::Handicap;
    // 手合割が書かれていなければ平手。知らない名前（局面図のない「その他」も）は開始局面が分からない
    const Handicaps::Handicap* handicap = Handicaps::byLabel(name);
    if (!name.isEmpty() && !handicap) report.fail(QStringLiteral("%1: unknown handicap: %2").arg(tag, name));
    Position initial = handicap ? Position::fromSfen(QString::fromLatin1(handicap->sfen)).value_or(Position::hirate())
                                : Position::hirate();
    // 局面図がなくても、明示された手番を優先する（駒落ちは上手番・下手番と書く）。
    for (const QString& raw : lines) {
        if (const std::optional<Turn> side = KifLexer::sideToMoveLine(raw)) {
            initial.setSideToMove(*side);
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

    /// 本譜の1手目が手番でない側の手なら、開始局面の手番を直して読む（本譜だけ。変化の開始局面は本譜から決まる）
    void allowStartTurnCorrection() { m_startTurnCorrectable = true; }

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
            if (line.isEmpty()) continue;
            if (m_builder.ended()) {
                // 終局の後の指し手は読まない。黙って捨てると、指し手でない行を独自の終局と読んだときに
                // 残りの手順が消えたことに気づけない
                if (!m_warnedAfterEnd && (KifLexer::startsWithMoveNumber(line) || Ki2Lexer::isKi2MoveLine(line))) {
                    m_report.warn(QStringLiteral("%1: moves after the end of the game were not read: %2").arg(m_tag, line));
                    m_warnedAfterEnd = true;
                }
                continue;
            }
            if (m_dialect == Dialect::Ki2 && readResultLine(line)) continue;
            // 局面図の筋の数字の行は、手数で始まる指し手の行に見える
            if (KifuParseCommon::isBoardHeaderOrFrame(line)) continue;
            // 指し手の行は、行内コメントに対局情報の語（「*第1図」「*場所…」）があっても読む。
            // それ以外の行（対局情報・手番・局面図・指し手の見出しなど）は手順に入れない
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
        addNamedTerminal(result->terminal, result->endingSide(m_builder.sideToMove()), line.trimmed(),
                         result->terminalText);
        return true;
    }

    /**
     * @brief 名指しされた側（KI2 の結果行・手番の印）の終局の行を、手番側の行にそろえて足す
     *
     * ほかの形式は終局の行を手番側の行としてしか書けないので、勝敗を変えずに手番側の行にする
     * （RecordEntry::forSideToMove。「まで3手で先手の反則負け」は後手の反則勝ち）。手番でない側の投了・
     * 入玉宣言勝ちなどはそろえられないので、名指しのまま残して知らせる
     */
    void addNamedTerminal(TerminalType type, Turn side, const QString& source, const QString& text = {})
    {
        const Turn sideToMove = m_builder.sideToMove();
        const RecordEntry::Ending ending = RecordEntry::forSideToMove(type, side, sideToMove);
        if (ending.side != sideToMove) {
            m_report.warn(QStringLiteral("%1: The ending is recorded for the side not to move; formats other than KI2 "
                                         "write it for the side to move: %2").arg(m_tag, source));
        }
        m_builder.addTerminal(ending.terminal, ending.side, KifLineBuilder::zeroTime(), text);
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
            } else if (isCustomTerminal(move.text)) {
                m_builder.addTerminal(TerminalType::Custom, m_builder.sideToMove(), time, move.text);
            } else if (!readMove(move.text, time, line)) {
                return;
            }
            if (!move.comment.isEmpty()) m_builder.appendComment(move.comment);
        }
    }

    /// 手番の印で始まる行（KI2。"▲７六歩    △３四歩 *コメント"）。行内コメントは行の最後の手に付ける
    void readMarkedLine(const QString& line)
    {
        const Ki2Lexer::MovesLine parsed = Ki2Lexer::splitMovesLine(line);
        for (const QString& move : parsed.moves) {
            if (m_builder.ended()) return;
            if (const TerminalType terminal = TerminalTypes::findWord(move); terminal != TerminalType::None) {
                // 「△投了」の印は終局を記録した側
                addNamedTerminal(terminal, ShogiNotation::turnOfMark(move.front()).value_or(m_builder.sideToMove()), move);
                break;
            }
            // 本譜の1手目なら、手番の印で開始局面の手番の書き違えを直す。印が手番と違う手は readMove が読まない
            if (const std::optional<Turn> marked = ShogiNotation::turnOfMark(move.front())) correctStartTurn(*marked, move);
            if (!readMove(move, KifLineBuilder::zeroTime(), move)) return;
        }
        if (!parsed.comment.isEmpty()) m_builder.appendComment(parsed.comment);
    }

    /// 指し手の文字列を読んで指す。読めない・指せない手（書かれた駒・手番の印が局面と違う手を含む）なら手順を
    /// 打ち切って false
    bool readMove(const QString& text, const RecordTime& time, const QString& source)
    {
        const int ply = m_builder.nextPly();
        if (text.contains(QStringLiteral("パス"))) {
            m_report.rejectPass(m_tag, ply);
            m_builder.stop();
            return false;
        }
        const auto parsed = KifMoveGrammar::parseMoveText(text);
        // 書かれたとおりに読み（手番の印があればその側の手として）、局面と合うかは KifLineBuilder::addMove が確かめる。
        // 印を見ずに手番側の手として読むと、相手の手、たとえば持っていない駒を打つ手に読み替えてしまう
        WrittenMove written;
        if (parsed) written.piece = parsed->piece;
        if (const QString trimmed = text.trimmed(); !trimmed.isEmpty()) written.side = ShogiNotation::turnOfMark(trimmed.front());
        const Turn mover = written.side.value_or(m_builder.sideToMove());
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
        if (!move->isDrop()) {
            if (const Piece piece = m_builder.position().pieceAt(move->from); piece != Piece::None)
                correctStartTurn(pieceSide(piece), source);
        }
        if (!m_builder.addMove(*move, time, notes, written)) {
            m_report.rejectMove(ply, m_builder.cannotRestore(m_tag, ply));
            m_builder.stop();
            return false;
        }
        m_builder.warnIrregularity(m_report, m_tag, ply);
        return true;
    }

    /**
     * @brief 1手目を指す側が開始局面の手番と違えば、開始局面の手番の書き違えとして直す（1手目でだけ）
     *
     * ShogiGUI 0.0.8.8 は駒落ちの棋譜を局面図で保存し、上手から指す棋譜にも「下手番」と書く。
     * そのまま読むと、1手目は下手が上手の駒を動かす手になる。
     */
    void correctStartTurn(Turn mover, const QString& source)
    {
        if (!std::exchange(m_startTurnCorrectable, false)) return;
        if (mover == m_builder.sideToMove() || !m_builder.correctStartTurn(mover)) return;
        m_report.warn(QStringLiteral("%1: the first move is played by the side the start position does not give the move to; "
                                     "the side to move was corrected: %2").arg(m_tag, source));
    }

    Dialect m_dialect;
    QString m_tag;
    ParseReport& m_report;
    KifLineBuilder& m_builder;
    bool m_warnedAfterEnd = false;  ///< 終局の後の手の警告を出したか（手順ごとに1回）
    bool m_startTurnCorrectable = false;  ///< 1手目で開始局面の手番を直してよいか（本譜の1手目の前だけ true）
};

QList<KifGameInfoItem> gameInfoOf(const QStringList& lines, Dialect dialect)
{
    return KifuParseCommon::extractHeaderGameInfo(lines, [dialect](const QString& t) { return isMoveLine(t, dialect); });
}

} // namespace

namespace KifTextParser {

KifuParseOutcome parse(QStringView text, Dialect dialect, const ParseOptions& options)
{
    KifuParseOutcome outcome;
    ParseReport report(options);
    const QStringList lines = KifuTextDecoder::splitLines(text.toString());
    const QString tag = tagOf(dialect);

    // 「変化：N手」の行で手順を区切る（最初の区切りまでが本譜）
    QList<qsizetype> boundaries;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (KifLexer::variationHeaderCaptureRe().match(lines.at(i)).hasMatch()) boundaries.append(i);
    }
    boundaries.append(lines.size());

    KifParseResult& record = outcome.record;
    {
        const Position start = initialPosition(startPositionLines(lines, dialect), &outcome.startKind, tag, report);
        if (report.failed()) {
            outcome.warning = report.warnings();
            return outcome;
        }
        KifLineBuilder main(start);
        BlockReader reader(dialect, report, main);
        reader.allowStartTurnCorrection();
        if (!reader.read(lines, 0, boundaries.first())) {
            outcome.warning = report.warnings();
            return outcome;
        }
        record.mainline = main.take();
    }

    for (qsizetype b = 0; b + 1 < boundaries.size(); ++b) {
        const auto match = KifLexer::variationHeaderCaptureRe().match(lines.at(boundaries.at(b)));
        const int startPly = ShogiNotation::numberFromDigits(match.captured(1));
        const auto base = record.latestBranchPoint(startPly);
        const std::optional<Position> start = base ? base->position : std::nullopt;
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
    if (KifLexer::readBodPosition(startPositionLines(text.toString().split(QLatin1Char('\n')), Dialect::Kif), position)
        != KifLexer::BodRead::Read) {
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
