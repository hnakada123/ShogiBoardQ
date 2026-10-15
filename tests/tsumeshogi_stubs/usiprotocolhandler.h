#ifndef TSUMESHOGI_TEST_USIPROTOCOLHANDLER_H
#define TSUMESHOGI_TEST_USIPROTOCOLHANDLER_H

#include <QList>
#include <QObject>
#include <QStringList>

// ジェネレータの状態遷移テスト用。送信を記録し、応答はテストから注入する。
class UsiProtocolHandler : public QObject
{
    Q_OBJECT
public:
    explicit UsiProtocolHandler(QObject* parent = nullptr) : QObject(parent) {}
    void sendPosition(const QString& position) { positions.append(position); }
    void sendGoMate(int timeMs, bool = false) { timeouts.append(timeMs); }
    void sendStop() { ++stops; }
    QStringList positions;
    QList<int> timeouts;
    int stops = 0;
signals:
    void checkmateSolved(const QStringList&);
    void checkmateNoMate();
    void checkmateNotImplemented();
    void checkmateUnknown();
};

#endif
