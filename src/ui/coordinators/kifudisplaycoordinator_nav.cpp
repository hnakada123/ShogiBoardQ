/// @file kifudisplaycoordinator_nav.cpp
/// @brief 棋譜表示コーディネータ - ナビゲーションイベント処理

#include "kifudisplaycoordinator.h"
#include "kifudisplaypresenter.h"
#include "kifuselectionsync.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
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
    syncRecordViewToCurrentLine();

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

bool KifuDisplayCoordinator::syncRecordViewToCurrentLine()
{
    if (m_state == nullptr) {
        return false;
    }

    const int newLineIndex = m_state->currentLineIndex();
    const bool lineIndexChanged = (newLineIndex != m_lastLineIndex);
    const bool modelNeedsUpdate = (m_presenter->lastModelLineIndex() >= 0
                                   && m_presenter->lastModelLineIndex() != newLineIndex);
    qCDebug(lcUi).noquote() << "syncRecordViewToCurrentLine: newLineIndex=" << newLineIndex
                       << "m_lastLineIndex=" << m_lastLineIndex
                       << "m_lastModelLineIndex=" << m_presenter->lastModelLineIndex();

    if (!lineIndexChanged && !modelNeedsUpdate) {
        return false;
    }

    m_lastLineIndex = newLineIndex;
    updateRecordView();
    return true;
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

    QString reason;
    const bool consistent = verifyDisplayConsistencyDetailed(&reason);
    if (!consistent) {
        qCWarning(lcUi).noquote() << "POST-NAVIGATION INCONSISTENCY:" << reason;
        qCDebug(lcUi).noquote() << consistencyReport();
    }

    // 表示の状態の取得（盤の SFEN の生成を含む）は、ログを出すときだけ
    if (lcUi().isDebugEnabled()) {
        const DisplaySnapshot snap = captureDisplaySnapshot();
        qCDebug(lcUi).noquote() << "NAV-RESULT: ply=" << snap.statePly
                            << "line=" << snap.stateLineIndex
                            << "recordHighlight=" << snap.modelHighlightRow
                            << "treeHL=(" << snap.treeHighlightLineIndex << "," << snap.treeHighlightPly << ")"
                            << "candidates=" << snap.branchCandidateCount
                            << "consistent=" << (consistent ? "YES" : "NO");
    }
}

// ============================================================
// onPositionChanged
// ============================================================

void KifuDisplayCoordinator::onPositionChanged(int lineIndex, int ply, const QString& sfen)
{
    Q_UNUSED(sfen)
    if (!m_navController || !m_tree || !m_state) return;
    const auto lines = m_tree->allLines();
    if (lineIndex < 0 || lineIndex >= lines.size() || ply < 0) return;
    const auto& nodes = lines.at(lineIndex).nodes;
    for (auto* node : nodes) {
        if (node->ply() != ply) continue;
        // 同手数でも分岐先が違えば移動する。範囲外を別ラインで補わない。
        if (m_state->currentNode() != node) m_navController->goToLine(lineIndex, ply);
        return;
    }
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
