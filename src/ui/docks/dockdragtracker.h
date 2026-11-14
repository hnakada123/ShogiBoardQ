#ifndef DOCKDRAGTRACKER_H
#define DOCKDRAGTRACKER_H

/// @file dockdragtracker.h
/// @brief ドックをタイトルバーでつかんで動かすドラッグの、始まりと終わりの知らせ

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QTimer>

class QDockWidget;
class QEvent;
class QMouseEvent;

/**
 * @brief ドックをタイトルバーでつかんで動かすドラッグの、始まりと終わりを知らせる
 *
 * QDockWidget はドラッグの始まりと終わりを知らせないので、ドックが受けたマウスのイベントから
 * Qt と同じ条件で求める。ドックは受けたイベントを、QDockWidget に渡す前（beforeEvent）と
 * 渡した後（afterEvent）にここへ渡す。
 *
 * 知らせるのは、ドッキングできるドラッグだけ（Ctrl を押したドラッグと、固定中のドックは
 * ドッキングしないので知らせない）。
 *
 * - dragStarted は、Qt がドックを引き出してドロップ先を探し始める前に出す
 * - dragFinished は、Qt がドラッグを終えた（ドッキングを始めた・浮動のまま置いた）後に出す
 *
 * 対象は、Qt が描くタイトルバーのドラッグ（Linux の X11・Wayland の全部と、Windows・macOS の
 * 収まっているドック）。Windows・macOS の浮動のドックは OS のタイトルバーで動かすので知らせない。
 */
class DockDragTracker : public QObject
{
    Q_OBJECT

public:
    explicit DockDragTracker(QObject* parent = nullptr);

    /// ドックが受けたイベントを、QDockWidget が処理する前に渡す
    void beforeEvent(QDockWidget* dock, QEvent* event);
    /// 同じイベントを、QDockWidget が処理した後に渡す
    void afterEvent(QDockWidget* dock, QEvent* event);

    /// ドラッグの途中か
    bool isDragging() const { return !m_dragging.isNull(); }

signals:
    /// ドラッグが始まる。dock は、まだ始まる前の状態（収まっているか浮動か）のまま
    void dragStarted(QDockWidget* dock);
    /// ドラッグが終わった
    void dragFinished();

private slots:
    /// ボタンを離したのにドックへ届かなかったとき（ドラッグを途中でやめた等）に終わらせる
    void finishIfReleased();

private:
    void pressed(QDockWidget* dock, const QMouseEvent* event);
    void finish();

    QPointer<QDockWidget> m_pressed;   ///< タイトルバーを押したドック（まだ動かしていない）
    QPointer<QDockWidget> m_dragging;  ///< ドラッグしているドック
    QPoint m_pressPos;                 ///< 押した位置（ドックの座標）
    QTimer m_releaseWatch;
};

#endif // DOCKDRAGTRACKER_H
