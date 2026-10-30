#ifndef GAMEINFOPANE_H
#define GAMEINFOPANE_H

/// @file gameinfopane.h
/// @brief 対局情報ドックの中身（編集用の操作ボタンと表）

#include <QList>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "fontsizehelper.h"
#include "kifparsetypes.h"  // KifGameInfoItem

class ColumnWidthMemory;
class EditActions;
class GameInfoEditModel;
class QLabel;
class QLineEdit;
class QTableView;
class QToolButton;

/**
 * @brief 対局情報の表と、その編集の操作（元に戻す・切り取り・行の追加など）
 *
 * 棋譜の文書（KifuDocument）の対局情報を編集する画面。表の中身が変わるたびに gameInfoEdited で
 * 文書に知らせ、文書が対局情報を置き換えたら setGameInfo で表を作り直す。未保存かは文書が決める
 * （入力中のセルは hasPendingEditor で数え、保存の前に commitPendingEditor で確定する）。
 * 表の中身と履歴は GameInfoEditModel が持つ。このクラスは表の見た目
 * （項目名の列幅・行の高さ・文字の大きさ）と、ボタン・ショートカットの操作を受け持つ。
 */
class GameInfoPane : public QWidget
{
    Q_OBJECT

public:
    explicit GameInfoPane(QWidget* parent = nullptr);
    ~GameInfoPane() override;

    QTableView* tableView() const { return m_table; }
    GameInfoEditModel* model() const { return m_model; }

    /// 対局情報を設定する（文書が対局情報を置き換えた。編集の履歴は消す）
    void setGameInfo(const QList<KifGameInfoItem>& items, bool beforeMatchStart = false);
    QList<KifGameInfoItem> gameInfo() const;

    /// 終了日時などを自動で更新する（編集中の他の項目と履歴は保つ）
    void updateGameInfoValue(const QString& key, const QString& value);

    /// 入力中のセルに、まだ表に入れていない文字があるか（未保存に数える）
    bool hasPendingEditor() const;
    /// 入力中のセルを確定する（保存・確認の前に使う。Ctrl+Enter でも確定する）
    void commitPendingEditor();

    int fontSize() const { return m_fontSize.fontSize(); }
    void setFontSize(int size) { m_fontSize.setFontSize(size); }

public slots:
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    /// 新しい行を足して項目名の入力を始める
    void addRow();
    /// 選択した行を削除する（元に戻す操作で復元できる）
    void removeRow();

signals:
    /// 表の中身が変わった（文書の対局情報を合わせる）
    void gameInfoEdited(const QList<KifGameInfoItem>& items);

private slots:
    void updateEditingIndicator();
    void notifyEdited();
    void observeEditor();
    void updateTablePresentation();
    void applyFontSize();

private:
    void buildToolbar();
    QLineEdit* activeEditor() const;
    /// フォーカスのあるセルの入力欄（切り取り・コピー・貼り付けは入力欄の選択範囲に効かせる）
    QLineEdit* focusedEditor() const;
    /// 選択しているセル（無ければ無効）
    QModelIndex selectedCell() const;
    /// 履歴を移した後も、同じ位置のセルを選んでおく
    void restoreCurrentCell(int row, int column);

    GameInfoEditModel* m_model = nullptr;
    QPointer<QWidget> m_toolbar;
    QTableView* m_table = nullptr;
    QToolButton* m_btnFontIncrease = nullptr;
    QToolButton* m_btnFontDecrease = nullptr;
    EditActions* m_editActions = nullptr;  ///< 元に戻す〜貼り付け（ボタンと標準ショートカット）
    QToolButton* m_btnAddRow = nullptr;
    QToolButton* m_btnRemoveRow = nullptr;
    QLabel* m_editingLabel = nullptr;

    FontSizeHelper m_fontSize;      ///< 表の文字の大きさ（変更のたびに保存する）
    ColumnWidthMemory* m_columnWidths = nullptr;  ///< 利用者が決めた項目名の列幅（0 は内容に合わせる。見出しが親）
};

#endif // GAMEINFOPANE_H
