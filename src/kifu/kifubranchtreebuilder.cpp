/// @file kifubranchtreebuilder.cpp
/// @brief パース時に保持した親子関係から分岐ツリーを構築する

#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifparsetypes.h"
#include "kifdisplayitem.h"

bool KifuBranchTreeBuilder::validateVariations(const KifParseResult& result)
{
    // 不正な親指定では、既存の棋譜を変えない。親は本譜か先に定義された変化。
    for (qsizetype i = 0; i < result.variations.size(); ++i) {
        const auto& variation = result.variations.at(i);
        const int parent = variation.parentVariation;
        if (parent < -1 || parent >= i || variation.startPly < 1) return false;
        const KifLine& line = parent < 0 ? result.mainline : result.variations.at(parent).line;
        const int firstPly = parent < 0 ? 1 : result.variations.at(parent).startPly;
        if (variation.startPly - 1 > firstPly - 1 + line.moveCount()) return false;
    }

    return true;
}

bool KifuBranchTreeBuilder::buildFromKifParseResult(KifuBranchTree* tree,
                                                   const KifParseResult& result,
                                                   const QString& startSfen)
{
    if (!tree || !validateVariations(result)) return false;

    tree->beginBatchUpdate();
    tree->clear();
    tree->setRootSfen(startSfen);
    QList<QList<KifuBranchNode*>> paths;
    paths.append(addKifLineToTree(tree, result.mainline, {tree->root()}));
    for (const auto& variation : result.variations) {
        const auto prefix = paths.at(variation.parentVariation + 1).mid(0, variation.startPly);
        paths.append(addKifLineToTree(tree, variation.line, prefix));
    }
    tree->endBatchUpdate();
    return true;
}

QList<KifuBranchNode*> KifuBranchTreeBuilder::addKifLineToTree(
    KifuBranchTree* tree, const KifLine& line, QList<KifuBranchNode*> path)
{
    KifuBranchNode* currentNode = path.last();

    if (line.opening) {
        if (!line.opening->comment.isEmpty()) currentNode->setComment(line.opening->comment);
        if (!line.opening->bookmark.isEmpty()) currentNode->setBookmark(line.opening->bookmark);
    }
    for (const auto& item : line.entries) {
        currentNode = tree->addRecord(currentNode, item.record);
        path.append(currentNode);
        currentNode->setComment(item.comment);
        currentNode->setBookmark(item.bookmark);
    }
    return path;
}
