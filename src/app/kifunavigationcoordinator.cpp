/// @file kifunavigationcoordinator.cpp
/// @brief 棋譜ナビゲーション同期コーディネータの実装

#include "kifunavigationcoordinator.h"

#include <QTableView>
#include <QItemSelectionModel>

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
    qCDebug(lcApp).noquote() << "navigateKifuViewToRow ENTER ply=" << ply;

    if (!m_deps.recordPane || !m_deps.kifuRecordModel) {
        qCDebug(lcApp).noquote() << "navigateKifuViewToRow ABORT: recordPane or kifuRecordModel is null";
        return;
    }

    QTableView* view = m_deps.recordPane->kifuView();
    if (!view) {
        qCDebug(lcApp).noquote() << "navigateKifuViewToRow ABORT: kifuView is null";
        return;
    }

    const int rows = m_deps.kifuRecordModel->rowCount();
    const int safe = (rows > 0) ? qBound(0, ply, rows - 1) : 0;

    qCDebug(lcApp).noquote() << "navigateKifuViewToRow: ply=" << ply
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

    // トラッキングを更新
    if (m_deps.activePly) *m_deps.activePly = safe;
    if (m_deps.currentSelectedPly) *m_deps.currentSelectedPly = safe;
    if (m_deps.currentMoveIndex) *m_deps.currentMoveIndex = safe;

    // 手番表示を更新
    if (m_deps.setCurrentTurn) m_deps.setCurrentTurn();

    qCDebug(lcApp).noquote() << "navigateKifuViewToRow LEAVE";
}

// `syncBoardAndHighlightsAtRow`: 指定手数の盤面・ハイライト・関連UI状態を同期する。
void KifuNavigationCoordinator::syncBoardAndHighlightsAtRow(int ply)
{
    qCDebug(lcApp) << "syncBoardAndHighlightsAtRow ENTER ply=" << ply;

    // 分岐ナビゲーション中に発生する再入を抑止する。
    // 分岐側の同期は `loadBoardWithHighlights()` が責務を持つため、
    // 通常経路の同期をここで走らせると二重反映になる。
    if (m_deps.skipBoardSyncForBranchNav && *m_deps.skipBoardSyncForBranchNav) {
        qCDebug(lcApp) << "syncBoardAndHighlightsAtRow skipped (branch navigation in progress)";
        return;
    }

    // 位置編集モード中はスキップ
    if (m_deps.shogiView && m_deps.shogiView->positionEditMode()) {
        qCDebug(lcApp) << "syncBoardAndHighlightsAtRow skipped (edit-mode)";
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

    // 現在局面SFENの更新:
    // 分岐ライン表示中は `sfenRecord()` が本譜ベースのため不整合が起こり得る。
    // そのため分岐中は branchTree を優先し、通常時のみ sfenRecord を使う。
    // ただし、対局進行中は sfenRecord を正とする（分岐ツリーには対局開始前の
    // KIF 継続手が残っている可能性があるため）。
    const bool gameActive = m_deps.isGameActivelyInProgress ? m_deps.isGameActivelyInProgress() : false;
    bool foundInBranch = false;
    if (!gameActive && m_deps.navState != nullptr && !m_deps.navState->isOnMainLine() && m_deps.branchTree != nullptr) {
        const int lineIndex = m_deps.navState->currentLineIndex();
        QVector<BranchLine> lines = m_deps.branchTree->allLines();
        if (lineIndex >= 0 && lineIndex < lines.size()) {
            const BranchLine& line = lines.at(lineIndex);
            if (ply >= 0 && ply < line.nodes.size()) {
                if (m_deps.currentSfenStr) {
                    *m_deps.currentSfenStr = line.nodes.at(ply)->sfen();
                }
                foundInBranch = true;
                qCDebug(lcApp) << "syncBoardAndHighlightsAtRow: updated currentSfenStr from branchTree";
            }
        }
    }
    QStringList* sfenRec = m_deps.getSfenRecord ? m_deps.getSfenRecord() : nullptr;
    if (!foundInBranch && sfenRec && ply >= 0 && ply < sfenRec->size()) {
        if (m_deps.currentSfenStr) {
            *m_deps.currentSfenStr = sfenRec->at(ply);
        }
        qCDebug(lcApp) << "syncBoardAndHighlightsAtRow: updated currentSfenStr=" << (m_deps.currentSfenStr ? *m_deps.currentSfenStr : QString());
    }

    // 定跡ウィンドウを更新
    if (m_deps.updateJosekiWindow) m_deps.updateJosekiWindow();

    qCDebug(lcApp) << "syncBoardAndHighlightsAtRow LEAVE";
}

// `handleBranchNodeHandled`: 分岐ノード処理完了後の盤面・ハイライト更新。
void KifuNavigationCoordinator::handleBranchNodeHandled(
    int ply, const QString& sfen,
    int previousFileTo, int previousRankTo,
    const QString& lastUsiMove)
{
    qCDebug(lcApp).noquote() << "onBranchNodeHandled ENTER ply=" << ply
                       << "sfen=" << sfen
                       << "fileTo=" << previousFileTo << "rankTo=" << previousRankTo
                       << "usiMove=" << lastUsiMove
                       << "playMode=" << (m_deps.playMode ? static_cast<int>(*m_deps.playMode) : -1)
                       << "match=" << (m_deps.match ? "valid" : "null");

    // plyインデックス変数を更新
    if (m_deps.activePly) *m_deps.activePly = ply;
    if (m_deps.currentSelectedPly) *m_deps.currentSelectedPly = ply;
    if (m_deps.currentMoveIndex) *m_deps.currentMoveIndex = ply;

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
        qCDebug(lcApp).noquote() << "onBranchNodeHandled: sending to engine:" << newPosition;
        if (m_deps.match->updateConsiderationPosition(newPosition, previousFileTo, previousRankTo, lastUsiMove)) {
            qCDebug(lcApp).noquote() << "onBranchNodeHandled: updateConsiderationPosition returned true";
            if (m_deps.analysisTab) {
                m_deps.analysisTab->startElapsedTimer();
            }
        } else {
            qCDebug(lcApp).noquote() << "onBranchNodeHandled: updateConsiderationPosition returned false (same position or not in consideration)";
        }
    } else {
        qCDebug(lcApp).noquote() << "onBranchNodeHandled: NOT in consideration mode or match/sfen missing";
    }
    qCDebug(lcApp).noquote() << "onBranchNodeHandled LEAVE";
}
