#ifndef COMMENTEDITORPANEL_H
#define COMMENTEDITORPANEL_H

/// @file commenteditorpanel.h
/// @brief 棋譜コメント編集パネルクラスの定義

#include <QObject>
#include <QString>
#include <QPoint>

#include "fontsizehelper.h"

class QWidget;
class QTextEdit;
class QToolButton;
class QPushButton;
class QLabel;
class QEvent;

class CommentEditorPanel : public QObject
{
    Q_OBJECT

public:
    explicit CommentEditorPanel(QObject* parent = nullptr);
    ~CommentEditorPanel() override;

    /// コメント編集UIを構築し、コンテナウィジェットを返す
    QWidget* buildCommentUi(QWidget* parent);

    /// プレーンテキストでコメントを設定（URLをリンクに変換）
    void setCommentText(const QString& text);

    /// 整形済みHTMLでコメントを設定
    void setCommentHtml(const QString& html);

    /// 現在の手数インデックスを設定
    void setCurrentMoveIndex(int index);

    /// 現在の手数インデックスを取得
    int currentMoveIndex() const { return m_currentMoveIndex; }

    /// 未保存の編集があるかチェック
    bool hasUnsavedComment() const { return m_isCommentDirty; }

    /// 未保存編集の警告ダイアログを表示（移動を続けるならtrue）
    bool confirmDiscardUnsavedComment();

    /// 編集状態をクリア（コメント更新後に呼ぶ）
    void clearCommentDirty();

    /// プレーンテキストをエスケープし、URLをHTMLリンクに変換
    static QString convertUrlsToLinks(const QString& text);

signals:
    /// コメント更新シグナル
    void commentUpdated(int moveIndex, const QString& newComment);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private slots:
    /// コメント欄の文字の大きさを変える（A-/A+）
    void applyCommentFontSize(int size);
    void onUpdateCommentClicked();
    void onCommentTextChanged();
    void onCommentUndo();
    void onCommentRedo();
    void onCommentCut();
    void onCommentCopy();
    void onCommentPaste();
    void updatePasteAvailability();

private:
    void buildCommentToolbar(QWidget* parentWidget);
    void updateEditingIndicator();
    void fixPositionLabelSize();
    bool showsAppliedHtml(const QString& html) const;

    // UI
    QTextEdit* m_comment = nullptr;
    QWidget* m_commentViewport = nullptr;
    QWidget* m_commentToolbar = nullptr;
    QToolButton* m_btnFontIncrease = nullptr;
    QToolButton* m_btnFontDecrease = nullptr;
    QToolButton* m_btnCommentUndo = nullptr;
    QToolButton* m_btnCommentRedo = nullptr;
    QToolButton* m_btnCommentCut = nullptr;
    QToolButton* m_btnCommentCopy = nullptr;
    QToolButton* m_btnCommentPaste = nullptr;
    QPushButton* m_btnUpdateComment = nullptr;
    QLabel* m_editingLabel = nullptr;
    QLabel* m_positionLabel = nullptr;

    // State
    FontSizeHelper m_fontSize;   ///< コメント欄の文字の大きさ（変更のたびに保存する）
    int m_currentMoveIndex = -1;
    QString m_originalComment;
    bool m_isCommentDirty = false;
    QString m_appliedHtml;              ///< 最後に setHtml で表示した文書
    bool m_appliedHtmlIntact = false;   ///< その後、文書の中身が変わっていないか（textChanged が来ていない）
    QString m_pressedAnchor;
    QPoint m_linkPressPosition;
};

#endif // COMMENTEDITORPANEL_H
