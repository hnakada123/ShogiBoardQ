/// @file dockdragtracker.cpp
/// @brief ドックをタイトルバーでつかんで動かすドラッグの、始まりと終わりの知らせの実装

#include "dockdragtracker.h"

#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>

namespace {
/// Qt がドックのドラッグを、OS のドラッグ＆ドロップとして行うか（QMainWindowLayout::needsPlatformDrag と同じ）。
/// このとき Qt は、ドラッグの始まりから終わりまでを、始まりのマウスの移動のイベントの処理の中で行う
bool usesPlatformDrag()
{
    static const bool wayland = QGuiApplication::platformName().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);
    return wayland;
}

/// タイトルバーを押してドッキングできるドラッグを始められるか（QDockWidget が押下を受ける条件に合わせる）
bool startsDockingDrag(const QDockWidget* dock, const QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return false;
    // Ctrl を押したドラッグと固定中のドックは、動かせてもドッキングしない
    if (event->modifiers().testFlag(Qt::ControlModifier)) return false;
    if (!dock->features().testFlag(QDockWidget::DockWidgetMovable)) return false;
    if (dock->allowedAreas() == Qt::NoDockWidgetArea) return false;
    // タイトルバーは中身の上にある（中身の上で押したイベントが届くのは、中身が受けなかったとき）
    const QWidget* content = dock->widget();
    return !content || event->position().toPoint().y() < content->geometry().top();
}
} // namespace

DockDragTracker::DockDragTracker(QObject* parent)
    : QObject(parent)
{
    m_releaseWatch.setInterval(200);
    connect(&m_releaseWatch, &QTimer::timeout, this, &DockDragTracker::finishIfReleased);
}

void DockDragTracker::beforeEvent(QDockWidget* dock, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        pressed(dock, static_cast<const QMouseEvent*>(event));
        break;
    case QEvent::MouseMove: {
        if (m_pressed != dock || m_dragging) break;
        const auto* mouse = static_cast<const QMouseEvent*>(event);
        if (!mouse->buttons().testFlag(Qt::LeftButton)) {
            m_pressed = nullptr;
            break;
        }
        // QDockWidget は、押した位置からこの距離を超えて動いたときにドックを引き出す
        if ((mouse->position().toPoint() - m_pressPos).manhattanLength() <= QApplication::startDragDistance()) break;
        m_dragging = dock;
        if (!usesPlatformDrag()) m_releaseWatch.start();
        emit dragStarted(dock);
        break;
    }
    default:
        break;
    }
}

void DockDragTracker::afterEvent(QDockWidget* dock, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseMove:
        if (m_dragging == dock && usesPlatformDrag()) finish();
        break;
    case QEvent::MouseButtonRelease:
        if (static_cast<const QMouseEvent*>(event)->button() != Qt::LeftButton) break;
        if (m_pressed == dock) m_pressed = nullptr;
        if (m_dragging == dock) finish();
        break;
    default:
        break;
    }
}

void DockDragTracker::pressed(QDockWidget* dock, const QMouseEvent* event)
{
    // 前のドラッグの終わりを受け取れていなければ、ここで終える
    finish();
    m_pressed = startsDockingDrag(dock, event) ? dock : nullptr;
    m_pressPos = event->position().toPoint();
}

void DockDragTracker::finishIfReleased()
{
    if (!QGuiApplication::mouseButtons().testFlag(Qt::LeftButton)) finish();
}

void DockDragTracker::finish()
{
    m_releaseWatch.stop();
    m_pressed = nullptr;
    if (!m_dragging) return;
    m_dragging = nullptr;
    emit dragFinished();
}
