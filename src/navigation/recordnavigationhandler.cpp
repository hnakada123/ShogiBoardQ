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
                                    << "m_sfenRecord.size=" << (m_deps.sfenRecord ? m_deps.sfenRecord->size() : -1);

    // 再入防止
    static bool s_inProgress = false;
    if (s_inProgress) {
        qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (re-entry guard)";
        return;
    }
    s_inProgress = true;

    // 分岐ナビゲーション中は二重更新を防ぐため盤面同期をスキップ
    if (m_deps.skipBoardSyncForBranchNav && *m_deps.skipBoardSyncForBranchNav) {
        qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (branch navigation in progress)";
        s_inProgress = false;
        return;
    }

    // CSA対局進行中は盤面がサーバーと同期されるため、ユーザー操作による盤面変更を抑止する
    if (m_deps.csaGameCoordinator) {
        if (m_deps.csaGameCoordinator->gameState() == CsaGameCoordinator::GameState::InGame) {
            qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (CSA game in progress)";
            s_inProgress = false;
            return;
        }
    }

    // 盤面同期とUI更新
    if (row >= 0) {
        // 分岐ライン上の場合は分岐ツリーからSFENを取得して盤面を更新
        bool handledByBranchTree = false;
        if (m_deps.navState != nullptr && !m_deps.navState->isOnMainLine() && m_deps.branchTree != nullptr) {
            const int lineIndex = m_deps.navState->currentLineIndex();
            QVector<BranchLine> lines = m_deps.branchTree->allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (row >= 0 && row < line.nodes.size()) {
                    QString currentSfen = line.nodes.at(row)->sfen();
                    QString prevSfen;
                    if (row > 0 && (row - 1) < line.nodes.size()) {
                        prevSfen = line.nodes.at(row - 1)->sfen();
                    }
                    qCDebug(lcNavigation).noquote() << "onMainRowChanged: using branch tree SFEN"
                                                    << "lineIndex=" << lineIndex << "row=" << row
                                                    << "sfen=" << currentSfen.left(40);
                    emit branchBoardSyncRequired(currentSfen, prevSfen);
                    handledByBranchTree = true;
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
            const QString bw = m_deps.shogiView->board()->currentPlayer();
            const bool isBlackTurn = (bw != QStringLiteral("w"));
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
    bool isOnVariation = (m_deps.navState != nullptr && !m_deps.navState->isOnMainLine());
    if (!isOnVariation && row >= 0 && m_deps.sfenRecord && row < m_deps.sfenRecord->size()) {
        if (m_deps.currentSfenStr) {
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

        // 分岐ライン上では m_branchTree から正しいSFENを取得
        if (m_deps.navState != nullptr && !m_deps.navState->isOnMainLine() && m_deps.branchTree != nullptr) {
            QVector<BranchLine> lines = m_deps.branchTree->allLines();
            if (lineIndex >= 0 && lineIndex < lines.size()) {
                const BranchLine& line = lines.at(lineIndex);
                if (row >= 0 && row < line.nodes.size()) {
                    sfen = line.nodes.at(row)->sfen();
                    foundInBranch = true;
                }
            }
        }

        // 本譜または分岐ツリーから取得できなかった場合は m_sfenRecord から取得
        if (!foundInBranch && m_deps.sfenRecord != nullptr && row >= 0 && row < m_deps.sfenRecord->size()) {
            sfen = m_deps.sfenRecord->at(row);
        }

        if (!sfen.isEmpty()) {
            m_deps.displayCoordinator->onPositionChanged(lineIndex, row, sfen);
        }
    }

    qCDebug(lcNavigation) << "onMainRowChanged LEAVE row=" << row;
    s_inProgress = false;
}
