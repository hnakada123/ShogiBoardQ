#ifndef COMMENTCOORDINATOR_H
#define COMMENTCOORDINATOR_H

/// @file commentcoordinator.h
/// @brief 表示中の手のコメント・しおりの表示と編集

#include <QObject>
#include <QString>

class CommentEditorPanel;
class KifuDocument;
class KifuNavigationState;
class QStatusBar;
class QWidget;

/**
 * @brief 表示中の手（KifuNavigationState）のコメントをコメント欄に出し、編集をその手のノードに書く
 *
 * コメント・しおりの正本は分岐ツリーのノード。書き込みは棋譜の文書（KifuDocument）を通す（未保存の判定と通知のため）。
 * コメント欄の本文は、表示した手（コメントを書いている手）に書く。利用者が別の手へ移るときは移る前に未更新の
 * 確認を通し、対局の進行などで表示中の手が変わったときは、書きかけの本文をそれまでの手に確定してから切り替える。
 */
class CommentCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        CommentEditorPanel* editor = nullptr;        ///< コメント欄（必須）
        KifuNavigationState* navState = nullptr;     ///< 表示中の手（必須）
        KifuDocument* document = nullptr;            ///< ノードへの書き込み（必須）
        QWidget* parentWidget = nullptr;             ///< しおりの入力・未更新の確認のダイアログの親
        QStatusBar* statusBar = nullptr;             ///< 書き込んだ知らせ
    };

    explicit CommentCoordinator(const Deps& deps, QObject* parent = nullptr);

    /// 別の手へ移る前に、未更新のコメントを更新するか捨てるか尋ねる（棋譜・定跡と同じ UnsavedChangesPrompt。
    /// 「保存」は書いていた手に確定し、「破棄」は捨てる。やめたら false）
    bool confirmLeaveEditingMove();
    /// コメント欄に未更新の本文があるか（棋譜の未保存に数える）
    bool hasPendingComment() const;
    /// 未更新の本文を、書いていた手に確定する（保存・書き出しの前）
    void commitPendingComment();

public slots:
    /// 表示中の手のコメントをコメント欄に出す
    void showCurrentComment();
    /// コメント欄の「コメント更新」（書いていた手のコメントにする）
    void onCommentUpdated(const QString& newComment);
    /// 棋譜欄の「しおり」ボタン（表示中の手のしおりを編集する）
    void onBookmarkEditRequested();

private:
    Deps m_deps;
    int m_editingNodeId = -1;   ///< コメント欄に出している手（コメントを書く手）のノード ID
};

#endif // COMMENTCOORDINATOR_H
