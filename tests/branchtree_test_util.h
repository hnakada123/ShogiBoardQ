#ifndef BRANCHTREE_TEST_UTIL_H
#define BRANCHTREE_TEST_UTIL_H

/// @file branchtree_test_util.h
/// @brief テストで分岐ツリーを作る・引くための関数（製品のコードは addRecord と ID で扱う）

#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "recordentry.h"
#include "sfenutils.h"

#include <QList>
#include <QString>

namespace TestTree {

/// 指した手を足す（表記と局面はそのまま使う）
inline KifuBranchNode* addMove(KifuBranchTree& tree, KifuBranchNode* parent, const ShogiMove& move,
                               const QString& displayText, const QString& sfen, const QString& timeText = QString())
{
    return tree.addRecord(parent, RecordEntry::played(move, displayText, sfen, RecordTime::fromText(timeText)));
}

/// 終局の行を足す（表記は終局語の表から作る）
inline KifuBranchNode* addTerminalMove(KifuBranchTree& tree, KifuBranchNode* parent, TerminalType type, Turn side,
                                       const QString& timeText = QString())
{
    return tree.addRecord(parent, RecordEntry::ending(type, side, RecordTime::fromText(timeText)));
}

/// 本譜の ply 手目（無ければ nullptr）
inline KifuBranchNode* findByPlyOnMainLine(const KifuBranchTree& tree, int ply)
{
    for (KifuBranchNode* node = tree.root(); node != nullptr; node = node->childAt(0)) {
        if (node->ply() == ply) return node;
    }
    return nullptr;
}

/**
 * @brief 局面が一致する手を探す（手数の欄は比べない。ply >= 0 なら手数でも絞る）
 *
 * ルートから本譜を優先する深さ優先順で探し、最初に一致した手を返す。
 * 終局の行は親と同じ局面を持つため対象外。
 */
inline KifuBranchNode* findBySfen(const KifuBranchTree& tree, const QString& sfen, int ply = -1)
{
    if (tree.root() == nullptr || sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() < 3) return nullptr;
    const QString target = SfenUtils::normalizeSfenKey(sfen);
    QList<KifuBranchNode*> stack{tree.root()};
    while (!stack.isEmpty()) {
        KifuBranchNode* node = stack.takeLast();
        if (!node->isTerminal() && (ply < 0 || node->ply() == ply)
            && node->sfen().split(QLatin1Char(' '), Qt::SkipEmptyParts).size() >= 3
            && SfenUtils::normalizeSfenKey(node->sfen()) == target) {
            return node;
        }
        for (int i = node->childCount() - 1; i >= 0; --i) stack.append(node->childAt(i));
    }
    return nullptr;
}

} // namespace TestTree

#endif // BRANCHTREE_TEST_UTIL_H
