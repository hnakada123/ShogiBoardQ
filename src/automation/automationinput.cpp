/// @file automationinput.cpp
/// @brief 自動化 API が送る通常のマウス入力の実装

#include "automationinput.h"

#include <QApplication>
#include <QMouseEvent>
#include <QWidget>

namespace AutomationInput {

namespace {
void send(QWidget* receiver, QEvent::Type type, const QPoint& global, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(receiver->mapFromGlobal(global)), QPointF(global), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(receiver, &event);
}
} // namespace

void sendClick(QWidget* receiver, const QPoint& pos, Qt::MouseButton button)
{
    const QPoint global = receiver->mapToGlobal(pos);
    send(receiver, QEvent::MouseMove, global, Qt::NoButton, Qt::NoButton);
    send(receiver, QEvent::MouseButtonPress, global, button, button);
    send(receiver, QEvent::MouseButtonRelease, global, button, Qt::NoButton);
}

void sendDrag(QWidget* receiver, const QList<QPoint>& globalPath)
{
    if (globalPath.isEmpty()) return;
    send(receiver, QEvent::MouseButtonPress, globalPath.constFirst(), Qt::LeftButton, Qt::LeftButton);
    for (qsizetype i = 1; i < globalPath.size(); ++i)
        send(receiver, QEvent::MouseMove, globalPath.at(i), Qt::NoButton, Qt::LeftButton);
    send(receiver, QEvent::MouseButtonRelease, globalPath.constLast(), Qt::LeftButton, Qt::NoButton);
}

} // namespace AutomationInput
