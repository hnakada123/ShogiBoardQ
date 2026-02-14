/// @file commentcoordinator.cpp
/// @brief コメントコーディネータクラスの実装

#include "commentcoordinator.h"
#include "mainwindow.h"

#include <QStatusBar>
#include <QInputDialog>
#include <QTableView>
#include <QItemSelectionModel>

#include "engineanalysistab.h"
#include "recordpane.h"
#include "gamerecordmodel.h"
#include "gamerecordpresenter.h"
#include "kifucontentbuilder.h"
#include "kifurecordlistmodel.h"

CommentCoordinator::CommentCoordinator(QObject* parent)
    : QObject(parent)
{
}

void CommentCoordinator::broadcastComment(const QString& text, bool asHtml)
{
    // 現在の手数インデックスをEngineAnalysisTabに設定
    if (m_analysisTab && m_currentMoveIndex) {
        m_analysisTab->setCurrentMoveIndex(*m_currentMoveIndex);
    }

    if (asHtml) {
        // 「*の手前で改行」＋「URLリンク化」付きのHTMLに整形して配信
        const QString html = KifuContentBuilder::toRichHtmlWithStarBreaksAndLinks(text);
        if (m_analysisTab) m_analysisTab->setCommentHtml(html);
        if (m_recordPane)  m_recordPane->setBranchCommentHtml(html);
    } else {
        // プレーンテキスト経路は従来通り
        if (m_analysisTab) m_analysisTab->setCommentText(text);
        if (m_recordPane)  m_recordPane->setBranchCommentText(text);
    }
}

void CommentCoordinator::onCommentUpdated(int moveIndex, const QString& newComment)
{
    qCDebug(lcApp).noquote()
        << "onCommentUpdated"
        << " moveIndex=" << moveIndex
        << " newComment.len=" << newComment.size();

    // 有効な手数インデックスかチェック
    if (moveIndex < 0) {
        qCWarning(lcApp).noquote() << "onCommentUpdated: invalid moveIndex";
        return;
    }

    // GameRecordModel がない場合は初期化を要求
    if (!m_gameRecord) {
        emit ensureGameRecordModelRequested();
    }

    // GameRecordModel を使ってコメントを更新
    if (m_gameRecord) {
        m_gameRecord->setComment(moveIndex, newComment);
    }

    // ステータスバーに通知
    if (m_statusBar) {
        m_statusBar->showMessage(tr("コメントを更新しました（手数: %1）").arg(moveIndex), 3000);
    }
}

void CommentCoordinator::onGameRecordCommentChanged(int ply, const QString& /*comment*/)
{
    qCDebug(lcApp).noquote() << "GameRecordModel::commentChanged ply=" << ply;
}

void CommentCoordinator::onCommentUpdateCallback(int ply, const QString& comment)
{
    qCDebug(lcApp).noquote() << "onCommentUpdateCallback ply=" << ply;

    if (!m_commentsByRow) {
        qCWarning(lcApp) << "onCommentUpdateCallback: m_commentsByRow is null";
        return;
    }

    // m_commentsByRow への同期（互換性・RecordPresenterへの供給用）
    while (m_commentsByRow->size() <= ply) {
        m_commentsByRow->append(QString());
    }
    (*m_commentsByRow)[ply] = comment;

    // RecordPresenter のコメント配列も更新（行選択時に正しいコメントを表示するため）
    if (m_recordPresenter) {
        QStringList updatedComments;
        updatedComments.reserve(m_commentsByRow->size());
        for (const QString& c : std::as_const(*m_commentsByRow)) {
            updatedComments.append(c);
        }
        m_recordPresenter->setCommentsByRow(updatedComments);
        qCDebug(lcApp).noquote() << "Updated RecordPresenter commentsByRow";
    }

    // KifuRecordListModel の該当行を更新
    if (m_kifuRecordModel != nullptr) {
        auto* item = m_kifuRecordModel->item(ply);
        if (item != nullptr) {
            item->setComment(comment);
            // dataChanged を発火して表示を更新
            const QModelIndex tl = m_kifuRecordModel->index(ply, 3);  // コメント列
            const QModelIndex br = m_kifuRecordModel->index(ply, 3);
            emit m_kifuRecordModel->dataChanged(tl, br, { Qt::DisplayRole });
        }
    }

    // 現在表示中のコメントを更新（両方のコメント欄に反映）
    const QString displayComment = comment.trimmed().isEmpty() ? tr("コメントなし") : comment;
    broadcastComment(displayComment, /*asHtml=*/true);
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
    bool ok = false;
    QWidget* parentWidget = m_recordPane ? m_recordPane : qobject_cast<QWidget*>(parent());
    const QString newBookmark = QInputDialog::getText(
        parentWidget,
        tr("しおりを編集"),
        tr("しおり名（手数: %1）:").arg(ply),
        QLineEdit::Normal,
        currentBookmark,
        &ok
    );

    if (!ok) return; // キャンセル

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

void CommentCoordinator::onBookmarkUpdateCallback(int ply, const QString& bookmark)
{
    qCDebug(lcApp).noquote() << "onBookmarkUpdateCallback ply=" << ply;

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
