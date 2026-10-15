#ifndef TSUMESHOGI_TEST_USI_H
#define TSUMESHOGI_TEST_USI_H

#include <QObject>
#include <QStringList>

// ジェネレータの状態遷移テスト用。送信を記録し、応答はテストから注入する。
class Usi : public QObject
{
    Q_OBJECT
public:
    Usi(void*, void*, QObject* parent) : QObject(parent) {}
    bool isInitializing() const { return false; }
    bool startEngine(const QString&, const QString&) { return true; }
    void cancelCurrentOperation() {}
    void cleanupEngineProcessAndThread(bool = true) {}
    void sendStopCommand() { ++stops; }
    void sendMateSearch(const QString& position, int timeout)
    {
        positions.append(position);
        timeouts.append(timeout);
    }
    QStringList positions;
    QList<int> timeouts;
    int stops = 0;
signals:
    void engineInitialized();
    void checkmateSolved(const QStringList&);
    void checkmateNoMate();
    void checkmateNotImplemented();
    void checkmateUnknown();
    void errorOccurred(const QString&);
};

#endif
