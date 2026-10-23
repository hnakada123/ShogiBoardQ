/// @file kifunavigationcontroller.cpp
/// @brief 棋譜を移る利用者の操作の実装

#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"

#include <QAbstractButton>
#include <QAction>

KifuNavigationController::KifuNavigationController(KifuNavigationState* state, QObject* parent)
    : QObject(parent)
    , m_state(state)
{
    struct Spec {
        QString text;
        void (KifuNavigationController::*slot)();
    };
    const Spec specs[StepCount] = {
        {tr("最初に戻る"), &KifuNavigationController::onFirstClicked},
        {tr("10手戻る"), &KifuNavigationController::onBack10Clicked},
        {tr("1手戻る"), &KifuNavigationController::onPrevClicked},
        {tr("1手進む"), &KifuNavigationController::onNextClicked},
        {tr("10手進む"), &KifuNavigationController::onFwd10Clicked},
        {tr("最後に進む"), &KifuNavigationController::onLastClicked},
    };
    for (std::size_t i = 0; i < m_actions.size(); ++i) {
        const Spec& spec = specs[i];
        auto* action = new QAction(spec.text, this);
        connect(action, &QAction::triggered, this, spec.slot);
        m_actions[i] = action;
    }
}

void KifuNavigationController::bindButtons(const Buttons& buttons)
{
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        QAbstractButton* button = buttons[i];
        QAction* stepAction = m_actions[i];
        if (!button) continue;
        connect(button, &QAbstractButton::clicked, stepAction, &QAction::trigger);
        connect(stepAction, &QAction::enabledChanged, button, &QWidget::setEnabled);
        button->setEnabled(stepAction->isEnabled());
    }
}

void KifuNavigationController::setStepActionsEnabled(bool on)
{
    for (QAction* const stepAction : m_actions) stepAction->setEnabled(on);
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

void KifuNavigationController::goToPlyOnLine(int lineEndNodeId, int ply)
{
    const KifuBranchTree* tree = m_state->tree();
    KifuBranchNode* node = tree->nodeOnLine(lineEndNodeId, ply);
    if (!node) return;
    // 手順を選んだので、その手順の終わりまでを以降の「進む」でたどる
    m_state->setCurrentNode(node, tree->nodeAt(lineEndNodeId));
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
    bool allowed = true;
    for (const std::function<bool()>& guard : std::as_const(m_leaveGuards)) {
        if (guard && !guard()) {
            allowed = false;
            break;
        }
    }
    if (allowed) return true;
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

void KifuNavigationController::selectBranchTreeNode(int nodeId)
{
    KifuBranchNode* node = m_state->tree()->nodeAt(nodeId);
    if (!node || !confirmLeave()) return;

    if (node->parent() == nullptr) {
        m_state->goToRoot();
        return;
    }
    // 分岐前の共有の手は親の手順の行に描いてあるので、表示中の手順にあればその手順のまま移る
    const QList<KifuBranchNode*> path = m_state->selectedPath();
    if (path.contains(node)) {
        m_state->setCurrentNode(node, path.constLast());
        return;
    }
    KifuBranchNode* lineEnd = node;
    while (lineEnd->childCount() > 0) lineEnd = lineEnd->childAt(0);
    m_state->setCurrentNode(node, lineEnd);
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
