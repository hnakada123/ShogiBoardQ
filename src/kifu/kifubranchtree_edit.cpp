/// @file kifubranchtree_edit.cpp
/// @brief 分岐ツリーの編集（本譜にする・並べ替え・削除）の実装

#include "kifubranchtree.h"

#include <memory>

// ============================================================
// 分岐の編集
// ============================================================

bool KifuBranchTree::moveChild(KifuBranchNode* child, int index)
{
    if (child == nullptr || child->parent() == nullptr) return false;
    if (child->parent()->children().indexOf(child) == index) return false;
    if (!moveChildQuiet(child, index)) return false;
    notifyTreeChanged();
    return true;
}

bool KifuBranchTree::promoteToMainLine(KifuBranchNode* node)
{
    if (node == nullptr || nodeAt(node->nodeId()) != node) return false;
    bool changed = false;
    const QList<KifuBranchNode*> path = pathToNode(node);
    for (KifuBranchNode* n : path) {
        if (n->parent() != nullptr && !n->isMainLine()) {
            changed = moveChildQuiet(n, 0) || changed;
        }
    }
    if (changed) notifyTreeChanged();
    return changed;
}

bool KifuBranchTree::removeSubtree(KifuBranchNode* node)
{
    if (node == nullptr || node == m_root || node->parent() == nullptr || nodeAt(node->nodeId()) != node) {
        return false;
    }
    node->parent()->removeChild(node);

    // 子のリストは非所有なので、子を取り出してから親を解放する
    QList<KifuBranchNode*> pending{node};
    while (!pending.isEmpty()) {
        KifuBranchNode* current = pending.takeLast();
        pending.append(current->children());
        std::unique_ptr<KifuBranchNode> removed(m_nodeById.take(current->nodeId()));
    }
    m_linesCache.clear();
    invalidateLineCache();
    notifyTreeChanged();
    return true;
}

int KifuBranchTree::subtreeSize(const KifuBranchNode* node)
{
    if (node == nullptr) return 0;
    int count = 0;
    QList<const KifuBranchNode*> pending{node};
    while (!pending.isEmpty()) {
        const KifuBranchNode* current = pending.takeLast();
        ++count;
        for (const KifuBranchNode* child : current->children()) pending.append(child);
    }
    return count;
}

KifuBranchNode* KifuBranchTree::variationHead(KifuBranchNode* node)
{
    for (KifuBranchNode* n = node; n != nullptr && n->parent() != nullptr; n = n->parent()) {
        if (n->parent()->childCount() > 1) return n;
    }
    return nullptr;
}
