/// @file branchtreeeditcontroller.cpp
/// @brief 分岐ツリーの右クリックメニューから分岐を編集するコントローラの実装

#include "branchtreeeditcontroller.h"
#include "branchtreemanager.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "playmode.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogimove.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>

BranchTreeEditController::BranchTreeEditController(QObject* parent)
    : QObject(parent)
{
}

void BranchTreeEditController::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void BranchTreeEditController::attach(BranchTreeManager* manager)
{
    if (manager == nullptr) return;
    connect(manager, &BranchTreeManager::nodeContextMenuAboutToShow,
            this, &BranchTreeEditController::onNodeContextMenuAboutToShow, Qt::UniqueConnection);
}

bool BranchTreeEditController::canEdit() const
{
    if (m_deps.tree == nullptr || m_deps.tree->isEmpty()) return false;
    // 対局中はツリーに手を追加している最中なので編集しない
    return m_deps.liveSession == nullptr || !m_deps.liveSession->isActive();
}

KifuBranchNode* BranchTreeEditController::nodeAt(int row, int ply) const
{
    if (m_deps.tree == nullptr || row < 0 || ply < 0) return nullptr;
    const QList<BranchLine> lines = m_deps.tree->allLines();
    if (row >= lines.size()) return nullptr;
    const QList<KifuBranchNode*>& nodes = lines.at(row).nodes;
    return ply < nodes.size() ? nodes.at(ply) : nullptr;
}

bool BranchTreeEditController::isApplicable(Operation operation, KifuBranchNode* node) const
{
    if (node == nullptr || node->parent() == nullptr || !canEdit()) return false;

    switch (operation) {
    case Operation::PromoteToMainLine:
        for (const KifuBranchNode* n = node; n != nullptr && n->parent() != nullptr; n = n->parent()) {
            if (!n->isMainLine()) return true;
        }
        return false;
    case Operation::MoveUp:
    case Operation::MoveDown: {
        KifuBranchNode* head = KifuBranchTree::variationHead(node);
        if (head == nullptr) return false;
        const qsizetype index = head->parent()->children().indexOf(head);
        return operation == Operation::MoveUp ? index > 0 : index + 1 < head->parent()->childCount();
    }
    case Operation::DeleteFromHere:
        return true;
    }
    return false;
}

void BranchTreeEditController::onNodeContextMenuAboutToShow(int row, int ply, QMenu* menu)
{
    m_menuTargetNodeId = -1;
    if (menu == nullptr || !canEdit()) return;
    addEditActions(menu, nodeAt(row, ply));
}

void BranchTreeEditController::onCandidateContextMenuRequested(int row, const QPoint& globalPos)
{
    m_menuTargetNodeId = -1;
    if (!canEdit() || m_deps.navState == nullptr || row < 0) return;
    // 「本譜へ戻る」の行は候補の数より後ろにあり、対象のノードを持たない
    const QList<KifuBranchNode*> candidates = m_deps.navState->branchCandidatesAtCurrent();
    if (row >= candidates.size()) return;

    QMenu menu(m_deps.parentWidget);
    addEditActions(&menu, candidates.at(row));
    if (menu.isEmpty()) return;
    menu.exec(globalPos);
}

void BranchTreeEditController::addEditActions(QMenu* menu, KifuBranchNode* node)
{
    if (menu == nullptr || node == nullptr || node->parent() == nullptr) return;
    m_menuTargetNodeId = node->nodeId();

    const struct {
        Operation operation;
        QString text;
    } items[] = {
        {Operation::PromoteToMainLine, tr("この手順を本譜にする")},
        {Operation::MoveUp, tr("変化を上へ移動する")},
        {Operation::MoveDown, tr("変化を下へ移動する")},
        {Operation::DeleteFromHere, tr("この手以降を削除する…")},
    };
    for (const auto& item : items) {
        if (item.operation == Operation::DeleteFromHere) menu->addSeparator();
        QAction* action = menu->addAction(item.text);
        action->setData(static_cast<int>(item.operation));
        action->setEnabled(isApplicable(item.operation, node));
    }
    connect(menu, &QMenu::triggered, this, &BranchTreeEditController::onMenuActionTriggered, Qt::UniqueConnection);
}

void BranchTreeEditController::onMenuActionTriggered(QAction* action)
{
    if (action == nullptr || m_deps.tree == nullptr) return;
    bool ok = false;
    const int code = action->data().toInt(&ok);
    if (!ok || code < static_cast<int>(Operation::PromoteToMainLine)
        || code > static_cast<int>(Operation::DeleteFromHere)) {
        return;   // 表示設定など、このコントローラの項目ではない
    }
    KifuBranchNode* node = m_deps.tree->nodeAt(m_menuTargetNodeId);
    m_menuTargetNodeId = -1;
    apply(static_cast<Operation>(code), node);
}

