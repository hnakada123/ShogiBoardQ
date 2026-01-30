#include "kifunavigationstate.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"

KifuNavigationState::KifuNavigationState(QObject* parent)
    : QObject(parent)
    , m_tree(nullptr)
    , m_currentNode(nullptr)
{
}

void KifuNavigationState::setTree(KifuBranchTree* tree)
{
    m_tree = tree;
    m_currentNode = nullptr;
    m_lastSelectedLineAtBranch.clear();

    if (m_tree != nullptr && m_tree->root() != nullptr) {
        setCurrentNode(m_tree->root());
    }
}

int KifuNavigationState::currentPly() const
{
    if (m_currentNode == nullptr) {
        return 0;
    }
    return m_currentNode->ply();
}

int KifuNavigationState::currentLineIndex() const
{
    if (m_currentNode == nullptr) {
        return 0;
    }
    return m_currentNode->lineIndex();
}

QString KifuNavigationState::currentLineName() const
{
    if (m_currentNode == nullptr) {
        return QStringLiteral("本譜");
    }
    return m_currentNode->lineName();
}

QString KifuNavigationState::currentSfen() const
{
    if (m_currentNode == nullptr) {
        return QString();
    }
    return m_currentNode->sfen();
}

void KifuNavigationState::setCurrentNode(KifuBranchNode* node)
{
    if (m_currentNode == node) {
        return;
    }

    KifuBranchNode* oldNode = m_currentNode;
    int oldLineIndex = currentLineIndex();

    m_currentNode = node;

    int newLineIndex = currentLineIndex();

    emit currentNodeChanged(m_currentNode, oldNode);

    if (newLineIndex != oldLineIndex) {
        emit lineChanged(newLineIndex, oldLineIndex);
    }
}

void KifuNavigationState::goToRoot()
{
    if (m_tree != nullptr) {
        setCurrentNode(m_tree->root());
    }
}

bool KifuNavigationState::isOnMainLine() const
{
    if (m_currentNode == nullptr) {
        return true;
    }
    return m_currentNode->isMainLine();
}

bool KifuNavigationState::canGoForward() const
{
    if (m_currentNode == nullptr) {
        return false;
    }
    // 子がいれば進める
    return m_currentNode->childCount() > 0;
}

bool KifuNavigationState::canGoBack() const
{
    if (m_currentNode == nullptr) {
        return false;
    }
    // 親がいれば戻れる
    return m_currentNode->parent() != nullptr;
}

int KifuNavigationState::maxPlyOnCurrentLine() const
{
    if (m_currentNode == nullptr || m_tree == nullptr) {
        return 0;
    }

    // 現在のノードから終端まで辿って最大plyを取得
    KifuBranchNode* node = m_currentNode;

    // まず、現在のノードが属するラインの終端を探す
    // 現在位置から最初の子を辿り続ける
    while (node->childCount() > 0) {
        // 最後に選択したラインがあればそれを使う
        int selectedLine = lastSelectedLineAt(node);
        if (selectedLine < node->childCount()) {
            node = node->childAt(selectedLine);
        } else {
            node = node->childAt(0);  // フォールバック
        }
    }

    return node->ply();
}

QVector<KifuBranchNode*> KifuNavigationState::branchCandidatesAtCurrent() const
{
    QVector<KifuBranchNode*> result;
    if (m_currentNode == nullptr || m_tree == nullptr) {
        return result;
    }

    // 現在ノードの子が分岐候補
    if (m_currentNode->childCount() > 1) {
        for (int i = 0; i < m_currentNode->childCount(); ++i) {
            result.append(m_currentNode->childAt(i));
        }
    }

    return result;
}

bool KifuNavigationState::hasBranchAtCurrent() const
{
    if (m_currentNode == nullptr) {
        return false;
    }
    return m_currentNode->childCount() > 1;
}

void KifuNavigationState::rememberLineSelection(KifuBranchNode* branchPoint, int lineIndex)
{
    if (branchPoint == nullptr) {
        return;
    }
    m_lastSelectedLineAtBranch.insert(branchPoint->nodeId(), lineIndex);
}

int KifuNavigationState::lastSelectedLineAt(KifuBranchNode* branchPoint) const
{
    if (branchPoint == nullptr) {
        return 0;
    }
    return m_lastSelectedLineAtBranch.value(branchPoint->nodeId(), 0);
}

void KifuNavigationState::clearLineSelections()
{
    m_lastSelectedLineAtBranch.clear();
}
