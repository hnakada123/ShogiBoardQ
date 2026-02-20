/// @file kifunavigationcontroller.cpp
/// @brief 棋譜ナビゲーションコントローラクラスの実装

#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "shogiutils.h"

#include <QLoggingCategory>
#include <QPushButton>

Q_LOGGING_CATEGORY(lcNavigation, "shogi.navigation")

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
    qCDebug(lcNavigation).noquote() << "goToLast ENTER currentPly=" << (m_state ? m_state->currentPly() : -1);

    if (m_state == nullptr || m_tree == nullptr) {
        qCDebug(lcNavigation).noquote() << "goToLast: early return (null state/tree)";
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        qCDebug(lcNavigation).noquote() << "goToLast: currentNode is null, returning";
        return;
    }

    // 優先ラインのパスを取得（allLines() に基づく高速ルックアップ用）
    QSet<KifuBranchNode*> linePathNodes;
    if (m_tree != nullptr) {
        int lineIdx = m_state->preferredLineIndex();
        if (lineIdx < 0) lineIdx = m_state->currentLineIndex();
        if (lineIdx >= 0) {
            QVector<BranchLine> lines = m_tree->allLines();
            if (lineIdx < lines.size()) {
                for (KifuBranchNode* n : std::as_const(lines.at(lineIdx).nodes)) {
                    linePathNodes.insert(n);
                }
            }
        }
    }

    // 現在のラインの終端まで進む
    while (node->childCount() > 0) {
        KifuBranchNode* nextNode = nullptr;

        // 優先ラインのパスに合致する子を探す
        if (!linePathNodes.isEmpty() && node->childCount() > 1) {
            for (int i = 0; i < node->childCount(); ++i) {
                if (linePathNodes.contains(node->childAt(i))) {
                    nextNode = node->childAt(i);
                    qCDebug(lcNavigation).noquote() << "goToLast: at ply=" << node->ply()
                                                    << "using allLines path childIndex=" << i;
                    break;
                }
            }
        }

        // フォールバック: lastSelectedLineAt または先頭の子
        if (nextNode == nullptr) {
            int selectedLine = m_state->lastSelectedLineAt(node);
            qCDebug(lcNavigation).noquote() << "goToLast: at ply=" << node->ply()
                                            << "childCount=" << node->childCount()
                                            << "selectedLine=" << selectedLine;
            if (selectedLine < node->childCount()) {
                nextNode = node->childAt(selectedLine);
            } else {
                nextNode = node->childAt(0);
            }
        }

        node = nextNode;
    }

    m_state->setCurrentNode(node);
    qCDebug(lcNavigation).noquote() << "goToLast LEAVE ply=" << node->ply();
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
    qCDebug(lcNavigation).noquote() << "findForwardNode ENTER";

    if (m_state == nullptr) {
        qCDebug(lcNavigation).noquote() << "findForwardNode: m_state is null";
        return nullptr;
    }

    KifuBranchNode* current = m_state->currentNode();
    if (current == nullptr || current->childCount() == 0) {
        qCDebug(lcNavigation).noquote() << "findForwardNode: current is null or no children";
        return nullptr;
    }

    qCDebug(lcNavigation).noquote() << "findForwardNode: current ply=" << current->ply()
                                    << "childCount=" << current->childCount();

    // 優先ラインが設定されている場合、allLines() のパスに合致する子を優先する。
    // lastSelectedLineAt は child index であり allLines() のライン path と一致しない
    // ケースがあるため、分岐がある場合はライン path を正とする。
    if (current->childCount() > 1 && m_tree != nullptr) {
        int lineIdx = m_state->preferredLineIndex();
        if (lineIdx < 0) lineIdx = m_state->currentLineIndex();
        if (lineIdx >= 0) {
            QVector<BranchLine> lines = m_tree->allLines();
            if (lineIdx < lines.size()) {
                const BranchLine& line = lines.at(lineIdx);
                const int nextPly = current->ply() + 1;
                for (KifuBranchNode* node : std::as_const(line.nodes)) {
                    if (node->ply() == nextPly && node->parent() == current) {
                        qCDebug(lcNavigation).noquote() << "findForwardNode: using allLines path"
                                                        << "lineIdx=" << lineIdx
                                                        << "childPly=" << node->ply()
                                                        << "childDisplayText=" << node->displayText();
                        return node;
                    }
                }
                qCDebug(lcNavigation).noquote() << "findForwardNode: allLines path did not find child on line" << lineIdx;
            }
        }
    }

    // 分岐がある場合、最後に選択したラインを優先
    int selectedLine = m_state->lastSelectedLineAt(current);
    qCDebug(lcNavigation).noquote() << "findForwardNode: lastSelectedLineAt=" << selectedLine;

    if (selectedLine < current->childCount()) {
        KifuBranchNode* child = current->childAt(selectedLine);
        qCDebug(lcNavigation).noquote() << "findForwardNode: using selectedLine"
                                        << "childPly=" << (child ? child->ply() : -1)
                                        << "childSfen=" << (child ? child->sfen().left(40) : "(null)");
        return child;
    }

    // フォールバック: 最初の子（本譜）
    KifuBranchNode* firstChild = current->childAt(0);
    qCDebug(lcNavigation).noquote() << "findForwardNode: fallback to child(0)"
                                    << "childPly=" << (firstChild ? firstChild->ply() : -1)
                                    << "childSfen=" << (firstChild ? firstChild->sfen().left(40) : "(null)");
    return firstChild;
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
    KifuBranchNode* current = node;
    while (current != nullptr) {
        KifuBranchNode* parent = current->parent();
        if (parent != nullptr && parent->childCount() > 1) {
            // 何番目の子かを探す
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->childAt(i) == current) {
                    m_state->rememberLineSelection(parent, i);
                    qCDebug(lcNavigation).noquote() << "goToNode: remembered branch at parentPly="
                                                    << parent->ply() << "childIndex=" << i;
                    break;
                }
            }
        }
        current = parent;
    }

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
    qCDebug(lcNavigation).noquote() << "selectBranchCandidate ENTER candidateIndex=" << candidateIndex;

    if (m_state == nullptr) {
        qCDebug(lcNavigation).noquote() << "selectBranchCandidate: m_state is null, returning";
        return;
    }

    QVector<KifuBranchNode*> candidates = m_state->branchCandidatesAtCurrent();
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
    const int lineIndex = m_tree ? m_tree->findLineIndexForNode(candidate) : candidate->lineIndex();
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

    // 本譜に戻る場合、優先ラインと選択記憶をリセット
    m_state->resetPreferredLineIndex();
    m_state->clearLineSelectionMemory();

    int currentPly = m_state->currentPly();
    KifuBranchNode* mainNode = m_tree->findByPlyOnMainLine(currentPly);

    if (mainNode != nullptr) {
        m_state->setCurrentNode(mainNode);
        qCDebug(lcNavigation).noquote() << "goToMainLineAtCurrentPly LEAVE ply=" << mainNode->ply();
        emitUpdateSignals();
    } else {
        qCDebug(lcNavigation).noquote() << "goToMainLineAtCurrentPly: mainNode not found for ply=" << currentPly;
    }
}

