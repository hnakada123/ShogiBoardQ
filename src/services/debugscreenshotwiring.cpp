/// @file debugscreenshotwiring.cpp
/// @brief F12キーでスクリーンショットを撮るためのショートカット配線の実装

#include "debugscreenshotwiring.h"

#ifdef QT_DEBUG
#include "debugscreenshotservice.h"

#include <QMainWindow>
#include <QShortcut>

DebugScreenshotWiring::DebugScreenshotWiring(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_service(new DebugScreenshotService(mainWindow))
{
    auto* shortcut = new QShortcut(Qt::Key_F12, mainWindow);
    connect(shortcut, &QShortcut::activated, this, &DebugScreenshotWiring::onShortcutActivated);
}

DebugScreenshotService* DebugScreenshotWiring::service() const
{
    return m_service;
}

void DebugScreenshotWiring::onShortcutActivated()
{
    m_service->captureMainWindow();
}

#endif // QT_DEBUG
