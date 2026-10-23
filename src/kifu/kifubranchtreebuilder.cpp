/// @file kifubranchtreebuilder.cpp
/// @brief パース時に保持した親子関係から分岐ツリーを構築する

#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifparsetypes.h"

namespace {

/// 木に足した手順（本譜か変化）。分岐の前の手は分岐元の手順のノードを使う
struct AddedLine {
    qsizetype parent = -1;            ///< 分岐元の手順の番号（本譜は -1）
    KifuBranchNode* base = nullptr;   ///< 最初の手の前の局面のノード
    QList<KifuBranchNode*> nodes;     ///< この手順で足したノード（最初の手から）
};

/// 手順 index の ply 手目の後の局面のノード。手順ごとに全経路を持たず、分岐元の手順をさかのぼって引く
/// （変化ごとに経路を複製すると、手数×変化数のメモリになる。セキュリティ監査 S6）
KifuBranchNode* nodeAt(const QList<AddedLine>& lines, qsizetype index, int ply)
{
    while (true) {
        const AddedLine& line = lines.at(index);
        const int first = line.base->ply() + 1;
        if (ply >= first) return line.nodes.at(ply - first);
        if (ply == first - 1 || line.parent < 0) return line.base;
        index = line.parent;
    }
}

} // namespace

bool KifuBranchTreeBuilder::buildFromKifParseResult(KifuBranchTree* tree, const KifParseResult& result)
{
    if (!tree) return false;
    // 分岐元は本譜か先に書かれた変化で、その手順が分岐の手の前まで続いている
    for (qsizetype i = 0; i < result.variations.size(); ++i) {
        const KifVariation& variation = result.variations.at(i);
        if (variation.parentVariation >= i || !result.branchPoint(variation.parentVariation, variation.startPly))
            return false;
    }

    tree->beginBatchUpdate();
    tree->setRootSfen(result.mainline.startSfen);
    // 0 が本譜、i + 1 が変化 i
    QList<AddedLine> lines;
    lines.reserve(result.variations.size() + 1);
    lines.append({-1, tree->root(), addKifLineToTree(tree, result.mainline, tree->root())});
    for (const auto& variation : result.variations) {
        const qsizetype parent = variation.parentVariation + 1;
        KifuBranchNode* from = nodeAt(lines, parent, variation.startPly - 1);
        lines.append({parent, from, addKifLineToTree(tree, variation.line, from)});
    }
    tree->endBatchUpdate();
    return true;
}

QList<KifuBranchNode*> KifuBranchTreeBuilder::addKifLineToTree(
    KifuBranchTree* tree, const RecordLine& line, KifuBranchNode* from)
{
    KifuBranchNode* currentNode = from;

    if (!line.openingComment.isEmpty()) currentNode->record().comment = line.openingComment;
    if (!line.openingBookmark.isEmpty()) currentNode->record().bookmark = line.openingBookmark;
    QList<KifuBranchNode*> added;
    added.reserve(line.entries.size());
    for (const auto& record : line.entries) {
        currentNode = tree->addRecord(currentNode, record);
        added.append(currentNode);
    }
    return added;
}
