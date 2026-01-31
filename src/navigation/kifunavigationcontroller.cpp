#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"

#include <QPushButton>

KifuNavigationController::KifuNavigationController(QObject* parent)
    : QObject(parent)
    , m_tree(nullptr)
    , m_state(nullptr)
{
}

void KifuNavigationController::setTreeAndState(KifuBranchTree* tree, KifuNavigationState* state)
{
    m_tree = tree;
    m_state = state;

    if (m_state != nullptr) {
        m_state->setTree(tree);
    }
}

void KifuNavigationController::connectButtons(const Buttons& buttons)
{
    if (buttons.first != nullptr) {
        connect(buttons.first, &QPushButton::clicked,
                this, &KifuNavigationController::onFirstClicked);
    }
    if (buttons.back10 != nullptr) {
        connect(buttons.back10, &QPushButton::clicked,
                this, &KifuNavigationController::onBack10Clicked);
    }
    if (buttons.prev != nullptr) {
        connect(buttons.prev, &QPushButton::clicked,
                this, &KifuNavigationController::onPrevClicked);
    }
    if (buttons.next != nullptr) {
        connect(buttons.next, &QPushButton::clicked,
                this, &KifuNavigationController::onNextClicked);
    }
    if (buttons.fwd10 != nullptr) {
        connect(buttons.fwd10, &QPushButton::clicked,
                this, &KifuNavigationController::onFwd10Clicked);
    }
    if (buttons.last != nullptr) {
        connect(buttons.last, &QPushButton::clicked,
                this, &KifuNavigationController::onLastClicked);
    }
}

void KifuNavigationController::goToFirst()
{
    if (m_state == nullptr || m_tree == nullptr) {
        return;
    }

    m_state->goToRoot();
    emitUpdateSignals();
}

void KifuNavigationController::goToLast()
{
    if (m_state == nullptr || m_tree == nullptr) {
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        return;
    }

    // 現在のラインの終端まで進む
    while (node->childCount() > 0) {
        // 最後に選択したラインがあればそれを使う
        int selectedLine = m_state->lastSelectedLineAt(node);
        if (selectedLine < node->childCount()) {
            node = node->childAt(selectedLine);
        } else {
            node = node->childAt(0);
        }
    }

    m_state->setCurrentNode(node);
    emitUpdateSignals();
}

void KifuNavigationController::goBack(int count)
{
    if (m_state == nullptr || m_tree == nullptr || count <= 0) {
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        return;
    }

    for (int i = 0; i < count && node->parent() != nullptr; ++i) {
        node = node->parent();
    }

    m_state->setCurrentNode(node);
    emitUpdateSignals();
}

void KifuNavigationController::goForward(int count)
{
    if (m_state == nullptr || m_tree == nullptr || count <= 0) {
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        return;
    }

    for (int i = 0; i < count && node->childCount() > 0; ++i) {
        node = findForwardNode();
        if (node == nullptr) {
            break;
        }
        m_state->setCurrentNode(node);
    }

    emitUpdateSignals();
}

KifuBranchNode* KifuNavigationController::findForwardNode() const
{
    if (m_state == nullptr) {
        return nullptr;
    }

    KifuBranchNode* current = m_state->currentNode();
    if (current == nullptr || current->childCount() == 0) {
        return nullptr;
    }

    // 分岐がある場合、最後に選択したラインを優先
    int selectedLine = m_state->lastSelectedLineAt(current);
    if (selectedLine < current->childCount()) {
        return current->childAt(selectedLine);
    }

    // フォールバック: 最初の子（本譜）
    return current->childAt(0);
}

void KifuNavigationController::goToNode(KifuBranchNode* node)
{
    if (m_state == nullptr || node == nullptr) {
        return;
    }

    // 分岐を選択した場合、その選択を記憶
    KifuBranchNode* parent = node->parent();
    if (parent != nullptr && parent->childCount() > 1) {
        // 何番目の子かを探す
        for (int i = 0; i < parent->childCount(); ++i) {
            if (parent->childAt(i) == node) {
                m_state->rememberLineSelection(parent, i);
                break;
            }
        }
    }

    m_state->setCurrentNode(node);
    emitUpdateSignals();
}

void KifuNavigationController::goToPly(int ply)
{
    if (m_state == nullptr || m_tree == nullptr) {
        return;
    }

    KifuBranchNode* current = m_state->currentNode();
    if (current == nullptr) {
        return;
    }

    int currentPly = current->ply();

    if (ply < currentPly) {
        // 戻る
        goBack(currentPly - ply);
    } else if (ply > currentPly) {
        // 進む
        goForward(ply - currentPly);
    }
    // 同じplyなら何もしない
}

