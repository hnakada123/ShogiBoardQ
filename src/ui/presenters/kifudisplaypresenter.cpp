/// @file kifudisplaypresenter.cpp
/// @brief 棋譜欄・分岐候補欄・分岐ツリーへの表示の反映

#include "kifudisplaypresenter.h"
#include "branchtreemanager.h"
#include "kifubranchlistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"

#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QTableView>

KifuDisplayPresenter::KifuDisplayPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.navState && m_deps.liveSession && m_deps.recordModel && m_deps.branchModel);
    connect(m_deps.navState, &KifuNavigationState::changed, this, &KifuDisplayPresenter::refresh);
    connect(m_deps.liveSession, &LiveGameSession::activeChanged, this, &KifuDisplayPresenter::refresh);
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
        // 分岐が増えると前の手の「+」の印が変わる（ツリーの構造が変わったときだけ描き直す）
        if (tree->revision() != m_markedRevision) {
            m_markedRevision = tree->revision();
            m_deps.recordModel->refreshAll();
        }
        selectCurrentRow(static_cast<int>(path.indexOf(current)));
    }

    // 分岐候補欄
    const QList<KifuBranchNode*> candidates = nav->branchCandidatesAtCurrent();
    m_deps.branchModel->setCandidates(candidates, static_cast<int>(candidates.indexOf(current)),
                                      nav->offersBackToMainLine());

    // 分岐ツリー（構造が変わったときだけ描き直す。手順の末尾に1手足しただけならその手を描き足す）
    if (BranchTreeManager* treeView = m_deps.treeView) {
        treeView->syncWithTree(tree);
        treeView->highlightBranchTreeAt(nav->currentLineIndex(), nav->currentPly());
    }
}

void KifuDisplayPresenter::refreshLabels()
{
    m_deps.recordModel->refreshAll();
    m_deps.branchModel->refreshAll();
    if (m_deps.treeView) m_deps.treeView->setBranchTree(m_deps.navState->tree());
    refresh();
}

void KifuDisplayPresenter::selectCurrentRow(int row)
{
    m_deps.recordModel->setCurrentHighlightRow(row);
    QTableView* view = m_deps.recordView;
    if (!view || !view->selectionModel() || row < 0) return;
    const QModelIndex index = m_deps.recordModel->index(row, 0);
    if (view->selectionModel()->currentIndex().row() != row) {
        // 選択の変更の通知は refresh() が止めている
        view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        view->viewport()->update();
    }
    view->scrollTo(index);
}
