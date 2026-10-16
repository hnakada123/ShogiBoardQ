#ifndef DEBUGSCREENSHOTSHORTCUT_H
#define DEBUGSCREENSHOTSHORTCUT_H

#include <QtGlobal> // QT_DEBUG の定義に必要
#ifdef QT_DEBUG

/// @file debugscreenshotshortcut.h
/// @brief F12キーでメインウィンドウのスクリーンショットを撮る（デバッグビルド専用）

#include <QObject>
#include <memory>

class QMainWindow;
class ScreenshotService;

/**
 * @brief F12 のショートカットを作り、押されたら ScreenshotService で撮る
 *
 * F12 キー押下で MainWindow 全体のスクリーンショットをキャプチャする。
 * デバッグビルド専用（QT_DEBUGガード）。
 */
class DebugScreenshotShortcut final : public QObject {
    Q_OBJECT
public:
    explicit DebugScreenshotShortcut(QMainWindow* mainWindow, QObject* parent = nullptr);
    ~DebugScreenshotShortcut() override;
    ScreenshotService* service() const;

public slots:
    void onShortcutActivated();

private:
    std::unique_ptr<ScreenshotService> m_service;
};

#endif // QT_DEBUG
#endif // DEBUGSCREENSHOTSHORTCUT_H
