/// @file kifudisplaycoordinator.cpp
/// @brief 棋譜表示コーディネータクラスの実装

#include "kifudisplaycoordinator.h"
#include "kifuselectionsync.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "recordpane.h"
#include "branchtreemanager.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "livegamesession.h"

#include "logcategories.h"

#include <QTableView>
#include <QModelIndex>
#include <utility>


KifuDisplayCoordinator::KifuDisplayCoordinator(
    KifuBranchTree* tree,
    KifuNavigationState* state,
    KifuNavigationController* navController,
    QObject* parent)
    : QObject(parent)
    , m_tree(tree)
    , m_state(state)
    , m_navController(navController)
    , m_presenter(std::make_unique<KifuDisplayPresenter>())
    , m_selectionSync(std::make_unique<KifuSelectionSync>())
{
}

KifuDisplayCoordinator::~KifuDisplayCoordinator() = default;

void KifuDisplayCoordinator::setBoardSfenProvider(BoardSfenProvider provider)
{
    m_boardSfenProvider = std::move(provider);
    syncPresenterRefs();
}

void KifuDisplayCoordinator::resetTracking()
{
    m_lastLineIndex = 0;
    m_presenter->setLastModelLineIndex(-1);
    m_pendingNavResultCheck = false;
}

void KifuDisplayCoordinator::setRecordPane(RecordPane* pane)
{
    m_recordPane = pane;
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setRecordModel(KifuRecordListModel* model)
{
    m_recordModel = model;
    syncPresenterRefs();
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setBranchModel(KifuBranchListModel* model)
{
    m_branchModel = model;
    syncPresenterRefs();
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setBranchTreeManager(BranchTreeManager* manager)
{
    m_branchTreeManager = manager;
    syncPresenterRefs();
    syncSelectionSyncRefs();
}

void KifuDisplayCoordinator::setLiveGameSession(LiveGameSession* session)
{
    // 古いセッションからの接続を解除
    if (m_liveSession != nullptr) {
        disconnect(m_liveSession, nullptr, this, nullptr);
    }

    m_liveSession = session;

    // 新しいセッションへの接続
    if (m_liveSession != nullptr) {
        connect(m_liveSession, &LiveGameSession::moveAdded,
                this, &KifuDisplayCoordinator::onLiveGameMoveAdded);
        connect(m_liveSession, &LiveGameSession::sessionStarted,
                this, &KifuDisplayCoordinator::onLiveGameSessionStarted);
        connect(m_liveSession, &LiveGameSession::sessionCommitted,
                this, &KifuDisplayCoordinator::onLiveGameCommitted);
        connect(m_liveSession, &LiveGameSession::sessionDiscarded,
                this, &KifuDisplayCoordinator::onLiveGameDiscarded);
        connect(m_liveSession, &LiveGameSession::recordModelUpdateRequired,
                this, &KifuDisplayCoordinator::onLiveGameRecordModelUpdateRequired);
    }
}

void KifuDisplayCoordinator::syncPresenterRefs()
{
    KifuDisplayPresenter::Refs refs;
    refs.tree = m_tree;
    refs.state = m_state;
    refs.recordModel = m_recordModel;
    refs.branchModel = m_branchModel;
    refs.branchTreeManager = m_branchTreeManager;
    refs.boardSfenProvider = m_boardSfenProvider;
    m_presenter->updateRefs(refs);
}

void KifuDisplayCoordinator::syncSelectionSyncRefs()
{
    KifuSelectionSync::Refs refs;
    refs.tree = m_tree;
    refs.state = m_state;
    refs.recordModel = m_recordModel;
    refs.branchModel = m_branchModel;
    refs.branchTreeManager = m_branchTreeManager;
    refs.recordPane = m_recordPane;
    m_selectionSync->updateRefs(refs);
}

KifuDisplayPresenter::TrackingState KifuDisplayCoordinator::trackingState() const
{
    return {m_lastLineIndex, m_selectionSync->expectedTreeLineIndex(), m_selectionSync->expectedTreePly()};
}

void KifuDisplayCoordinator::wireSignals()
{
    if (m_signalsWired)
        return;
    m_signalsWired = true;

    if (m_navController != nullptr) {
        connect(m_navController, &KifuNavigationController::navigationCompleted,
                this, &KifuDisplayCoordinator::onNavigationCompleted);
        connect(m_navController, &KifuNavigationController::boardUpdateRequired,
                this, &KifuDisplayCoordinator::onBoardUpdateRequired);
        connect(m_navController, &KifuNavigationController::recordHighlightRequired,
                this, &KifuDisplayCoordinator::onRecordHighlightRequired);
        connect(m_navController, &KifuNavigationController::branchTreeHighlightRequired,
                this, &KifuDisplayCoordinator::onBranchTreeHighlightRequired);
        connect(m_navController, &KifuNavigationController::branchCandidatesUpdateRequired,
                this, &KifuDisplayCoordinator::onBranchCandidatesUpdateRequired);
    }

    if (m_tree != nullptr) {
        connect(m_tree, &KifuBranchTree::treeChanged,
                this, &KifuDisplayCoordinator::onTreeChanged);
    }

    // 分岐ツリー（BranchTreeManager）のクリックは BranchNavigationWiring::onBranchNodeActivated 経由で
    // KifuNavigationController::handleBranchNodeActivated に届く

    // 分岐候補のクリックをナビゲーションに接続
    if (m_recordPane != nullptr && m_navController != nullptr) {
        connect(m_recordPane, &RecordPane::branchActivated,
                this, &KifuDisplayCoordinator::onBranchCandidateActivated);
    }
}

// ============================================================
// イベントハンドラ
// ============================================================

void KifuDisplayCoordinator::onBranchCandidateActivated(const QModelIndex& index)
{
    qCDebug(lcNavTrace).noquote() << "onBranchCandidateActivated ENTER"
                       << "index.valid=" << index.isValid()
                       << "row=" << index.row()
                       << "navController=" << (m_navController ? "yes" : "null")
                       << "branchModel=" << (m_branchModel ? "yes" : "null");

    if (!index.isValid() || m_navController == nullptr || m_branchModel == nullptr) {
        qCDebug(lcNavTrace).noquote() << "onBranchCandidateActivated: guard failed, returning";
        return;
    }

    const int row = index.row();
    if (m_branchModel->isBackToMainRow(row)) {
        qCDebug(lcNavTrace).noquote() << "onBranchCandidateActivated: back to main row, calling goToMainLineAtCurrentPly";
        m_navController->goToMainLineAtCurrentPly();
        return;
    }

    qCDebug(lcNavTrace).noquote() << "onBranchCandidateActivated: calling selectBranchCandidate(" << row << ")";
    m_navController->selectBranchCandidate(row);
}

void KifuDisplayCoordinator::onTreeChanged()
{
    m_lastLineIndex = (m_state != nullptr) ? m_state->currentLineIndex() : 0;

    updateRecordView();
    updateBranchTreeView();
    updateBranchCandidatesView();
    highlightCurrentPosition();
}

// ============================================================
// 内部更新メソッド
// ============================================================

void KifuDisplayCoordinator::updateRecordView()
{
    qCDebug(lcNavTrace).noquote() << "updateRecordView: CALLED";
    m_presenter->populateRecordModel();
    m_presenter->populateBranchMarks();

    // ビューの明示的な更新を強制
    if (m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        QTableView* view = m_recordPane->kifuView();
        view->viewport()->update();
        qCDebug(lcNavTrace).noquote() << "updateRecordView: forced view update, model rowCount=" << m_recordModel->rowCount();
    }
}

void KifuDisplayCoordinator::updateBranchTreeView()
{
    if (m_branchTreeManager != nullptr && m_tree != nullptr) {
        m_branchTreeManager->setBranchTreeRows(KifuDisplayPresenter::buildBranchTreeRows(m_tree));
    } else if (m_branchTreeManager != nullptr) {
        m_branchTreeManager->setBranchTreeRows({});
    }
}

void KifuDisplayCoordinator::updateBranchCandidatesView()
{
    if (m_state == nullptr) {
        return;
    }

    QList<KifuBranchNode*> candidates = m_state->branchCandidatesAtCurrent();
    onBranchCandidatesUpdateRequired(candidates);
}

void KifuDisplayCoordinator::highlightCurrentPosition()
{
    const auto commentInfo = m_selectionSync->highlightCurrentPosition(m_presenter->lastModelLineIndex());

    // モデル不一致が検出された場合の再構築
    if (m_selectionSync->checkModelLineMismatch(m_presenter->lastModelLineIndex())) {
        updateRecordView();
        if (m_state != nullptr) {
            m_selectionSync->applyRecordHighlight(m_state->currentPly());
        }
    }

    // コメント更新シグナル発火
    if (commentInfo.ply >= 0) {
        const QString& comment = commentInfo.comment;
        emit commentUpdateRequired(commentInfo.ply,
                                   comment.isEmpty() ? tr("コメントなし") : comment,
                                   commentInfo.asHtml);
    }
}
