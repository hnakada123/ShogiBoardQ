/// @file kifudisplaycoordinator_nav.cpp
/// @brief 棋譜表示コーディネータ - ナビゲーションイベント処理

#include "kifudisplaycoordinator.h"
#include "kifudisplaypresenter.h"
#include "kifuselectionsync.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"

#include "logcategories.h"

#include <utility>

// ============================================================
// ナビゲーション完了イベントハンドラ
// ============================================================

void KifuDisplayCoordinator::onNavigationCompleted(KifuBranchNode* node)
{
    qCDebug(lcUi).noquote() << "onNavigationCompleted ENTER node="
                       << (node ? QString("ply=%1").arg(node->ply()) : "null")
                       << "preferredLineIndex=" << (m_state ? m_state->preferredLineIndex() : -999);

    // ラインが変更された場合は棋譜欄の内容を更新
    if (m_state != nullptr) {
        const int newLineIndex = m_state->currentLineIndex();
        qCDebug(lcUi).noquote() << "onNavigationCompleted: newLineIndex=" << newLineIndex
                           << "m_lastLineIndex=" << m_lastLineIndex
                           << "m_lastModelLineIndex=" << m_presenter->lastModelLineIndex();

        const bool lineIndexChanged = (newLineIndex != m_lastLineIndex);
        const bool modelNeedsUpdate = (m_presenter->lastModelLineIndex() >= 0
                                       && m_presenter->lastModelLineIndex() != newLineIndex);

        if (lineIndexChanged || modelNeedsUpdate) {
            qCDebug(lcUi).noquote() << "onNavigationCompleted: updating record view"
                               << "lineIndexChanged=" << lineIndexChanged
                               << "modelNeedsUpdate=" << modelNeedsUpdate;
            m_lastLineIndex = newLineIndex;
            updateRecordView();
        } else {
            qCDebug(lcUi).noquote() << "onNavigationCompleted: line NOT changed, skipping updateRecordView";
        }
    }

    // 盤面とハイライトを更新
    if (node != nullptr) {
        QString currentSfen = node->sfen();
        QString prevSfen;
        if (node->parent() != nullptr) {
            prevSfen = node->parent()->sfen();
        }
        if (currentSfen.isEmpty() && !prevSfen.isEmpty()) {
            currentSfen = prevSfen;
            qCDebug(lcUi).noquote() << "onNavigationCompleted: terminal node (empty SFEN),"
                               << "using parent's SFEN";
        }
        qCDebug(lcUi).noquote() << "onNavigationCompleted: emitting boardWithHighlightsRequired"
                           << "currentSfen=" << currentSfen.left(40)
                           << "prevSfen=" << (prevSfen.isEmpty() ? "(empty)" : prevSfen.left(40));
        emit boardWithHighlightsRequired(currentSfen, prevSfen);
    }

    highlightCurrentPosition();

    // 一致性チェックは onBranchCandidatesUpdateRequired 完了後に遅延実行する
    m_pendingNavResultCheck = true;
}

void KifuDisplayCoordinator::onBoardUpdateRequired(const QString& sfen)
{
    Q_UNUSED(sfen);
}

void KifuDisplayCoordinator::onRecordHighlightRequired(int ply)
{
    if (m_recordModel == nullptr) {
        return;
    }

    qCDebug(lcUi).noquote() << "onRecordHighlightRequired: ply=" << ply
                       << "modelRowCount=" << m_recordModel->rowCount()
                       << "lastModelLineIndex=" << m_presenter->lastModelLineIndex()
                       << "preferredLineIndex=" << (m_state ? m_state->preferredLineIndex() : -99);

    // モデルのラインとナビゲーション状態のラインが一致しない場合、モデルを再構築
    if (m_selectionSync->checkModelLineMismatch(m_presenter->lastModelLineIndex())) {
        updateRecordView();
        qCDebug(lcUi).noquote() << "onRecordHighlightRequired: after rebuild, m_lastModelLineIndex="
                           << m_presenter->lastModelLineIndex();
    }

    m_selectionSync->applyRecordHighlight(ply);
}

void KifuDisplayCoordinator::onBranchTreeHighlightRequired(int lineIndex, int ply)
{
    const bool inconsistent = m_selectionSync->applyBranchTreeHighlight(lineIndex, ply);
    if (inconsistent) {
        qCDebug(lcUi).noquote() << consistencyReport();
    }
}

void KifuDisplayCoordinator::onBranchCandidatesUpdateRequired(const QList<KifuBranchNode*>& candidates)
{
    m_selectionSync->applyBranchCandidates(candidates);
    runPendingNavResultCheck();
}

void KifuDisplayCoordinator::runPendingNavResultCheck()
{
    if (!m_pendingNavResultCheck) {
        return;
    }
    m_pendingNavResultCheck = false;

    const bool consistent = verifyDisplayConsistency();
    if (!consistent) {
        qCWarning(lcUi).noquote() << "POST-NAVIGATION INCONSISTENCY:";
        qCDebug(lcUi).noquote() << consistencyReport();
    }

    const DisplaySnapshot snap = captureDisplaySnapshot();
    qCDebug(lcUi).noquote() << "NAV-RESULT: ply=" << snap.statePly
                        << "line=" << snap.stateLineIndex
                        << "recordHighlight=" << snap.modelHighlightRow
                        << "treeHL=(" << snap.treeHighlightLineIndex << "," << snap.treeHighlightPly << ")"
                        << "candidates=" << snap.branchCandidateCount
                        << "consistent=" << (consistent ? "YES" : "NO");
}

// ============================================================
// onPositionChanged
// ============================================================

