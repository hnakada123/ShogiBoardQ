#ifndef EDITACTIONS_H
#define EDITACTIONS_H

/// @file editactions.h
/// @brief 元に戻す・やり直す・切り取り・コピー・貼り付けの QAction の組（標準ショートカット付き）

#include <QObject>
#include <array>

class QAction;
class QToolButton;
class QWidget;

/**
 * @brief 編集欄のツールバーで使う5つの編集操作
 *
 * 文字・アイコン・ツールチップ（実際のショートカットを添える）と標準ショートカットはここだけで決める。
 * QAction は host に追加し、ショートカットは host とその子にフォーカスがあるときだけ効く
 * （Qt::WidgetWithChildrenShortcut）。入力欄にフォーカスがあるときは、入力欄が自分で処理する。
 * 何をするかは呼び出し側が action(kind) の triggered を受けて決め、有効・無効も呼び出し側が決める。
 * ボタンは createButton() で作り、QAction の状態（有効・無効）に従う。
 */
class EditActions : public QObject
{
    Q_OBJECT

public:
    enum Kind { Undo, Redo, Cut, Copy, Paste, KindCount };

    /// 5つの QAction を作って host に追加する（この部品は host の子になる。初めはすべて無効）
    explicit EditActions(QWidget* host);

    QAction* action(Kind kind) const { return m_actions.at(static_cast<std::size_t>(kind)); }
    void setEnabled(Kind kind, bool on);

    /// action(kind) を既定の操作にするボタン（フォーカスを取らず、入力欄の選択範囲を保つ）
    QToolButton* createButton(Kind kind, QWidget* parent) const;

private:
    std::array<QAction*, KindCount> m_actions{};
};

#endif // EDITACTIONS_H