bool BranchTreeEditController::confirmDeletion(KifuBranchNode* node) const
{
    const int count = KifuBranchTree::subtreeSize(node);
    QMessageBox box(QMessageBox::Question, tr("手順の削除"),
                    tr("「%1」以降の %2 手を削除します。この操作は取り消せません。\n削除しますか？")
                        .arg(node->displayText()).arg(count),
                    QMessageBox::Yes | QMessageBox::Cancel, m_deps.parentWidget);
    box.setDefaultButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Yes;
}

bool BranchTreeEditController::apply(Operation operation, KifuBranchNode* node, bool confirmDelete)
{
    if (!isApplicable(operation, node)) return false;
    KifuBranchTree* tree = m_deps.tree;
    KifuBranchNode* current = m_deps.navState ? m_deps.navState->currentNode() : nullptr;

    bool changed = false;
    switch (operation) {
    case Operation::PromoteToMainLine:
        changed = tree->promoteToMainLine(node);
        break;
    case Operation::MoveUp:
    case Operation::MoveDown: {
        KifuBranchNode* head = KifuBranchTree::variationHead(node);
        const int index = static_cast<int>(head->parent()->children().indexOf(head));
        changed = tree->moveChild(head, operation == Operation::MoveUp ? index - 1 : index + 1);
        break;
    }
    case Operation::DeleteFromHere: {
        if (confirmDelete && !confirmDeletion(node)) return false;
        // 現在の手が削除範囲にあれば、先に削除する手の直前へ移る
        bool currentInside = false;
        for (KifuBranchNode* n = current; n != nullptr; n = n->parent()) {
            if (n == node) {
                currentInside = true;
                break;
            }
        }
        if (currentInside) {
            if (m_deps.navController != nullptr && !m_deps.navController->canLeaveCurrentPosition()) return false;
            current = node->parent();
            if (m_deps.navController != nullptr) m_deps.navController->goToNode(current);
        }
        changed = tree->removeSubtree(node);
        break;
    }
    }

    if (!changed) return false;
    qCDebug(lcUi).noquote() << "BranchTreeEditController: applied operation" << static_cast<int>(operation);
    refreshAfterEdit(current);
    if (m_deps.markGameRecordDirty) m_deps.markGameRecordDirty();
    return true;
}

void BranchTreeEditController::refreshAfterEdit(KifuBranchNode* current)
{
    // ライン番号が変わるため、記憶している分岐の選択を捨てて現在の手から表示し直す
    if (m_deps.navState != nullptr) {
        m_deps.navState->resetPreferredLineIndex();
        m_deps.navState->clearLineSelectionMemory();
    }
    if (m_deps.navController != nullptr && current != nullptr) {
        m_deps.navController->goToNode(current);
    }
}

bool BranchTreeEditController::recordBoardMove(QPoint& from, QPoint& to, ShogiGameController* gc)
{
    if (gc == nullptr || gc->board() == nullptr || !canEdit()) return false;
    KifuBranchNode* parent = (m_deps.navState != nullptr) ? m_deps.navState->currentNode() : nullptr;
    if (parent == nullptr) parent = m_deps.tree->root();
    // 終局手（投了など）を表示中なら、その直前の局面から指したものとする
    if (parent != nullptr && parent->isTerminal()) parent = parent->parent();
    if (parent == nullptr || parent->sfen().isEmpty()) return false;

    // 合法判定・成りの確認・指し手表記の生成は対局と同じ処理を使う。
    // 対局の棋譜（SFEN履歴・指し手列）は変えず、作業用の記録に書き込む
    QString sfen = parent->sfen();
    gc->board()->setSfen(sfen);
    gc->setCurrentPlayer(sfen.section(QLatin1Char(' '), 1, 1) == QLatin1String("w")
                             ? ShogiGameController::Player2 : ShogiGameController::Player1);
    gc->setPreviousMoveDestination(parent->isActualMove()
        ? parent->move().toSquare + QPoint(1, 1) : QPoint());

    QStringList history{sfen};
    QList<ShogiMove> moves;
    QString record;
    PlayMode mode = PlayMode::HumanVsHuman;   // 成り・不成は利用者に確認する
    if (!gc->validateAndMove(from, to, record, mode, parent->ply() + 1, &history, moves)
        || moves.isEmpty() || history.size() < 2) {
        return false;
    }

    const ShogiMove& move = moves.last();
    const QString newSfen = history.last();
    KifuBranchNode* target = m_deps.tree->findMatchingChild(parent, move, record, newSfen);
    const bool created = (target == nullptr);
    if (created) {
        target = m_deps.tree->addMove(parent, move, record, newSfen);
        if (target == nullptr) return false;
    }
    qCDebug(lcUi).noquote() << "BranchTreeEditController: board move" << record
                            << (created ? "added as a new move" : "matched an existing move");

    // 以前にクリックした手順（ライン）ではなく、指した手を含む手順を表示する
    if (m_deps.navState != nullptr) m_deps.navState->resetPreferredLineIndex();
    if (m_deps.navController != nullptr) m_deps.navController->goToNode(target);
    if (created && m_deps.markGameRecordDirty) m_deps.markGameRecordDirty();
    return true;
}

