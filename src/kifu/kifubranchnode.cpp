/// @file kifubranchnode.cpp
/// @brief 分岐ツリーノードクラスの実装

#include "kifubranchnode.h"

#include <algorithm>

KifuBranchNode::KifuBranchNode() = default;

KifuBranchNode::~KifuBranchNode()
{
    // 長い手順でも解放が深い再帰にならないよう、子孫を外してから1つずつ解放する
    std::vector<std::unique_ptr<KifuBranchNode>> pending = std::move(m_children);
    while (!pending.empty()) {
        std::unique_ptr<KifuBranchNode> node = std::move(pending.back());
        pending.pop_back();
        for (auto& child : node->m_children) pending.push_back(std::move(child));
        node->m_children.clear();
    }
}

// ============================================================
// ツリー構造
// ============================================================

QList<KifuBranchNode*> KifuBranchNode::children() const
{
    QList<KifuBranchNode*> result;
    result.reserve(static_cast<qsizetype>(m_children.size()));
    for (const auto& child : m_children) result.append(child.get());
    return result;
}

KifuBranchNode* KifuBranchNode::childAt(int index) const
{
    if (index < 0 || index >= childCount()) return nullptr;
    return m_children.at(static_cast<std::size_t>(index)).get();
}

int KifuBranchNode::indexOfChild(const KifuBranchNode* child) const
{
    const auto it = std::find_if(m_children.cbegin(), m_children.cend(),
                                 [child](const std::unique_ptr<KifuBranchNode>& c) { return c.get() == child; });
    return it == m_children.cend() ? -1 : static_cast<int>(it - m_children.cbegin());
}

KifuBranchNode* KifuBranchNode::appendChild(std::unique_ptr<KifuBranchNode> child)
{
    child->m_parent = this;
    child->m_ply = m_ply + 1;
    m_children.push_back(std::move(child));
    return m_children.back().get();
}

std::unique_ptr<KifuBranchNode> KifuBranchNode::takeChild(const KifuBranchNode* child)
{
    const int index = indexOfChild(child);
    if (index < 0) return nullptr;
    const auto it = m_children.begin() + index;
    std::unique_ptr<KifuBranchNode> taken = std::move(*it);
    m_children.erase(it);
    taken->m_parent = nullptr;
    return taken;
}

bool KifuBranchNode::moveChild(const KifuBranchNode* child, int index)
{
    const int from = indexOfChild(child);
    if (from < 0 || index < 0 || index >= childCount()) return false;
    const auto first = m_children.begin();
    if (from < index) std::rotate(first + from, first + from + 1, first + index + 1);
    else if (from > index) std::rotate(first + index, first + from, first + from + 1);
    return true;
}

// ============================================================
// クエリ
// ============================================================

bool KifuBranchNode::isMainLine() const
{
    // 親の最初の子が自分なら本譜
    return m_parent == nullptr || m_parent->childAt(0) == this;
}
