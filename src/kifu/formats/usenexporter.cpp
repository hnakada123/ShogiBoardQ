/// @file usenexporter.cpp
/// @brief USEN形式棋譜エクスポータクラスの実装

#include "usenexporter.h"
#include "terminalcodes.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "logcategories.h"
#include "kifuexportcommon.h"
#include "usencodec.h"

// ========================================
// エクスポート
// ========================================

QStringList UsenExporter::exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx)
{
    QStringList out;

    // 1) 初期局面をUSEN形式に変換
    QString position = UsenCodec::encodePosition(ctx.startSfen);

    // 2) 本譜の指し手をエンコード
    QString mainMoves;
    QString terminalCode;

    // 本譜の各手（分岐ツリーのノード）をUSEN形式にエンコード
    const KifuBranchTree* branchTree = &recordTree;
    const QList<KifuBranchNode*> mainLine =
        (branchTree && !branchTree->isEmpty()) ? branchTree->mainLine() : QList<KifuBranchNode*>();
    for (const KifuBranchNode* node : mainLine) {
        if (node->isTerminal()) {
            terminalCode = TerminalCodes::usen(node->terminalType());
            break;
        }
        if (node->isActualMove()) mainMoves += UsenCodec::encodeMove(node->move().move.toString());
    }

    // 本譜のUSEN文字列を構築
    // USEN構造: [初期局面]~[オフセット].[指し手][.終局コード]
    // 初期局面は~の前に置く（平手の場合は空）、本譜のオフセットは0
    QString usen = position + QStringLiteral("~0.%1").arg(mainMoves);

    // 未終局は空の終局フィールドで表す。
    usen += QStringLiteral(".") + terminalCode;

    // 3) 分岐を追加
    if (branchTree != nullptr && !branchTree->isEmpty()) {
        QList<BranchLine> lines = branchTree->allLines();
        // lineIndex = 0 は本譜なのでスキップ、1以降が分岐
        for (int lineIdx = 1; lineIdx < lines.size(); ++lineIdx) {
            const BranchLine& line = lines.at(lineIdx);

            // オフセット（分岐開始位置 - 1）
            int offset = line.branchPly - 1;
            if (offset < 0) offset = 0;

            QString branchMoves;
            QString branchTerminal;

            // 分岐点より後の各手（分岐ツリーのノード）
            for (qsizetype i = line.branchPly; i < line.nodes.size(); ++i) {
                const KifuBranchNode* node = line.nodes.at(i);
                if (node->isTerminal()) {
                    branchTerminal = TerminalCodes::usen(node->terminalType());
                    break;
                }
                if (node->isActualMove()) branchMoves += UsenCodec::encodeMove(node->move().move.toString());
            }

            // 分岐のUSEN文字列を構築
            QString branchUsen = QStringLiteral("~%1.%2.%3").arg(offset).arg(branchMoves, branchTerminal);

            usen += branchUsen;
        }
    }

    out << usen;

    const int branchCount = (branchTree != nullptr) ? (branchTree->lineCount() - 1) : 0;
    qCDebug(lcKifu).noquote() << "toUsenLines: generated USEN with"
                              << mainMoves.size() / 3 << "mainline moves,"
                              << branchCount << "variations";

    return out;
}
