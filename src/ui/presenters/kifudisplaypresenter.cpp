/// @file kifudisplaypresenter.cpp
/// @brief 棋譜欄・分岐候補欄・分岐ツリーへの表示の反映

#include "kifudisplaypresenter.h"
#include "branchtreeview.h"
#include "kifubranchlistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"

#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QTableView>

#include <utility>

namespace {

/// 表の1行を描き直す（表示領域に掛からなければ何もしない）
void updateRow(QTableView* view, int row)
{
    if (row < 0 || !view->model() || row >= view->model()->rowCount(view->rootIndex())) return;
    QWidget* area = view->viewport();
    const int top = view->rowViewportPosition(row);
    const int height = view->rowHeight(row);
    if (top + height > 0 && top < area->height()) area->update(0, top, area->width(), height);
}

} // namespace

KifuDisplayPresenter::KifuDisplayPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.navState && m_deps.liveSession && m_deps.recordModel && m_deps.branchModel);
    connect(m_deps.navState, &KifuNavigationState::changed, this, &KifuDisplayPresenter::refresh);
    connect(m_deps.liveSession, &LiveGameSession::activeChanged, this, &KifuDisplayPresenter::refresh);
    KifuBranchTree* tree = m_deps.navState->tree();
    connect(tree, &KifuBranchTree::nodeAppended, this, &KifuDisplayPresenter::onNodeAppended);
    connect(tree, &KifuBranchTree::childrenReordered, this, &KifuDisplayPresenter::refreshBranchMarks);
    connect(tree, &KifuBranchTree::subtreeRemoved, this, &KifuDisplayPresenter::refreshBranchMarks);
    connect(tree, &KifuBranchTree::reset, this, &KifuDisplayPresenter::refreshBranchMarks);
    if (m_deps.treeView) m_deps.treeView->setTree(tree);
}

void KifuDisplayPresenter::refreshBranchMarks()
{
    m_deps.recordModel->refreshAll();
}

void KifuDisplayPresenter::onNodeAppended(int parentId)
{
    // 分岐の印（「+」と行の色）は、その手の親に子が2つ以上あるかで決まる（KifuRecordListModel::isBranchRow）。
    // 手順の末尾に1手足しただけ（親の子がその1手だけ）なら、どの行の印も変わらない。対局中は1手ごとにここへ
    // 来るので、そのたびに全行の変更を知らせない（表が全行を描き直し、列幅も全行ぶん測り直す）
    const KifuBranchNode* parent = m_deps.navState->tree()->nodeAt(parentId);
    if (parent && !parent->hasBranch()) return;
    refreshBranchMarks();
}

void KifuDisplayPresenter::refresh()
{
    KifuNavigationState* nav = m_deps.navState;
    KifuBranchTree* tree = nav->tree();
    KifuBranchNode* current = nav->currentNode();

    // 棋譜欄の手順。対局中は最新の手まで（以前の棋譜の続きは隠す）
    const LiveGameSession* live = m_deps.liveSession;
    const QList<KifuBranchNode*> path = live->isActive() ? tree->pathToNode(live->liveNode()) : nav->selectedPath();
    {
        // 行を削ると表が選択中の行を動かす。表示中の手に合わせた変更を、利用者が行を選んだ操作として扱わない
        QItemSelectionModel* selection = m_deps.recordView ? m_deps.recordView->selectionModel() : nullptr;
        const QSignalBlocker blocker(selection);
        m_deps.recordModel->setPath(path);
        selectCurrentRow(static_cast<int>(path.indexOf(current)));
    }

    // 分岐候補欄
    const QList<KifuBranchNode*> candidates = nav->branchCandidatesAtCurrent();
    m_deps.branchModel->setCandidates(candidates, static_cast<int>(candidates.indexOf(current)),
                                      nav->offersBackToMainLine());

    // 分岐ツリー（図はツリーの変更を自分で受けて描き直す）
    if (m_deps.treeView) m_deps.treeView->setCurrentNode(current ? current->nodeId() : -1);
}

void KifuDisplayPresenter::selectCurrentRow(int row)
{
    m_deps.recordModel->setCurrentHighlightRow(row);
    QTableView* view = m_deps.recordView;
    if (!view || !view->selectionModel() || row < 0) return;
    const QModelIndex index = m_deps.recordModel->index(row, 0);
    QItemSelectionModel* selection = view->selectionModel();
    QList<int> changedRows;
    if (selection->currentIndex().row() != row) {
        // 選択の変更の通知は refresh() が止めているので、表は選択の変わった行を自分では描き直さない。
        // 選択が外れる行と付く行を覚えておき、その行だけを描き直す（表示領域の全体は描き直さない。
        // 対局中は1手ごとに現在行が移る）
        changedRows = {selection->currentIndex().row(), row};
        const QModelIndexList selected = selection->selectedRows();
        for (const QModelIndex& selectedRow : selected) changedRows.append(selectedRow.row());
        selection->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    view->scrollTo(index);
    // 行の位置はスクロールで変わるので、表示位置を決めてから描き直しを頼む
    for (const int changed : std::as_const(changedRows)) updateRow(view, changed);
}
