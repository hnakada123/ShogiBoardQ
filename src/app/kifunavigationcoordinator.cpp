/// @file kifunavigationcoordinator.cpp
/// @brief 棋譜ナビゲーション同期コーディネータの実装

#include "kifunavigationcoordinator.h"

#include <QTableView>
#include <QItemSelectionModel>
#include <QTimer>

#include "recordpane.h"
#include "kifurecordlistmodel.h"
#include "boardsyncpresenter.h"
#include "shogiview.h"
#include "kifunavigationstate.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "matchcoordinator.h"
#include "engineanalysistab.h"
#include "uistatepolicymanager.h"
#include "playmode.h"
#include "logcategories.h"

KifuNavigationCoordinator::KifuNavigationCoordinator(QObject* parent)
    : QObject(parent)
{
}

void KifuNavigationCoordinator::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

// `navigateToRow`: 棋譜ビューを指定手数の行にスクロールし盤面を同期する。
void KifuNavigationCoordinator::navigateToRow(int ply)
{
    qCDebug(lcNavTrace).noquote() << "navigateKifuViewToRow ENTER ply=" << ply;

    if (!m_deps.recordPane || !m_deps.kifuRecordModel) {
        qCWarning(lcApp).noquote() << "navigateKifuViewToRow ABORT: recordPane or kifuRecordModel is null";
        return;
    }

    QTableView* view = m_deps.recordPane->kifuView();
    if (!view) {
        qCWarning(lcApp).noquote() << "navigateKifuViewToRow ABORT: kifuView is null";
        return;
    }

    const int rows = m_deps.kifuRecordModel->rowCount();
    const int safe = (rows > 0) ? qBound(0, ply, rows - 1) : 0;

    qCDebug(lcNavTrace).noquote() << "navigateKifuViewToRow: ply=" << ply
                       << "rows=" << rows << "safe=" << safe;

    const QModelIndex idx = m_deps.kifuRecordModel->index(safe, 0);
    if (idx.isValid()) {
        if (auto* sel = view->selectionModel()) {
            sel->setCurrentIndex(
                idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        } else {
            view->setCurrentIndex(idx);
        }
        view->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    }

    // 棋譜欄のハイライト行を更新
    m_deps.kifuRecordModel->setCurrentHighlightRow(safe);

    // 盤・ハイライト即時同期
    syncBoardAndHighlightsAtRow(safe);

    // 表示中の手数（分岐ツリーのカーソル）を合わせる。行選択の通知で既に動いていれば何もしない
    syncNavStateToPly(safe);

    // 手番表示を更新
    if (m_deps.setCurrentTurn) m_deps.setCurrentTurn();

    qCDebug(lcNavTrace).noquote() << "navigateKifuViewToRow LEAVE";
}

void KifuNavigationCoordinator::syncLoadedPositionAtRow(int ply)
{
    // 新しい棋譜の読込は、以前の分岐選択に伴う一時的な同期抑止に優先する。
    if (m_deps.skipBoardSyncForBranchNav) *m_deps.skipBoardSyncForBranchNav = false;
    syncBoardAndHighlightsAtRow(ply);
    if (m_deps.setCurrentTurn) m_deps.setCurrentTurn();
}

// `syncBoardAndHighlightsAtRow`: 指定手数の盤面・ハイライト・関連UI状態を同期する。
void KifuNavigationCoordinator::syncBoardAndHighlightsAtRow(int ply)
{
    qCDebug(lcNavTrace) << "syncBoardAndHighlightsAtRow ENTER ply=" << ply;

    // 分岐ナビゲーション中に発生する再入を抑止する。
    // 分岐側の同期は `loadBoardWithHighlights()` が責務を持つため、
    // 通常経路の同期をここで走らせると二重反映になる。
    if (m_deps.skipBoardSyncForBranchNav && *m_deps.skipBoardSyncForBranchNav) {
        qCDebug(lcNavTrace) << "syncBoardAndHighlightsAtRow skipped (branch navigation in progress)";
        return;
    }

    // 位置編集モード中はスキップ
    if (m_deps.shogiView && m_deps.shogiView->positionEditMode()) {
        qCDebug(lcNavTrace) << "syncBoardAndHighlightsAtRow skipped (edit-mode)";
        return;
    }

    // BoardSyncPresenterを確保して盤面同期
    if (m_deps.ensureBoardSyncPresenter) m_deps.ensureBoardSyncPresenter();
    if (m_deps.boardSync) {
        m_deps.boardSync->syncBoardAndHighlightsAtRow(ply);
    }

    // 矢印ボタンの活性化
    if (m_deps.uiStatePolicy &&
        !m_deps.uiStatePolicy->isEnabled(UiStatePolicyManager::UiElement::WidgetNavigation)) {
        // ポリシーで無効化されている場合はスキップ
    } else if (m_deps.recordPane) {
        m_deps.recordPane->setArrowButtonsEnabled(true);
    }

    // 現在局面SFENを、表示中の手順（分岐ツリー）の ply 手目の局面にする。対局中も
    // 対局の手順は LiveGameSession が分岐ツリーに積んでいるので同じ手順から取れる
    if (m_deps.currentSfenStr && m_deps.navState != nullptr) {
        const RecordLine line = m_deps.navState->currentRecordLine();
        if (!line.isEmpty()) {
            *m_deps.currentSfenStr = line.sfens.at(qBound(0, ply, static_cast<int>(line.sfens.size()) - 1));
        }
    }

    // 定跡ウィンドウを更新
    if (m_deps.updateJosekiWindow) m_deps.updateJosekiWindow();

    qCDebug(lcNavTrace) << "syncBoardAndHighlightsAtRow LEAVE";
}

// `handleBranchNodeHandled`: 分岐ノード処理完了後の盤面・ハイライト更新。
void KifuNavigationCoordinator::handleBranchNodeHandled(
    int ply, const QString& sfen,
    int previousFileTo, int previousRankTo,
    const QString& lastUsiMove)
{
    qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled ENTER ply=" << ply
                       << "sfen=" << sfen
                       << "fileTo=" << previousFileTo << "rankTo=" << previousRankTo
                       << "usiMove=" << lastUsiMove
                       << "playMode=" << (m_deps.playMode ? static_cast<int>(*m_deps.playMode) : -1)
                       << "match=" << (m_deps.match ? "valid" : "null");

    // currentSfenStr を更新
    if (!sfen.isEmpty() && m_deps.currentSfenStr) {
        *m_deps.currentSfenStr = sfen;
    }

    // 定跡ウィンドウを更新
    if (m_deps.updateJosekiWindow) m_deps.updateJosekiWindow();

    // 検討モード時はエンジンに新しい局面を送信
    if (m_deps.playMode && *m_deps.playMode == PlayMode::ConsiderationMode
        && m_deps.match && !sfen.isEmpty()) {
        const QString newPosition = QStringLiteral("position sfen ") + sfen;
        qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled: sending to engine:" << newPosition;
        if (m_deps.match->updateConsiderationPosition(newPosition, previousFileTo, previousRankTo, lastUsiMove)) {
            qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled: updateConsiderationPosition returned true";
            if (m_deps.analysisTab) {
                m_deps.analysisTab->startElapsedTimer();
            }
        } else {
            qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled: updateConsiderationPosition returned false (same position or not in consideration)";
        }
    } else {
        qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled: NOT in consideration mode or match/sfen missing";
    }
    qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled LEAVE";
}

// `selectKifuRow`: 棋譜欄の指定行を選択し盤面を同期する。
void KifuNavigationCoordinator::selectKifuRow(int row)
{
    qCDebug(lcNavTrace).noquote() << "selectKifuRow: row=" << row;

    if (!m_deps.recordPane) return;

    QTableView* view = m_deps.recordPane->kifuView();
    if (!view) return;

    if (view->model() && row >= 0 && row < view->model()->rowCount()) {
        const QModelIndex idx = view->model()->index(row, 0);
        view->setCurrentIndex(idx);
        qCDebug(lcNavTrace).noquote() << "selectKifuRow: selected row=" << row;

        // 盤面・ハイライトも同期
        syncBoardAndHighlightsAtRow(row);
    }
}

// `syncNavStateToPly`: ナビゲーション状態を手数に同期する。
void KifuNavigationCoordinator::syncNavStateToPly(int selectedPly)
{
    if (!m_deps.navState || !m_deps.branchTree) return;

    // 本譜固定で探すと、分岐表示中に「1手戻る」が本譜へ飛ぶため、
    // 現在ライン上の同一plyノードを優先して同期する。
    KifuBranchNode* node = nullptr;
    const int currentLine = m_deps.navState->currentLineIndex();
    const QList<BranchLine> lines = m_deps.branchTree->allLines();
    if (currentLine >= 0 && currentLine < lines.size()) {
        const BranchLine& line = lines.at(currentLine);
        for (KifuBranchNode* candidate : std::as_const(line.nodes)) {
            if (candidate != nullptr && candidate->ply() == selectedPly) {
                node = candidate;
                break;
            }
        }
    }

    if (!node) {
        node = m_deps.branchTree->findByPlyOnMainLine(selectedPly);
    }
    if (node) {
        m_deps.navState->setCurrentNode(node);
    }
}

void KifuNavigationCoordinator::beginBranchNavGuard()
{
    if (m_deps.skipBoardSyncForBranchNav) {
        *m_deps.skipBoardSyncForBranchNav = true;
    }
    // 同一イベントループ内の連鎖通知を吸収したあと、次周回でガードを解除する。
    QTimer::singleShot(0, this, &KifuNavigationCoordinator::clearBranchNavGuard);
}

void KifuNavigationCoordinator::clearBranchNavGuard()
{
    if (m_deps.skipBoardSyncForBranchNav) {
        *m_deps.skipBoardSyncForBranchNav = false;
    }
}
