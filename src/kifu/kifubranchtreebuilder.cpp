/// @file kifubranchtreebuilder.cpp
/// @brief 分岐ツリー構築ビルダークラスの実装

#include "kifubranchtreebuilder.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifutypes.h"
#include "kifparsetypes.h"
#include "kifdisplayitem.h"
#include "kifulogging.h"

KifuBranchTree* KifuBranchTreeBuilder::fromResolvedRows(const QVector<ResolvedRow>& rows,
                                                        const QString& startSfen)
{
    if (rows.isEmpty()) {
        return nullptr;
    }

    auto* tree = new KifuBranchTree();
    tree->setRootSfen(startSfen);

    // 行ごとの最終ノードを記録（分岐接続用）
    QVector<KifuBranchNode*> rowEndNodes(rows.size(), nullptr);

    // 本譜（row=0）を先に処理
    const ResolvedRow& mainRow = rows.at(0);
    KifuBranchNode* currentNode = tree->root();

    // disp[0]は開始局面のラベルなのでスキップ、disp[1]以降が実際の指し手
    for (int i = 1; i < mainRow.disp.size(); ++i) {
        const KifDisplayItem& item = mainRow.disp.at(i);
        QString sfen = (i < mainRow.sfen.size()) ? mainRow.sfen.at(i) : QString();
        ShogiMove move;
        if (i - 1 < mainRow.gm.size()) {
            move = mainRow.gm.at(i - 1);
        }

        currentNode = tree->addMove(currentNode, move, item.prettyMove, sfen, item.timeText);
        if (currentNode != nullptr) {
            currentNode->setComment(item.comment);
        }
    }
    rowEndNodes[0] = currentNode;

    // 分岐行（row=1以降）を処理
    for (int rowIdx = 1; rowIdx < rows.size(); ++rowIdx) {
        const ResolvedRow& row = rows.at(rowIdx);
        int parentRowIdx = row.parent;
        qCDebug(lcKifu).noquote() << "row" << rowIdx << ": parent=" << row.parent
                                  << "startPly=" << row.startPly << "disp.size=" << row.disp.size();
        if (parentRowIdx < 0) {
            parentRowIdx = 0;  // 親がなければ本譜から
        }

        // 分岐開始点を見つける
        int branchPly = row.startPly;
        KifuBranchNode* branchPoint = nullptr;

        // 親行から分岐点を探す
        if (parentRowIdx < rowEndNodes.size() && rowEndNodes[parentRowIdx] != nullptr) {
            branchPoint = tree->findByPlyOnLine(rowEndNodes[parentRowIdx], branchPly - 1);
            qCDebug(lcKifu).noquote() << "findByPlyOnLine(rowEndNodes[" << parentRowIdx << "], "
                                      << (branchPly - 1) << ") = " << (branchPoint ? branchPoint->displayText() : "null");
        }

        if (branchPoint == nullptr) {
            // フォールバック: 本譜から探す
            branchPoint = tree->findByPlyOnMainLine(branchPly - 1);
            qCDebug(lcKifu).noquote() << "FALLBACK findByPlyOnMainLine(" << (branchPly - 1) << ") = "
                                      << (branchPoint ? branchPoint->displayText() : "null");
        }

        if (branchPoint == nullptr) {
            // まだ見つからない場合はルートから
            branchPoint = tree->root();
        }

        currentNode = branchPoint;

        // この分岐の指し手を追加
        // disp[0..startPly-1]は親からコピーされたプレフィックスなのでスキップ
        // disp[startPly]以降がこの分岐の実際の手
        for (int i = branchPly; i < row.disp.size(); ++i) {
            const KifDisplayItem& item = row.disp.at(i);
            QString sfen = (i < row.sfen.size()) ? row.sfen.at(i) : QString();
            ShogiMove move;
            if (i - 1 < row.gm.size()) {
                move = row.gm.at(i - 1);
            }

            currentNode = tree->addMove(currentNode, move, item.prettyMove, sfen, item.timeText);
            if (currentNode != nullptr) {
                currentNode->setComment(item.comment);
            }
        }
        rowEndNodes[rowIdx] = currentNode;
    }

    return tree;
}

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

KifuBranchTree* KifuBranchTreeBuilder::fromKifLine(const KifLine& line,
                                                   const QString& startSfen)
{
    auto* tree = new KifuBranchTree();
    tree->setRootSfen(startSfen);

    addKifLineToTree(tree, line, 1);

    return tree;
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
            // 開始局面のコメントがあればルートに設定
            if (!first.comment.isEmpty() && currentNode != nullptr) {
                currentNode->setComment(first.comment);
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
        }
    }
}
