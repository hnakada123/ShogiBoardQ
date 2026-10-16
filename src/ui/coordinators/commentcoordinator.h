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
 */
class CommentCoordinator : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        CommentEditorPanel* editor = nullptr;        ///< コメント欄（必須）
        KifuNavigationState* navState = nullptr;     ///< 表示中の手（必須）
        KifuDocument* document = nullptr;            ///< ノードへの書き込み（必須）
        QWidget* parentWidget = nullptr;             ///< しおりの入力ダイアログの親
        QStatusBar* statusBar = nullptr;             ///< 書き込んだ知らせ
    };

    explicit CommentCoordinator(const Deps& deps, QObject* parent = nullptr);

    /// 別の手へ移る前に、未更新のコメントを破棄してよいか確認する（やめたら false）
    bool confirmLeaveEditingMove();

public slots:
    /// 表示中の手のコメントをコメント欄に出す
    void showCurrentComment();
    /// コメント欄の「コメント更新」
    void onCommentUpdated(int moveIndex, const QString& newComment);
    /// 棋譜欄の「しおり」ボタン（表示中の手のしおりを編集する）
    void onBookmarkEditRequested();

private:
    Deps m_deps;
};

#endif // COMMENTCOORDINATOR_H
