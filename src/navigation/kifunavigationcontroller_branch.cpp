/// @file kifunavigationcontroller_branch.cpp
/// @brief 棋譜ナビゲーションコントローラ - 分岐ツリーノード処理

#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"

#include "logcategories.h"

// ============================================================
// 分岐ツリー操作
// ============================================================

void KifuNavigationController::handleBranchNodeActivated(int row, int ply)
{
    qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated ENTER row=" << row << "ply=" << ply;

    if (m_tree == nullptr || m_state == nullptr) {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: tree or state is null, cannot proceed";
        return;
    }

    // 移動をやめたときは、クリックで強調したノードを現在の手に戻す
    if (!canLeaveCurrentPosition()) {
        emit branchTreeHighlightRequired(m_state->currentLineIndex(), m_state->currentPly());
        return;
    }

    QList<BranchLine> lines = m_tree->allLines();
    if (lines.isEmpty()) {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: no lines available";
        return;
    }

    // 境界チェック
    if (row < 0 || row >= lines.size()) {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: row out of bounds (row=" << row << ", lines=" << lines.size() << ")";
        return;
    }

    // ply=0の場合（開始局面）は常にルートノードを使用
    if (ply == 0) {
        KifuBranchNode* targetNode = m_tree->root();
        if (targetNode != nullptr) {
            m_state->resetPreferredLineIndex();
            goToNode(targetNode);
        }
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated LEAVE (root node)";
        return;
    }

    // 分岐前の共有ノードをクリックした場合は現在のライン上にとどまる。
    // 分岐ツリーでは共有ノードは親ライン（本譜または親分岐）の行に描かれるため、
    // クリックされた行と現在ラインが異なっても、同じ手数のノードが同一なら
    // ラインを切り替えない。入れ子分岐では branchPly が「最後の分岐点」になるため
    // 手数比較ではなく実際にノードが同一かを比較する。
    int effectiveRow = row;
    const int currentLine = m_state->currentLineIndex();
    if (row != currentLine && currentLine >= 0 && currentLine < lines.size()) {
        auto findNodeAtPly = [](const BranchLine& targetLine, int targetPly) -> KifuBranchNode* {
            for (KifuBranchNode* node : std::as_const(targetLine.nodes)) {
                if (node != nullptr && node->ply() == targetPly) {
                    return node;
                }
            }
            return nullptr;
        };

        const KifuBranchNode* clickedNodeAtPly = findNodeAtPly(lines.at(row), ply);
        const KifuBranchNode* currentNodeAtPly = findNodeAtPly(lines.at(currentLine), ply);
        if (clickedNodeAtPly != nullptr && clickedNodeAtPly == currentNodeAtPly) {
            effectiveRow = currentLine;
            qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: shared node clicked on row=" << row
                                            << ", keeping current line=" << currentLine;
        }
    }

    goToLine(effectiveRow, ply);
}

void KifuNavigationController::goToLine(int lineIndex, int ply)
{
    if (!m_tree || !m_state) return;
    const auto lines = m_tree->allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) return;

    const BranchLine& line = lines.at(lineIndex);
    const int maxPly = line.nodes.isEmpty() ? 0 : line.nodes.last()->ply();
    const int selPly = qBound(0, ply, maxPly);

    // 分岐ラインを選択した場合、以降のナビゲーションで優先されるよう設定する
    if (lineIndex > 0) {
        m_state->setPreferredLineIndex(lineIndex);
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: setPreferredLineIndex=" << lineIndex;
    } else {
        m_state->resetPreferredLineIndex();
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: resetPreferredLineIndex (main line)";
    }

    // 対応するノードを探してナビゲート
    KifuBranchNode* targetNode = nullptr;
    for (KifuBranchNode* node : std::as_const(line.nodes)) {
        if (node->ply() == selPly) {
            targetNode = node;
            break;
        }
    }

    if (targetNode == nullptr && selPly == 0) {
        targetNode = m_tree->root();
    }

    if (targetNode != nullptr) {
        goToNode(targetNode);
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: goToNode ply=" << selPly;

    } else {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: node not found for ply=" << selPly;
    }

    qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated LEAVE";
}
