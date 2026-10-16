/// @file branchtreeeditcontroller.cpp
/// @brief 分岐ツリーの右クリックメニューから分岐を編集するコントローラの実装

#include "branchtreeeditcontroller.h"
#include "branchtreemanager.h"
#include "dialogutils.h"
#include "gamerecordmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifupresentation.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogimove.h"

#include <QAction>
#include <QMenu>

BranchTreeEditController::BranchTreeEditController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.tree && m_deps.navState && m_deps.navController && m_deps.liveSession && m_deps.gameRecord);
}

bool BranchTreeEditController::canEdit() const
{
    // 対局中はツリーに手を追加している最中なので編集しない
    return !m_deps.liveSession->isActive();
}

bool BranchTreeEditController::canRecordBoardMove() const
{
    return !m_deps.liveSession->isActive();
}

KifuBranchNode* BranchTreeEditController::nodeAt(int row, int ply) const
{
    if (row < 0 || ply < 0) return nullptr;
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
    if (!canEdit() || row < 0) return;
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
    if (action == nullptr) return;
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
    // 手は分岐ツリーと同じ表記（英語式の表記・終局の訳）で示す
    const KifuBranchNode* parent = node->parent();
    const QString moveText = KifuPresentation::label(
        node->displayText(), parent ? parent->sfen() : QString(),
        node->isTerminal() ? QString() : KifuPresentation::usiMove(node->move()));
    // 「Yes」は KDE で訳されないため、操作名のボタンで尋ねる
    return DialogUtils::confirmAction(m_deps.parentWidget, tr("手順の削除"),
                                      tr("「%1」以降の %2 手を削除します。\nこの操作は取り消せません。")
                                          .arg(moveText).arg(count),
                                      tr("削除する"));
}

bool BranchTreeEditController::apply(Operation operation, KifuBranchNode* node, bool confirmDelete)
{
    if (!isApplicable(operation, node)) return false;
    KifuBranchTree* tree = m_deps.tree;
    KifuBranchNode* current = m_deps.navState->currentNode();

    if (operation == Operation::DeleteFromHere) {
        if (confirmDelete && !confirmDeletion(node)) return false;
        // 現在の手が削除範囲にあれば、削除する手の直前へ移る
        bool currentInside = false;
        for (const KifuBranchNode* n = current; n != nullptr; n = n->parent()) {
            if (n == node) {
                currentInside = true;
                break;
            }
        }
        if (currentInside) {
            if (!m_deps.navController->confirmLeave()) return false;
            current = node->parent();
        }
    }

    // 表示中の手を移してからツリーの変更を知らせる（表示は編集後のツリーで1回だけ描き直す）
    tree->beginBatchUpdate();
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
    case Operation::DeleteFromHere:
        changed = tree->removeSubtree(node);
        break;
    }
    // 選択経路はノードIDで保持され、並べ替え後も同じ手順を指す
    if (changed) m_deps.navController->goToNode(current);
    tree->endBatchUpdate();

    if (!changed) return false;
    qCDebug(lcUi).noquote() << "BranchTreeEditController: applied operation" << static_cast<int>(operation);
    m_deps.gameRecord->markDirty();
    return true;
}

bool BranchTreeEditController::recordBoardMove(QPoint& from, QPoint& to, ShogiGameController* gc)
{
    if (gc == nullptr || !canRecordBoardMove()) return false;

    KifuBranchNode* parent = m_deps.navState->currentNode();
    if (parent == nullptr) parent = m_deps.tree->root();
    // 終局手（投了など）を表示中なら、その直前の局面から指したものとする
    if (parent != nullptr && parent->isTerminal()) parent = parent->parent();
    if (parent == nullptr || parent->sfen().isEmpty()) return false;

    // 合法判定・成りの確認・指し手表記の生成は対局と同じ処理を使う
    gc->board()->setSfen(parent->sfen());

    gc->setPreviousMoveDestination(parent->isActualMove() ? parent->move().move.to : BoardSquare{});

    // 成り・不成は利用者に確認する
    const std::optional<RecordEntry> played = gc->validateAndMove(from, to);
    if (!played) return false;

    // 足した手へ移ってからツリーの変更を知らせる（盤は指した後の局面のまま、表示は1回だけ描き直す）
    m_deps.tree->beginBatchUpdate();
    KifuBranchNode* target = m_deps.tree->findMatchingChild(parent, *played);
    const bool created = (target == nullptr);
    if (created) target = m_deps.tree->addRecord(parent, *played);
    // 以前にクリックした手順（ライン）ではなく、指した手を含む手順を表示する
    if (target) m_deps.navState->resetToNode(target);
    m_deps.tree->endBatchUpdate();
    if (target == nullptr) return false;

    qCDebug(lcUi).noquote() << "BranchTreeEditController: board move" << played->displayText
                            << (created ? "added as a new move" : "matched an existing move");
    if (created) m_deps.gameRecord->markDirty();
    return true;
}

