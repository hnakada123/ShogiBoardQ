/// @file usiexporter.cpp
/// @brief USI形式棋譜エクスポータクラスの実装

#include "usiexporter.h"
#include "terminalcodes.h"
#include "logcategories.h"
#include "kifuexportcommon.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"

// ========================================
// エクスポート
// ========================================

QStringList UsiExporter::exportLines(const GameRecordModel& model, const GameRecordModel::ExportContext& ctx)
{
    const KifuBranchTree* tree = model.branchTree();
    if (!tree || tree->isEmpty()) return exportPosition(ctx.startSfen, {});
    const QList<KifuBranchNode*> line = tree->mainLine();
    return exportPosition(ctx.startSfen, tree->mainRecordLine().usiMoves(), line.constLast()->terminalType());
}

QStringList UsiExporter::exportPosition(const QString& startSfen, const QStringList& usiMoves,
                                        TerminalType terminal)
{
    QStringList out;

    // 1) 初期局面を判定
    // 平手初期局面のSFEN
    static const QString HIRATE_SFEN = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    QString positionStr;

    // 初期SFENが空または平手初期局面の場合は "startpos" を使用
    if (startSfen.isEmpty() || startSfen == HIRATE_SFEN) {
        positionStr = QStringLiteral("position startpos");
    } else {
        // 元のSFEN手数も保持する。
        positionStr = QStringLiteral("position sfen %1").arg(startSfen);
    }

    // 2) USI moves
    QStringList mainlineUsi = usiMoves;

    // 3) 終局コードを取得
    const QString terminalCode = TerminalCodes::usi(terminal);

    // 4) position コマンド文字列を構築
    QString usiLine = positionStr;

    if (!mainlineUsi.isEmpty()) {
        usiLine += QStringLiteral(" moves");
        for (const QString& move : std::as_const(mainlineUsi)) {
            usiLine += QStringLiteral(" ") + move;
        }
    }

    // 5) 終局コードを追加（指し手の後にスペース区切りで）
    if (!terminalCode.isEmpty()) {
        usiLine += QStringLiteral(" ") + terminalCode;
    }

    out << usiLine;

    qCDebug(lcKifu).noquote() << "toUsiLines: generated USI position command with"
                              << mainlineUsi.size() << "moves,"
                              << "terminal:" << terminalCode;

    return out;
}
