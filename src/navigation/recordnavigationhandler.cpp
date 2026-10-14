/// @file recordnavigationhandler.cpp
/// @brief 棋譜欄行変更ハンドラクラスの実装

#include "recordnavigationhandler.h"
#include "csagamewiring.h"

#include "logcategories.h"
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
#include <QScopedValueRollback>

RecordNavigationHandler::RecordNavigationHandler(QObject* parent)
    : QObject(parent)
{
}

void RecordNavigationHandler::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

bool RecordNavigationHandler::lineSfenAt(int row, QString* sfen, QString* prevSfen) const
{
    // 棋譜欄の行は表示中の手順（分岐ツリーのライン）のノードと1対1に対応する（終局行を含む）
    if (m_deps.navState == nullptr || m_deps.branchTree == nullptr || row < 0) return false;
    const int lineIndex = m_deps.navState->currentLineIndex();
    const QList<BranchLine> lines = m_deps.branchTree->allLines();
    if (lineIndex < 0 || lineIndex >= lines.size()) return false;
    const QList<KifuBranchNode*>& nodes = lines.at(lineIndex).nodes;
    if (row >= nodes.size() || nodes.at(row) == nullptr) return false;

    const QString previous = (row > 0 && nodes.at(row - 1) != nullptr) ? nodes.at(row - 1)->sfen() : QString();
    QString current = nodes.at(row)->sfen();
    // SFENを持たない特殊ノード（投了・中断など）では直前の局面を使う
    if (current.isEmpty()) current = previous;
    if (sfen) *sfen = current;
    if (prevSfen) *prevSfen = previous;
    return true;
}

void RecordNavigationHandler::onMainRowChanged(int row)
{
    qCDebug(lcNavigation).noquote() << "onMainRowChanged ENTER row=" << row
                                    << "navState lineIndex=" << (m_deps.navState ? m_deps.navState->currentLineIndex() : -1)
                                    << "isOnMainLine=" << (m_deps.navState ? m_deps.navState->isOnMainLine() : true);

    // 再入防止（インスタンス単位）
    if (m_mainRowChangeInProgress) {
        qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (re-entry guard)";
        return;
    }
    const QScopedValueRollback<bool> rollback(m_mainRowChangeInProgress, true);

    // 分岐ナビゲーション中は二重更新を防ぐ。
    // ただし、ユーザーが別手を明示的に選んだ場合は処理を継続する。
    if (m_deps.skipBoardSyncForBranchNav && *m_deps.skipBoardSyncForBranchNav) {
        const int statePly = (m_deps.navState != nullptr) ? m_deps.navState->currentPly() : -1;
        if (row == statePly) {
            qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (branch navigation in progress, same ply)";
            return;
        }
        qCDebug(lcNavigation) << "onMainRowChanged: branch navigation guard active but proceeding"
                              << "(requested row differs from current ply)"
                              << "row=" << row << "statePly=" << statePly;
    }

    // CSA対局進行中は盤面がサーバーと同期されるため、ユーザー操作による盤面変更を抑止する
    if (const CsaGameCoordinator* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr) {
        if (csa->gameState() == CsaGameCoordinator::GameState::InGame) {
            qCDebug(lcNavigation) << "onMainRowChanged: SKIPPED (CSA game in progress)";
            return;
        }
    }

    // 対局進行中（HvE/HvH/EvE）は盤面と現在局面を対局ロジックが更新するので、
    // 棋譜欄の行追加に伴う行変更では盤面を再設定しない。
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

    // 表示中の手順（分岐ツリー）の row 行目の局面
    QString rowSfen;
    QString prevSfen;
    const bool foundInLine = lineSfenAt(row, &rowSfen, &prevSfen);

    if (row >= 0) {
        // 盤面同期。局面は棋譜の正本（分岐ツリー）から取る
        if (!gameActivelyInProgress) {
            if (foundInLine && !rowSfen.isEmpty()) {
                qCDebug(lcNavigation).noquote() << "onMainRowChanged: board from the record line row=" << row
                                                << "sfen=" << rowSfen.left(60);
                emit branchBoardSyncRequired(rowSfen, prevSfen);
            } else {
                emit boardSyncRequired(row);
            }
        }

        // 表示中の手数は分岐ツリーのカーソル（KifuNavigationState）が持つ。カーソルは下の
        // displayCoordinator への通知（対局中は LiveGameSession の着手通知）で動く

        // 棋譜欄のハイライト行を更新
        if (m_deps.kifuRecordModel) {
            m_deps.kifuRecordModel->setCurrentHighlightRow(row);
        }

        // 手番表示を更新
        emit turnUpdateRequired();

        // 盤面の手番ラベルを更新。
        // 盤面モデルの手番は setSfen() でしか更新されないため、盤面を再設定しない対局中は
        // 参照せず、TurnManager 経由（GC が正）の更新に任せる。
        if (!gameActivelyInProgress && m_deps.shogiView && m_deps.shogiView->board()) {
            const Turn boardTurn = m_deps.shogiView->board()->currentPlayer();
            const bool isBlackTurn = (boardTurn == Turn::Black);
            m_deps.shogiView->setActiveSide(isBlackTurn);

            const auto player = isBlackTurn ? Turn::Black : Turn::White;
            m_deps.shogiView->updateTurnIndicator(player);
        }

        // 評価値グラフの縦線（カーソルライン）を更新
        if (m_deps.evalGraphController) {
            m_deps.evalGraphController->setCurrentPly(row);
        }
    }

    // 矢印ボタンを有効化
    emit enableArrowButtonsRequired();

    // 現在局面を表示中の局面に更新（対局中は対局ロジックが着手のたびに更新する）
    if (m_deps.currentSfenStr && row >= 0 && !gameActivelyInProgress && foundInLine && !rowSfen.isEmpty()) {
        *m_deps.currentSfenStr = rowSfen;
    }

    // 定跡ウィンドウを更新
    emit josekiUpdateRequired();

    // 検討モード中であれば、選択した手の局面で検討を再開
    if (m_deps.playMode && *m_deps.playMode == PlayMode::ConsiderationMode && m_deps.match) {
        emit buildPositionRequired(row);
    }

    // KifuDisplayCoordinator に位置変更を通知。
    // 対局中は棋譜欄の行追加がツリーへのノード追加より先に走るため、ここで通知すると
    // 旧ラインの同手数ノードへ一時的に同期されてしまう。対局中の状態・ハイライト同期は
    // KifuDisplayCoordinator::onLiveGameMoveAdded が行うので、ここでは通知しない。
    // SFENが取れなくても、plyベースでノード同期は可能（onPositionChanged は sfen を直接参照しない）。
    if (m_deps.displayCoordinator != nullptr && !gameActivelyInProgress) {
        const int lineIndex = (m_deps.navState != nullptr) ? m_deps.navState->currentLineIndex() : 0;
        qCDebug(lcNavigation).noquote() << "onMainRowChanged: calling displayCoordinator->onPositionChanged"
                                        << "lineIndex=" << lineIndex << "row=" << row
                                        << "sfen=" << (rowSfen.isEmpty() ? "(empty)" : rowSfen.left(60));
        m_deps.displayCoordinator->onPositionChanged(lineIndex, row, rowSfen);
    }

    qCDebug(lcNavigation).noquote() << "onMainRowChanged LEAVE row=" << row;
}
