/// @file kifunavigationstate.cpp
/// @brief 現在位置と分岐の選択をノードIDで管理する

#include "kifunavigationstate.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"

KifuNavigationState::KifuNavigationState(QObject* parent) : QObject(parent) {}

void KifuNavigationState::setTree(KifuBranchTree* tree)
{
    m_tree = tree;
    m_currentNodeId = -1;
    m_selectedChildIds.clear();
    goToRoot();
}

KifuBranchNode* KifuNavigationState::currentNode() const
{
    return m_tree ? m_tree->nodeAt(m_currentNodeId) : nullptr;
}

int KifuNavigationState::currentPly() const
{
    const auto* node = currentNode();
    return node ? node->ply() : 0;
}

QList<KifuBranchNode*> KifuNavigationState::selectedPath() const
{
    QList<KifuBranchNode*> path;
    for (auto* node = m_tree ? m_tree->root() : nullptr; node; node = nextNode(node)) path.append(node);
    return path;
}

int KifuNavigationState::currentLineIndex() const
{
    const auto path = selectedPath();
    return path.isEmpty() ? 0 : m_tree->findLineIndexForNode(path.last()).value_or(0);
}

int KifuNavigationState::preferredLineIndex() const
{
    const int index = currentLineIndex();
    return index > 0 ? index : -1;
}

void KifuNavigationState::setPreferredLineIndex(int lineIndex)
{
    if (!m_tree) return;
    const auto lines = m_tree->allLines();
    if (lineIndex < 0 || lineIndex >= lines.size() || lines.at(lineIndex).nodes.isEmpty()) return;
    rememberPathSelections(lines.at(lineIndex).nodes.last());
}

void KifuNavigationState::resetPreferredLineIndex()
{
    clearLineSelectionMemory();
}

void KifuNavigationState::rememberPathSelections(KifuBranchNode* node)
{
    for (auto* child = node; child && child->parent(); child = child->parent()) {
        m_selectedChildIds.insert(child->parent()->nodeId(), child->nodeId());
    }
}

void KifuNavigationState::clearLineSelectionMemory()
{
    m_selectedChildIds.clear();
}

QString KifuNavigationState::currentSfen() const
{
    const auto* node = currentNode();
    return node ? node->sfen() : QString();
}

void KifuNavigationState::setCurrentNode(KifuBranchNode* node)
{
    m_currentNodeId = node ? node->nodeId() : -1;
    rememberPathSelections(node);
}

void KifuNavigationState::goToRoot()
{
    setCurrentNode(m_tree ? m_tree->root() : nullptr);
}

bool KifuNavigationState::isOnMainLine() const { return currentLineIndex() == 0; }
bool KifuNavigationState::canGoForward() const { return nextNode(currentNode()) != nullptr; }
bool KifuNavigationState::canGoBack() const { return currentNode() && currentNode()->parent(); }

QList<KifuBranchNode*> KifuNavigationState::branchCandidatesAtCurrent() const
{
    const auto* node = currentNode();
    const auto* parent = node ? node->parent() : nullptr;
    return parent && parent->childCount() > 1 ? parent->children() : QList<KifuBranchNode*>{};
}

bool KifuNavigationState::hasBranchAtCurrent() const
{
    return !branchCandidatesAtCurrent().isEmpty();
}

void KifuNavigationState::rememberChildSelection(KifuBranchNode* branchPoint, int childIndex)
{
    if (!branchPoint) return;
    if (auto* child = branchPoint->childAt(childIndex))
        m_selectedChildIds.insert(branchPoint->nodeId(), child->nodeId());
}

KifuBranchNode* KifuNavigationState::nextNode(KifuBranchNode* node) const
{
    if (!node) return nullptr;
    const int selectedId = m_selectedChildIds.value(node->nodeId(), -1);
    for (auto* child : node->children()) {
        if (child->nodeId() == selectedId) return child;
    }
    return node->childAt(0);
}

int KifuNavigationState::lastSelectedChildAt(KifuBranchNode* branchPoint) const
{
    if (!branchPoint) return 0;
    const auto* selected = nextNode(branchPoint);
    return selected ? static_cast<int>(branchPoint->children().indexOf(selected)) : 0;
}

RecordLine KifuNavigationState::currentRecordLine() const
{
    return KifuBranchTree::toRecordLine(selectedPath(), currentPly());
}
