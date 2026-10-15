/// @file kifunavigationcontroller.cpp
/// @brief 棋譜ナビゲーションコントローラクラスの実装

#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"

#include "logcategories.h"

#include <QPushButton>

// ============================================================
// 初期化
// ============================================================

KifuNavigationController::KifuNavigationController(QObject* parent)
    : QObject(parent)
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

// ============================================================
// ナビゲーション操作
// ============================================================

void KifuNavigationController::goToFirst()
{
    qCDebug(lcNavigation).noquote() << "goToFirst ENTER currentPly=" << (m_state ? m_state->currentPly() : -1);

    if (m_state == nullptr || m_tree == nullptr) {
        qCDebug(lcNavigation).noquote() << "goToFirst: early return (null state/tree)";
        return;
    }

    m_state->goToRoot();
    qCDebug(lcNavigation).noquote() << "goToFirst LEAVE ply=" << m_state->currentPly();
    emitUpdateSignals();
}

void KifuNavigationController::goToLast()
{
    if (!m_state || !m_state->currentNode()) return;
    auto* node = m_state->currentNode();
    while (auto* next = m_state->nextNode(node)) node = next;
    m_state->setCurrentNode(node);
    emitUpdateSignals();
}

void KifuNavigationController::goBack(int count)
{
    qCDebug(lcNavigation).noquote() << "goBack ENTER count=" << count
                                    << "currentPly=" << (m_state ? m_state->currentPly() : -1);

    if (m_state == nullptr || m_tree == nullptr || count <= 0) {
        qCDebug(lcNavigation).noquote() << "goBack: early return (null state/tree or count<=0)";
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        qCDebug(lcNavigation).noquote() << "goBack: currentNode is null, returning";
        return;
    }

    for (int i = 0; i < count && node->parent() != nullptr; ++i) {
        node = node->parent();
    }

    m_state->setCurrentNode(node);
    qCDebug(lcNavigation).noquote() << "goBack LEAVE resultPly=" << node->ply();
    emitUpdateSignals();
}

void KifuNavigationController::goForward(int count)
{
    qCDebug(lcNavigation).noquote() << "goForward ENTER count=" << count;

    if (m_state == nullptr || m_tree == nullptr || count <= 0) {
        qCDebug(lcNavigation).noquote() << "goForward: early return (null state/tree or count<=0)";
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        qCDebug(lcNavigation).noquote() << "goForward: currentNode is null, returning";
        return;
    }

    qCDebug(lcNavigation).noquote() << "goForward: starting from ply=" << node->ply()
                                    << "sfen=" << node->sfen().left(40)
                                    << "childCount=" << node->childCount();

    for (int i = 0; i < count && node->childCount() > 0; ++i) {
        node = findForwardNode();
        if (node == nullptr) {
            qCDebug(lcNavigation).noquote() << "goForward: findForwardNode returned null at step" << i;
            break;
        }
        qCDebug(lcNavigation).noquote() << "goForward: step" << i << "-> ply=" << node->ply()
                                        << "sfen=" << node->sfen().left(40)
                                        << "displayText=" << node->displayText();
        m_state->setCurrentNode(node);
    }

    qCDebug(lcNavigation).noquote() << "goForward LEAVE";
    emitUpdateSignals();
}

KifuBranchNode* KifuNavigationController::findForwardNode() const
{
    return m_state ? m_state->nextNode(m_state->currentNode()) : nullptr;
}

void KifuNavigationController::goToNode(KifuBranchNode* node)
{
    qCDebug(lcNavigation).noquote() << "goToNode ENTER ply=" << (node ? node->ply() : -1)
                                    << "displayText=" << (node ? node->displayText() : "(null)");

    if (m_state == nullptr || node == nullptr) {
        qCDebug(lcNavigation).noquote() << "goToNode: early return (null state or node)";
        return;
    }

    // 戻る→進むナビゲーション時に正しいパスを辿れるよう、全分岐点で選択を記憶する
    m_state->rememberPathSelections(node);

    m_state->setCurrentNode(node);
    qCDebug(lcNavigation).noquote() << "goToNode LEAVE ply=" << node->ply()
                                    << "lineIndex=" << m_state->currentLineIndex();
    emitUpdateSignals();
}

