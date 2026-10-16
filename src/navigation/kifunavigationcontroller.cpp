/// @file kifunavigationcontroller.cpp
/// @brief 棋譜を移る利用者の操作の実装

#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"

#include <QPushButton>

KifuNavigationController::KifuNavigationController(KifuNavigationState* state, QObject* parent)
    : QObject(parent)
    , m_state(state)
{
}

void KifuNavigationController::connectButtons(const Buttons& buttons)
{
    const std::pair<QPushButton*, void (KifuNavigationController::*)()> pairs[] = {
        {buttons.first, &KifuNavigationController::onFirstClicked},
        {buttons.back10, &KifuNavigationController::onBack10Clicked},
        {buttons.prev, &KifuNavigationController::onPrevClicked},
        {buttons.next, &KifuNavigationController::onNextClicked},
        {buttons.fwd10, &KifuNavigationController::onFwd10Clicked},
        {buttons.last, &KifuNavigationController::onLastClicked},
    };
    for (const auto& [button, slot] : pairs) {
        if (button) connect(button, &QPushButton::clicked, this, slot);
    }
}

// ============================================================
// 移動
// ============================================================

// 分岐点での選択は足していくだけなので、戻ったり進んだりしても選んでいる手順は変わらない

void KifuNavigationController::goToFirst()
{
    goToNode(m_state->tree()->root());
}

void KifuNavigationController::goToLast()
{
    const auto path = m_state->selectedPath();
    if (!path.isEmpty()) m_state->setCurrentNode(path.constLast());
}

void KifuNavigationController::goBack(int count)
{
    KifuBranchNode* node = m_state->currentNode();
    if (!node || count <= 0) return;
    for (int i = 0; i < count && node->parent(); ++i) node = node->parent();
    m_state->setCurrentNode(node);
}

void KifuNavigationController::goForward(int count)
{
    KifuBranchNode* node = m_state->currentNode();
    if (!node || count <= 0) return;
    for (int i = 0; i < count; ++i) {
        KifuBranchNode* next = m_state->nextNode(node);
        if (!next) break;
        node = next;
    }
    m_state->setCurrentNode(node);
}

void KifuNavigationController::goToPly(int ply)
{
    const auto path = m_state->selectedPath();
    if (path.isEmpty()) return;
    m_state->setCurrentNode(path.at(qBound(0, ply, static_cast<int>(path.size()) - 1)));
}

void KifuNavigationController::goToNode(KifuBranchNode* node)
{
    if (node) m_state->setCurrentNode(node);
}

void KifuNavigationController::goToLine(int lineIndex, int ply)
{
    const auto lines = m_state->tree()->allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) return;
    const auto& nodes = lines.at(lineIndex).nodes;
    if (nodes.isEmpty()) return;
    // 分岐の手順を選んだので、その手順の終わりまでを以降の「進む」でたどる
    m_state->setCurrentNode(nodes.at(qBound(0, ply, static_cast<int>(nodes.size()) - 1)), nodes.constLast());
}

void KifuNavigationController::goToMainLineAtCurrentPly()
{
    const QList<KifuBranchNode*> mainNodes = m_state->tree()->mainLine();
    if (mainNodes.isEmpty()) return;
    const int ply = qBound(0, m_state->currentPly(), static_cast<int>(mainNodes.size()) - 1);
    m_state->resetToNode(mainNodes.at(ply));
}

// ============================================================
// 利用者の操作
// ============================================================

bool KifuNavigationController::confirmLeave()
{
    if (!m_leaveGuard || m_leaveGuard()) return true;
    emit navigationRejected();
    return false;
}

void KifuNavigationController::selectRecordRow(int row)
{
    const auto path = m_state->selectedPath();
    if (row < 0 || row >= path.size() || path.at(row) == m_state->currentNode()) return;
    if (!confirmLeave()) return;
    m_state->setCurrentNode(path.at(row));

}

void KifuNavigationController::selectBranchCandidateRow(int row)
{
    const QList<KifuBranchNode*> candidates = m_state->branchCandidatesAtCurrent();
    const bool backToMain = row == candidates.size() && m_state->offersBackToMainLine();
    if (!backToMain && (row < 0 || row >= candidates.size())) return;
    if (!confirmLeave()) return;
    if (backToMain) {
        goToMainLineAtCurrentPly();
        return;
    }
    // 候補の手順の終わりまでを選ぶ（その先も同じ分岐をたどる）
    KifuBranchNode* candidate = candidates.at(row);
    KifuBranchNode* end = candidate;
    while (KifuBranchNode* next = end->childAt(0)) end = next;
    m_state->setCurrentNode(candidate, end);
}

void KifuNavigationController::selectBranchTreeNode(int row, int ply)
{
    const auto lines = m_state->tree()->allLines();
    if (row < 0 || row >= lines.size()) return;
    if (!confirmLeave()) return;

    if (ply == 0) {
        m_state->goToRoot();
        return;
    }
    // 分岐前の共有ノードは親の行に描いてあるので、クリックした行と表示中の手順が違っても、
    // 同じ手数のノードが同じなら表示中の手順のまま移る
    const int currentLine = m_state->currentLineIndex();
    if (row != currentLine && currentLine >= 0 && currentLine < lines.size()) {
        const auto& clicked = lines.at(row).nodes;
        const auto& current = lines.at(currentLine).nodes;
        if (ply < clicked.size() && ply < current.size() && clicked.at(ply) == current.at(ply)) {
            goToLine(currentLine, ply);
            return;
        }
    }
    goToLine(row, ply);
}

void KifuNavigationController::selectPly(int ply)
{
    if (ply == m_state->currentPly() || !confirmLeave()) return;
    goToPly(ply);
}

void KifuNavigationController::onFirstClicked() { if (confirmLeave()) goToFirst(); }
void KifuNavigationController::onBack10Clicked() { if (confirmLeave()) goBack(10); }
void KifuNavigationController::onPrevClicked() { if (confirmLeave()) goBack(1); }
void KifuNavigationController::onNextClicked() { if (confirmLeave()) goForward(1); }
void KifuNavigationController::onFwd10Clicked() { if (confirmLeave()) goForward(10); }
void KifuNavigationController::onLastClicked() { if (confirmLeave()) goToLast(); }
