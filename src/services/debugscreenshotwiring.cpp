/// @file debugscreenshotwiring.cpp
/// @brief F12キーでスクリーンショットを撮るためのショートカット配線の実装

#include "debugscreenshotwiring.h"

#ifdef QT_DEBUG
#include "debugscreenshotservice.h"

#include <QMainWindow>
#include <QShortcut>
#include <memory>

DebugScreenshotWiring::DebugScreenshotWiring(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_service(std::make_unique<DebugScreenshotService>(mainWindow))
{
    auto* shortcut = new QShortcut(Qt::Key_F12, mainWindow);
    connect(shortcut, &QShortcut::activated, this, &DebugScreenshotWiring::onShortcutActivated);
}

DebugScreenshotWiring::~DebugScreenshotWiring() = default;

DebugScreenshotService* DebugScreenshotWiring::service() const
{
    return m_service.get();
}

void DebugScreenshotWiring::onShortcutActivated()
{
    m_service->captureMainWindow();
}

#endif // QT_DEBUG
