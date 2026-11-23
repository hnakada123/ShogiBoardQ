#ifndef DRAG_EVENT_HELPER_H
#define DRAG_EVENT_HELPER_H

/// @file drag_event_helper.h
/// @brief 試験から部品へ送るドラッグの知らせを、使っている Qt の版に合うコンストラクタで作る

#include <QDragEnterEvent>
#include <QMimeData>
#include <QPointF>
#include <QtGlobal>

namespace DragEvents {

/// 位置 pos に入ったドラッグの知らせ。QDragEnterEvent の位置は、Qt 6.11 までは QPoint だけで受け、6.12 で QPointF を
/// 受けるコンストラクタができて QPoint のものは非推奨になった（CMake の最小は 6.7）
inline QDragEnterEvent enter(QPointF pos, Qt::DropActions actions, const QMimeData* data, Qt::MouseButtons buttons,
                             Qt::KeyboardModifiers modifiers)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    return QDragEnterEvent(pos, actions, data, buttons, modifiers);
#else
    return QDragEnterEvent(pos.toPoint(), actions, data, buttons, modifiers);
#endif
}

} // namespace DragEvents

#endif // DRAG_EVENT_HELPER_H
