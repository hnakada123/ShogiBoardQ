/// @file csaexporter.cpp
/// @brief CSA形式棋譜エクスポータクラスの実装

#include "csaexporter.h"
#include "shoginotation.h"
#include "csaformatter.h"
#include "kifuexportcommon.h"
#include "gameinfokeys.h"
#include "gamerecordmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifparsetypes.h"
#include "logcategories.h"
#include "sfencsapositionconverter.h"
#include "sfenutils.h"
#include "terminalcodes.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>

static const QRegularExpression& newlineRe()
{
    static const QRegularExpression re(QStringLiteral("\r?\n"));
    return re;
}

static void appendCsaComments(const QString& comment, QStringList& out)
{
    if (comment.isEmpty()) return;
    const QStringList lines = comment.split(newlineRe(), Qt::KeepEmptyParts);
    for (const QString& line : lines) out << (QStringLiteral("'*") + line);
}

// ========================================
// CsaExporter 本体
// ========================================

QStringList CsaExporter::exportLines(const GameRecordModel& model, const GameRecordModel::ExportContext& ctx)
{
    QStringList out;

    // 1) 文字コード宣言 (V3.0で追加)
    out << QStringLiteral("'CSA encoding=UTF-8");

    // 2) バージョン
    out << QStringLiteral("V3.0");

    // 3) 棋譜情報を先に収集（対局者名もここから取得）
    const QList<KifGameInfoItem> header = GameRecordModel::collectGameInfo(ctx);

    // 4) 対局者名（駒落ちの下手は先手、上手は後手）
    const auto isBlackKey = [](const QString& key) {
        return key == GameInfoKeys::kBlackPlayer || key == GameInfoKeys::kShitatePlayer;
    };
    const auto isWhiteKey = [](const QString& key) {
        return key == GameInfoKeys::kWhitePlayer || key == GameInfoKeys::kUwatePlayer;
    };
    QString blackPlayer, whitePlayer;
    for (const auto& it : std::as_const(header)) {
        const QString key = it.key.trimmed();
        const QString val = it.value.trimmed();
        if (isBlackKey(key) && !val.isEmpty()) {
            blackPlayer = val;
        } else if (isWhiteKey(key) && !val.isEmpty()) {
            whitePlayer = val;
        }
    }
    if (blackPlayer.isEmpty() || whitePlayer.isEmpty()) {
        QString resolvedBlack, resolvedWhite;
        GameRecordModel::resolvePlayerNames(ctx, resolvedBlack, resolvedWhite);
        if (blackPlayer.isEmpty()) blackPlayer = resolvedBlack;
        if (whitePlayer.isEmpty()) whitePlayer = resolvedWhite;
    }
    if (!blackPlayer.isEmpty()) {
        out << QStringLiteral("N+%1").arg(blackPlayer);
    }
    if (!whitePlayer.isEmpty()) {
        out << QStringLiteral("N-%1").arg(whitePlayer);
    }

    // 5) 棋譜情報（対局者名以外）
    bool hasStartTime = false;
    bool hasTime = false;

    for (const auto& it : std::as_const(header)) {
        const QString key = it.key.trimmed();
        const QString val = it.value.trimmed();
        if (key.isEmpty() || val.isEmpty()) continue;

        if (isBlackKey(key) || isWhiteKey(key)) {
            continue;
        }

        if (key == GameInfoKeys::kTimeControl) {
            // このアプリで対局した持ち時間は ctx.timeControl から書く（下）。読み込んだ棋譜の持ち時間は文字列から変換する
            if (!ctx.timeControl.isUnlimited()) continue;
            // 時間制限のない対局は $TIME を書かずに表す
            if (val != QStringLiteral("無制限")) out << CsaFormatter::convertToCsaTimeLines(val);
            hasTime = true;
            continue;
        }
        const QString csaKey = CsaFormatter::csaKeyFromGameInfo(key);
        if (csaKey.isEmpty()) continue;
        QString value = val;
        if (csaKey == QLatin1String("START_TIME") || csaKey == QLatin1String("END_TIME"))
            value = CsaFormatter::convertToCsaDateTime(val);
        else if (csaKey == QLatin1String("NOTE"))
            value = CsaFormatter::escapeNote(val);
        if (csaKey == QLatin1String("START_TIME")) hasStartTime = true;
        if (csaKey.startsWith(QLatin1String("TIME"))) hasTime = true;
        out << QStringLiteral("$%1:%2").arg(csaKey, value);
    }

    // 5-2) 対局情報に開始日時がなくても、このアプリで対局した開始時刻が分かれば記録する。
    // 記録のない日時（読み込んだ棋譜の変換時刻など）は書かない（$START_TIME/$END_TIME は省略可能）。
    if (!hasStartTime && ctx.gameStartDateTime.isValid()) {
        out << QStringLiteral("$START_TIME:%1")
                   .arg(ctx.gameStartDateTime.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")));
    }

    if (!hasTime && !ctx.timeControl.isUnlimited()) {
        const auto csaTime = [](const TimeControlSide& side) {
            return QStringLiteral("%1+%2+%3")
                .arg(side.mainMs / 1000).arg(side.byoyomiMs / 1000).arg(side.incrementMs / 1000);
        };
        // 先後で持ち時間が違う対局は、CSA V3.0 の $TIME+ と $TIME- に分けて書く
        const TimeControl& tc = ctx.timeControl;
        if (csaTime(tc.black) == csaTime(tc.white)) {
            out << QStringLiteral("$TIME:%1").arg(csaTime(tc.black));
        } else {
            out << QStringLiteral("$TIME+:%1").arg(csaTime(tc.black));
            out << QStringLiteral("$TIME-:%1").arg(csaTime(tc.white));
        }
    }

    // 6) 開始局面
    const bool isHirate = ctx.startSfen.isEmpty() || SfenUtils::isHirateStart(ctx.startSfen);

    if (isHirate) {
        out << QStringLiteral("PI");
        out << QStringLiteral("+");
    } else {
        QString convErr;
        const auto csaPos =
            SfenCsaPositionConverter::toCsaPositionLines(ctx.startSfen, &convErr);
        if (csaPos && !csaPos->isEmpty()) {
            out << *csaPos;
        } else {
            qCWarning(lcKifu).noquote()
                << "SFEN->CSA position conversion failed. fallback to PI. error=" << convErr;
            out << QStringLiteral("PI");
            out << QStringLiteral("+");
        }
    }

    // 7) 本譜の指し手（分岐ツリーの各手の指し手・指した側・消費時間・終局をそのまま書く）
    const KifuBranchTree* tree = model.branchTree();
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
