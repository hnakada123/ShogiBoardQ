/// @file kifubranchnode.cpp
/// @brief 分岐ツリーノードクラスの実装

#include "kifubranchnode.h"

// ============================================================
// 初期化
// ============================================================

KifuBranchNode::KifuBranchNode() = default;

KifuBranchNode::~KifuBranchNode()
{
    // 子ノードは所有しない（KifuBranchTreeが一括管理）
}

// ============================================================
// ツリー構造操作
// ============================================================

void KifuBranchNode::addChild(KifuBranchNode* child)
{
    if (child && !m_children.contains(child)) {
        m_children.append(child);
        child->setParent(this);
    }
}

void KifuBranchNode::removeChild(KifuBranchNode* child)
{
    if (m_children.removeOne(child)) {
        child->setParent(nullptr);
    }
}

bool KifuBranchNode::moveChild(KifuBranchNode* child, int index)
{
    const qsizetype from = m_children.indexOf(child);
    if (from < 0 || index < 0 || index >= m_children.size()) {
        return false;
    }
    if (from != index) {
        m_children.move(from, index);
    }
    return true;
}

KifuBranchNode* KifuBranchNode::childAt(int index) const
{
    if (index >= 0 && index < m_children.size()) {
        return m_children.at(index);
    }
    return nullptr;
}

// ============================================================
// クエリ
// ============================================================

bool KifuBranchNode::isMainLine() const
{
    if (m_parent == nullptr) {
        return true;
    }
    // 親の最初の子が自分なら本譜
    const auto& siblings = m_parent->children();
    return !siblings.isEmpty() && siblings.first() == this;
}

int KifuBranchNode::lineIndex() const
{
    // ルートから辿って、最初の分岐点での子インデックスを返す
    // 本譜（常に最初の子）を辿っていれば0、分岐があれば1以降

    QList<const KifuBranchNode*> path;
    const KifuBranchNode* node = this;
    while (node != nullptr) {
        path.prepend(node);
        node = node->parent();
    }

    for (int i = 0; i < path.size() - 1; ++i) {
        const KifuBranchNode* current = path.at(i);
        const KifuBranchNode* next = path.at(i + 1);

        if (current->childCount() > 1) {
            const auto& children = current->children();
            for (int j = 0; j < children.size(); ++j) {
                if (children.at(j) == next) {
                    return j;
                }
            }
        }
    }

    return 0;
}

QList<KifuBranchNode*> KifuBranchNode::siblings() const
{
    QList<KifuBranchNode*> result;
    if (m_parent == nullptr) {
        return result;
    }

    for (KifuBranchNode* child : std::as_const(m_parent->m_children)) {
        if (child != this) {
            result.append(child);
        }
    }
    return result;
}
