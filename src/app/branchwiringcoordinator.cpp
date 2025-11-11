#include "branchwiringcoordinator.h"

#include "recordpane.h"
#include "kifubranchlistmodel.h"
#include "branchcandidatescontroller.h"
#include "kifuloadcoordinator.h"

#include <QTableView>
#include <QDebug>

BranchWiringCoordinator::BranchWiringCoordinator(const Deps& d)
    : QObject(d.parent)
    , m_recordPane(d.recordPane)
    , m_branchModel(d.branchModel)
    , m_varEngine(d.variationEngine)
    , m_loader(d.kifuLoader)
{
}

void BranchWiringCoordinator::setupBranchView()
{
    if (!m_recordPane) return;

    if (!m_branchModel) {
        m_branchModel = new KifuBranchListModel(this);
        qDebug() << "[WIRE] created KifuBranchListModel =" << m_branchModel;
    }

    QTableView* view = m_recordPane->branchView();
    if (!view) return;

    if (view->model() != m_branchModel) {
        view->setModel(m_branchModel);
        qDebug() << "[WIRE] branchView.setModel done model=" << m_branchModel;
    }

    // ★初期から表示しておく：候補が空でもコメント欄の上に常に見せる
    view->setVisible(true);
    view->setEnabled(false);
}

void BranchWiringCoordinator::setupBranchCandidatesWiring()
{
    if (!m_recordPane) return;

    // モデル/ビューの配線は別関数 setupBranchView() で済ませ済み

    // Controller を用意（1回だけ）
    if (!m_branchCtl) {
        m_branchCtl = new BranchCandidatesController(m_varEngine, m_branchModel, this);
        qDebug() << "[WIRE] BranchCandidatesController created ve=" << static_cast<void*>(m_varEngine)
                 << " model=" << static_cast<void*>(m_branchModel);
    }

    // RecordPane 側と結線（クリック → activateCandidate）
    m_branchCtl->attachRecordPane(m_recordPane);

    // Plan クリック → 行/手ジャンプ
    const bool okA = connect(m_branchCtl, &BranchCandidatesController::planActivated,
                             this,         &BranchWiringCoordinator::onBranchPlanActivated,
                             Qt::UniqueConnection);
    qDebug() << "[WIRE] connect planActivated -> onBranchPlanActivated :" << okA;

    // RecordPane の行アクティベート → 候補の有効化
    const bool okB = connect(m_recordPane, &RecordPane::branchActivated,
                             this,         &BranchWiringCoordinator::onRecordPaneBranchActivated,
                             Qt::UniqueConnection);
    qDebug() << "[WIRE] connect RecordPane.branchActivated -> onRecordPaneBranchActivated :" << okB;

    // ★★ ここが肝：Loader に Controller を教える（以降、Loader が候補を流し込める）
    if (m_loader) {
        m_loader->setBranchCandidatesController(m_branchCtl);
    }
}

void BranchWiringCoordinator::onRecordPaneBranchActivated(const QModelIndex& index)
{
    if (!index.isValid() || !m_branchCtl) return;
    m_branchCtl->activateCandidate(index.row());
}

void BranchWiringCoordinator::onBranchPlanActivated(int row, int ply1)
{
    qDebug() << "[BRANCH] planActivated -> applyResolvedRowAndSelect row=" << row << " ply=" << ply1;
    applyResolvedRowAndSelect_(row, ply1);
}

void BranchWiringCoordinator::onBranchNodeActivated(int row, int ply)
{
    if (!m_branchModel) return;
    if (row < 0 || row >= m_branchModel->rowCount()) return;

    // その行の手数内にクランプ（0=開始局面, 1..N）
    const int maxPly = m_branchModel->data(m_branchModel->index(row, 0),
                                           KifuBranchListModel::DispCountRole).toInt();
    const int selPly = qBound(0, ply, maxPly);

    applyResolvedRowAndSelect_(row, selPly);
}

void BranchWiringCoordinator::applyResolvedRowAndSelect_(int row, int selPly)
{
    if (!m_loader) return;
    // KifuLoadCoordinator へ委譲（局面/棋譜/ビューの同期を一括で）
    m_loader->applyResolvedRowAndSelect(row, selPly);
}

void BranchWiringCoordinator::setKifuLoader(KifuLoadCoordinator* loader)
{
    m_loader = loader;
}
