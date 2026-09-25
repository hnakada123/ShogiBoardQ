/// @file tst_automation_dispatcher.cpp
/// @brief AutomationDispatcher（JSON-RPC 2.0 解析・メソッド表）のユニットテスト

#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>

#include "automationdispatcher.h"
#include "automationparams.h"

class TestAutomationDispatcher : public QObject
{
    Q_OBJECT

private:
    static QJsonObject parse(const QByteArray& response)
    {
        return QJsonDocument::fromJson(response).object();
    }

    static int errorCode(const QJsonObject& response)
    {
        return response.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt();
    }

    static AutomationDispatcher makeDispatcher()
    {
        AutomationDispatcher dispatcher;
        dispatcher.registerMethod(QStringLiteral("echo"), [](const QJsonObject& params) {
            return QJsonValue(params);
        });
        dispatcher.registerMethod(QStringLiteral("add"), [](const QJsonObject& params) {
            const int a = AutomationParams::requireInt(params, QStringLiteral("a"), -1000, 1000);
            const int b = AutomationParams::optionalInt(params, QStringLiteral("b"), 0, -1000, 1000);
            return QJsonValue(a + b);
        });
        dispatcher.registerMethod(QStringLiteral("fail"), [](const QJsonObject&) -> QJsonValue {
            throw AutomationError(AutomationErrorCode::NotAllowed, QStringLiteral("nope"), QStringLiteral("try something else"));
        });
        dispatcher.registerMethod(QStringLiteral("crash"), [](const QJsonObject&) -> QJsonValue {
            throw std::runtime_error("boom");
        });
        return dispatcher;
    }

private slots:
    void resultAndParams()
    {
        const AutomationDispatcher dispatcher = makeDispatcher();
        const QJsonObject r = parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":7,"method":"add","params":{"a":2,"b":3}})"));
        QCOMPARE(r.value(QStringLiteral("jsonrpc")).toString(), QStringLiteral("2.0"));
        QCOMPARE(r.value(QStringLiteral("id")).toInt(), 7);
        QCOMPARE(r.value(QStringLiteral("result")).toInt(), 5);
        QVERIFY(!r.contains(QStringLiteral("error")));

        const QJsonObject s = parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":"x","method":"echo","params":{"k":"v"}})"));
        QCOMPARE(s.value(QStringLiteral("id")).toString(), QStringLiteral("x"));
        QCOMPARE(s.value(QStringLiteral("result")).toObject().value(QStringLiteral("k")).toString(), QStringLiteral("v"));

        // params 省略は空オブジェクトとして扱う
        const QJsonObject t = parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":1,"method":"echo"})"));
        QVERIFY(t.value(QStringLiteral("result")).isObject());
    }

    void standardErrors()
    {
        const AutomationDispatcher dispatcher = makeDispatcher();
        QCOMPARE(errorCode(parse(dispatcher.handleLine("{not json"))), AutomationErrorCode::ParseError);
        QCOMPARE(errorCode(parse(dispatcher.handleLine("[]"))), AutomationErrorCode::InvalidRequest);
        QCOMPARE(errorCode(parse(dispatcher.handleLine(R"({"id":1,"method":"echo"})"))), AutomationErrorCode::InvalidRequest);
        const QJsonObject unknown = parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":2,"method":"nope"})"));
        QCOMPARE(errorCode(unknown), AutomationErrorCode::MethodNotFound);
        QVERIFY(unknown.value(QStringLiteral("error")).toObject().value(QStringLiteral("data")).toObject()
                    .value(QStringLiteral("hint")).toString().contains(QStringLiteral("echo")));
        QCOMPARE(errorCode(parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":3,"method":"echo","params":[1]})"))),
                 AutomationErrorCode::InvalidParams);
        // 必須パラメータ欠落と範囲外
        QCOMPARE(errorCode(parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":4,"method":"add","params":{}})"))),
                 AutomationErrorCode::InvalidParams);
        QCOMPARE(errorCode(parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":5,"method":"add","params":{"a":5000}})"))),
                 AutomationErrorCode::InvalidParams);
    }

    void handlerErrorsAndExceptions()
    {
        const AutomationDispatcher dispatcher = makeDispatcher();
        const QJsonObject fail = parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":9,"method":"fail"})"));
        const QJsonObject error = fail.value(QStringLiteral("error")).toObject();
        QCOMPARE(error.value(QStringLiteral("code")).toInt(), AutomationErrorCode::NotAllowed);
        QCOMPARE(error.value(QStringLiteral("message")).toString(), QStringLiteral("nope"));
        QCOMPARE(error.value(QStringLiteral("data")).toObject().value(QStringLiteral("hint")).toString(), QStringLiteral("try something else"));
        QCOMPARE(fail.value(QStringLiteral("id")).toInt(), 9);

        const QJsonObject crash = parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":10,"method":"crash"})"));
        QCOMPARE(errorCode(crash), AutomationErrorCode::InternalError);
        QVERIFY(crash.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString().contains(QStringLiteral("boom")));
    }

    void notificationsAndBlankLines()
    {
        const AutomationDispatcher dispatcher = makeDispatcher();
        QVERIFY(dispatcher.handleLine(R"({"jsonrpc":"2.0","method":"echo","params":{}})").isEmpty());
        QVERIFY(dispatcher.handleLine(R"({"jsonrpc":"2.0","method":"nope"})").isEmpty());
        QVERIFY(dispatcher.handleLine("   \r\n").isEmpty());
        QVERIFY(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":1,"method":"echo"})").endsWith('\n'));
        QCOMPARE(dispatcher.methodNames(), QStringList({QStringLiteral("add"), QStringLiteral("crash"), QStringLiteral("echo"), QStringLiteral("fail")}));
    }
};

QTEST_GUILESS_MAIN(TestAutomationDispatcher)
#include "tst_automation_dispatcher.moc"
