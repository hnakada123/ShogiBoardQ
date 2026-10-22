#ifndef PVTABLEVIEW_H
#define PVTABLEVIEW_H

/// @file pvtableview.h
/// @brief 読み筋の表（思考・検討の表と棋譜解析の結果の表で共通の部分）

#include <QList>

#include "incrementaltableview.h"

class QMenu;

/**
 * @brief 読み筋を1行ずつ出す表
 *
 * 数値の列は右寄せの3桁区切り、「盤面」列はボタン（クリックか Enter で boardRequested）、読み筋の列は
 * 幅に合わせて短くして描く。行の右クリックのメニューの項目は受け手が足す（rowContextMenuAboutToShow）。行は1行ずつ選び、編集はしない。文字の大きさは setFontPointSize で変え、
 * 行の高さを合わせる。列幅の決め方は使う側（ThinkingTableView・棋譜解析の結果の表）が決める。
 */
class PvTableView : public IncrementalTableView
{
    Q_OBJECT

public:
    /// 列の役割（無い役割は -1）
    struct Columns {
        QList<int> numeric;   ///< 右寄せの3桁区切りで出す列
        int board = -1;       ///< 読み筋の局面を開くボタンの列
        int pv = -1;          ///< 読み筋の列
    };

    /// rowPadding は行の高さを文字の高さより大きくする分（ピクセル）
    PvTableView(const PvTableView::Columns& columns, int rowPadding, QWidget* parent = nullptr);

    /// 文字の大きさ（ポイント）を変え、行の高さを合わせる（列幅は fitColumns で使う側が合わせる）
    void setFontPointSize(int pointSize);

    /// 選択を外す（読み筋の局面を閉じたとき）
    void clearRowSelection();

signals:
    /// 行の「盤面」ボタンが押された（クリックか Enter）
    void boardRequested(int row);
    /// 行を右クリックした。受け手が menu に項目を足す（同じスレッドでつなぐ。足されなければメニューは出さない）
    void rowContextMenuAboutToShow(int row, QMenu* menu);

protected:
    /// 文字の大きさを変えた後に列幅を合わせる
    virtual void fitColumns() {}

private slots:
    void onClicked(const QModelIndex& index);
    void onActivated(const QModelIndex& index);
    void onCustomContextMenuRequested(const QPoint& pos);

private:
    void updateRowHeight();

    Columns m_columns;
    int m_rowPadding;
};

#endif // PVTABLEVIEW_H