void KifuDisplayCoordinator::onPositionChanged(int lineIndex, int ply, const QString& sfen)
{
    Q_UNUSED(sfen)
    Q_UNUSED(lineIndex)

    qCDebug(lcUi).noquote() << "onPositionChanged ENTER lineIndex=" << lineIndex << "ply=" << ply;

    if (m_tree == nullptr || m_state == nullptr) {
        qCDebug(lcUi).noquote() << "onPositionChanged LEAVE (no tree or state)";
        return;
    }

    KifuBranchNode* targetNode = nullptr;
    QList<BranchLine> lines = m_tree->allLines();

    // ply=0は開始局面（ルートノード）
    if (ply == 0) {
        targetNode = m_tree->root();
        qCDebug(lcUi).noquote() << "ply=0: using root node";
    } else if (!lines.isEmpty()) {
        int currentLine = m_state->currentLineIndex();
        if (currentLine < 0 || currentLine >= lines.size()) {
            currentLine = 0;
        }

        qCDebug(lcUi).noquote() << "searching on line" << currentLine << "for ply" << ply;

        const BranchLine& line = lines.at(currentLine);
        for (KifuBranchNode* node : std::as_const(line.nodes)) {
            if (node->ply() == ply) {
                targetNode = node;
                qCDebug(lcUi).noquote() << "found node on line" << currentLine
                                   << "ply=" << ply << "displayText=" << node->displayText();
                break;
            }
        }

        // 現在のラインで見つからない場合、全ラインを検索
        if (targetNode == nullptr) {
            for (int li = 0; li < lines.size(); ++li) {
                if (li == currentLine) continue;
                for (KifuBranchNode* node : std::as_const(lines.at(li).nodes)) {
                    if (node->ply() == ply) {
                        targetNode = node;
                        qCDebug(lcUi).noquote() << "found node on line" << li
                                           << "(fallback) ply=" << ply;
                        break;
                    }
                }
                if (targetNode != nullptr) break;
            }
        }
    }

    if (targetNode == nullptr) {
        qCDebug(lcUi).noquote() << "onPositionChanged LEAVE (target node not found for ply=" << ply << ")";
        return;
    }

    qCDebug(lcUi).noquote() << "onPositionChanged: found node ply=" << targetNode->ply()
                       << "displayText=" << targetNode->displayText()
                       << "childCount=" << targetNode->childCount()
                       << "parent=" << (targetNode->parent() ? "yes" : "null");

    // 分岐点での選択を記憶
    KifuBranchNode* nodeParent = targetNode->parent();
    if (nodeParent != nullptr && nodeParent->childCount() > 1) {
        for (int i = 0; i < nodeParent->childCount(); ++i) {
            if (nodeParent->childAt(i) == targetNode) {
                m_state->rememberLineSelection(nodeParent, i);
                qCDebug(lcUi).noquote() << "onPositionChanged: remembered line selection at parent ply="
                                   << nodeParent->ply() << "index=" << i;
                break;
            }
        }
    }

    m_state->setCurrentNode(targetNode);

    // 分岐候補の同期（KifuSelectionSyncに委譲）
    m_selectionSync->syncBranchCandidatesForNode(targetNode);

    // 分岐ツリーハイライトの同期（KifuSelectionSyncに委譲）
    const int highlightLineIndex = m_selectionSync->syncBranchTreeHighlightForNode(
        targetNode, ply, m_state->currentLineIndex());

    // m_lastLineIndex を同期
    const int oldLastLineIndex = m_lastLineIndex;
    m_lastLineIndex = highlightLineIndex;
    qCDebug(lcUi).noquote() << "onPositionChanged: synced m_lastLineIndex from" << oldLastLineIndex
                       << "to" << m_lastLineIndex
                       << "(model rowCount=" << (m_recordModel ? m_recordModel->rowCount() : -1) << ")";

    // コメント表示の更新
    {
        const QString comment = targetNode->comment().trimmed();
        emit commentUpdateRequired(ply, comment.isEmpty() ? tr("コメントなし") : comment, true);
    }

    // 位置変更完了時の一致性チェック
    const bool consistent = verifyDisplayConsistency();
    if (!consistent) {
        qCWarning(lcUi).noquote() << "POST-NAVIGATION INCONSISTENCY:";
        qCDebug(lcUi).noquote() << consistencyReport();
    }

    // NAV-RESULT サマリー
    {
        const DisplaySnapshot snap = captureDisplaySnapshot();
        qCDebug(lcUi).noquote() << "NAV-RESULT: ply=" << snap.statePly
                            << "line=" << snap.stateLineIndex
                            << "recordHighlight=" << snap.modelHighlightRow
                            << "treeHL=(" << snap.treeHighlightLineIndex << "," << snap.treeHighlightPly << ")"
                            << "candidates=" << snap.branchCandidateCount
                            << "consistent=" << (consistent ? "YES" : "NO");
    }

    qCDebug(lcUi).noquote() << "onPositionChanged LEAVE";
}

// ============================================================
// 一致性検証（プレゼンタに委譲）
// ============================================================

KifuDisplayCoordinator::DisplaySnapshot KifuDisplayCoordinator::captureDisplaySnapshot() const
{
    return m_presenter->captureDisplaySnapshot(trackingState());
}

bool KifuDisplayCoordinator::verifyDisplayConsistencyDetailed(QString* reason) const
{
    return m_presenter->verifyDisplayConsistencyDetailed(trackingState(), reason);
}

bool KifuDisplayCoordinator::verifyDisplayConsistency() const
{
    return m_presenter->verifyDisplayConsistency(trackingState());
}

QString KifuDisplayCoordinator::consistencyReport() const
{
    return m_presenter->consistencyReport(trackingState());
}
