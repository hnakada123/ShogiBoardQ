/// @file kifubranchtreebuilder.cpp
/// @brief パース時に保持した親子関係から分岐ツリーを構築する

#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifparsetypes.h"
#include "kifdisplayitem.h"

namespace {
int displayMoveCount(const KifLine& line)
{
    const bool opening = !line.disp.isEmpty()
        && (line.disp.first().ply == 0 || line.disp.first().prettyMove.isEmpty());
    return static_cast<int>(line.disp.size()) - (opening ? 1 : 0);
}

int lastPlayablePly(const KifLine& line, int firstPly)
{
    const bool terminal = !line.disp.isEmpty()
        && detectTerminalType(line.disp.last().prettyMove) != TerminalType::None;
    return firstPly - 1 + displayMoveCount(line) - (terminal ? 1 : 0);
}
}

bool KifuBranchTreeBuilder::validateVariations(const KifParseResult& result)
{
    // 不正な親指定では、既存の棋譜を変えない。親は本譜か先に定義された変化。
    for (qsizetype i = 0; i < result.variations.size(); ++i) {
        const auto& variation = result.variations.at(i);
        const int parent = variation.parentVariation;
        if (parent < -1 || parent >= i || variation.startPly < 1) return false;
        const KifLine& line = parent < 0 ? result.mainline : result.variations.at(parent).line;
        const int firstPly = parent < 0 ? 1 : result.variations.at(parent).startPly;
        if (variation.startPly - 1 > lastPlayablePly(line, firstPly)) return false;
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

    // 指し手を追加
    // disp[0]が開始局面エントリ（ply=0 または prettyMove が空）の場合はスキップ
    int startIndex = 0;
    if (!line.disp.isEmpty()) {
        const KifDisplayItem& first = line.disp.at(0);
        if (first.ply == 0 || first.prettyMove.isEmpty()) {
            startIndex = 1;  // 開始局面エントリをスキップ
            // 開始局面のコメント・しおりがあればルートに設定
            if (!first.comment.isEmpty() && currentNode != nullptr) {
                currentNode->setComment(first.comment);
            }
            if (!first.bookmark.isEmpty() && currentNode != nullptr) {
                currentNode->setBookmark(first.bookmark);
            }
        }
    }

    for (int i = startIndex; i < line.disp.size(); ++i) {
        const KifDisplayItem& item = line.disp.at(i);

        QString sfen;
        // sfenList[0]はbase、sfenList[1]以降が各手後
        // startIndexを考慮してインデックスを調整
        int sfenIdx = i - startIndex + 1;
        if (sfenIdx < line.sfenList.size()) {
            sfen = line.sfenList.at(sfenIdx);
        }

        ShogiMove move;
        int moveIdx = i - startIndex;
        if (moveIdx < line.gameMoves.size()) {
            move = line.gameMoves.at(moveIdx);
        }

        currentNode = tree->addMove(currentNode, move, item.prettyMove, sfen, item.timeText);
        path.append(currentNode);
        if (currentNode != nullptr) {
            currentNode->setComment(item.comment);
            currentNode->setBookmark(item.bookmark);
        }
    }
    return path;
}
