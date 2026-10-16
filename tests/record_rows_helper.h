#ifndef RECORD_ROWS_HELPER_H
#define RECORD_ROWS_HELPER_H

/// @file record_rows_helper.h
/// @brief 棋譜欄のモデル（行は分岐ツリーのノード）に、テスト用の行を末尾から足していく組み立て

#include <QList>
#include <QString>

#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifurecordlistmodel.h"
#include "sfenutils.h"
#include "shogimove.h"

/// 開始局面の行から始まる棋譜欄。足した行は表示中の手順になる
struct RecordRows {
    KifuBranchTree tree;
    QList<KifuBranchNode*> path;
    KifuRecordListModel model{&tree};

    explicit RecordRows(const QString& startSfen = SfenUtils::hirateSfen()) { reset(startSfen); }

    /// 開始局面の行だけにする（行を全部入れ替える）
    void reset(const QString& startSfen = SfenUtils::hirateSfen())
    {
        tree.setRootSfen(startSfen);
        path = {tree.root()};
        model.setPath(path);
    }

    /// 末尾に1行足す。move を指定すると、表記は手の局面（開始局面から指した手）で作る
    KifuBranchNode* append(const QString& text, const QString& time = QString(), const QString& comment = QString(),
                           const QString& bookmark = QString(), const ShogiMove& move = ShogiMove())
    {
        KifuBranchNode* node = TestTree::addMove(tree, path.last(), move, text, path.last()->sfen(), time);
        node->record().comment = comment;
        node->record().bookmark = bookmark;
        path.append(node);
        model.setPath(path);
        return node;
    }

    /// row 行目に分岐の印（「+」）を付ける（同じ親に別の手を足す）
    void markBranch(int row)
    {
        KifuBranchNode* parent = path.at(row)->parent();
        TestTree::addMove(tree, parent, ShogiMove(), QStringLiteral("▲９六歩(97)"), parent->sfen());
        model.refreshAll();
    }
};

#endif // RECORD_ROWS_HELPER_H
