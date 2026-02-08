/// @file commentcoordinator.cpp
/// @brief コメントコーディネータクラスの実装

#include "commentcoordinator.h"

#include <QDebug>
#include <QStatusBar>

#include "engineanalysistab.h"
#include "recordpane.h"
#include "gamerecordmodel.h"
#include "recordpresenter.h"
#include "kifucontentbuilder.h"

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
    qDebug().noquote()
        << "[CommentCoordinator] onCommentUpdated"
        << " moveIndex=" << moveIndex
        << " newComment.len=" << newComment.size();

    // 有効な手数インデックスかチェック
    if (moveIndex < 0) {
        qWarning().noquote() << "[CommentCoordinator] onCommentUpdated: invalid moveIndex";
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
    qDebug().noquote() << "[CommentCoordinator] GameRecordModel::commentChanged ply=" << ply;
}

void CommentCoordinator::onCommentUpdateCallback(int ply, const QString& comment)
{
    qDebug().noquote() << "[CommentCoordinator] onCommentUpdateCallback ply=" << ply;

    if (!m_commentsByRow) {
        qWarning() << "[CommentCoordinator] onCommentUpdateCallback: m_commentsByRow is null";
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
        qDebug().noquote() << "[CommentCoordinator] Updated RecordPresenter commentsByRow";
    }

    // 現在表示中のコメントを更新（両方のコメント欄に反映）
    const QString displayComment = comment.trimmed().isEmpty() ? tr("コメントなし") : comment;
    broadcastComment(displayComment, /*asHtml=*/true);
}
