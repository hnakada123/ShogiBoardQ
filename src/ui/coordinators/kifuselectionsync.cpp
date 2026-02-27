/// @file kifuselectionsync.cpp
/// @brief 棋譜選択状態同期クラスの実装

#include "kifuselectionsync.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifudisplay.h"
#include "kifdisplayitem.h"
#include "branchtreemanager.h"
#include "branchtreewidget.h"
#include "recordpane.h"

#include "logcategories.h"

#include <QTableView>
#include <QSignalBlocker>


KifuSelectionSync::KifuSelectionSync() = default;

void KifuSelectionSync::updateRefs(const Refs& refs)
{
    m_refs = refs;
}

// ============================================================
// 棋譜欄ハイライト
// ============================================================

bool KifuSelectionSync::checkModelLineMismatch(int lastModelLineIndex) const
{
    if (m_refs.state == nullptr || m_refs.recordModel == nullptr) {
        return false;
    }

    const int stateLineIndex = m_refs.state->currentLineIndex();

    if (lastModelLineIndex >= 0 && lastModelLineIndex != stateLineIndex) {
        qCDebug(lcUi).noquote() << "checkModelLineMismatch: Line mismatch detected ("
                           << lastModelLineIndex << "!=" << stateLineIndex << "), rebuild needed";
        return true;
    }
    return false;
}

void KifuSelectionSync::applyRecordHighlight(int ply)
{
    if (m_refs.recordModel == nullptr) {
        return;
    }

    const int stateLineIndex = m_refs.state ? m_refs.state->currentLineIndex() : -1;
    qCDebug(lcUi).noquote() << "applyRecordHighlight: ply=" << ply
                       << "modelRowCount=" << m_refs.recordModel->rowCount()
                       << "stateLineIndex=" << stateLineIndex;

    // ply=0は開始局面で、モデルの行0に対応
    m_refs.recordModel->setCurrentHighlightRow(ply);

    // 棋譜欄のビューで該当行を選択
    // シグナルブロック: ナビゲーション起因の行変更でonRecordRowChangedByPresenterが
    //    再トリガーされ、本譜の局面でエンジン位置が上書きされるのを防止
    if (m_refs.recordPane != nullptr && m_refs.recordPane->kifuView() != nullptr) {
        QTableView* view = m_refs.recordPane->kifuView();
        QModelIndex idx = m_refs.recordModel->index(ply, 0);
        {
            QSignalBlocker blocker(view->selectionModel());
            view->setCurrentIndex(idx);
        }
        view->scrollTo(idx);
    }
}

// ============================================================
// 分岐ツリーハイライト
// ============================================================

bool KifuSelectionSync::applyBranchTreeHighlight(int lineIndex, int ply)
{
    // 期待値を追跡
    m_expectedTreeLineIndex = lineIndex;
    m_expectedTreePly = ply;

    qCDebug(lcUi).noquote() << "applyBranchTreeHighlight:"
                       << "lineIndex=" << lineIndex
                       << "ply=" << ply
                       << "state->currentLineIndex()=" << (m_refs.state != nullptr ? m_refs.state->currentLineIndex() : -1);

    // 即時不一致検出
    bool inconsistencyDetected = false;
    if (m_refs.state != nullptr && lineIndex != m_refs.state->currentLineIndex()) {
        qCWarning(lcUi).noquote() << "INCONSISTENCY DETECTED:"
                             << "Tree will highlight line" << lineIndex
                             << "but state says current line is" << m_refs.state->currentLineIndex();
        inconsistencyDetected = true;
    }

    // BranchTreeWidget がある場合はそちらを使用
    if (m_refs.branchTreeWidget != nullptr) {
        m_refs.branchTreeWidget->highlightNode(lineIndex, ply);
    }
    // BranchTreeManager がある場合はそちらを使用（通常はこちら）
    if (m_refs.branchTreeManager != nullptr) {
        m_refs.branchTreeManager->highlightBranchTreeAt(lineIndex, ply, /*centerOn=*/false);
    }

    return inconsistencyDetected;
}

// ============================================================
// 分岐候補更新
// ============================================================

void KifuSelectionSync::applyBranchCandidates(const QList<KifuBranchNode*>& candidates)
{
    if (m_refs.branchModel == nullptr) {
        return;
    }

    // 候補の詳細をログ出力
    {
        QStringList candidateTexts;
        for (KifuBranchNode* node : std::as_const(candidates)) {
            candidateTexts << node->displayText();
        }
        qCDebug(lcUi).noquote() << "applyBranchCandidates: count=" << candidates.size()
                            << "[" << candidateTexts.join(QStringLiteral(", ")) << "]";
    }

    m_refs.branchModel->resetBranchCandidates();

    if (candidates.isEmpty()) {
        m_refs.branchModel->setHasBackToMainRow(false);
        if (m_refs.recordPane != nullptr && m_refs.recordPane->branchView() != nullptr) {
            m_refs.recordPane->branchView()->setEnabled(true);
        }
        return;
    }

    // 分岐候補をKifDisplayItemに変換してモデルに設定
    QList<KifDisplayItem> items;
    for (KifuBranchNode* node : std::as_const(candidates)) {
        KifDisplayItem item;
        item.prettyMove = node->displayText();
        item.timeText = node->timeText();
        items.append(item);
    }
    m_refs.branchModel->updateBranchCandidates(items);

    // 本譜にいない場合は「本譜に戻る」を表示
    bool isOnMainLine = (m_refs.state != nullptr) ? (m_refs.state->currentLineIndex() == 0) : true;
    m_refs.branchModel->setHasBackToMainRow(!isOnMainLine);

    // 現在選択されている分岐をハイライト
    if (m_refs.state != nullptr && m_refs.state->currentNode() != nullptr) {
        KifuBranchNode* current = m_refs.state->currentNode();
        KifuBranchNode* parent = current->parent();
        if (parent != nullptr) {
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->childAt(i) == current) {
                    m_refs.branchModel->setCurrentHighlightRow(i);
                    break;
                }
            }
        }
    }

    // 分岐候補ビューを有効化
    if (m_refs.recordPane != nullptr && m_refs.recordPane->branchView() != nullptr) {
        m_refs.recordPane->branchView()->setEnabled(true);
    }
}

