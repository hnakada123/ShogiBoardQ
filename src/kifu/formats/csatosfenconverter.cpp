/// @file csatosfenconverter.cpp
/// @brief CSA形式棋譜コンバータクラスの実装
///
/// 字句解析・トークン化は CsaLexer に委譲。
/// 本ファイルはファイルI/O・パーサ状態管理・オーケストレーションを担当する。

#include "csatosfenconverter.h"
#include "csaformatter.h"
#include "csalexer.h"
#include "gameinfokeys.h"
#include "kiflinebuilder.h"
#include "terminalcodes.h"
#include "shiftjiscodec.h"
#include "shogiposition.h"

#include <QFile>
#include <QStringDecoder>

// ============================================================
// ファイル読込
// ============================================================

bool CsaToSfenConverter::readFileText(const QString& path, QString& outText, QString* warn)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (warn) *warn += QStringLiteral("Failed to open: %1\n").arg(path);
        return false;
    }

    const QByteArray raw = f.readAll();
    const QByteArray head = raw.left(128);
    const bool utf8Header  = head.contains("CSA encoding=UTF-8")
                          || head.contains("'encoding=UTF-8");
    const bool sjisHeader  = head.contains("CSA encoding=SHIFT_JIS")
                          || head.contains("CSA encoding=Shift_JIS")
                          || head.contains("'encoding=SHIFT_JIS")
                          || head.contains("'encoding=Shift_JIS");

    QString text;

    if (utf8Header) {
        QStringDecoder dec(QStringDecoder::Utf8);
        text = dec(raw);
    } else if (sjisHeader) {
        if (auto decoded = ShiftJisCodec::decode(raw)) {
            text = std::move(*decoded);
        } else {
            QStringDecoder dec(QStringDecoder::System);
            text = dec(raw);
        }
    } else {
        QStringDecoder decUtf8(QStringDecoder::Utf8);
        QString t = decUtf8(raw);
        if (!decUtf8.hasError() && !t.isEmpty()) {
            text = t;
        } else {
            if (auto decoded = ShiftJisCodec::decode(raw)) {
                text = std::move(*decoded);
            } else {
                QStringDecoder decSys(QStringDecoder::System);
                text = decSys(raw);
            }
        }
    }

    outText = std::move(text);
    return true;
}

QStringList CsaToSfenConverter::splitStatements(const QString& rawText, QString* warn)
{
    QString text = rawText;
    if (!text.isEmpty() && text.front() == QChar(0xFEFF)) text.remove(0, 1);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QStringList outLines;
    // 初期配置を含めてマルチステートメントを展開する。コメントと文字列値の
    // カンマは本文として保持し、複数対局の指し手を一つの棋譜に混ぜない。
    const QStringList physicalLines = text.split(QLatin1Char('\n'));
    for (QString line : physicalLines) {
        while (!line.isEmpty()) {
            line = line.trimmed();
            if (line == QLatin1String("/")) {
                if (warn) *warn += QStringLiteral("CSA: only the first game in the file is loaded.\n");
                return outLines;
            }
            const qsizetype comma = line.indexOf(QLatin1Char(','));
            if (comma < 0 || line.startsWith(QLatin1Char('\''))
                || line.startsWith(QLatin1Char('N')) || line.startsWith(QLatin1Char('$'))) {
                outLines.append(line);
                break;
            }
            outLines.append(line.left(comma));
            line = line.mid(comma + 1);
        }
    }
    return outLines;
}

// ============================================================
// 対局情報
// ============================================================

namespace {

QList<KifGameInfoItem> gameInfoOf(const QStringList& lines)
{
    QList<KifGameInfoItem> items;
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (CsaLexer::isMoveLine(line)) break;
        if (line.startsWith(QLatin1Char('P')) || line == QLatin1String("PI")) continue;

        if (line.startsWith(QLatin1String("N+"))) {
            items.append({ GameInfoKeys::kBlackPlayer, line.mid(2).trimmed() });
        } else if (line.startsWith(QLatin1String("N-"))) {
            items.append({ GameInfoKeys::kWhitePlayer, line.mid(2).trimmed() });
        } else if (line.startsWith(QLatin1Char('$'))) {
            const qsizetype colon = line.indexOf(QLatin1Char(':'));
            if (colon > 0) {
                const QString csaKey = line.mid(1, colon - 1).trimmed();
                QString val = line.mid(colon + 1).trimmed();
                if (csaKey == QLatin1String("NOTE")) val = CsaFormatter::unescapeNote(val);
                const QString key = CsaFormatter::gameInfoKeyFromCsa(csaKey);
                items.append({ key.isEmpty() ? csaKey : key, val });
            }
        } else if (line.startsWith(QLatin1Char('V'))) {
            items.append({ GameInfoKeys::kCsaVersion, line });
        }
    }
    return items;
}

