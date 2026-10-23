/// @file usentosfenconverter.cpp
/// @brief USEN形式棋譜コンバータクラスの実装 - 公開API

#include "usentosfenconverter.h"
#include "terminalcodes.h"
#include "kiflinebuilder.h"
#include "shogiposition.h"
#include "sfenutils.h"
#include "usencodec.h"

#include <limits>
#include <utility>
#include "logcategories.h"

// ============================================================================
// 解析
// ============================================================================

namespace {

/// 指し手を順に指し、終局コードがあれば終局の行を足す。指せない手があれば、その手の手前で打ち切る
void readMoves(const UsenLine& line, KifLineBuilder& builder, ParseReport& report)
{
    for (const UsiMove& move : line.moves) {
        const int ply = builder.nextPly();
        if (!builder.addMove(move)) {
            report.rejectMove(ply, builder.cannotRestore(QStringLiteral("USEN"), ply));
            builder.stop();
            return;
        }
    }
    if (!line.terminal.isEmpty()) builder.addTerminal(TerminalCodes::fromUsen(line.terminal), builder.sideToMove());
}

} // namespace

QString UsenToSfenConverter::initialSfenOf(const QString& content, StartKind* kind)
{
    // ~ の前の部分が初期局面（空なら平手）
    const qsizetype tildePos = content.indexOf(QChar('~'));
    if (tildePos < 0) {
        if (kind) *kind = StartKind::Default;
        return SfenUtils::hirateSfen();
    }
    const QString positionPart = content.left(tildePos);
    if (positionPart.isEmpty()) {
        if (kind) *kind = StartKind::Handicap;
        return SfenUtils::hirateSfen();
    }
    if (kind) *kind = StartKind::Specified;
    return UsenCodec::decodePosition(positionPart);
}

KifuParseOutcome UsenToSfenConverter::parse(QStringView text, const ParseOptions& options)
{
    KifuParseOutcome outcome;
    ParseReport report(options);
    const QString content = text.trimmed().toString();
    if (content.isEmpty()) {
        outcome.warning = QStringLiteral("USEN: empty text");
        return outcome;
    }

    const std::optional<Position> start = Position::fromSfen(initialSfenOf(content, &outcome.startKind));
    if (!start) {
        outcome.warning = QStringLiteral("USEN: invalid initial position");
        return outcome;
    }

    // 本譜と分岐（"~" で1回だけ分け、手順ごとに指し手にする）
    QString error;
    const std::optional<QList<UsenLine>> lines = UsenCodec::decodeLines(content, &error);
    if (!lines) {
        outcome.warning = QStringLiteral("USEN: %1").arg(error);
        return outcome;
    }
    KifParseResult& record = outcome.record;
    {
        KifLineBuilder main(*start);
        readMoves(lines->constFirst(), main, report);
        if (report.failed()) {
            outcome.warning = report.warnings();
            return outcome;
        }
        record.mainline = main.take();
    }

    // 分岐は「直前に記述された手順」から派生する（KIF の「変化：N手」と同じ）。オフセットはその前の手数
    for (qsizetype i = 1; i < lines->size(); ++i) {
        const UsenLine& line = lines->at(i);
        // オフセットが int の最大値なら次の手数を作れないので、読めない分岐にする（桁あふれさせない）
        const int startPly = line.offset < std::numeric_limits<int>::max() ? line.offset + 1 : 0;
        const auto base = record.latestBranchPoint(startPly);
        const std::optional<Position> from = base ? base->position : std::nullopt;
        if (!from) {
            outcome.warning = QStringLiteral("USEN: invalid variation start: %1").arg(startPly);
            outcome.record = {};
            return outcome;
        }
        KifLineBuilder branch(*from, startPly, base->previousMove, KifLineBuilder::LineKind::Variation);
        readMoves(line, branch, report);
        if (report.failed()) {
            outcome.warning = report.warnings();
            outcome.record = {};
            return outcome;
        }
        record.addVariation(*base, branch.take());
    }

    outcome.success = true;
    outcome.warning = report.warnings();
    return outcome;
}