void KifuNavigationController::switchToLine(int lineIndex)
{
    if (m_state == nullptr || m_tree == nullptr) {
        return;
    }

    // 全ラインを取得して、指定インデックスのラインの最終ノードに移動
    QVector<BranchLine> lines = m_tree->allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) {
        return;
    }

    const BranchLine& line = lines.at(lineIndex);
    if (line.nodes.isEmpty()) {
        return;
    }

    // 現在のplyと同じ位置に移動（可能であれば）
    int currentPly = m_state->currentPly();
    KifuBranchNode* targetNode = nullptr;

    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == currentPly) {
            targetNode = node;
            break;
        }
    }

    if (targetNode == nullptr) {
        // 同じplyがない場合は最も近いノードを探す
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->ply() <= currentPly) {
                targetNode = node;
            }
        }
    }

    if (targetNode == nullptr && !line.nodes.isEmpty()) {
        // フォールバック: ラインの最初のノード
        targetNode = line.nodes.first();
    }

    if (targetNode != nullptr) {
        m_state->setCurrentNode(targetNode);
        emitUpdateSignals();
    }
}

void KifuNavigationController::selectBranchCandidate(int candidateIndex)
{
    qDebug().noquote() << "[KNC] selectBranchCandidate ENTER candidateIndex=" << candidateIndex;

    if (m_state == nullptr) {
        qDebug().noquote() << "[KNC] selectBranchCandidate: m_state is null, returning";
        return;
    }

    QVector<KifuBranchNode*> candidates = m_state->branchCandidatesAtCurrent();
    qDebug().noquote() << "[KNC] selectBranchCandidate: candidates.size()=" << candidates.size()
                       << "currentNode=" << (m_state->currentNode() ? "yes" : "null")
                       << "currentPly=" << m_state->currentPly();

    if (candidateIndex < 0 || candidateIndex >= candidates.size()) {
        qDebug().noquote() << "[KNC] selectBranchCandidate: candidateIndex out of range, returning";
        return;
    }

    KifuBranchNode* candidate = candidates.at(candidateIndex);
    qDebug().noquote() << "[KNC] selectBranchCandidate: navigating to candidate"
                       << "ply=" << candidate->ply()
                       << "displayText=" << candidate->displayText();

    // ★ 分岐を選択した場合、そのラインを優先ラインとして記憶
    // node->lineIndex()は最初の分岐点のインデックスしか返さないため、
    // ツリーから実際のラインインデックスを取得する
    const int lineIndex = m_tree ? m_tree->findLineIndexForNode(candidate) : candidate->lineIndex();
    if (lineIndex > 0) {
        m_state->setPreferredLineIndex(lineIndex);
        qDebug().noquote() << "[KNC] selectBranchCandidate: setPreferredLineIndex=" << lineIndex;
    }

    goToNode(candidate);
}

void KifuNavigationController::goToMainLineAtCurrentPly()
{
    if (m_state == nullptr || m_tree == nullptr) {
        return;
    }

    // ★ 本譜に戻る場合、優先ラインをリセット
    m_state->resetPreferredLineIndex();
    qDebug().noquote() << "[KNC] goToMainLineAtCurrentPly: resetPreferredLineIndex";

    int currentPly = m_state->currentPly();
    KifuBranchNode* mainNode = m_tree->findByPlyOnMainLine(currentPly);

    if (mainNode != nullptr) {
        m_state->setCurrentNode(mainNode);
        emitUpdateSignals();
    }
}

void KifuNavigationController::onFirstClicked(bool checked)
{
    Q_UNUSED(checked)
    goToFirst();
}

void KifuNavigationController::onBack10Clicked(bool checked)
{
    Q_UNUSED(checked)
    goBack(10);
}

void KifuNavigationController::onPrevClicked(bool checked)
{
    Q_UNUSED(checked)
    goBack(1);
}

void KifuNavigationController::onNextClicked(bool checked)
{
    Q_UNUSED(checked)
    goForward(1);
}

void KifuNavigationController::onFwd10Clicked(bool checked)
{
    Q_UNUSED(checked)
    goForward(10);
}

void KifuNavigationController::onLastClicked(bool checked)
{
    Q_UNUSED(checked)
    goToLast();
}

void KifuNavigationController::emitUpdateSignals()
{
    if (m_state == nullptr) {
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        return;
    }

    emit navigationCompleted(node);
    emit boardUpdateRequired(node->sfen());
    emit recordHighlightRequired(node->ply());
    emit branchTreeHighlightRequired(m_state->currentLineIndex(), node->ply());
    emit branchCandidatesUpdateRequired(m_state->branchCandidatesAtCurrent());
}
