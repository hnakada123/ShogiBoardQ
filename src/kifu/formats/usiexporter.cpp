/// @file usiexporter.cpp
/// @brief USI形式棋譜エクスポータクラスの実装

#include "usiexporter.h"
#include "terminalcodes.h"
#include "logcategories.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "usiposition.h"

// ========================================
// エクスポート
// ========================================

QStringList UsiExporter::exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx)
{
    if (recordTree.isEmpty()) return exportPosition(ctx.startSfen, {});
    const QList<KifuBranchNode*> line = recordTree.mainLine();
    return exportPosition(ctx.startSfen, recordTree.mainRecordLine().usiMoves(), line.constLast()->terminalType());
}

QStringList UsiExporter::exportPosition(const QString& startSfen, const QStringList& usiMoves,
                                        TerminalType terminal)
{
    // 開始局面の手数は書かれたまま残す。終局コードは指し手の後に空白で区切って足す
    QString usiLine = UsiPosition::commandFor(startSfen, usiMoves);
    const QString terminalCode = TerminalCodes::usi(terminal);
    if (!terminalCode.isEmpty()) {
        // 0手で終局した棋譜でも、終局コードは開始局面ではなく手順の欄に置く。
        if (usiMoves.isEmpty()) usiLine += QStringLiteral(" moves");
        usiLine += QLatin1Char(' ') + terminalCode;
    }

    qCDebug(lcKifu).noquote() << "toUsiLines: generated USI position command with" << usiMoves.size()
                              << "moves, terminal:" << terminalCode;
    return {usiLine};
}
