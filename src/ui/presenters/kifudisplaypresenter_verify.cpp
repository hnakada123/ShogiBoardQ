/// @file kifudisplaypresenter_verify.cpp
/// @brief 棋譜表示プレゼンタ - 表示一致性の検証とレポート

#include "kifudisplaypresenter.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "kifubranchlistmodel.h"
#include "kifudisplay.h"
#include "branchtreemanager.h"

#include <QStringList>
#include <QTextStream>

namespace {
QString normalizeSfenForCompare(const QString& sfen)
{
    const QString trimmed = sfen.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        return trimmed;
    }
    // 盤面・手番・持ち駒までを比較対象にする（手数は除外）
    return QStringLiteral("%1 %2 %3").arg(parts.at(0), parts.at(1), parts.at(2));
}
}

// ============================================================
// 一致性検証
// ============================================================

KifuDisplayPresenter::DisplaySnapshot KifuDisplayPresenter::captureDisplaySnapshot(const TrackingState& tracking) const
{
    DisplaySnapshot snapshot;
    snapshot.trackedLineIndex = tracking.lastLineIndex;
    snapshot.liveSessionActive = tracking.liveSessionActive;
    snapshot.modelLineIndex = m_lastModelLineIndex;
    snapshot.expectedTreeLineIndex = tracking.expectedTreeLineIndex;
    snapshot.expectedTreePly = tracking.expectedTreePly;

    if (m_refs.state != nullptr) {
        snapshot.stateLineIndex = m_refs.state->currentLineIndex();
        snapshot.statePly = m_refs.state->currentPly();
        snapshot.stateOnMainLine = m_refs.state->isOnMainLine();
        snapshot.stateSfen = m_refs.state->currentSfen();
        snapshot.stateSfenNormalized = normalizeSfenForCompare(snapshot.stateSfen);
    }

    if (m_refs.recordModel != nullptr) {
        snapshot.modelRowCount = m_refs.recordModel->rowCount();
        snapshot.modelHighlightRow = m_refs.recordModel->currentHighlightRow();

        if (snapshot.statePly >= 0 && snapshot.statePly < m_refs.recordModel->rowCount()) {
            if (KifuDisplay* item = m_refs.recordModel->item(snapshot.statePly)) {
                snapshot.displayedMoveAtPly = item->currentMove();
            }
        }
    }

    if (m_refs.tree != nullptr && snapshot.stateLineIndex >= 0) {
        const QList<BranchLine> lines = m_refs.tree->allLines();
        if (snapshot.stateLineIndex < lines.size()) {
            const BranchLine& line = lines.at(snapshot.stateLineIndex);
            for (KifuBranchNode* node : std::as_const(line.nodes)) {
                if (node != nullptr && node->ply() == snapshot.statePly) {
                    snapshot.expectedMoveAtPly = node->displayText();
                    break;
                }
            }
        }
    }

    if (m_refs.branchTreeManager != nullptr && m_refs.branchTreeManager->hasHighlightedNode()) {
        snapshot.treeHighlightLineIndex = m_refs.branchTreeManager->lastHighlightedRow();
        snapshot.treeHighlightPly = m_refs.branchTreeManager->lastHighlightedPly();
    }

    if (m_refs.branchModel != nullptr) {
        snapshot.branchCandidateCount = m_refs.branchModel->branchCandidateCount();
        snapshot.hasBackToMainRow = m_refs.branchModel->hasBackToMainRow();
    }

    if (m_refs.boardSfenProvider) {
        snapshot.boardSfen = m_refs.boardSfenProvider();
        snapshot.boardSfenNormalized = normalizeSfenForCompare(snapshot.boardSfen);
    }

    return snapshot;
}

