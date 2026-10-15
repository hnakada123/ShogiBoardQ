#ifndef TSUMESHOGI_TEST_USIENGINESESSION_H
#define TSUMESHOGI_TEST_USIENGINESESSION_H

#include <QObject>
#include <QString>

#include "usiprotocolhandler.h"

// ジェネレータの状態遷移テスト用。起動は即座に成功し、送信は handler() が記録する。
class UsiEngineSession : public QObject
{
    Q_OBJECT
public:
    explicit UsiEngineSession(QObject* parent = nullptr) : QObject(parent), m_handler(new UsiProtocolHandler(this)) {}
    bool startAsync(const QString&, const QString&, QString* = nullptr) { return true; }
    bool isInitializing() const { return false; }
    void cancelCurrentOperation() {}
    void quit() {}
    UsiProtocolHandler* handler() const { return m_handler; }
signals:
    void ready();
    void errorOccurred(const QString&);
private:
    UsiProtocolHandler* m_handler;
};

#endif
