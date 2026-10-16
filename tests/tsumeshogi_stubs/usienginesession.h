#ifndef TSUMESHOGI_TEST_USIENGINESESSION_H
#define TSUMESHOGI_TEST_USIENGINESESSION_H

#include <QObject>
#include <QString>

#include "usigotime.h"
#include "usiposition.h"
#include "usiprotocolhandler.h"

struct UsiSearchRequest {
    UsiPosition position;
    UsiGo go;
    int stopAfterMs = 0;
    int replyTimeoutMs = 0;
};

// ジェネレータの状態遷移テスト用。起動は即座に成功し、探索の依頼は handler() が記録する。
class UsiEngineSession : public QObject
{
    Q_OBJECT
public:
    explicit UsiEngineSession(QObject* parent = nullptr) : QObject(parent), m_handler(new UsiProtocolHandler(this)) {}
    bool startAsync(const QString&, const QString&, QString* = nullptr) { return true; }
    bool isInitializing() const { return false; }
    void cancelCurrentOperation() {}
    void quit() {}
    void search(const UsiSearchRequest& request)
    {
        m_handler->positions.append(request.position.command());
        m_handler->timeouts.append(request.go.mateTimeMs);
    }
    void stop() { ++m_handler->stops; }
    UsiProtocolHandler* handler() const { return m_handler; }
signals:
    void ready();
    void errorOccurred(const QString&);
    void searchTimedOut();
private:
    UsiProtocolHandler* m_handler;
};

#endif