bool KifuDisplayPresenter::verifyDisplayConsistencyDetailed(const TrackingState& tracking, QString* reason) const
{
    if (reason != nullptr) {
        reason->clear();
    }

    // m_state がない場合は検証不可
    if (m_refs.state == nullptr || m_refs.tree == nullptr) {
        return true;
    }

    const DisplaySnapshot snapshot = captureDisplaySnapshot(tracking);
    auto fail = [&](const QString& msg) {
        if (reason != nullptr) {
            *reason = msg;
        }
        return false;
    };

    // 1. コーディネータ追跡ラインと状態ライン
    if (snapshot.trackedLineIndex != snapshot.stateLineIndex) {
        return fail(QStringLiteral("ライン不一致: tracked=%1 state=%2")
                    .arg(snapshot.trackedLineIndex)
                    .arg(snapshot.stateLineIndex));
    }

    // 2. 棋譜欄モデルのライン/行/ハイライト
    if (m_refs.recordModel != nullptr) {
        if (snapshot.modelLineIndex >= 0 && snapshot.modelLineIndex != snapshot.stateLineIndex) {
            return fail(QStringLiteral("棋譜欄ライン不一致: model=%1 state=%2")
                        .arg(snapshot.modelLineIndex)
                        .arg(snapshot.stateLineIndex));
        }
        if (snapshot.modelHighlightRow != snapshot.statePly) {
            return fail(QStringLiteral("棋譜欄ハイライト不一致: modelHighlight=%1 statePly=%2")
                        .arg(snapshot.modelHighlightRow)
                        .arg(snapshot.statePly));
        }

        const QList<BranchLine> lines = m_refs.tree->allLines();
        if (snapshot.stateLineIndex >= 0 && snapshot.stateLineIndex < lines.size()) {
            const BranchLine& line = lines.at(snapshot.stateLineIndex);
            // 対局中の棋譜欄は「開始局面〜現在の手」だけを表示し、同じライン上にある
            // 既存の続き（指し直しで再利用した手の先）は隠す。そのため対局中は
            // ラインの長さではなく現在手数+1 を期待値にする。
            const int expectedRowCount = snapshot.liveSessionActive
                ? snapshot.statePly + 1
                : (line.nodes.isEmpty() ? 1 : static_cast<int>(line.nodes.size()));
            if (qAbs(expectedRowCount - snapshot.modelRowCount) > 1) {
                return fail(QStringLiteral("棋譜欄行数不一致: expected~=%1 actual=%2")
                            .arg(expectedRowCount)
                            .arg(snapshot.modelRowCount));
            }
        }

        if (snapshot.statePly == 0) {
            if (!snapshot.displayedMoveAtPly.contains(QStringLiteral("開始局面"))) {
                return fail(QStringLiteral("棋譜欄0行が開始局面ではありません: [%1]")
                            .arg(snapshot.displayedMoveAtPly));
            }
        } else if (!snapshot.expectedMoveAtPly.isEmpty()
                   && !snapshot.displayedMoveAtPly.contains(snapshot.expectedMoveAtPly)) {
            return fail(QStringLiteral("棋譜欄指し手不一致: expected contains [%1], actual [%2]")
                        .arg(snapshot.expectedMoveAtPly, snapshot.displayedMoveAtPly));
        }
    }

    // 3. 分岐ツリーハイライト（BranchTreeManager）
    if (m_refs.branchTreeManager != nullptr && snapshot.treeHighlightLineIndex >= 0 && snapshot.treeHighlightPly >= 0) {
        if (snapshot.treeHighlightLineIndex != snapshot.stateLineIndex
            || snapshot.treeHighlightPly != snapshot.statePly) {
            // 共有ノードチェック: 分岐点より前のノードは複数ラインで共有されるため、
            // ツリーウィジェット上ではライン0（または親ライン）にハイライトされる。
            bool isSharedNode = false;
            if (snapshot.treeHighlightPly == snapshot.statePly && m_refs.tree != nullptr) {
                const QList<BranchLine> allLines = m_refs.tree->allLines();
                if (snapshot.treeHighlightLineIndex < allLines.size()
                    && snapshot.stateLineIndex < allLines.size()) {
                    auto findAtPly = [](const BranchLine& ln, int p) -> KifuBranchNode* {
                        for (KifuBranchNode* n : std::as_const(ln.nodes)) {
                            if (n->ply() == p) return n;
                        }
                        return nullptr;
                    };
                    KifuBranchNode* treeNode = findAtPly(allLines.at(snapshot.treeHighlightLineIndex),
                                                         snapshot.treeHighlightPly);
                    KifuBranchNode* stateNode = findAtPly(allLines.at(snapshot.stateLineIndex),
                                                          snapshot.statePly);
                    isSharedNode = (treeNode != nullptr && treeNode == stateNode);
                }
            }
            if (!isSharedNode) {
                return fail(QStringLiteral("分岐ツリーハイライト不一致: tree=(%1,%2) state=(%3,%4)")
                            .arg(snapshot.treeHighlightLineIndex)
                            .arg(snapshot.treeHighlightPly)
                            .arg(snapshot.stateLineIndex)
                            .arg(snapshot.statePly));
            }
        }
    }

    // 4. 分岐候補欄
    if (m_refs.branchModel != nullptr && m_refs.state->currentNode() != nullptr) {
        int expectedCandidateCount = 0;
        if (KifuBranchNode* parent = m_refs.state->currentNode()->parent();
            parent != nullptr && parent->childCount() > 1) {
            expectedCandidateCount = parent->childCount();
            for (int i = 0; i < expectedCandidateCount; ++i) {
                if (m_refs.branchModel->labelAt(i) != parent->childAt(i)->displayText()) {
                    return fail(QStringLiteral("分岐候補指し手不一致: row=%1 expected=[%2] actual=[%3]")
                                .arg(i)
                                .arg(parent->childAt(i)->displayText(), m_refs.branchModel->labelAt(i)));
                }
            }
        }
        if (snapshot.branchCandidateCount != expectedCandidateCount) {
            return fail(QStringLiteral("分岐候補件数不一致: expected=%1 actual=%2")
                        .arg(expectedCandidateCount)
                        .arg(snapshot.branchCandidateCount));
        }
        const bool expectedBackToMain = (expectedCandidateCount > 0 && !snapshot.stateOnMainLine);
        if (snapshot.hasBackToMainRow != expectedBackToMain) {
            return fail(QStringLiteral("本譜に戻る表示不一致: expected=%1 actual=%2")
                        .arg(expectedBackToMain ? QStringLiteral("true") : QStringLiteral("false"),
                             snapshot.hasBackToMainRow ? QStringLiteral("true") : QStringLiteral("false")));
        }
    }

    // 5. 盤面SFEN（任意）
    if (!snapshot.boardSfenNormalized.isEmpty() && !snapshot.stateSfenNormalized.isEmpty()
        && snapshot.boardSfenNormalized != snapshot.stateSfenNormalized) {
        return fail(QStringLiteral("盤面SFEN不一致: board=[%1] state=[%2]")
                    .arg(snapshot.boardSfenNormalized, snapshot.stateSfenNormalized));
    }

    return true;
}

