/// @file kifubranchtreebuilder.cpp
/// @brief 分岐ツリー構築ビルダークラスの実装

#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifutypes.h"
#include "kifparsetypes.h"
#include "kifdisplayitem.h"
#include "kifulogging.h"

KifuBranchTree* KifuBranchTreeBuilder::fromKifParseResult(const KifParseResult& result,
                                                          const QString& startSfen)
{
    auto* tree = new KifuBranchTree();
    buildFromKifParseResult(tree, result, startSfen);
    return tree;
}

void KifuBranchTreeBuilder::buildFromKifParseResult(KifuBranchTree* tree,
                                                     const KifParseResult& result,
                                                     const QString& startSfen)
{
    if (tree == nullptr) {
        return;
    }

    // ツリーをクリアして再構築
    tree->clear();
    tree->setRootSfen(startSfen);

    // 本譜を追加
    addKifLineToTree(tree, result.mainline, 1);

    // 分岐を追加
    for (const KifVariation& var : std::as_const(result.variations)) {
        addKifLineToTree(tree, var.line, var.startPly);
    }
}

void KifuBranchTreeBuilder::addKifLineToTree(KifuBranchTree* tree,
                                             const KifLine& line,
                                             int startPly)
{
    if (tree == nullptr || tree->root() == nullptr) {
        return;
    }

    qCDebug(lcKifu).noquote() << "addKifLineToTree: startPly=" << startPly
                              << "disp.size=" << line.disp.size();
    if (!line.disp.isEmpty()) {
        qCDebug(lcKifu).noquote() << "first disp: ply=" << line.disp.at(0).ply
                                  << "prettyMove=" << line.disp.at(0).prettyMove;
    }

    // 分岐開始点を見つける
    KifuBranchNode* branchPoint = nullptr;
    if (startPly <= 1) {
        branchPoint = tree->root();
        qCDebug(lcKifu).noquote() << "branchPoint = root";
    } else {
        // 分岐点を探す：baseSfen が分岐前の局面なので、それを使って正しいノードを見つける
        // sfenList[0] が空の場合は line.baseSfen を使用する
        QString baseSfen;
        if (!line.sfenList.isEmpty()) {
            baseSfen = line.sfenList.at(0);
        } else if (!line.baseSfen.isEmpty()) {
            baseSfen = line.baseSfen;
        }

        // SFEN から手数部分を除去（比較用）
        if (!baseSfen.isEmpty()) {
            qsizetype lastSpace = baseSfen.lastIndexOf(QLatin1Char(' '));
            if (lastSpace > 0) {
                baseSfen = baseSfen.left(lastSpace);
            }
        }

        qCDebug(lcKifu).noquote() << "baseSfen=" << baseSfen
                                  << "(sfenList.size=" << line.sfenList.size()
                                  << "line.baseSfen=" << line.baseSfen << ")";

        // まず SFEN で分岐点を探す
        if (!baseSfen.isEmpty()) {
            branchPoint = tree->findBySfen(baseSfen);
            qCDebug(lcKifu).noquote() << "findBySfen() = "
                                      << (branchPoint ? branchPoint->displayText() : "null");
        }

        // SFEN で見つからない場合は本譜から探す（フォールバック）
        if (branchPoint == nullptr) {
            branchPoint = tree->findByPlyOnMainLine(startPly - 1);
            qCDebug(lcKifu).noquote() << "findByPlyOnMainLine(" << (startPly - 1) << ") = "
                                      << (branchPoint ? branchPoint->displayText() : "null");
        }

        if (branchPoint == nullptr) {
            branchPoint = tree->root();
            qCDebug(lcKifu).noquote() << "fallback to root";
        }
    }

    KifuBranchNode* currentNode = branchPoint;

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
        if (currentNode != nullptr) {
            currentNode->setComment(item.comment);
            currentNode->setBookmark(item.bookmark);
        }
    }
}

void KifuBranchTreeBuilder::addLineToTree(KifuBranchTree* tree,
                                          const QList<KifDisplayItem>& disp,
                                          const QStringList& sfens,
                                          const QVector<ShogiMove>& moves,
                                          int startPly,
                                          int parentRow)
{
    Q_UNUSED(parentRow)  // 将来の拡張用

    if (tree == nullptr || tree->root() == nullptr) {
        return;
    }

    // 分岐開始点を見つける
    KifuBranchNode* branchPoint = nullptr;
    if (startPly <= 1) {
        branchPoint = tree->root();
    } else {
        branchPoint = tree->findByPlyOnMainLine(startPly - 1);
        if (branchPoint == nullptr) {
            branchPoint = tree->root();
        }
    }

    KifuBranchNode* currentNode = branchPoint;

    // disp[0]は開始局面ラベルなのでスキップ
    for (int i = startPly; i < disp.size(); ++i) {
        const KifDisplayItem& item = disp.at(i);
        QString sfen = (i < sfens.size()) ? sfens.at(i) : QString();
        ShogiMove move;
        if (i - 1 < moves.size()) {
            move = moves.at(i - 1);
        }

        currentNode = tree->addMove(currentNode, move, item.prettyMove, sfen, item.timeText);
        if (currentNode != nullptr) {
            currentNode->setComment(item.comment);
            currentNode->setBookmark(item.bookmark);
        }
    }
}
