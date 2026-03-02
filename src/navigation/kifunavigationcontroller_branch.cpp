/// @file kifunavigationcontroller_branch.cpp
/// @brief 棋譜ナビゲーションコントローラ - 分岐ツリーノード処理

#include "kifunavigationcontroller.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "shogiutils.h"

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
            const QString sfen = targetNode->sfen().isEmpty() ? QString() : targetNode->sfen();
            emit branchNodeHandled(0, sfen, 0, 0, QString());
        }
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated LEAVE (root node)";
        return;
    }

    // 分岐前の共有ノードをクリックした場合は現在のライン上にとどまる。
    // ただし、入れ子分岐では branchPly が「最後の分岐点」になるため、
    // 手数比較だけでは共有ノード判定を誤る。実際に main/current が同一ノードかを比較する。
    int effectiveRow = row;
    const int currentLine = m_state->currentLineIndex();
    if (row == 0 && currentLine > 0 && currentLine < lines.size()) {
        const BranchLine& mainLine = lines.at(0);
        const BranchLine& currentBranchLine = lines.at(currentLine);
        auto findNodeAtPly = [](const BranchLine& targetLine, int targetPly) -> KifuBranchNode* {
            for (KifuBranchNode* node : std::as_const(targetLine.nodes)) {
                if (node != nullptr && node->ply() == targetPly) {
                    return node;
                }
            }
            return nullptr;
        };

        const KifuBranchNode* mainNodeAtPly = findNodeAtPly(mainLine, ply);
        const KifuBranchNode* currentNodeAtPly = findNodeAtPly(currentBranchLine, ply);
        if (mainNodeAtPly != nullptr && mainNodeAtPly == currentNodeAtPly) {
            effectiveRow = currentLine;
            qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: shared node clicked on main line, keeping current line=" << currentLine;
        }
    }

    const BranchLine& line = lines.at(effectiveRow);
    const int maxPly = line.nodes.isEmpty() ? 0 : line.nodes.last()->ply();
    const int selPly = qBound(0, ply, maxPly);

    // 分岐ラインを選択した場合、以降のナビゲーションで優先されるよう設定する
    if (effectiveRow > 0) {
        m_state->setPreferredLineIndex(effectiveRow);
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: setPreferredLineIndex=" << effectiveRow;
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

        const QString sfen = targetNode->sfen().isEmpty() ? QString() : targetNode->sfen();

        // 検討モード用: 移動先座標とUSI表記を取得
        int fileTo = 0;
        int rankTo = 0;
        QString usiMove;
        if (targetNode->isActualMove()) {
            const ShogiMove& move = targetNode->move();
            // movingPieceが空白の場合はデフォルト構築された無効な指し手（KIF分岐でgameMoves未設定時）
            if (move.movingPiece != Piece::None) {
                fileTo = move.toSquare.x();
                rankTo = move.toSquare.y();
                usiMove = ShogiUtils::moveToUsi(move);
            }
        }

        emit branchNodeHandled(selPly, sfen, fileTo, rankTo, usiMove);
    } else {
        qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated: node not found for ply=" << selPly;
    }

    qCDebug(lcNavigation).noquote() << "handleBranchNodeActivated LEAVE";
}