void KifuNavigationController::goToPly(int ply)
{
    qCDebug(lcNavigation).noquote() << "goToPly ENTER ply=" << ply
                                    << "currentPly=" << (m_state ? m_state->currentPly() : -1);

    if (m_state == nullptr || m_tree == nullptr) {
        qCDebug(lcNavigation).noquote() << "goToPly: early return (null state/tree)";
        return;
    }

    KifuBranchNode* current = m_state->currentNode();
    if (current == nullptr) {
        qCDebug(lcNavigation).noquote() << "goToPly: currentNode is null, returning";
        return;
    }

    int currentPly = current->ply();

    if (ply < currentPly) {
        // 戻る
        qCDebug(lcNavigation).noquote() << "goToPly: going back by" << (currentPly - ply);
        goBack(currentPly - ply);
    } else if (ply > currentPly) {
        // 進む
        qCDebug(lcNavigation).noquote() << "goToPly: going forward by" << (ply - currentPly);
        goForward(ply - currentPly);
    } else {
        qCDebug(lcNavigation).noquote() << "goToPly: same ply, no action";
    }
}

void KifuNavigationController::selectBranchCandidate(int candidateIndex)
{
    qCDebug(lcNavigation).noquote() << "selectBranchCandidate ENTER candidateIndex=" << candidateIndex;

    if (m_state == nullptr) {
        qCDebug(lcNavigation).noquote() << "selectBranchCandidate: m_state is null, returning";
        return;
    }

    QList<KifuBranchNode*> candidates = m_state->branchCandidatesAtCurrent();
    qCDebug(lcNavigation).noquote() << "selectBranchCandidate: candidates.size()=" << candidates.size()
                                    << "currentNode=" << (m_state->currentNode() ? "yes" : "null")
                                    << "currentPly=" << m_state->currentPly();

    if (candidateIndex < 0 || candidateIndex >= candidates.size()) {
        qCDebug(lcNavigation).noquote() << "selectBranchCandidate: candidateIndex out of range, returning";
        return;
    }

    KifuBranchNode* candidate = candidates.at(candidateIndex);
    qCDebug(lcNavigation).noquote() << "selectBranchCandidate: navigating to candidate"
                                    << "ply=" << candidate->ply()
                                    << "displayText=" << candidate->displayText();

    // node->lineIndex()は最初の分岐点のインデックスしか返さないため、
    // ツリーから実際のラインインデックスを取得して優先ラインとして記憶する
    const int lineIndex = m_tree ? m_tree->findLineIndexForNode(candidate).value_or(candidate->lineIndex())
                                : candidate->lineIndex();
    if (lineIndex > 0) {
        m_state->setPreferredLineIndex(lineIndex);
        qCDebug(lcNavigation).noquote() << "selectBranchCandidate: setPreferredLineIndex=" << lineIndex;
    } else {
        // 本譜の手を選択した場合は優先ラインをリセット
        m_state->resetPreferredLineIndex();
        qCDebug(lcNavigation).noquote() << "selectBranchCandidate: resetPreferredLineIndex (main line selected)";
    }

    // ライン選択変更を通知
    emit lineSelectionChanged(lineIndex);
    qCDebug(lcNavigation).noquote() << "selectBranchCandidate: emitted lineSelectionChanged=" << lineIndex;

    goToNode(candidate);
}