// ============================================================
// ボタンスロット
// ============================================================

void KifuNavigationController::onFirstClicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] first clicked";
    goToFirst();
}

void KifuNavigationController::onBack10Clicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] back10 clicked";
    goBack(10);
}

void KifuNavigationController::onPrevClicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] prev clicked";
    goBack(1);
}

void KifuNavigationController::onNextClicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] next clicked";
    goForward(1);
}

void KifuNavigationController::onFwd10Clicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] fwd10 clicked";
    goForward(10);
}

void KifuNavigationController::onLastClicked(bool checked)
{
    Q_UNUSED(checked)
    qCDebug(lcNavigation).noquote() << "[BUTTON] last clicked";
    goToLast();
}

// ============================================================
// 分岐ツリー操作
// ============================================================

void KifuNavigationController::handleBranchNodeActivated(int row, int ply)
{
    qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated ENTER row=" << row << "ply=" << ply;

    if (m_tree == nullptr || m_state == nullptr) {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: tree or state is null, cannot proceed";
        return;
    }

    QVector<BranchLine> lines = m_tree->allLines();
    if (lines.isEmpty()) {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: no lines available";
        return;
    }

    // 境界チェック
    if (row < 0 || row >= lines.size()) {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: row out of bounds (row=" << row << ", lines=" << lines.size() << ")";
        return;
    }

    // ply=0の場合（開始局面）は常にルートノードを使用
    if (ply == 0) {
        KifuBranchNode* targetNode = m_tree->root();
        if (targetNode != nullptr) {
            m_state->resetPreferredLineIndex();
            goToNode(targetNode);
            const QString sfen = targetNode->sfen().isEmpty() ? QString() : targetNode->sfen();
            emit branchNodeHandled(0, sfen, 0, 0, QString());
        }
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated LEAVE (root node)";
        return;
    }

    // 分岐前の共有ノードをクリックした場合は現在のライン上にとどまる。
    // ただし、入れ子分岐では branchPly が「最後の分岐点」になるため、
    // 手数比較だけでは共有ノード判定を誤る。実際に main/current が同一ノードかを比較する。
    int effectiveRow = row;
    const int currentLine = m_state->currentLineIndex();
    if (row == 0 && currentLine > 0 && currentLine < lines.size()) {
        const BranchLine& mainLine = lines.at(0);
        const BranchLine& currentBranchLine = lines.at(currentLine);
        auto findNodeAtPly = [](const BranchLine& targetLine, int targetPly) -> KifuBranchNode* {
            for (KifuBranchNode* node : std::as_const(targetLine.nodes)) {
                if (node != nullptr && node->ply() == targetPly) {
                    return node;
                }
            }
            return nullptr;
        };

        const KifuBranchNode* mainNodeAtPly = findNodeAtPly(mainLine, ply);
        const KifuBranchNode* currentNodeAtPly = findNodeAtPly(currentBranchLine, ply);
        if (mainNodeAtPly != nullptr && mainNodeAtPly == currentNodeAtPly) {
            effectiveRow = currentLine;
            qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: shared node clicked on main line, keeping current line=" << currentLine;
        }
    }

    const BranchLine& line = lines.at(effectiveRow);
    const int maxPly = line.nodes.isEmpty() ? 0 : line.nodes.last()->ply();
    const int selPly = qBound(0, ply, maxPly);

    // 分岐ラインを選択した場合、以降のナビゲーションで優先されるよう設定する
    if (effectiveRow > 0) {
        m_state->setPreferredLineIndex(effectiveRow);
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: setPreferredLineIndex=" << effectiveRow;
    } else {
        m_state->resetPreferredLineIndex();
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: resetPreferredLineIndex (main line)";
    }

    // 対応するノードを探してナビゲート
    KifuBranchNode* targetNode = nullptr;
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == selPly) {
            targetNode = node;
            break;
        }
    }

    if (targetNode == nullptr && selPly == 0) {
        targetNode = m_tree->root();
    }

    if (targetNode != nullptr) {
        goToNode(targetNode);
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: goToNode ply=" << selPly;

        const QString sfen = targetNode->sfen().isEmpty() ? QString() : targetNode->sfen();

        // 検討モード用: 移動先座標とUSI表記を取得
        int fileTo = 0;
        int rankTo = 0;
        QString usiMove;
        if (targetNode->isActualMove()) {
            const ShogiMove& move = targetNode->move();
            // movingPieceが空白の場合はデフォルト構築された無効な指し手（KIF分岐でgameMoves未設定時）
            if (move.movingPiece != QLatin1Char(' ')) {
                fileTo = move.toSquare.x();
                rankTo = move.toSquare.y();
                usiMove = ShogiUtils::moveToUsi(move);
            }
        }

        emit branchNodeHandled(selPly, sfen, fileTo, rankTo, usiMove);
    } else {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: node not found for ply=" << selPly;
    }

    qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated LEAVE";
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

    qCDebug(lcNavigation).noquote() << "emitUpdateSignals LEAVE";
}
