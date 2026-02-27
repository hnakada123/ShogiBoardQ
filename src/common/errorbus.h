#ifndef ERRORBUS_H
#define ERRORBUS_H

/// @file errorbus.h
/// @brief アプリケーション全体のエラー通知を一元管理するイベントバス

#include <QObject>
#include <QString>

/**
 * @brief シングルトンのエラー通知バス
 *
 * どこからでも postError() でエラーを発行し、errorOccurred シグナルで
 * 購読者へ一斉配信する。
 *
 */
class ErrorBus final : public QObject {
    Q_OBJECT

public:
    // --- 公開API ---

    /// シングルトンインスタンスを返す
    static ErrorBus& instance();

    /// エラーメッセージを発行する
    void postError(const QString& message) {
        emit errorOccurred(message);
    }

signals:
    // --- シグナル ---

    /// エラーが発生した（→ MainWindow::onErrorBusOccurred）
    void errorOccurred(const QString& message);

private:
    explicit ErrorBus(QObject* parent = nullptr) : QObject(parent) {}
    Q_DISABLE_COPY_MOVE(ErrorBus)
};

#endif // ERRORBUS_H
