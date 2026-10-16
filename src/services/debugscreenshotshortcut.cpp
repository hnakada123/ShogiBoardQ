/// @file debugscreenshotshortcut.cpp
/// @brief F12キーでスクリーンショットを撮るショートカットの実装

#include "debugscreenshotshortcut.h"

#ifdef QT_DEBUG
#include "screenshotservice.h"

#include <QMainWindow>
#include <QShortcut>
#include <memory>

DebugScreenshotShortcut::DebugScreenshotShortcut(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_service(std::make_unique<ScreenshotService>(mainWindow))
{
    // 従来どおりデバッグ用の保存先を使う
    m_service->setOutputDirectory(QStringLiteral("/tmp/shogiboardq-debug"));
    auto* shortcut = new QShortcut(Qt::Key_F12, mainWindow);
    connect(shortcut, &QShortcut::activated, this, &DebugScreenshotShortcut::onShortcutActivated);
}

DebugScreenshotShortcut::~DebugScreenshotShortcut() = default;

ScreenshotService* DebugScreenshotShortcut::service() const
{
    return m_service.get();
}

void DebugScreenshotShortcut::onShortcutActivated()
{
    m_service->captureMainWindow();
}

#endif // QT_DEBUG
