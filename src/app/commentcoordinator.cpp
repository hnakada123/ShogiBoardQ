/// @file commentcoordinator.cpp
/// @brief コメントコーディネータクラスの実装

#include "dialogfontscale.h"
#include "commentcoordinator.h"
#include "logcategories.h"
#include "mainwindow.h"

#include <QStatusBar>
#include <QInputDialog>
#include <QTableView>
#include <QItemSelectionModel>

#include "commenteditorpanel.h"
#include "recordpane.h"
#include "gamerecordmodel.h"
#include "gamerecordpresenter.h"
#include "kifurecordlistmodel.h"

CommentCoordinator::CommentCoordinator(QObject* parent)
    : QObject(parent)
{
}

void CommentCoordinator::broadcastComment(const QString& text, bool asHtml)
{
    Q_UNUSED(asHtml)
    // 現在の手数インデックスをCommentEditorPanelに設定
    if (m_commentEditor && m_currentMoveIndex) {
        m_commentEditor->setCurrentMoveIndex(*m_currentMoveIndex);
    }

    // 棋譜のコメントはプレーンテキスト。表示用の整形で本文を書き換えない。
    if (m_commentEditor) m_commentEditor->setCommentText(text);
}

bool CommentCoordinator::handleRecordRowChangeRequest(int row, const QString& comment)
{
    // 未保存コメントの確認
    const int editingRow = m_commentEditor ? m_commentEditor->currentMoveIndex() : -1;
    if (m_commentEditor && m_commentEditor->hasUnsavedComment()) {
        if (row == editingRow) return true;
        if (row != editingRow) {
            if (!m_commentEditor->confirmDiscardUnsavedComment()) {
                // キャンセル：元の行に戻す
                if (m_recordPane && m_recordPane->kifuView()) {
                    QTableView* kifuView = m_recordPane->kifuView();
                    if (kifuView->model() && editingRow >= 0
                        && editingRow < kifuView->model()->rowCount()) {
                        QSignalBlocker blocker(kifuView->selectionModel());
                        kifuView->setCurrentIndex(
                            kifuView->model()->index(editingRow, 0));
                    }
                }
                return false;
            }
        }
    }

    // コメント表示
    broadcastComment(comment, true);
    return true;
}

void CommentCoordinator::onCommentUpdated(int moveIndex, const QString& newComment)
{
    // m_currentMoveIndex を優先して有効なインデックスを決定する
    // (EngineAnalysisTab::m_state.currentMoveIndex は broadcastComment() 経由でしか
    // 同期されないため、MainWindow::m_state.currentMoveIndex を信頼する)
    const int effectiveIndex = (m_currentMoveIndex && *m_currentMoveIndex >= 0)
                                   ? *m_currentMoveIndex : moveIndex;

    qCDebug(lcApp).noquote()
        << "onCommentUpdated"
        << " signalIndex=" << moveIndex
        << " effectiveIndex=" << effectiveIndex
        << " newComment.len=" << newComment.size();

    // 有効な手数インデックスかチェック
    if (effectiveIndex < 0) {
        qCWarning(lcApp).noquote() << "onCommentUpdated: invalid effectiveIndex";
        return;
    }

    // GameRecordModel がない場合は初期化を要求
    if (!m_gameRecord) {
        emit ensureGameRecordModelRequested();
    }

    // GameRecordModel を使ってコメントを更新
    if (m_gameRecord) {
        m_gameRecord->setComment(effectiveIndex, newComment);
    }

    // ステータスバーに通知
    if (m_statusBar) {
        m_statusBar->showMessage(tr("コメントを更新しました（手数: %1）").arg(effectiveIndex), 3000);
    }
}

void CommentCoordinator::onGameRecordCommentChanged(int ply, const QString& comment)
{
    // KifuRecordListModel の該当行を更新
    if (m_kifuRecordModel != nullptr) {
        auto* item = m_kifuRecordModel->item(ply);
        if (item != nullptr) {
            item->setComment(comment);
            // dataChanged を発火して表示を更新
            const QModelIndex tl = m_kifuRecordModel->index(ply, 3);  // コメント列
            const QModelIndex br = m_kifuRecordModel->index(ply, 3);
            emit m_kifuRecordModel->dataChanged(tl, br, { Qt::DisplayRole, Qt::ToolTipRole });
        }
    }

    // 現在表示中のコメントを更新（両方のコメント欄に反映）
    broadcastComment(comment, /*asHtml=*/true);
}

void CommentCoordinator::onBookmarkEditRequested()
{
    // 現在選択中の行（ply）を取得
    int ply = -1;
    if (m_recordPresenter) {
        ply = m_recordPresenter->currentRow();
    }
    // プレゼンターから取得できない場合は RecordPane の kifuView から直接取得
    if (ply < 0 && m_recordPane) {
        if (auto* kifuView = m_recordPane->kifuView()) {
            if (auto* sel = kifuView->selectionModel()) {
                const QModelIndex idx = sel->currentIndex();
                if (idx.isValid()) {
                    ply = idx.row();
                }
            }
        }
    }
    if (ply < 0) {
        if (m_statusBar) {
            m_statusBar->showMessage(tr("手を選択してください"), 3000);
        }
        return;
    }

    // GameRecordModel がない場合は初期化を要求
    if (!m_gameRecord) {
        emit ensureGameRecordModelRequested();
    }

    // 現在のしおりテキストを取得
    QString currentBookmark;
    if (m_gameRecord) {
        currentBookmark = m_gameRecord->bookmark(ply);
    }

    // 入力ダイアログを表示
    QWidget* parentWidget = m_recordPane ? m_recordPane : qobject_cast<QWidget*>(parent());
    QInputDialog dialog(parentWidget);
    dialog.setWindowTitle(tr("しおりを編集"));
    dialog.setLabelText(tr("しおり名（手数: %1）:").arg(ply));
    dialog.setTextValue(currentBookmark);
    dialog.ensurePolished();
    DialogFontScale::install(&dialog, QStringLiteral("bookmark"), true);
    if (dialog.exec() != QDialog::Accepted) return;
    const QString newBookmark = dialog.textValue();

    // GameRecordModel に保存
    if (m_gameRecord) {
        m_gameRecord->setBookmark(ply, newBookmark);
    }

    // ステータスバーに通知
    if (m_statusBar) {
        if (newBookmark.isEmpty()) {
            m_statusBar->showMessage(tr("しおりを削除しました（手数: %1）").arg(ply), 3000);
        } else {
            m_statusBar->showMessage(tr("しおりを設定しました（手数: %1）").arg(ply), 3000);
        }
    }
}

void CommentCoordinator::onNavigationCommentUpdate(int ply, const QString& comment, bool asHtml)
{
    if (m_currentMoveIndex) {
        *m_currentMoveIndex = ply;
    }
    broadcastComment(comment, asHtml);
}

void CommentCoordinator::onBookmarkChanged(int ply, const QString& bookmark)
{
    qCDebug(lcApp).noquote() << "onBookmarkChanged ply=" << ply;

    // KifuRecordListModel の該当行を更新
    if (m_kifuRecordModel != nullptr) {
        auto* item = m_kifuRecordModel->item(ply);
        if (item != nullptr) {
            item->setBookmark(bookmark);
            // dataChanged を発火して表示を更新
            const QModelIndex tl = m_kifuRecordModel->index(ply, 2);  // しおり列
            const QModelIndex br = m_kifuRecordModel->index(ply, 2);
            emit m_kifuRecordModel->dataChanged(tl, br, { Qt::DisplayRole });
        }
    }
}
