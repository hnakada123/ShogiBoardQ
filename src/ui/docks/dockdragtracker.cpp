/// @file dockdragtracker.cpp
/// @brief ドックをタイトルバーでつかんで動かすドラッグの、始まりと終わりの知らせの実装

#include "dockdragtracker.h"

#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QMainWindow>
#include <QMouseEvent>

namespace {
/// Qt がドックのドラッグを、OS のドラッグ＆ドロップとして行うか（QMainWindowLayout::needsPlatformDrag と同じ）。
/// このとき Qt は、ドラッグの始まりから終わりまでを、始まりのマウスの移動のイベントの処理の中で行う
bool usesPlatformDrag()
{
    static const bool wayland = QGuiApplication::platformName().startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);
    return wayland;
}

/// OS のタイトルバーでつかんだドラッグを、Qt がボタンを離した知らせで終えるか
/// （QDockWidgetPrivate::nonClientAreaMouseEvent と同じ。ほかの OS では、動かし終えた後の最初のマウスの移動で終える）
constexpr bool kNativeTitleDragEndsOnRelease =
#if defined(Q_OS_MACOS) || defined(Q_OS_WASM) || defined(Q_OS_HARMONY)
    true;
#else
    false;
#endif

/// ドラッグでドッキングできるドックか（Ctrl を押したドラッグと固定中のドックは、動かせてもドッキングしない）
bool canDockByDrag(const QDockWidget* dock, const QMouseEvent* event)
{
    if (event->modifiers().testFlag(Qt::ControlModifier)) return false;
    if (!dock->features().testFlag(QDockWidget::DockWidgetMovable)) return false;
    return dock->allowedAreas() != Qt::NoDockWidgetArea;
}

/// タイトルバーを押してドッキングできるドラッグを始められるか（QDockWidget が押下を受ける条件に合わせる）
bool startsDockingDrag(const QDockWidget* dock, const QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return false;
    if (!canDockByDrag(dock, event)) return false;
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
    case QEvent::NonClientAreaMouseButtonPress:
        nativeTitlePressed(dock, static_cast<const QMouseEvent*>(event));
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
        qApp->installEventFilter(this);
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
    // OS のタイトルバーでつかんだドラッグは、Qt が終える知らせで終える
    case QEvent::NonClientAreaMouseButtonRelease:
        if (m_dragging == dock && m_nativeTitleDrag && kNativeTitleDragEndsOnRelease) finish();
        break;
    case QEvent::NonClientAreaMouseMove:
        if (m_dragging == dock && m_nativeTitleDrag && !kNativeTitleDragEndsOnRelease) finish();
        break;
    default:
        break;
    }
}

bool DockDragTracker::eventFilter(QObject* watched, QEvent* event)
{
    // ドックのドラッグの間、ドロップを受け付ける子の部品に DragEnter を受けさせない（受けなければ、Qt は親へ順に
    // 渡し、メインウィンドウが受ける）。ウィンドウ（QWindow）へ届く元の知らせはそのまま通す
    if (event->type() == QEvent::DragEnter && m_dragging && watched->isWidgetType()
        && !qobject_cast<QMainWindow*>(watched)) {
        event->ignore();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void DockDragTracker::pressed(QDockWidget* dock, const QMouseEvent* event)
{
    // 前のドラッグの終わりを受け取れていなければ、ここで終える
    finish();
    m_pressed = startsDockingDrag(dock, event) ? dock : nullptr;
    m_pressPos = event->position().toPoint();
}

void DockDragTracker::nativeTitlePressed(QDockWidget* dock, const QMouseEvent* event)
{
    finish();
    // 浮動のドックの、OS が描くタイトルバーを押した（Windows・macOS）。Qt は押した時点でドラッグを始め、
    // ウィンドウが動くたびにドロップ先を探す（QDockWidgetPrivate::nonClientAreaMouseEvent・moveEvent）
    if (!dock->isFloating() || !canDockByDrag(dock, event)) return;
    // タイトルバーは中身（ウィンドウの領域）の上にある。枠のほかの辺は大きさの変更
    if (event->globalPosition().toPoint().y() >= dock->geometry().top()) return;
    m_dragging = dock;
    m_nativeTitleDrag = true;
    qApp->installEventFilter(this);
    emit dragStarted(dock);
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
    m_nativeTitleDrag = false;
    qApp->removeEventFilter(this);
    emit dragFinished();
}
