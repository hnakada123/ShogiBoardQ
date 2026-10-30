#ifndef COLUMNWIDTHMEMORY_H
#define COLUMNWIDTHMEMORY_H

/// @file columnwidthmemory.h
/// @brief 利用者が決めた表の列幅の覚えと保存（表の見出し TableHeaderView に付ける）

#include <QList>
#include <QObject>

#include "setting.h"

class TableHeaderView;

/**
 * @brief 利用者が決めた表の列幅を覚えて保存する
 *
 * 規則は1つ: 列ごとに幅を持ち、0 は自動（表が内容・文字の大きさに合わせて決める）、正の値は利用者が決めた幅。
 * 利用者が見出しの境目をドラッグしたとき（TableHeaderView::sectionResizedByUser）だけ、その列の幅を覚えて保存する。
 * 復元・自動の幅では保存しない。表示領域に合わせて伸び縮みする最後の列（stretchLastSection）も覚えない。表は列幅を決めるとき widthOr() で、利用者の幅があれば
 * それを、なければ自分で決めた幅を使う（利用者の幅を最小値として扱わない）。
 */
class ColumnWidthMemory : public QObject
{
    Q_OBJECT

public:
    /// header の列 0〜columns-1 の幅を setting に覚える（作るときに保存した値を読む。header が親）
    ColumnWidthMemory(TableHeaderView* header, const ListSetting<int>& setting, int columns);

    /// 利用者が決めた幅（0 は自動）
    int userWidth(int column) const { return m_widths.value(column); }
    /// 利用者が決めた幅があればそれを、なければ automatic を返す
    int widthOr(int column, int automatic) const;

signals:
    /// 利用者が列 column の幅を変え、覚えて保存した
    void userWidthChanged(int column);

private slots:
    void onSectionResizedByUser(int column, int oldSize, int newSize);

private:
    TableHeaderView* m_header = nullptr;
    ListSetting<int> m_setting;
    QList<int> m_widths;   ///< 列ごとの幅（0 は自動）
};

#endif // COLUMNWIDTHMEMORY_H
