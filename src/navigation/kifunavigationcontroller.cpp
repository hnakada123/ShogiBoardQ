#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "shogiutils.h"

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
    qDebug().noquote() << "[KNC] goForward ENTER count=" << count;

    if (m_state == nullptr || m_tree == nullptr || count <= 0) {
        qDebug().noquote() << "[KNC] goForward: early return (null state/tree or count<=0)";
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        qDebug().noquote() << "[KNC] goForward: currentNode is null, returning";
        return;
    }

    qDebug().noquote() << "[KNC] goForward: starting from ply=" << node->ply()
                       << "sfen=" << node->sfen().left(40)
                       << "childCount=" << node->childCount();

    for (int i = 0; i < count && node->childCount() > 0; ++i) {
        node = findForwardNode();
        if (node == nullptr) {
            qDebug().noquote() << "[KNC] goForward: findForwardNode returned null at step" << i;
            break;
        }
        qDebug().noquote() << "[KNC] goForward: step" << i << "-> ply=" << node->ply()
                           << "sfen=" << node->sfen().left(40)
                           << "displayText=" << node->displayText();
        m_state->setCurrentNode(node);
    }

    qDebug().noquote() << "[KNC] goForward LEAVE";
    emitUpdateSignals();
}

KifuBranchNode* KifuNavigationController::findForwardNode() const
{
    qDebug().noquote() << "[KNC] findForwardNode ENTER";

    if (m_state == nullptr) {
        qDebug().noquote() << "[KNC] findForwardNode: m_state is null";
        return nullptr;
    }

    KifuBranchNode* current = m_state->currentNode();
    if (current == nullptr || current->childCount() == 0) {
        qDebug().noquote() << "[KNC] findForwardNode: current is null or no children";
        return nullptr;
    }

    qDebug().noquote() << "[KNC] findForwardNode: current ply=" << current->ply()
                       << "childCount=" << current->childCount();

    // 分岐がある場合、最後に選択したラインを優先
    int selectedLine = m_state->lastSelectedLineAt(current);
    qDebug().noquote() << "[KNC] findForwardNode: lastSelectedLineAt=" << selectedLine;

    if (selectedLine < current->childCount()) {
        KifuBranchNode* child = current->childAt(selectedLine);
        qDebug().noquote() << "[KNC] findForwardNode: using selectedLine"
                           << "childPly=" << (child ? child->ply() : -1)
                           << "childSfen=" << (child ? child->sfen().left(40) : "(null)");
        return child;
    }

    // フォールバック: 最初の子（本譜）
    KifuBranchNode* firstChild = current->childAt(0);
    qDebug().noquote() << "[KNC] findForwardNode: fallback to child(0)"
                       << "childPly=" << (firstChild ? firstChild->ply() : -1)
                       << "childSfen=" << (firstChild ? firstChild->sfen().left(40) : "(null)");
    return firstChild;
}

void KifuNavigationController::goToNode(KifuBranchNode* node)
{
    if (m_state == nullptr || node == nullptr) {
        return;
    }

    // ★ ルートからこのノードまでの全分岐点で選択を記憶
    // これにより、戻る→進むナビゲーション時に正しいパスを辿れる
    KifuBranchNode* current = node;
    while (current != nullptr) {
        KifuBranchNode* parent = current->parent();
        if (parent != nullptr && parent->childCount() > 1) {
            // 何番目の子かを探す
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->childAt(i) == current) {
                    m_state->rememberLineSelection(parent, i);
                    break;
                }
            }
        }
        current = parent;
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
    } else {
        // ★ 本譜の手を選択した場合は優先ラインをリセット
        m_state->resetPreferredLineIndex();
        qDebug().noquote() << "[KNC] selectBranchCandidate: resetPreferredLineIndex (main line selected)";
    }

    // ★ ライン選択変更を通知
    emit lineSelectionChanged(lineIndex);
    qDebug().noquote() << "[KNC] selectBranchCandidate: emitted lineSelectionChanged=" << lineIndex;

    goToNode(candidate);
}

