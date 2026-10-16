#ifndef HEADER_DRAG_HELPER_H
#define HEADER_DRAG_HELPER_H

/// @file header_drag_helper.h
/// @brief 表の見出しの境目をマウスでドラッグして列幅を変える（利用者の操作の再現）

#include <QHeaderView>
#include <QTest>

namespace HeaderDrag {

/// logicalIndex の列の右端の境目を押してドラッグし、列幅を width にする
inline void resizeSection(QHeaderView* header, int logicalIndex, int width)
{
    QWidget* viewport = header->viewport();
    const int edge = header->sectionViewportPosition(logicalIndex) + header->sectionSize(logicalIndex) - 1;
    const int y = header->height() / 2;
    const QPoint from(edge, y);
    const QPoint to(edge + width - header->sectionSize(logicalIndex), y);
    QTest::mousePress(viewport, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(viewport, to);
    QTest::mouseRelease(viewport, Qt::LeftButton, Qt::NoModifier, to);
}

} // namespace HeaderDrag

#endif // HEADER_DRAG_HELPER_H