// ============================================================
// 複合操作
// ============================================================

KifuSelectionSync::CommentInfo KifuSelectionSync::highlightCurrentPosition(int lastModelLineIndex)
{
    CommentInfo commentInfo;

    qCDebug(lcUi).noquote() << "highlightCurrentPosition ENTER"
                        << "ply=" << (m_refs.state ? m_refs.state->currentPly() : -1)
                        << "lineIndex=" << (m_refs.state ? m_refs.state->currentLineIndex() : -1);

    if (m_refs.state == nullptr) {
        qCDebug(lcUi).noquote() << "highlightCurrentPosition: early return (null state)";
        return commentInfo;
    }

    const int ply = m_refs.state->currentPly();
    const int lineIndex = m_refs.state->currentLineIndex();

    // 棋譜欄のハイライト（モデル不一致チェック込み）
    if (checkModelLineMismatch(lastModelLineIndex)) {
        // KDCが再構築を処理する（戻り値で判定）
        // ここでは再構築なしでハイライトのみ適用
    }
    applyRecordHighlight(ply);

    // 分岐ツリーのハイライト
    applyBranchTreeHighlight(lineIndex, ply);

    // コメント情報を返す
    if (m_refs.state->currentNode() != nullptr) {
        const QString comment = m_refs.state->currentNode()->comment().trimmed();
        commentInfo.ply = ply;
        commentInfo.comment = comment;
        commentInfo.asHtml = true;
    }

    qCDebug(lcUi).noquote() << "highlightCurrentPosition LEAVE ply=" << ply << "lineIndex=" << lineIndex;
    return commentInfo;
}

void KifuSelectionSync::syncBranchCandidatesForNode(KifuBranchNode* targetNode)
{
    if (m_refs.branchModel == nullptr || targetNode == nullptr) {
        return;
    }

    m_refs.branchModel->resetBranchCandidates();

    KifuBranchNode* parentNode = targetNode->parent();

    qCDebug(lcUi).noquote() << "syncBranchCandidatesForNode:"
                        << "parentNode=" << (parentNode ? "yes" : "null")
                        << "parentNode->childCount()=" << (parentNode ? parentNode->childCount() : -1);

    if (parentNode != nullptr && parentNode->childCount() > 1) {
        QList<KifDisplayItem> items;
        for (int i = 0; i < parentNode->childCount(); ++i) {
            KifuBranchNode* sibling = parentNode->childAt(i);
            KifDisplayItem item;
            item.prettyMove = sibling->displayText();
            item.timeText = sibling->timeText();
            items.append(item);
        }
        m_refs.branchModel->updateBranchCandidates(items);

        bool isOnMainLine = targetNode->isMainLine();
        m_refs.branchModel->setHasBackToMainRow(!isOnMainLine);

        if (m_refs.recordPane != nullptr && m_refs.recordPane->branchView() != nullptr) {
            m_refs.recordPane->branchView()->setEnabled(true);
        }

        qCDebug(lcUi).noquote() << "syncBranchCandidatesForNode: set" << items.size()
                           << "branch candidates (siblings at ply" << targetNode->ply() << ")";
    } else {
        m_refs.branchModel->setHasBackToMainRow(false);
        m_refs.branchModel->setLocked(true);

        if (m_refs.recordPane != nullptr && m_refs.recordPane->branchView() != nullptr) {
            m_refs.recordPane->branchView()->setEnabled(true);
        }

        qCDebug(lcUi).noquote() << "syncBranchCandidatesForNode: no branch candidates (parent has"
                           << (parentNode ? parentNode->childCount() : 0) << "children)";
    }
}

int KifuSelectionSync::syncBranchTreeHighlightForNode(KifuBranchNode* targetNode, int ply, int currentLineIndex)
{
    if (m_refs.tree == nullptr || targetNode == nullptr) {
        return 0;
    }

    int highlightLineIndex = currentLineIndex;
    QVector<BranchLine> allLines = m_refs.tree->allLines();

    // 現在のラインにノードが存在するか確認
    bool foundInCurrentLine = false;
    if (highlightLineIndex >= 0 && highlightLineIndex < allLines.size()) {
        for (KifuBranchNode* node : std::as_const(allLines.at(highlightLineIndex).nodes)) {
            if (node == targetNode) {
                foundInCurrentLine = true;
                break;
            }
        }
    }

    // 現在のラインになければ、本譜（line 0）を使用
    if (!foundInCurrentLine) {
        highlightLineIndex = 0;
    }

    qCDebug(lcUi).noquote() << "syncBranchTreeHighlightForNode: highlighting at line="
                       << highlightLineIndex << "ply=" << ply;

    if (m_refs.branchTreeWidget != nullptr) {
        m_refs.branchTreeWidget->highlightNode(highlightLineIndex, ply);
    }
    if (m_refs.branchTreeManager != nullptr) {
        m_refs.branchTreeManager->highlightBranchTreeAt(highlightLineIndex, ply, /*centerOn=*/false);
    }

    return highlightLineIndex;
}
