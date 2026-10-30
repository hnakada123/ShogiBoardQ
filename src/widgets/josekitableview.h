#ifndef JOSEKITABLEVIEW_H
#define JOSEKITABLEVIEW_H

/// @file josekitableview.h
/// @brief 定跡ウィンドウの定跡手の表（ボタンの列・ダブルクリック・右クリックのメニュー）

#include <QTableView>
#include <functional>

class JosekiTableModel;
class QAction;
class QLabel;
class QMenu;

/**
 * @brief 定跡手の表
 *
 * 「着手」「編集」「削除」の列のクリック・ダブルクリック（着手）・右クリックのメニューを、行の操作
 * （rowActionRequested）にして知らせる。何をするかは表を持つ側が決める。メニューの項目を押せるかは
 * setActionAvailability で渡した判定で、メニューを開くたびに決める。定跡手が無いときの案内も表の中に出す。
 * 横の見出しは TableHeaderView（利用者が列幅を変えたときだけ sectionResizedByUser を出す）。
 */
class JosekiTableView : public QTableView
{
    Q_OBJECT

public:
    /// 行の操作
    enum class RowAction {
        Play,             ///< 着手
        Edit,             ///< 編集...
        Delete,           ///< 削除
        CopyMove,         ///< 指し手をコピー
        MoveToTop,        ///< 先頭へ移動
        MoveUp,           ///< 上へ移動
        MoveDown,         ///< 下へ移動
        SortByFrequency,  ///< 出現頻度の順に並べる
        SortByValue,      ///< 評価値の順に並べる
    };
    Q_ENUM(RowAction)

    explicit JosekiTableView(JosekiTableModel *model, QWidget *parent = nullptr);

    /// メニューの項目を押せるかの判定（行と操作を受け取る）。無ければすべて押せる
    void setActionAvailability(std::function<bool(JosekiTableView::RowAction, int)> available);
    /// row 行目のメニューの項目を押せるかを決め直す（メニューを開く前に呼ぶ）
    void updateMenuActions(int row);
    /// メニューの項目
    QAction *action(JosekiTableView::RowAction rowAction) const;

    /// 定跡手が無いときに表の中に出す案内（空なら隠す）
    void setEmptyGuide(const QString &text);

signals:
    void rowActionRequested(JosekiTableView::RowAction action, int row);

private slots:
    void onClicked(const QModelIndex &index);
    void onDoubleClicked(const QModelIndex &index);
    void onContextMenuRequested(const QPoint &pos);
    void onMenuTriggered(QAction *triggered);

private:
    QAction *addMenuAction(const QString &text, JosekiTableView::RowAction rowAction);

    JosekiTableModel *m_model = nullptr;
    QMenu *m_menu = nullptr;
    QLabel *m_emptyGuide = nullptr;
    std::function<bool(JosekiTableView::RowAction, int)> m_available;
    int m_menuRow = -1;  ///< 開いているメニューの行
};

#endif // JOSEKITABLEVIEW_H
