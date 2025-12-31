#include "recordnavigationcontroller.h"

#include <QDebug>
#include <QTableView>
#include <QItemSelectionModel>
#include <QSignalBlocker>

#include "kifutypes.h"
#include "shogiview.h"
#include "boardsyncpresenter.h"
#include "kifurecordlistmodel.h"
#include "kifuloadcoordinator.h"
#include "replaycontroller.h"
#include "engineanalysistab.h"
#include "recordpane.h"
#include "recordpresenter.h"

RecordNavigationController::RecordNavigationController(QObject* parent)
    : QObject(parent)
{
}

RecordNavigationController::~RecordNavigationController() = default;

// --------------------------------------------------------
// 依存オブジェクトの設定
// --------------------------------------------------------

void RecordNavigationController::setShogiView(ShogiView* view)
{
    m_shogiView = view;
}

void RecordNavigationController::setBoardSyncPresenter(BoardSyncPresenter* boardSync)
{
    m_boardSync = boardSync;
}

void RecordNavigationController::setKifuRecordModel(KifuRecordListModel* model)
{
    m_kifuRecordModel = model;
}

void RecordNavigationController::setKifuLoadCoordinator(KifuLoadCoordinator* coordinator)
{
    m_kifuLoadCoordinator = coordinator;
}

void RecordNavigationController::setReplayController(ReplayController* replayController)
{
    m_replayController = replayController;
}

void RecordNavigationController::setAnalysisTab(EngineAnalysisTab* analysisTab)
{
    m_analysisTab = analysisTab;
}

void RecordNavigationController::setRecordPane(RecordPane* recordPane)
{
    m_recordPane = recordPane;
}

void RecordNavigationController::setRecordPresenter(GameRecordPresenter* presenter)
{
    m_recordPresenter = presenter;
}

// --------------------------------------------------------
// 状態参照の設定
// --------------------------------------------------------

void RecordNavigationController::setResolvedRows(const QVector<ResolvedRow>* resolvedRows)
{
    m_resolvedRows = resolvedRows;
}

void RecordNavigationController::setActiveResolvedRow(const int* activeResolvedRow)
{
    m_activeResolvedRow = activeResolvedRow;
}

// --------------------------------------------------------
// コールバック設定
// --------------------------------------------------------

void RecordNavigationController::setEnsureBoardSyncCallback(EnsureBoardSyncCallback cb)
{
    m_ensureBoardSync = std::move(cb);
}

void RecordNavigationController::setEnableArrowButtonsCallback(EnableArrowButtonsCallback cb)
{
    m_enableArrowButtons = std::move(cb);
}

void RecordNavigationController::setSetCurrentTurnCallback(SetCurrentTurnCallback cb)
{
    m_setCurrentTurn = std::move(cb);
}

void RecordNavigationController::setBroadcastCommentCallback(BroadcastCommentCallback cb)
{
    m_broadcastComment = std::move(cb);
}

void RecordNavigationController::setApplyResolvedRowAndSelectCallback(ApplyResolvedRowAndSelectCallback cb)
{
    m_applyResolvedRowAndSelect = std::move(cb);
}

void RecordNavigationController::setUpdatePlyStateCallback(UpdatePlyStateCallback cb)
{
    m_updatePlyState = std::move(cb);
}

// --------------------------------------------------------
// ナビゲーション処理
// --------------------------------------------------------

void RecordNavigationController::syncBoardAndHighlightsAtRow(int ply)
{
    // 位置編集モード中はスキップ
    if (m_shogiView && m_shogiView->positionEditMode()) {
        qDebug() << "[RecordNav] syncBoardAndHighlightsAtRow skipped (edit-mode). ply=" << ply;
        return;
    }

    if (m_ensureBoardSync) {
        m_ensureBoardSync();
    }

    if (m_boardSync) {
        m_boardSync->syncBoardAndHighlightsAtRow(ply);
    }

    // 矢印ボタンの活性化
    if (m_enableArrowButtons) {
        m_enableArrowButtons();
    }
}

void RecordNavigationController::applySelect(int row, int ply)
{
    // 未保存コメントの確認
    if (!checkUnsavedComment(ply)) {
        return;  // ナビゲーションを中断
    }

    // ライブ append 中 or 解決済み行が未構築のとき
    const bool liveAppendMode = m_replayController ? m_replayController->isLiveAppendMode() : false;
    const bool resolvedRowsEmpty = !m_resolvedRows || m_resolvedRows->isEmpty();

    if (liveAppendMode || resolvedRowsEmpty) {
        navigateKifuViewToRow(ply);
        return;
    }

    // 通常（KIF再生/分岐再生）ルート
    if (m_applyResolvedRowAndSelect) {
        m_applyResolvedRowAndSelect(row, ply);
    }
}

