/// @file kifunavigationstate.cpp
/// @brief 表示中の手と分岐の選択をノードIDで持つ

#include "kifunavigationstate.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"

#include <utility>

KifuNavigationState::KifuNavigationState(KifuBranchTree* tree, QObject* parent)
    : QObject(parent)
    , m_tree(tree)
{
    Q_ASSERT(m_tree);
    connect(m_tree, &KifuBranchTree::nodeAppended, this, &KifuNavigationState::onTreeChanged);
    connect(m_tree, &KifuBranchTree::childrenReordered, this, &KifuNavigationState::onTreeChanged);
    connect(m_tree, &KifuBranchTree::subtreeRemoved, this, &KifuNavigationState::onTreeChanged);
    connect(m_tree, &KifuBranchTree::reset, this, &KifuNavigationState::onTreeChanged);
    m_currentNodeId = m_tree->root() ? m_tree->root()->nodeId() : -1;
    m_notifiedRevision = m_tree->revision();
}

void KifuNavigationState::onTreeChanged()
{
    // ノード ID は再利用されないので、消えた手を指したままにはならない。消えていたら開始局面へ
    if (currentNode() == nullptr) {
        goToRoot();
        return;
    }
    // 手順の分岐が増えた・並びが変わったときも表示を合わせる（その構造をもう知らせていれば繰り返さない）
    if (m_tree->revision() == m_notifiedRevision) return;
    m_notifiedRevision = m_tree->revision();
    emit changed();
}

KifuBranchNode* KifuNavigationState::currentNode() const
{
    return m_tree->nodeAt(m_currentNodeId);
}

int KifuNavigationState::currentPly() const
{
    const auto* node = currentNode();
    return node ? node->ply() : 0;
}

QString KifuNavigationState::currentSfen() const
{
    const auto* node = currentNode();
    return node ? node->sfen() : QString();
}

QList<KifuBranchNode*> KifuNavigationState::selectedPath() const
{
    QList<KifuBranchNode*> path;
    for (auto* node = m_tree->root(); node; node = nextNode(node)) path.append(node);
    return path;
}

int KifuNavigationState::currentLineIndex() const
{
    const auto path = selectedPath();
    return path.isEmpty() ? 0 : m_tree->findLineIndexForNode(path.last()).value_or(0);
}

bool KifuNavigationState::isOnMainLine() const
{
    return currentLineIndex() == 0;
}

RecordLine KifuNavigationState::currentRecordLine() const
{
    return KifuBranchTree::toRecordLine(selectedPath(), currentPly());
}

std::optional<UsiPosition> KifuNavigationState::currentUsiPosition() const
{
    const RecordLine line = currentRecordLine();
    if (line.isEmpty()) return UsiPosition(Position::hirate());
    return line.usiPosition(qBound(0, line.currentPly, line.moveCount()));
}

KifuBranchNode* KifuNavigationState::nextNode(const KifuBranchNode* node) const
{
    if (!node) return nullptr;
    const int selectedId = m_selectedChildIds.value(node->nodeId(), -1);
    for (int i = 0; i < node->childCount(); ++i) {
        if (node->childAt(i)->nodeId() == selectedId) return node->childAt(i);
    }
    return node->childAt(0);
}

QList<KifuBranchNode*> KifuNavigationState::branchCandidatesAtCurrent() const
{
    const auto* node = currentNode();
    const auto* parent = node ? node->parent() : nullptr;
    return parent && parent->childCount() > 1 ? parent->children() : QList<KifuBranchNode*>{};
}

bool KifuNavigationState::offersBackToMainLine() const
{
    return !branchCandidatesAtCurrent().isEmpty() && !isOnMainLine();
}

void KifuNavigationState::rememberPath(KifuBranchNode* node)
{
    for (auto* child = node; child && child->parent(); child = child->parent()) {
        m_selectedChildIds.insert(child->parent()->nodeId(), child->nodeId());
    }
}

void KifuNavigationState::apply(int nodeId, QHash<int, int> selections)
{
    if (nodeId == m_currentNodeId && selections == m_selectedChildIds && m_tree->revision() == m_notifiedRevision) {
        return;
    }
    m_currentNodeId = nodeId;
    m_selectedChildIds = std::move(selections);
    m_notifiedRevision = m_tree->revision();
    emit changed();
}

void KifuNavigationState::setCurrentNode(KifuBranchNode* node)
{
    setCurrentNode(node, node);
}

void KifuNavigationState::setCurrentNode(KifuBranchNode* node, KifuBranchNode* lineEnd)
{
    const QHash<int, int> before = m_selectedChildIds;
    rememberPath(lineEnd ? lineEnd : node);
    QHash<int, int> selections = std::exchange(m_selectedChildIds, before);
    apply(node ? node->nodeId() : -1, std::move(selections));
}

void KifuNavigationState::resetToNode(KifuBranchNode* node)
{
    const QHash<int, int> before = std::exchange(m_selectedChildIds, {});
    rememberPath(node);
    QHash<int, int> selections = std::exchange(m_selectedChildIds, before);
    apply(node ? node->nodeId() : -1, std::move(selections));
}

void KifuNavigationState::goToRoot()
{
    resetToNode(m_tree->root());
}
