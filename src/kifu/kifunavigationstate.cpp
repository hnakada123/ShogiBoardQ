/// @file kifunavigationstate.cpp
/// @brief 棋譜ナビゲーション状態管理クラスの実装
/// @todo remove コメントスタイルガイド適用済み

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
    m_preferredLineIndex = -1;  // ツリー設定時にリセット

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

    // 優先ラインが設定されている場合は、それを返す
    // これにより、分岐を選択した後は分岐点より前に戻っても、
    // または別のサブブランチにいても、選択したラインを維持できる
    if (m_preferredLineIndex > 0) {
        return m_preferredLineIndex;
    }

    // 優先ラインが設定されていない場合は、ツリーから正確なインデックスを取得
    // KifuBranchNode::lineIndex() は最初の分岐点での子インデックスのみを返すため
    // allLines() の順序（DFS順）と一致しない問題がある
    if (m_tree != nullptr) {
        int lineIndex = m_tree->findLineIndexForNode(m_currentNode);
        if (lineIndex >= 0) {
            return lineIndex;
        }
    }

    // フォールバック: 本譜
    return 0;
}

void KifuNavigationState::setPreferredLineIndex(int lineIndex)
{
    m_preferredLineIndex = lineIndex;
}

void KifuNavigationState::resetPreferredLineIndex()
{
    m_preferredLineIndex = -1;
}

void KifuNavigationState::clearLineSelectionMemory()
{
    m_lastSelectedLineAtBranch.clear();
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
    if (m_currentNode == nullptr) return result;

    KifuBranchNode* parent = m_currentNode->parent();
    if (parent == nullptr) return result;

    // 分岐候補は「親ノードの子」＝現在手の兄弟（同じ手数での別の指し手）
    if (parent->childCount() > 1) {
        result.reserve(parent->childCount());
        for (int i = 0; i < parent->childCount(); ++i) {
            result.append(parent->childAt(i));
        }
    }

    return result;
}

bool KifuNavigationState::hasBranchAtCurrent() const
{
    if (m_currentNode == nullptr) return false;
    KifuBranchNode* parent = m_currentNode->parent();
    if (parent == nullptr) return false;
    return parent->childCount() > 1;
}

void KifuNavigationState::rememberLineSelection(KifuBranchNode* branchPoint, int lineIndex)
{
    if (branchPoint == nullptr) {
        qDebug().noquote() << "[KNS] rememberLineSelection: branchPoint is null, returning";
        return;
    }
    qDebug().noquote() << "[KNS] rememberLineSelection: nodeId=" << branchPoint->nodeId()
                       << "ply=" << branchPoint->ply()
                       << "lineIndex=" << lineIndex
                       << "(childCount=" << branchPoint->childCount() << ")";
    m_lastSelectedLineAtBranch.insert(branchPoint->nodeId(), lineIndex);
}

int KifuNavigationState::lastSelectedLineAt(KifuBranchNode* branchPoint) const
{
    if (branchPoint == nullptr) {
        qDebug().noquote() << "[KNS] lastSelectedLineAt: branchPoint is null, returning 0";
        return 0;
    }
    const int result = m_lastSelectedLineAtBranch.value(branchPoint->nodeId(), 0);
    qDebug().noquote() << "[KNS] lastSelectedLineAt: nodeId=" << branchPoint->nodeId()
                       << "ply=" << branchPoint->ply()
                       << "childCount=" << branchPoint->childCount()
                       << "result=" << result
                       << "(map size=" << m_lastSelectedLineAtBranch.size() << ")";
    return result;
}

void KifuNavigationState::clearLineSelections()
{
    m_lastSelectedLineAtBranch.clear();
}
