/// @file kifudisplaycoordinator_live.cpp
/// @brief 棋譜表示コーディネータ - ライブ対局セッション処理

#include "kifudisplaycoordinator.h"
#include "kifudisplaypresenter.h"
#include "kifuselectionsync.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "branchtreewidget.h"
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

    // BranchTreeManager の分岐ツリーを更新
    if (m_branchTreeManager != nullptr && m_tree != nullptr) {
        m_branchTreeManager->setBranchTreeRows(KifuDisplayPresenter::buildBranchTreeRows(m_tree));

        // ハイライト更新
        if (m_liveSession != nullptr) {
            const int totalPly = m_liveSession->totalPly();
            m_branchTreeManager->highlightBranchTreeAt(liveLineIndex, totalPly, false);
        }
    }

    // BranchTreeWidget も更新
    if (m_branchTreeWidget != nullptr && m_tree != nullptr) {
        m_branchTreeWidget->setTree(m_tree);
    }

    // ライブ対局中は最新ノードへナビゲートし、分岐ラインを同期
    if (liveNode != nullptr && m_navController != nullptr && m_state != nullptr) {
        if (liveLineIndex > 0) {
            m_state->setPreferredLineIndex(liveLineIndex);
        } else {
            m_state->resetPreferredLineIndex();
        }
        m_navController->goToNode(liveNode);
    }
}

void KifuDisplayCoordinator::onLiveGameSessionStarted(KifuBranchNode* branchPoint)
{
    qCDebug(lcUi).noquote() << "onLiveGameSessionStarted: branchPoint="
                       << (branchPoint ? QString("ply=%1").arg(branchPoint->ply()) : "null");

    int branchLineIndex = 0;
    if (branchPoint != nullptr && m_tree != nullptr) {
        branchLineIndex = m_tree->findLineIndexForNode(branchPoint);
        if (branchLineIndex < 0) {
            branchLineIndex = 0;
        }
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
    if (m_state != nullptr && !path.isEmpty()) {
        for (int i = 0; i < path.size(); ++i) {
            KifuBranchNode* node = path.at(i);
            KifuBranchNode* parent = node->parent();
            if (parent != nullptr && parent->childCount() > 1) {
                for (int j = 0; j < parent->childCount(); ++j) {
                    if (parent->childAt(j) == node) {
                        m_state->rememberLineSelection(parent, j);
                        qCDebug(lcUi).noquote() << "onLiveGameSessionStarted: remembered line selection"
                                           << "parentPly=" << parent->ply() << "childIndex=" << j;
                        break;
                    }
                }
            }
        }
    }

    const int highlightRow = m_presenter->populateRecordModelFromPath(path, branchPoint->ply());
    onRecordHighlightRequired(highlightRow);
}

void KifuDisplayCoordinator::onLiveGameBranchMarksUpdated(const QSet<int>& branchPlys)
{
    if (m_recordModel != nullptr) {
        m_recordModel->setBranchPlyMarks(branchPlys);
    }
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
