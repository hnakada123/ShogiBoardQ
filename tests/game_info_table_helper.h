#ifndef GAME_INFO_TABLE_HELPER_H
#define GAME_INFO_TABLE_HELPER_H

/// @file game_info_table_helper.h
/// @brief 対局情報の表（GameInfoPane の QTableView）をセル単位で読み書きする

#include <QAbstractItemModel>
#include <QTableView>

struct GameInfoTable {
    QTableView* view = nullptr;

    QModelIndex index(int row, int column) const { return view->model()->index(row, column); }
    int rowCount() const { return view->model()->rowCount(); }
    /// 書いたままの文字列
    QString text(int row, int column) const { return index(row, column).data(Qt::EditRole).toString(); }
    /// 利用者がセルを書き換えたのと同じ（編集の履歴に積む）
    void setText(int row, int column, const QString& text) { view->model()->setData(index(row, column), text); }
    bool editable(int row, int column) const { return index(row, column).flags() & Qt::ItemIsEditable; }
    QString toolTip(int row, int column) const { return index(row, column).data(Qt::ToolTipRole).toString(); }
    QString accessibleText(int row, int column) const
    {
        return index(row, column).data(Qt::AccessibleTextRole).toString();
    }
    void setCurrentCell(int row, int column) { view->setCurrentIndex(index(row, column)); }
    /// 選んでいるセルの入力を始める
    void editCurrent() { view->edit(view->currentIndex()); }
};

#endif // GAME_INFO_TABLE_HELPER_H