void KifuNavigationController::goToMainLineAtCurrentPly()
{
    qCDebug(lcNavigation).noquote() << "goToMainLineAtCurrentPly ENTER"
                                    << "currentPly=" << (m_state ? m_state->currentPly() : -1)
                                    << "currentLineIndex=" << (m_state ? m_state->currentLineIndex() : -1);

    if (m_state == nullptr || m_tree == nullptr) {
        qCDebug(lcNavigation).noquote() << "goToMainLineAtCurrentPly: early return (null state/tree)";
        return;
    }

    const int currentPly = m_state->currentPly();
    KifuBranchNode* mainNode = m_tree->findByPlyOnMainLine(currentPly);

    // 本譜が現在手数より短い場合は本譜の最終手へ移動する
    if (mainNode == nullptr) {
        const QList<KifuBranchNode*> mainNodes = m_tree->mainLine();
        if (!mainNodes.isEmpty()) {
            mainNode = mainNodes.last();
            qCDebug(lcNavigation).noquote() << "goToMainLineAtCurrentPly: main line shorter than ply="
                                            << currentPly << ", using last main node ply=" << mainNode->ply();
        }
    }

    if (mainNode == nullptr) {
        // 移動先が無い場合は状態を変えずに終了する
        qCDebug(lcNavigation).noquote() << "goToMainLineAtCurrentPly: main line is empty, no action";
        return;
    }

    // 本譜に戻るので、優先ラインと選択記憶をリセットしてから移動する
    m_state->resetPreferredLineIndex();
    m_state->clearLineSelectionMemory();
    m_state->setCurrentNode(mainNode);
    qCDebug(lcNavigation).noquote() << "goToMainLineAtCurrentPly LEAVE ply=" << mainNode->ply();
    emitUpdateSignals();
}

// ============================================================
// ボタンスロット
// ============================================================

void KifuNavigationController::onFirstClicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] first clicked";
    if (!canLeaveCurrentPosition()) return;
    goToFirst();
}

void KifuNavigationController::onBack10Clicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] back10 clicked";
    if (!canLeaveCurrentPosition()) return;
    goBack(10);
}

void KifuNavigationController::onPrevClicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] prev clicked";
    if (!canLeaveCurrentPosition()) return;
    goBack(1);
}

void KifuNavigationController::onNextClicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] next clicked";
    if (!canLeaveCurrentPosition()) return;
    goForward(1);
}

void KifuNavigationController::onFwd10Clicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] fwd10 clicked";
    if (!canLeaveCurrentPosition()) return;
    goForward(10);
}

void KifuNavigationController::onLastClicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] last clicked";
    if (!canLeaveCurrentPosition()) return;
    goToLast();
}

void KifuNavigationController::emitUpdateSignals()
{
    qCDebug(lcNavigation).noquote() << "emitUpdateSignals ENTER";

    if (m_state == nullptr) {
        qCDebug(lcNavigation).noquote() << "emitUpdateSignals: m_state is null, returning";
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        qCDebug(lcNavigation).noquote() << "emitUpdateSignals: currentNode is null, returning";
        return;
    }

    qCDebug(lcNavigation).noquote() << "emitUpdateSignals: node ply=" << node->ply()
                                    << "sfen=" << node->sfen()
                                    << "displayText=" << node->displayText()
                                    << "lineIndex=" << m_state->currentLineIndex();

    emit navigationCompleted(node);
    emit boardUpdateRequired(node->sfen());
    emit recordHighlightRequired(node->ply());
    emit branchTreeHighlightRequired(m_state->currentLineIndex(), node->ply());
    emit branchCandidatesUpdateRequired(m_state->branchCandidatesAtCurrent());

    // 移動経路によらず、検討エンジンにも移動先の局面と直前手を通知する。
    int fileTo = 0;
    int rankTo = 0;
    QString usiMove;
    if (node->isActualMove() && node->move().movingPiece != Piece::None) {
        const ShogiMove& move = node->move();
        fileTo = move.toSquare.x();
        rankTo = move.toSquare.y();
        usiMove = move.toUsiMove().toString();
    }
    emit branchNodeHandled(node->ply(), node->sfen(), fileTo, rankTo, usiMove);

    qCDebug(lcNavigation).noquote() << "emitUpdateSignals LEAVE";
}