void RecordNavigationController::onRecordRowChangedByPresenter(int row, const QString& comment)
{
    const int modelRowsBefore =
        (m_kifuRecordModel ? m_kifuRecordModel->rowCount() : -1);
    const int presenterCurBefore =
        (m_recordPresenter ? m_recordPresenter->currentRow() : -1);

    qDebug().noquote()
        << "[RecordNav] onRecordRowChangedByPresenter ENTER"
        << " row=" << row
        << " comment.len=" << comment.size()
        << " modelRows(before)=" << modelRowsBefore
        << " presenter.cur(before)=" << presenterCurBefore;

    // 未保存コメントの確認
    const int editingRow = (m_analysisTab ? m_analysisTab->currentMoveIndex() : -1);
    qDebug().noquote()
        << "[RecordNav] UNSAVED_COMMENT_CHECK:"
        << " hasUnsavedComment=" << (m_analysisTab ? m_analysisTab->hasUnsavedComment() : false)
        << " row=" << row
        << " editingRow=" << editingRow;

    if (m_analysisTab && m_analysisTab->hasUnsavedComment()) {
        if (row != editingRow) {
            qDebug().noquote() << "[RecordNav] row differs, showing confirm dialog...";
            if (!m_analysisTab->confirmDiscardUnsavedComment()) {
                // キャンセル：元の行に戻す
                qDebug().noquote() << "[RecordNav] User cancelled, reverting to row=" << editingRow;
                if (m_recordPane && m_recordPane->kifuView()) {
                    QTableView* kifuView = m_recordPane->kifuView();
                    if (kifuView->model() && editingRow >= 0 && editingRow < kifuView->model()->rowCount()) {
                        QSignalBlocker blocker(kifuView->selectionModel());
                        kifuView->setCurrentIndex(kifuView->model()->index(editingRow, 0));
                    }
                }
                return;
            }
        }
    }

    // 盤面・ハイライト同期
    if (row >= 0) {
        qDebug().noquote() << "[RecordNav] syncBoardAndHighlightsAtRow row=" << row;
        syncBoardAndHighlightsAtRow(row);

        // 現在手数トラッキングを更新
        if (m_updatePlyState) {
            m_updatePlyState(row, row, row);
        }

        // 分岐候補欄の更新
        if (m_kifuLoadCoordinator && m_resolvedRows && m_activeResolvedRow) {
            const qsizetype rows = m_resolvedRows->size();
            const int resolvedRow = (rows <= 0) ? 0 : static_cast<int>(qBound(qsizetype(0), qsizetype(*m_activeResolvedRow), rows - 1));
            const int safePly = (row < 0) ? 0 : row;

            qDebug().noquote()
                << "[RecordNav] showBranchCandidates"
                << " resolvedRow=" << resolvedRow
                << " safePly=" << safePly;

            m_kifuLoadCoordinator->showBranchCandidates(resolvedRow, safePly);
        }
    }

    // コメント表示
    const QString cmt = comment.trimmed();
    if (m_broadcastComment) {
        m_broadcastComment(cmt.isEmpty() ? tr("コメントなし") : cmt, true);
    }

    // 矢印ボタンなどの活性化
    if (m_enableArrowButtons) {
        m_enableArrowButtons();
    }

    qDebug().noquote() << "[RecordNav] onRecordRowChangedByPresenter LEAVE";
}

// --------------------------------------------------------
// Private メソッド
// --------------------------------------------------------

bool RecordNavigationController::checkUnsavedComment(int targetRow)
{
    if (!m_analysisTab || !m_analysisTab->hasUnsavedComment()) {
        return true;
    }

    const int editingRow = m_analysisTab->currentMoveIndex();
    if (targetRow == editingRow) {
        return true;  // 同じ行への移動は警告不要
    }

    qDebug().noquote()
        << "[RecordNav] checkUnsavedComment"
        << " targetRow=" << targetRow
        << " editingRow=" << editingRow;

    if (!m_analysisTab->confirmDiscardUnsavedComment()) {
        qDebug().noquote() << "[RecordNav] User cancelled navigation";
        return false;
    }

    return true;
}

void RecordNavigationController::navigateKifuViewToRow(int ply)
{
    qDebug().noquote() << "[RecordNav-DEBUG] navigateKifuViewToRow ENTER ply=" << ply;

    if (!m_recordPane || !m_kifuRecordModel) {
        qDebug().noquote() << "[RecordNav-DEBUG] navigateKifuViewToRow ABORT: recordPane or kifuRecordModel is null";
        return;
    }

    QTableView* view = m_recordPane->kifuView();
    if (!view) {
        qDebug().noquote() << "[RecordNav-DEBUG] navigateKifuViewToRow ABORT: kifuView is null";
        return;
    }

    const int rows = m_kifuRecordModel->rowCount();
    const int safe = (rows > 0) ? qBound(0, ply, rows - 1) : 0;

    qDebug().noquote() << "[RecordNav-DEBUG] navigateKifuViewToRow: ply=" << ply
                       << "rows=" << rows << "safe=" << safe;

    const QModelIndex idx = m_kifuRecordModel->index(safe, 0);
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
    m_kifuRecordModel->setCurrentHighlightRow(safe);

    // 盤・ハイライト即時同期
    syncBoardAndHighlightsAtRow(safe);

    // トラッキングを更新
    if (m_updatePlyState) {
        m_updatePlyState(safe, safe, safe);
    }

    // 手番表示を更新
    if (m_setCurrentTurn) {
        m_setCurrentTurn();
    }

    qDebug().noquote() << "[RecordNav-DEBUG] navigateKifuViewToRow LEAVE";
}