/// 指し手・終局・コメント・消費時間の行を順に手順へ足す（読めない手があれば、そこで読むのをやめる）
void readMoves(const QStringList& lines, qsizetype begin, KifLineBuilder& builder, ParseReport& report)
{
    for (qsizetype i = begin; i < lines.size(); ++i) {
        const QString token = lines.at(i).trimmed();
        if (token.isEmpty()) continue;

        if (token.startsWith(QLatin1Char('\''))) {
            const QString comment = CsaLexer::normalizeCsaCommentLine(token);
            if (!comment.isNull()) builder.appendComment(comment);
        } else if (CsaLexer::isTurnMarker(token)) {
            continue;  // 手番は局面が持つ
        } else if (token.startsWith(QLatin1Char('%'))) {
            const Turn side = builder.sideToMove();
            TerminalType terminal = TerminalCodes::fromCsaSpecial(token, side);
            if (terminal == TerminalType::None) terminal = TerminalType::Error;
            builder.addTerminal(terminal, side);
        } else if (token.startsWith(QLatin1Char('T'))) {
            // 直前の行（指し手か終局）の消費時間。累計は手番ごとに足す
            const auto elapsed = CsaLexer::parseTimeTokenMs(token);
            if (!elapsed) {
                report.warn(QStringLiteral("Failed to parse time token: %1").arg(token));
            } else if (const auto mover = builder.lastMover()) {
                builder.setLastTime(builder.timeFor(*mover, *elapsed));
            }
        } else if (CsaLexer::isMoveLine(token)) {
            if (builder.ended()) continue;
            const Turn mover = (token.at(0) == QLatin1Char('+')) ? Turn::Black : Turn::White;
            const int ply = builder.nextPly();

            // 手番と違う符号の手・読めない手・局面に合わない手は、その手の手前で手順を打ち切る（全形式で同じ）
            QString detail;
            std::optional<UsiMove> move;
            if (mover != builder.sideToMove()) {
                detail = QStringLiteral("Not the side to move: %1").arg(token);
            } else {
                move = CsaLexer::parseMoveLine(token, mover, builder.position(), &detail);
            }
            if (!move) {
                report.rejectMove(ply, QStringLiteral("CSA: cannot read move %1: %2").arg(ply).arg(detail));
                builder.stop();
                return;
            }
            if (!builder.addMove(*move, KifLineBuilder::zeroTime(), {}, Position::DropRule::RequireHand)) {
                // 駒台にない駒を打つ手（CSA は持ち駒を確かめる）
                report.rejectMove(ply, QStringLiteral("CSA: cannot read move %1: Move does not match the position: %2 (%3)")
                                           .arg(ply).arg(token, builder.lastError()));
                builder.stop();
                return;
            }
        }
    }
}

} // namespace

// ============================================================
// 解析
// ============================================================

KifuParseOutcome CsaToSfenConverter::parse(QStringView text, const ParseOptions& options)
{
    KifuParseOutcome outcome;
    ParseReport report(options);
    QString splitWarning;
    const QStringList lines = splitStatements(text.toString(), &splitWarning);
    if (!splitWarning.isEmpty()) report.warn(splitWarning);

    int index = 0;
    Position start;
    if (!CsaLexer::parseStartPos(lines, index, start)) {
        report.fail(QStringLiteral("Failed to parse start position."));
        outcome.warning = report.warnings();
        return outcome;
    }

    KifLineBuilder builder(start);
    readMoves(lines, index, builder, report);
    outcome.warning = report.warnings();
    if (report.failed()) return outcome;

    outcome.record.mainline = builder.take();
    outcome.startKind = start == Position::hirate() ? StartKind::Handicap : StartKind::Specified;
    outcome.gameInfo = gameInfoOf(lines);
    outcome.success = true;
    return outcome;
}