bool KifuDisplayPresenter::verifyDisplayConsistency(const TrackingState& tracking) const
{
    return verifyDisplayConsistencyDetailed(tracking, nullptr);
}

QString KifuDisplayPresenter::consistencyReport(const TrackingState& tracking) const
{
    QString report;
    QTextStream out(&report);

    out << "=== Consistency Report ===" << Qt::endl;

    QString reason;
    const DisplaySnapshot snapshot = captureDisplaySnapshot(tracking);
    const bool consistent = verifyDisplayConsistencyDetailed(tracking, &reason);

    // State 情報
    out << "State: lineIndex=" << snapshot.stateLineIndex
        << ", ply=" << snapshot.statePly
        << ", onMainLine=" << (snapshot.stateOnMainLine ? "true" : "false")
        << ", sfen=" << snapshot.stateSfenNormalized
        << Qt::endl;

    // Coordinator 情報
    out << "Coordinator: trackedLine=" << snapshot.trackedLineIndex
        << ", modelLine=" << snapshot.modelLineIndex
        << ", expectedTree=(" << snapshot.expectedTreeLineIndex << "," << snapshot.expectedTreePly << ")"
        << ", liveSession=" << (snapshot.liveSessionActive ? "true" : "false")
        << Qt::endl;

    // Model 情報
    if (m_refs.recordModel != nullptr) {
        out << "Model: rowCount=" << snapshot.modelRowCount
            << ", highlightRow=" << snapshot.modelHighlightRow
            << Qt::endl;
        out << "ModelMoveAtPly: actual=[" << snapshot.displayedMoveAtPly
            << "] expected=[" << snapshot.expectedMoveAtPly << "]" << Qt::endl;
    } else {
        out << "Model: null" << Qt::endl;
    }

    out << "TreeHighlight(actual): lineIndex=" << snapshot.treeHighlightLineIndex
        << ", ply=" << snapshot.treeHighlightPly << Qt::endl;

    out << "BranchCandidates: count=" << snapshot.branchCandidateCount
        << ", hasBackToMain=" << (snapshot.hasBackToMainRow ? "true" : "false") << Qt::endl;

    out << "BoardSfen(normalized): " << snapshot.boardSfenNormalized << Qt::endl;

    // 一致性判定結果
    out << "Consistent: " << (consistent ? "YES" : "NO") << Qt::endl;
    if (!reason.isEmpty()) {
        out << "Reason: " << reason << Qt::endl;
    }

    return report;
}
