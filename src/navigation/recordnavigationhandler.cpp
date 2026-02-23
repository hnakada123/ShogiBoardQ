/// @file recordnavigationhandler.cpp
/// @brief 棋譜欄行変更ハンドラクラスの実装

#include "recordnavigationhandler.h"

#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifudisplaycoordinator.h"
#include "kifurecordlistmodel.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "matchcoordinator.h"
#include "evaluationgraphcontroller.h"
#include "csagamecoordinator.h"

RecordNavigationHandler::RecordNavigationHandler(QObject* parent)
    : QObject(parent)
{
}

void RecordNavigationHandler::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void RecordNavigationHandler::onMainRowChanged(int row)
{
    qCDebug(lcNavigation).noquote() << "onMainRowChanged ENTER row=" << row
                                    << "navState lineIndex=" << (m_deps.navState ? m_deps.navState->currentLineIndex() : -1)
                                    << "isOnMainLine=" << (m_deps.navState ? m_deps.navState->isOnMainLine() : true)
                                    << "m_sfenHistory.size=" << (m_deps.sfenRecord ? m_deps.sfenRecord->size() : -1);

    // 再入防止
    static bool s_inProgress = false;
    if (s_inProgress) {
        qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (re-entry guard)";
        return;
    }
    s_inProgress = true;

    // 分岐ナビゲーション中は二重更新を防ぐ。
    // ただし、ユーザーが別手を明示的に選んだ場合は処理を継続する。
    if (m_deps.skipBoardSyncForBranchNav && *m_deps.skipBoardSyncForBranchNav) {
        const int statePly = (m_deps.navState != nullptr) ? m_deps.navState->currentPly() : -1;
        if (row == statePly) {
            qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (branch navigation in progress, same ply)";
            s_inProgress = false;
            return;
        }
        qCDebug(lcNavigation) << "onMainRowChanged: branch navigation guard active but proceeding"
                              << "(requested row differs from current ply)"
                              << "row=" << row << "statePly=" << statePly;
    }

    // CSA対局進行中は盤面がサーバーと同期されるため、ユーザー操作による盤面変更を抑止する
    if (m_deps.csaGameCoordinator) {
        if (m_deps.csaGameCoordinator->gameState() == CsaGameCoordinator::GameState::InGame) {
            qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (CSA game in progress)";
            s_inProgress = false;
            return;
        }
    }

    // 対局進行中（HvE/HvH/EvE）は sfenRecord の更新が正であるため、
    // 分岐ツリーからの盤面上書きをスキップする。
    // 分岐ツリーには対局開始前の KIF 継続手が残っており、
    // エンジンの実際の指し手と異なる局面で盤面が上書きされてしまうのを防ぐ。
    const bool gameActivelyInProgress = [&]() {
        if (!m_deps.playMode || !m_deps.match) return false;
        switch (*m_deps.playMode) {
        case PlayMode::HumanVsHuman:
        case PlayMode::EvenHumanVsEngine:
        case PlayMode::EvenEngineVsHuman:
        case PlayMode::EvenEngineVsEngine:
        case PlayMode::HandicapHumanVsEngine:
        case PlayMode::HandicapEngineVsHuman:
        case PlayMode::HandicapEngineVsEngine:
            return !m_deps.match->gameOverState().isOver;
        default:
            return false;
        }
    }();

    // 盤面同期とUI更新
    if (row >= 0) {
        // 分岐ツリーが存在する場合はツリーからSFENを取得して盤面を更新
        // ただし、対局進行中は sfenRecord を正とするためスキップする
        // 本譜（lineIndex=0）でもツリーを使う: sfenRecordは分岐切り替え後に
        // ツリーのmain lineと不整合になることがあるため
        bool handledByBranchTree = false;
        if (!gameActivelyInProgress && m_deps.navState != nullptr && m_deps.branchTree != nullptr) {
            const int lineIndex = m_deps.navState->currentLineIndex();
            QVector<BranchLine> lines = m_deps.branchTree->allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (row >= 0 && row < line.nodes.size()) {
                    QString currentSfen = line.nodes.at(row)->sfen();
                    // 投了・中断などSFENを持たない特殊ノードの場合、直前ノードのSFENを使用
                    if (currentSfen.isEmpty() && row > 0 && (row - 1) < line.nodes.size()) {
                        currentSfen = line.nodes.at(row - 1)->sfen();
                        qCDebug(lcNavigation).noquote() << "onMainRowChanged: terminal node (empty SFEN),"
                                                        << "using previous node's SFEN at row=" << (row - 1);
                    }
                    QString prevSfen;
                    if (row > 0 && (row - 1) < line.nodes.size()) {
                        prevSfen = line.nodes.at(row - 1)->sfen();
                    }
                    if (!currentSfen.isEmpty()) {
                        qCDebug(lcNavigation).noquote() << "onMainRowChanged: using branch tree SFEN"
                                                        << "lineIndex=" << lineIndex << "row=" << row
                                                        << "sfen=" << currentSfen.left(60)
                                                        << "prevSfen=" << (prevSfen.isEmpty() ? "(empty)" : prevSfen.left(60));
                        emit branchBoardSyncRequired(currentSfen, prevSfen);
                        handledByBranchTree = true;
                    } else {
                        qCDebug(lcNavigation).noquote() << "onMainRowChanged: branch tree SFEN is empty,"
                                                        << "fallback to boardSyncRequired row=" << row;
                    }
                }
            }
        }

        // 本譜の場合は従来の方法を使用
        if (!handledByBranchTree) {
            emit boardSyncRequired(row);
        }

        // 現在手数トラッキングを更新
        if (m_deps.activePly) *m_deps.activePly = row;
        if (m_deps.currentSelectedPly) *m_deps.currentSelectedPly = row;
        if (m_deps.currentMoveIndex) *m_deps.currentMoveIndex = row;

        // 棋譜欄のハイライト行を更新
        if (m_deps.kifuRecordModel) {
            m_deps.kifuRecordModel->setCurrentHighlightRow(row);
        }

        // 手番表示を更新
        emit turnUpdateRequired();

        // 盤面の手番ラベルを更新
        if (m_deps.shogiView && m_deps.shogiView->board()) {
            const Turn boardTurn = m_deps.shogiView->board()->currentPlayer();
            const bool isBlackTurn = (boardTurn == Turn::Black);
            m_deps.shogiView->setActiveSide(isBlackTurn);

            const auto player = isBlackTurn ? ShogiGameController::Player1 : ShogiGameController::Player2;
            m_deps.shogiView->updateTurnIndicator(player);
        }

        // 評価値グラフの縦線（カーソルライン）を更新
        if (m_deps.evalGraphController) {
            m_deps.evalGraphController->setCurrentPly(row);
        }
    }

    // 矢印ボタンを有効化
    emit enableArrowButtonsRequired();

    // m_currentSfenStrを現在の局面に更新
    // 対局進行中は分岐状態に関係なく sfenRecord を正とする
    if (m_deps.currentSfenStr && row >= 0) {
        bool updatedFromTree = false;
        // 分岐ツリーが存在する場合は、ツリーからSFENを取得（対局進行中を除く）
        if (!gameActivelyInProgress && m_deps.navState != nullptr && m_deps.branchTree != nullptr) {
            const int lineIdx = m_deps.navState->currentLineIndex();
            QVector<BranchLine> treeLines = m_deps.branchTree->allLines();
            if (lineIdx >= 0 && lineIdx < treeLines.size()) {
                const BranchLine& ln = treeLines.at(lineIdx);
                if (row < ln.nodes.size() && ln.nodes.at(row) != nullptr) {
                    QString treeSfen = ln.nodes.at(row)->sfen();
                    // 投了・中断などSFENを持たない特殊ノードの場合、直前ノードのSFENを使用
                    if (treeSfen.isEmpty() && row > 0 && (row - 1) < ln.nodes.size()
                        && ln.nodes.at(row - 1) != nullptr) {
                        treeSfen = ln.nodes.at(row - 1)->sfen();
                    }
                    if (!treeSfen.isEmpty()) {
                        *m_deps.currentSfenStr = treeSfen;
                        updatedFromTree = true;
                    }
                }
            }
        }
        // フォールバック: sfenRecordから取得
        if (!updatedFromTree && m_deps.sfenRecord && row < m_deps.sfenRecord->size()) {
            *m_deps.currentSfenStr = m_deps.sfenRecord->at(row);
        }
    }

    // 定跡ウィンドウを更新
    emit josekiUpdateRequired();

    // 検討モード中であれば、選択した手の局面で検討を再開
    if (m_deps.playMode && *m_deps.playMode == PlayMode::ConsiderationMode && m_deps.match) {
        emit buildPositionRequired(row);
    }

    // KifuDisplayCoordinator に位置変更を通知
    if (m_deps.displayCoordinator != nullptr) {
        QString sfen;
        bool foundInBranch = false;
        const int lineIndex = (m_deps.navState != nullptr) ? m_deps.navState->currentLineIndex() : 0;

        // 分岐ツリーが存在する場合はツリーから正しいSFENを取得
        // 対局進行中は sfenRecord を正とするためスキップする
        if (!gameActivelyInProgress && m_deps.navState != nullptr && m_deps.branchTree != nullptr) {
            QVector<BranchLine> lines = m_deps.branchTree->allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (row >= 0 && row < line.nodes.size()) {
                    sfen = line.nodes.at(row)->sfen();
                    // 投了・中断などSFENを持たない特殊ノードの場合、直前ノードのSFENを使用
                    if (sfen.isEmpty() && row > 0 && (row - 1) < line.nodes.size()) {
                        sfen = line.nodes.at(row - 1)->sfen();
                    }
                    foundInBranch = true;
                }
            }
        }

        // 本譜または分岐ツリーから取得できなかった場合は m_sfenHistory から取得
        if (!foundInBranch && m_deps.sfenRecord != nullptr && row >= 0 && row < m_deps.sfenRecord->size()) {
            sfen = m_deps.sfenRecord->at(row);
        }

        // 補助フォールバック: SFENが取れなくても、plyベースでノード同期は可能。
        // onPositionChanged() は sfen を直接参照しないため、空でも処理させる。
        if (sfen.isEmpty() && m_deps.branchTree != nullptr && lineIndex >= 0) {
            QVector<BranchLine> lines = m_deps.branchTree->allLines();
            if (lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (row >= 0 && row < line.nodes.size() && line.nodes.at(row) != nullptr) {
                    sfen = line.nodes.at(row)->sfen();
                }
            }
        }

        qCDebug(lcNavigation).noquote() << "onMainRowChanged: calling displayCoordinator->onPositionChanged"
                                        << "lineIndex=" << lineIndex << "row=" << row
                                        << "sfen=" << (sfen.isEmpty() ? "(empty)" : sfen.left(60))
                                        << "foundInBranch=" << foundInBranch;
        m_deps.displayCoordinator->onPositionChanged(lineIndex, row, sfen);
    }

    qCDebug(lcNavigation).noquote() << "onMainRowChanged LEAVE row=" << row;
    s_inProgress = false;
}
