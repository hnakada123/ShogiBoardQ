/// @file usiexporter.cpp
/// @brief USI形式棋譜エクスポータクラスの実装

#include "usiexporter.h"
#include "terminalcodes.h"
#include "logcategories.h"
#include "kifuexportcommon.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "usiposition.h"

// ========================================
// エクスポート
// ========================================

QStringList UsiExporter::exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx)
{
    const KifuBranchTree* tree = &recordTree;
    if (!tree || tree->isEmpty()) return exportPosition(ctx.startSfen, {});
    const QList<KifuBranchNode*> line = tree->mainLine();
    return exportPosition(ctx.startSfen, tree->mainRecordLine().usiMoves(), line.constLast()->terminalType());
}

QStringList UsiExporter::exportPosition(const QString& startSfen, const QStringList& usiMoves,
                                        TerminalType terminal)
{
    // 開始局面の手数は書かれたまま残す。終局コードは指し手の後に空白で区切って足す
    QString usiLine = UsiPosition::commandFor(startSfen, usiMoves);
    const QString terminalCode = TerminalCodes::usi(terminal);
    if (!terminalCode.isEmpty()) usiLine += QLatin1Char(' ') + terminalCode;

    qCDebug(lcKifu).noquote() << "toUsiLines: generated USI position command with" << usiMoves.size()
                              << "moves, terminal:" << terminalCode;
    return {usiLine};
}