void KifuNavigationController::goToMainLineAtCurrentPly()
{
    if (m_state == nullptr || m_tree == nullptr) {
        return;
    }

    // ★ 本譜に戻る場合、優先ラインと選択記憶をリセット
    m_state->resetPreferredLineIndex();
    m_state->clearLineSelectionMemory();
    qDebug().noquote() << "[KNC] goToMainLineAtCurrentPly: resetPreferredLineIndex and clearLineSelectionMemory";

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

void KifuNavigationController::handleBranchNodeActivated(int row, int ply)
{
    qDebug().noquote() << "[KNC] handleBranchNodeActivated ENTER row=" << row << "ply=" << ply;

    if (m_tree == nullptr || m_state == nullptr) {
        qDebug().noquote() << "[KNC] handleBranchNodeActivated: tree or state is null, cannot proceed";
        return;
    }

    QVector<BranchLine> lines = m_tree->allLines();
    if (lines.isEmpty()) {
        qDebug().noquote() << "[KNC] handleBranchNodeActivated: no lines available";
        return;
    }

    // 境界チェック
    if (row < 0 || row >= lines.size()) {
        qDebug().noquote() << "[KNC] handleBranchNodeActivated: row out of bounds (row=" << row << ", lines=" << lines.size() << ")";
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
        qDebug().noquote() << "[KNC] handleBranchNodeActivated LEAVE (root node)";
        return;
    }

    // ★ 共有ノードのクリック時はライン維持
    int effectiveRow = row;
    const int currentLine = m_state->currentLineIndex();
    if (currentLine > 0 && currentLine < lines.size()) {
        const BranchLine& currentBranchLine = lines.at(currentLine);
        if (ply < currentBranchLine.branchPly) {
            effectiveRow = currentLine;
            qDebug().noquote() << "[KNC] handleBranchNodeActivated: shared node clicked, keeping current line=" << currentLine;
        }
    }

    const BranchLine& line = lines.at(effectiveRow);
    const int maxPly = line.nodes.isEmpty() ? 0 : line.nodes.last()->ply();
    const int selPly = qBound(0, ply, maxPly);

    // ★ 分岐ラインを選択した場合、優先ラインを設定
    if (effectiveRow > 0) {
        m_state->setPreferredLineIndex(effectiveRow);
        qDebug().noquote() << "[KNC] handleBranchNodeActivated: setPreferredLineIndex=" << effectiveRow;
    } else {
        m_state->resetPreferredLineIndex();
        qDebug().noquote() << "[KNC] handleBranchNodeActivated: resetPreferredLineIndex (main line)";
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
        qDebug().noquote() << "[KNC] handleBranchNodeActivated: goToNode ply=" << selPly;

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
        qDebug().noquote() << "[KNC] handleBranchNodeActivated: node not found for ply=" << selPly;
    }

    qDebug().noquote() << "[KNC] handleBranchNodeActivated LEAVE";
}

void KifuNavigationController::emitUpdateSignals()
{
    qDebug().noquote() << "[KNC] emitUpdateSignals ENTER";

    if (m_state == nullptr) {
        qDebug().noquote() << "[KNC] emitUpdateSignals: m_state is null, returning";
        return;
    }

    KifuBranchNode* node = m_state->currentNode();
    if (node == nullptr) {
        qDebug().noquote() << "[KNC] emitUpdateSignals: currentNode is null, returning";
        return;
    }

    qDebug().noquote() << "[KNC] emitUpdateSignals: node ply=" << node->ply()
                       << "sfen=" << node->sfen()
                       << "displayText=" << node->displayText()
                       << "lineIndex=" << m_state->currentLineIndex();

    emit navigationCompleted(node);
    emit boardUpdateRequired(node->sfen());
    emit recordHighlightRequired(node->ply());
    emit branchTreeHighlightRequired(m_state->currentLineIndex(), node->ply());
    emit branchCandidatesUpdateRequired(m_state->branchCandidatesAtCurrent());

    qDebug().noquote() << "[KNC] emitUpdateSignals LEAVE";
}
