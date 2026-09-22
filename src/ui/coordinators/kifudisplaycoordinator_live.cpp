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
    // 盤面を SFEN から再設定し、手番同期と分岐ナビガードまで走らせてしまう。
    // ここではナビゲーション状態と各表示のハイライトだけを最新手に同期する。
    if (liveLineIndex > 0) {
        m_state->setPreferredLineIndex(liveLineIndex);
    } else {
        m_state->resetPreferredLineIndex();
    }
    m_state->rememberPathSelections(liveNode);   // 対局後の「戻る→進む」で最新手の経路を辿れるように
    m_state->setCurrentNode(liveNode);

    syncRecordViewToCurrentLine();
    highlightCurrentPosition();
    m_pendingNavResultCheck = true;
    updateBranchCandidatesView();
}

void KifuDisplayCoordinator::onLiveGameMovesAboutToBeUndone(KifuBranchNode* target)
{
    if (m_state != nullptr) {
        // setCurrentNode は旧ノードも参照するため、解放前に移動する。
        m_state->setCurrentNode(target);
        m_state->clearLineSelectionMemory();
        m_state->resetPreferredLineIndex();
    }
}

void KifuDisplayCoordinator::onLiveGameMovesUndone()
{
    if (m_tree == nullptr || m_state == nullptr || m_liveSession == nullptr) {
        return;
    }
    KifuBranchNode* node = m_liveSession->liveNode();
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
    if (m_branchTreeNodeCount < 0) {
        return false;   // まだ一度も反映していない
    }
    if (nodeCount == m_branchTreeNodeCount) {
        return true;    // 既存ノードの再利用（指し直し）: 描画済みなので何もしない
    }
    if (nodeCount != m_branchTreeNodeCount + 1 || liveNode->childCount() != 0) {
        return false;   // 想定外の変化
    }

    // 新しいラインができた場合は行の並びが変わり得るため全再構築に任せる
    if (m_tree->lineCount() != m_branchTreeManager->rowCount()) {
        return false;
    }
    const auto lineIndex = m_tree->findLineIndexForNode(liveNode);
    if (!lineIndex.has_value()) {
        return false;
    }

    KifDisplayItem item;
    item.prettyMove = liveNode->displayText();
    item.timeText = liveNode->timeText();
    item.comment = liveNode->comment();
    if (!m_branchTreeManager->appendNodeToRow(*lineIndex, liveNode->ply(), item, liveNode->sfen())) {
        return false;
    }
    m_branchTreeNodeCount = nodeCount;
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

void KifuDisplayCoordinator::onLiveGameRecordModelUpdateRequired()
{
    if (m_recordPane != nullptr && m_recordPane->kifuView() != nullptr) {
        m_recordPane->kifuView()->viewport()->update();
    }
}
