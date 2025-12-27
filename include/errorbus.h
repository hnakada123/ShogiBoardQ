#ifndef ERRORBUS_H
#define ERRORBUS_H

#include <QObject>
#include <QString>

class ErrorBus final : public QObject {
    Q_OBJECT
public:
    static ErrorBus& instance();

signals:
    void errorOccurred(const QString& message);

public:
    // どこからでも呼べる公開API
    void postError(const QString& message) {
        emit errorOccurred(message);
    }

private:
    explicit ErrorBus(QObject* parent = nullptr) : QObject(parent) {}
    Q_DISABLE_COPY_MOVE(ErrorBus)
};

#endif // ERRORBUS_H
