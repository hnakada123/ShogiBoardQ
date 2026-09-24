#ifndef TSUMESHOGI_TEST_USI_H
#define TSUMESHOGI_TEST_USI_H

#include <QObject>
#include <QStringList>

// ジェネレータの状態遷移テスト用。送信を記録し、応答はテストから注入する。
class Usi : public QObject
{
    Q_OBJECT
public:
    Usi(void*, void*, void*, QObject* parent) : QObject(parent) {}
    bool startAndInitializeEngine(const QString&, const QString&) { return true; }
    void setClonedBoardData(const QList<QChar>&) {}
    void cancelCurrentOperation() {}
    void cleanupEngineProcessAndThread(bool = true) {}
    void sendStopCommand() { ++stops; }
    void sendPositionAndGoMateCommands(int timeout, QString& position)
    {
        positions.append(position);
        timeouts.append(timeout);
    }
    QStringList positions;
    QList<int> timeouts;
    int stops = 0;
signals:
    void checkmateSolved(const QStringList&);
    void checkmateNoMate();
    void checkmateNotImplemented();
    void checkmateUnknown();
    void errorOccurred(const QString&);
};

#endif
