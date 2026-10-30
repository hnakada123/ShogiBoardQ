/// @file columnwidthmemory.cpp
/// @brief 利用者が決めた表の列幅の覚えと保存の実装

#include "columnwidthmemory.h"

#include "tableheaderview.h"

ColumnWidthMemory::ColumnWidthMemory(TableHeaderView* header, const ListSetting<int>& setting, int columns)
    : QObject(header)
    , m_header(header)
    , m_setting(setting)
    , m_widths(setting())
{
    m_widths.resize(columns);   // 足りない列は自動、多い分は捨てる
    for (int& width : m_widths) width = qMax(0, width);
    connect(header, &TableHeaderView::sectionResizedByUser, this, &ColumnWidthMemory::onSectionResizedByUser);
}

int ColumnWidthMemory::widthOr(int column, int automatic) const
{
    const int width = userWidth(column);
    return width > 0 ? width : automatic;
}

void ColumnWidthMemory::onSectionResizedByUser(int column, int, int newSize)
{
    if (column < 0 || column >= m_widths.size() || newSize <= 0) return;
    // 伸び縮みする最後の列は、境目のドラッグで隣の列と一緒に変わるだけなので覚えない
    if (m_header->stretchLastSection() && column == m_header->count() - 1) return;
    m_widths[column] = newSize;
    m_setting.set(m_widths);
    emit userWidthChanged(column);
}
