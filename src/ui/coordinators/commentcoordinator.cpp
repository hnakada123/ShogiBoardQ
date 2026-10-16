/// @file commentcoordinator.cpp
/// @brief 表示中の手のコメント・しおりの表示と編集

#include "commentcoordinator.h"
#include "commenteditorpanel.h"
#include "appsettings.h"
#include "dialogfontscale.h"
#include "kifudocument.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"

#include <QInputDialog>
#include <QStatusBar>

CommentCoordinator::CommentCoordinator(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.editor && m_deps.navState && m_deps.document);
    connect(m_deps.navState, &KifuNavigationState::changed, this, &CommentCoordinator::showCurrentComment);
    connect(m_deps.document, &KifuDocument::commentChanged, this, &CommentCoordinator::showCurrentComment);
}

void CommentCoordinator::showCurrentComment()
{
    const KifuBranchNode* node = m_deps.navState->currentNode();
    m_deps.editor->setCurrentMoveIndex(m_deps.navState->currentPly());
    // 棋譜のコメントはプレーンテキスト。表示用の整形で本文を書き換えない
    m_deps.editor->setCommentText(node ? node->comment() : QString());
}

bool CommentCoordinator::confirmLeaveEditingMove()
{
    return m_deps.editor->confirmDiscardUnsavedComment();
}

void CommentCoordinator::onCommentUpdated(int /*moveIndex*/, const QString& newComment)
{
    // コメント欄が編集しているのは表示中の手（移る前に未更新の確認を通している）
    KifuBranchNode* node = m_deps.navState->currentNode();
    if (!node) return;
    m_deps.document->setComment(node, newComment);
    if (m_deps.statusBar)
        m_deps.statusBar->showMessage(tr("コメントを更新しました（手数: %1）").arg(node->ply()), 3000);
}

void CommentCoordinator::onBookmarkEditRequested()
{
    KifuBranchNode* node = m_deps.navState->currentNode();
    if (!node) {
        if (m_deps.statusBar) m_deps.statusBar->showMessage(tr("手を選択してください"), 3000);
        return;
    }

    QInputDialog dialog(m_deps.parentWidget);
    dialog.setWindowTitle(tr("しおりを編集"));
    dialog.setLabelText(tr("しおり名（手数: %1）:").arg(node->ply()));
    dialog.setTextValue(node->bookmark());
    dialog.ensurePolished();
    DialogFontScale::install(&dialog, AppSettings::bookmarkDialogFontSize, {&AppSettings::bookmarkDialogSize});
    if (dialog.exec() != QDialog::Accepted) return;

    const QString newBookmark = dialog.textValue();
    m_deps.document->setBookmark(node, newBookmark);
    if (m_deps.statusBar) {
        m_deps.statusBar->showMessage(newBookmark.isEmpty() ? tr("しおりを削除しました（手数: %1）").arg(node->ply())
                                                            : tr("しおりを設定しました（手数: %1）").arg(node->ply()),
                                      3000);
    }
}
