#ifndef DEBUGSCREENSHOTWIRING_H
#define DEBUGSCREENSHOTWIRING_H

#include <QtGlobal> // QT_DEBUG の定義に必要
#ifdef QT_DEBUG

/// @file debugscreenshotwiring.h
/// @brief F12キーでスクリーンショットを撮るためのショートカット配線

#include <QObject>
#include <memory>

class QMainWindow;
class DebugScreenshotService;

/**
 * @brief QShortcut::activated を受けて DebugScreenshotService を呼び出す中継クラス
 *
 * F12 キー押下で MainWindow 全体のスクリーンショットをキャプチャする。
 * デバッグビルド専用（QT_DEBUGガード）。
 */
class DebugScreenshotWiring final : public QObject {
    Q_OBJECT
public:
    explicit DebugScreenshotWiring(QMainWindow* mainWindow, QObject* parent = nullptr);
    ~DebugScreenshotWiring() override;
    DebugScreenshotService* service() const;

public slots:
    void onShortcutActivated();

private:
    std::unique_ptr<DebugScreenshotService> m_service;
};

#endif // QT_DEBUG
#endif // DEBUGSCREENSHOTWIRING_H
