/// @file usentosfenconverter.cpp
/// @brief USEN形式棋譜コンバータクラスの実装 - 公開API

#include "usentosfenconverter.h"
#include "terminalcodes.h"
#include "kiflinebuilder.h"
#include "shogiposition.h"
#include "sfenutils.h"
#include "usencodec.h"

#include <utility>
#include "logcategories.h"

// ============================================================================
// 解析
// ============================================================================

namespace {

/// USI の指し手を順に指し、終局コードがあれば終局の行を足す。指せない手があれば、その手の手前で打ち切る
void readMoves(const QStringList& moves, const QString& terminalCode, KifLineBuilder& builder, ParseReport& report)
{
    for (const QString& text : moves) {
        const int ply = builder.nextPly();
        const std::optional<UsiMove> move = UsiMove::parse(text);
        if (!move || !builder.addMove(*move)) {
            report.rejectMove(ply, move ? builder.cannotRestore(QStringLiteral("USEN"), ply)
                                        : QStringLiteral("USEN: Invalid USI move %1: %2").arg(ply).arg(text));
            builder.stop();
            return;
        }
    }
    if (!terminalCode.isEmpty()) builder.addTerminal(TerminalCodes::fromUsen(terminalCode), builder.sideToMove());
}

} // namespace

QString UsenToSfenConverter::initialSfenOf(const QString& content, QString* detectedLabel)
{
    // ~ の前の部分が初期局面（空なら平手）
    const qsizetype tildePos = content.indexOf(QChar('~'));
    if (tildePos < 0) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return SfenUtils::hirateSfen();
    }
    const QString positionPart = content.left(tildePos);
    if (positionPart.isEmpty()) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手");
        return SfenUtils::hirateSfen();
    }

    const QString sfen = UsenCodec::decodePosition(positionPart);
    if (detectedLabel) *detectedLabel = QStringLiteral("局面指定");
    return sfen;
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

    const std::optional<Position> start = Position::fromSfen(initialSfenOf(content, &outcome.teaiLabel));
    if (!start) {
        outcome.warning = QStringLiteral("USEN: invalid initial position");
        return outcome;
    }

    // 本譜と分岐を分離
    QString mainlineUsen;
    QList<QPair<int, QString>> variations;  // (startPly, usenMoves)
    QString terminalCode;
    QString warn;
    if (!parseUsenString(content, mainlineUsen, variations, &terminalCode, &warn)) {
        outcome.warning = warn;
        return outcome;
    }

    // 本譜のデコード
    QString mainTerminal;
    const auto decodedMain = UsenCodec::decodeMoves(mainlineUsen, &mainTerminal);
    if (decodedMain.invalidCount > 0) {
        outcome.warning = decodedMain.firstError;
        return outcome;
    }
    KifParseResult& record = outcome.record;
    {
        KifLineBuilder main(*start);
        readMoves(decodedMain.moves, mainTerminal, main, report);
        if (report.failed()) {
            outcome.warning = report.warnings();
            return outcome;
        }
        record.mainline = main.take();
    }

    // 分岐のデコード。USEN の分岐は「直前に記述された手順」から派生する（KIF の「変化：N手」と同じ）
    for (const auto& var : std::as_const(variations)) {
        const int startPly = var.first;
        QString varTerminal;
        const auto decodedVariation = UsenCodec::decodeMoves(var.second, &varTerminal);
        if (decodedVariation.invalidCount > 0) {
            outcome.warning = decodedVariation.firstError;
            outcome.record = {};
            return outcome;
        }
        const auto base = record.latestBranchPoint(startPly);
        const std::optional<Position> from = base ? Position::fromSfen(base->sfen) : std::nullopt;
        if (!from) {
            outcome.warning = QStringLiteral("USEN: invalid variation start: %1").arg(startPly);
            outcome.record = {};
            return outcome;
        }
        KifLineBuilder branch(*from, startPly, base->previousMove, KifLineBuilder::LineKind::Variation);
        readMoves(decodedVariation.moves, varTerminal, branch, report);
        if (report.failed()) {
            outcome.warning = report.warnings();
            outcome.record = {};
            return outcome;
        }
        record.addVariation(*base, branch.take());
    }

    outcome.success = true;
    outcome.initialSfen = record.mainline.startSfen;
    outcome.warning = report.warnings();
    return outcome;
}

// ============================================================
// USEN形式棋譜コンバータ - デコード・パース処理
// ============================================================

// ============================================================================
// デコード・パースメソッド
// ============================================================================

bool UsenToSfenConverter::parseUsenString(const QString& usen,
                                           QString& mainlineUsen,
                                           QList<QPair<int, QString>>& variations,
                                           QString* terminalCode,
                                           QString* warn)
{
    variations.clear();
    if (terminalCode) terminalCode->clear();

    // 構造: [初期局面]~[本譜]~[分岐1]~[分岐2]...
    // 各パート: [オフセット].[指し手1][指し手2]...[.終局コード]

    // ~ の前の初期局面部分を除去してから ~ で分割
    const qsizetype firstTilde = usen.indexOf(QChar('~'));
    if (firstTilde < 0) {
        if (warn) *warn = QStringLiteral("USENが空です");
        return false;
    }

    // ~ 以降を分割（初期局面部分はスキップ）
    const QString afterPosition = usen.mid(firstTilde + 1);
    const QStringList parts = afterPosition.split(QChar('~'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (warn) *warn = QStringLiteral("USENが空です");
        return false;
    }

    // parts[0] = 本譜, parts[1..] = 分岐

    bool foundMainline = false;

    for (qsizetype partIdx = 0; partIdx < parts.size(); ++partIdx) {
        const QString& part = parts[partIdx];

        const qsizetype dotPos = part.indexOf(QChar('.'));
        if (dotPos < 0) continue;

        const QString prefix = part.left(dotPos);
        const QString movesAndTerminal = part.mid(dotPos + 1);

        // 終局コードのチェック (末尾が .x の形式)
        QString moves = movesAndTerminal;
        QString terminal;

        // 末尾から終局コードを探す
        const qsizetype lastDot = movesAndTerminal.lastIndexOf(QChar('.'));
        if (lastDot >= 0) {
            const QString possibleTerminal = movesAndTerminal.mid(lastDot + 1);
            // 1文字の終局コードかチェック
            if (possibleTerminal.isEmpty() || TerminalCodes::isUsenCode(possibleTerminal)) {
                terminal = possibleTerminal;
                moves = movesAndTerminal.left(lastDot);
            }
        }

        // 最初のパートを本譜として扱う
        if (partIdx == 0) {
            mainlineUsen = QStringLiteral("~") + part;
            if (terminalCode && !terminal.isEmpty()) {
                *terminalCode = terminal;
            }
            foundMainline = true;
        } else {
            // 分岐: prefix は分岐開始位置（0-indexed）
            // USENのオフセットは「何手目の後から分岐するか」を示す
            // 例: オフセット2 = 2手目の後から分岐 = 3手目から分岐
            // したがって startPly = offset + 1
            bool ok;
            const int offset = prefix.toInt(&ok);
            if (ok && offset >= 0) {
                const int startPly = offset + 1;  // 1-indexed に変換
                // 分岐の指し手部分を抽出
                QString varUsen = QStringLiteral("~0.") + moves;
                if (!terminal.isEmpty()) {
                    varUsen += QStringLiteral(".") + terminal;
                }
                variations.append(qMakePair(startPly, varUsen));
            }
        }
    }

    if (!foundMainline) {
        if (warn) *warn = QStringLiteral("本譜が見つかりません");
        return false;
    }

    return true;
}
