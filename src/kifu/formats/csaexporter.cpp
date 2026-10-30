/// @file csaexporter.cpp
/// @brief CSA形式棋譜エクスポータクラスの実装

#include "csaexporter.h"
#include "shoginotation.h"
#include "csaformatter.h"
#include "kifuexportcommon.h"
#include "gameinfo.h"
#include "gameinfokeys.h"
#include "handicaps.h"
#include "kifuexportmetadata.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifparsetypes.h"
#include "logcategories.h"
#include "sfencsapositionconverter.h"
#include "terminalcodes.h"

#include <QDateTime>
#include <QStringList>

static void appendCsaComments(const QString& comment, QStringList& out)
{
    if (comment.isEmpty()) return;
    const QStringList lines = KifuExportCommon::splitLines(comment);
    for (const QString& line : lines) out << (QStringLiteral("'*") + line);
}

// ========================================
// CsaExporter 本体
// ========================================

QStringList CsaExporter::exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx)
{
    QStringList out;

    // 1) 文字コード宣言 (V3.0で追加)
    out << (ctx.encoding == KifuEncoding::ShiftJis ? QStringLiteral("'CSA encoding=Shift_JIS")
                                                    : QStringLiteral("'CSA encoding=UTF-8"));

    // 2) バージョン
    out << QStringLiteral("V3.0");

    // 3) 棋譜情報を先に収集（対局者名もここから取得）
    const QList<KifGameInfoItem>& header = ctx.gameInfoItems;

    // 4) 対局者名（駒落ちの下手は先手、上手は後手。名前が無ければ書かない。表示言語の「先手」などの語は書かない）
    for (const Turn side : {Turn::Black, Turn::White}) {
        const QString name = GameInfo::playerName(header, side);
        if (!name.isEmpty())
            out << (side == Turn::Black ? QStringLiteral("N+") : QStringLiteral("N-")) + KifuExportCommon::singleLine(name);
    }

    // 5) 棋譜情報（対局者名以外）。開始日時・持ち時間も KIF と同じく対局情報から書く
    // （途中から続けた対局は対局情報を元の棋譜のまま残すので、続けた対局の日時・持ち時間は書かない。
    //  $START_TIME・$TIME は省略できる）
    for (const auto& it : std::as_const(header)) {
        const QString key = it.key.trimmed();
        const QString val = it.value.trimmed();
        if (key.isEmpty() || val.isEmpty()) continue;

        if (GameInfo::playerSide(key)) continue;

        if (key == GameInfoKeys::kTimeControl) {
            // 先後で違う持ち時間は $TIME+ と $TIME- に分ける。時間制限のない対局は $TIME を書かずに表す
            out << CsaFormatter::convertToCsaTimeLines(val);
            continue;
        }
        const QString csaKey = CsaFormatter::csaKeyFromGameInfo(key);
        if (csaKey.isEmpty()) continue;
        QString value = val;
        if (csaKey == QLatin1String("START_TIME") || csaKey == QLatin1String("END_TIME"))
            value = CsaFormatter::convertToCsaDateTime(val);
        else if (csaKey == QLatin1String("NOTE"))
            value = CsaFormatter::escapeNote(val);
        out << QStringLiteral("$%1:%2").arg(csaKey, KifuExportCommon::singleLine(value));
    }

    // 6) 開始局面（平手は PI）
    QString convErr;
    const std::optional<Position> start =
        ctx.startSfen.trimmed().isEmpty() ? Position::hirate() : Position::fromSfenLike(ctx.startSfen, &convErr);
    if (start && !Handicaps::isHirate(*start)) {
        out << SfenCsaPositionConverter::toCsaPositionLines(*start);
    } else {
        if (!start)
            qCWarning(lcKifu).noquote() << "SFEN->CSA position conversion failed. fallback to PI. error=" << convErr;
        out << QStringLiteral("PI");
        out << QStringLiteral("+");
    }

    // 7) 本譜の指し手（分岐ツリーの各手の指し手・指した側・消費時間・終局をそのまま書く）
    const KifuBranchTree* tree = &recordTree;
    const QList<KifuBranchNode*> line = (tree && !tree->isEmpty()) ? tree->mainLine() : QList<KifuBranchNode*>();
    if (!line.isEmpty()) appendCsaComments(line.first()->comment(), out);   // 開始局面のコメント

    int moves = 0;
    for (qsizetype i = 1; i < line.size(); ++i) {
        const KifuBranchNode* node = line.at(i);
        if (node->isTerminal()) {
            const QString special = TerminalCodes::csaSpecial(node->terminalType(), node->terminalSide(), false);
            if (!special.isEmpty()) {
                out << QLatin1Char('%') + special;
                out << CsaFormatter::timeToken(node->time());
            }
            appendCsaComments(node->comment(), out);
            break;
        }
        // CSA の駒は指した後の駒（成りは成った駒）
        const ShogiMove move = node->move();
        const QString csaPiece = ShogiNotation::csaPieceCode(move.move.promote ? promote(move.movingPiece)
                                                                                 : move.movingPiece);
        if (!move.isValid() || csaPiece.isEmpty()) {
            qCDebug(lcKifu).noquote() << "toCsaLines: skipped a move without the moved piece at ply" << node->ply();
            continue;
        }
        const QString sign = node->record().mover == Turn::Black ? QStringLiteral("+") : QStringLiteral("-");
        const QString from = move.isDrop() ? QStringLiteral("00")
                                           : QStringLiteral("%1%2").arg(move.move.from.file).arg(move.move.from.rank);
        out << QStringLiteral("%1%2%3%4%5").arg(sign, from, QString::number(move.move.to.file),
                                                QString::number(move.move.to.rank), csaPiece);
        out << CsaFormatter::timeToken(node->time());
        appendCsaComments(node->comment(), out);
        ++moves;
    }

    qCDebug(lcKifu).noquote() << "toCsaLines: generated" << out.size() << "lines," << moves << "moves";
    return out;
}
