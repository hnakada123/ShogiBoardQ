/// @file kifubranchtreebuilder.cpp
/// @brief パース時に保持した親子関係から分岐ツリーを構築する

#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifparsetypes.h"

bool KifuBranchTreeBuilder::buildFromKifParseResult(KifuBranchTree* tree,
                                                   const KifParseResult& result,
                                                   const QString& startSfen)
{
    if (!tree) return false;
    // 分岐元は本譜か先に書かれた変化で、その手順が分岐の手の前まで続いている
    for (qsizetype i = 0; i < result.variations.size(); ++i) {
        const KifVariation& variation = result.variations.at(i);
        if (variation.parentVariation >= i || !result.branchPoint(variation.parentVariation, variation.startPly))
            return false;
    }

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
    KifuBranchTree* tree, const RecordLine& line, QList<KifuBranchNode*> path)
{
    KifuBranchNode* currentNode = path.last();

    if (!line.openingComment.isEmpty()) currentNode->record().comment = line.openingComment;
    if (!line.openingBookmark.isEmpty()) currentNode->record().bookmark = line.openingBookmark;
    for (const auto& record : line.entries) {
        currentNode = tree->addRecord(currentNode, record);
        path.append(currentNode);
    }
    return path;
}
