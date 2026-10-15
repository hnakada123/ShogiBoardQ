/// @file kifudisplaycoordinator_live.cpp
/// @brief 棋譜表示コーディネータ - ライブ対局セッション処理

#include "kifudisplaycoordinator.h"
#include "kifudisplaypresenter.h"
#include "kifuselectionsync.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "branchtreemanager.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "recordpane.h"

#include "logcategories.h"

#include <QTableView>

// ============================================================
// ライブ対局セッション
// ============================================================

void KifuDisplayCoordinator::onLiveGameMoveAdded(int ply, const QString& displayText)
{
    Q_UNUSED(ply);
    Q_UNUSED(displayText);

    const int liveLineIndex = (m_liveSession != nullptr) ? m_liveSession->currentLineIndex() : 0;
    KifuBranchNode* liveNode = (m_liveSession != nullptr) ? m_liveSession->liveNode() : nullptr;

    // 分岐ツリーは毎手の全再構築を避け、可能なら末尾ノードの差分追加だけ行う
    if (!appendLiveNodeToBranchTree(liveNode)) {
        updateBranchTreeView();
    }

    if (liveNode == nullptr || m_state == nullptr) {
        return;
    }

    // 対局中は盤面が対局ロジックによって既に更新されているため、
    // KifuNavigationController::goToNode() は使わない。goToNode() は棋譜閲覧用の経路で、
    // 盤面を SFEN から再設定し、手番同期も行う。
    // ここではナビゲーション状態と各表示のハイライトだけを最新手に同期する。
    if (liveLineIndex > 0) {
        m_state->setPreferredLineIndex(liveLineIndex);
    } else {
        m_state->resetPreferredLineIndex();
    }
    m_state->rememberPathSelections(liveNode);   // 対局後の「戻る→進む」で最新手の経路を辿れるように
    m_state->setCurrentNode(liveNode);

    // 正本を更新し終えた後に表示を作る。同じ経路で通常手・終局・既存手の再利用を扱う。
    m_presenter->presentLiveNode(liveNode);
    m_lastLineIndex = m_state->currentLineIndex();
    highlightCurrentPosition();
    m_pendingNavResultCheck = true;
    updateBranchCandidatesView();
}

void KifuDisplayCoordinator::onLiveGameMovesUndone()
{
    if (m_tree == nullptr || m_state == nullptr || m_liveSession == nullptr) {
        return;
    }
    KifuBranchNode* node = m_liveSession->liveNode();
    m_state->setCurrentNode(node);
    m_state->clearLineSelectionMemory();
    m_lastLineIndex = m_liveSession->currentLineIndex();
    m_state->setPreferredLineIndex(m_lastLineIndex);
    m_state->rememberPathSelections(node);

    // 既存の継続手を再利用した場合も、対局中の棋譜欄は現在手までにする。
    m_presenter->populateRecordModelFromPath(m_tree->pathToNode(node), m_state->currentPly());
    updateBranchTreeView();
    highlightCurrentPosition();
    m_pendingNavResultCheck = true;
    updateBranchCandidatesView();
}

bool KifuDisplayCoordinator::appendLiveNodeToBranchTree(KifuBranchNode* liveNode)
{
    if (m_branchTreeManager == nullptr || m_tree == nullptr || liveNode == nullptr) {
        return false;
    }

    const int nodeCount = m_tree->nodeCount();
    const quint64 revision = m_tree->revision();
    if (m_branchTreeNodeCount < 0) {
        return false;   // まだ一度も反映していない
    }
    if (nodeCount == m_branchTreeNodeCount && revision == m_branchTreeRevision) {
        return true;    // 既存ノードの再利用（指し直し）: 描画済みなので何もしない
    }
    // 1ノードの追加以外（待ったで残した手順との並べ替えなど）は全再構築に任せる
    if (nodeCount != m_branchTreeNodeCount + 1 || revision != m_branchTreeRevision + 1
        || liveNode->childCount() != 0) {
        return false;
    }

    // 新しいラインができた場合は行の並びが変わり得るため全再構築に任せる
    if (m_tree->lineCount() != m_branchTreeManager->rowCount()) {
        return false;
    }
    const auto lineIndex = m_tree->findLineIndexForNode(liveNode);
    if (!lineIndex.has_value()) {
        return false;
    }

    if (!m_branchTreeManager->appendNode(liveNode)) {
        return false;
    }
    m_branchTreeNodeCount = nodeCount;
    m_branchTreeRevision = revision;
    return true;
}

void KifuDisplayCoordinator::onLiveGameSessionStarted(KifuBranchNode* branchPoint)
{
    qCDebug(lcUi).noquote() << "onLiveGameSessionStarted: branchPoint="
                       << (branchPoint ? QString("ply=%1").arg(branchPoint->ply()) : "null");

    int branchLineIndex = 0;
    if (branchPoint != nullptr && m_tree != nullptr) {
        branchLineIndex = m_tree->findLineIndexForNode(branchPoint).value_or(0);
    }

    if (m_state != nullptr) {
        if (branchLineIndex > 0) {
            m_state->setPreferredLineIndex(branchLineIndex);
        } else {
            m_state->resetPreferredLineIndex();
        }
        m_state->clearLineSelectionMemory();
        // 対局は分岐点（途中局面）か開始局面から始まる。表示中の手数（カーソル）をそこに合わせる
        KifuBranchNode* startNode = branchPoint != nullptr ? branchPoint
                                                            : (m_tree != nullptr ? m_tree->root() : nullptr);
        if (startNode != nullptr) {
            m_state->setCurrentNode(startNode);
        }
        qCDebug(lcUi).noquote() << "onLiveGameSessionStarted: set preferredLineIndex=" << branchLineIndex
                           << "and clearLineSelectionMemory";
    }

    m_lastLineIndex = branchLineIndex;

    if (m_recordModel == nullptr || m_tree == nullptr) {
        return;
    }

    if (branchPoint == nullptr) {
        m_recordModel->setBranchPlyMarks(QSet<int>());
        onRecordHighlightRequired(0);
        return;
    }

    const QList<KifuBranchNode*> path = m_tree->pathToNode(branchPoint);

    // branchPoint までのパスの分岐選択を記憶
    if (m_state != nullptr) {
        m_state->rememberPathSelections(branchPoint);
    }

    const int highlightRow = m_presenter->populateRecordModelFromPath(path, branchPoint->ply());
    onRecordHighlightRequired(highlightRow);
}

void KifuDisplayCoordinator::onLiveGameCommitted(KifuBranchNode* newLineEnd)
{
    updateRecordView();
    updateBranchTreeView();

    if (newLineEnd != nullptr && m_navController != nullptr) {
        m_navController->goToNode(newLineEnd);
    }
}

void KifuDisplayCoordinator::onLiveGameDiscarded()
{
    updateRecordView();
    updateBranchTreeView();
}

