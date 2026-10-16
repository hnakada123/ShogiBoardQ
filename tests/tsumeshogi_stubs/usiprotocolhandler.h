#ifndef TSUMESHOGI_TEST_USIPROTOCOLHANDLER_H
#define TSUMESHOGI_TEST_USIPROTOCOLHANDLER_H

#include <QList>
#include <QObject>
#include <QStringList>

#include "usimessages.h"

// ジェネレータの状態遷移テスト用。送った探索を記録し、応答はテストから注入する。
class UsiProtocolHandler : public QObject
{
    Q_OBJECT
public:
    explicit UsiProtocolHandler(QObject* parent = nullptr) : QObject(parent) {}
    QStringList positions;
    QList<int> timeouts;
    int stops = 0;
signals:
    void checkmateReceived(const UsiCheckmate& result);
};

#endif
