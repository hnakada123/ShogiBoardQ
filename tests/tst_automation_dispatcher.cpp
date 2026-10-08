/// @file tst_automation_dispatcher.cpp
/// @brief AutomationDispatcher（JSON-RPC 2.0 解析・メソッド表）のユニットテスト

#include <QtTest>

#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QPointer>
#include <QTemporaryDir>

#include "automationdispatcher.h"
#include "automationserver.h"
#include "automationcommands.h"
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

    QTemporaryDir m_config;

    /// サーバー側の接続ソケット（QLocalServer の子）の数
    static qsizetype serverSocketCount(const AutomationServer& server)
    {
        return server.findChildren<QLocalSocket*>().size();
    }

    static QByteArray request(const QString& socketPath, const QByteArray& line)
    {
        QLocalSocket client;
        client.connectToServer(socketPath);
        if (!client.waitForConnected(3000)) return QByteArray();
        client.write(line);
        client.flush();
        QByteArray response;
        QElapsedTimer timer;
        timer.start();
        while (!response.endsWith('\n') && timer.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            response += client.readAll();
        }
        return response;
    }

private slots:
    void initTestCase()
    {
        // listen() が書くエンドポイント情報をユーザーの設定ディレクトリに作らない
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    /// ハンドラの実行中（入れ子のイベントループ）に届いた別の要求は実行せず -32002 を返す
    void requestDuringRunningHandlerIsRejected()
    {
        AutomationDispatcher dispatcher;
        QByteArray nestedResponse;
        bool pingRan = false;
        dispatcher.registerMethod(QStringLiteral("ping"), [&](const QJsonObject&) {
            pingRan = true;
            return QJsonValue(QStringLiteral("pong"));
        });
        dispatcher.registerMethod(QStringLiteral("outer"), [&](const QJsonObject&) {
            nestedResponse = dispatcher.handleLine(R"({"jsonrpc":"2.0","id":2,"method":"ping"})");
            return QJsonValue(true);
        });
        const QJsonObject outer = parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":1,"method":"outer"})"));
        QCOMPARE(outer.value(QStringLiteral("result")).toBool(), true);
        QVERIFY(!pingRan);
        const QJsonObject nested = parse(nestedResponse);
        QCOMPARE(nested.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt(), -32002);
        // 実行中でなければ通常どおり処理する
        const QJsonObject ping = parse(dispatcher.handleLine(R"({"jsonrpc":"2.0","id":3,"method":"ping"})"));
        QCOMPARE(ping.value(QStringLiteral("result")).toString(), QStringLiteral("pong"));
    }

    /// ダイアログ表示中（入れ子のイベントループ）にクライアントが切断しても、
    /// 処理中の接続を入れ子のループで削除せず（readyRead を通知中の Qt が削除済みの
    /// ソケットに触れないように）、ハンドラが戻ってから削除し、以後の接続を受け付けられる。
    void clientDisconnectDuringNestedLoopIsSafe()
    {
        QTemporaryDir dir(QDir::tempPath() + QStringLiteral("/sbq-XXXXXX"));
        QVERIFY(dir.isValid());
        const QString socketPath = dir.filePath(QStringLiteral("a.sock"));

        AutomationServer server;
        QPointer<QLocalSocket> client = new QLocalSocket(this);
        bool handled = false;
        bool socketKeptDuringHandler = false;
        server.dispatcher().registerMethod(QStringLiteral("nested"), [&](const QJsonObject&) {
            client->abort();
            // 切断を受け取るまで入れ子のイベントループを回す（削除の予約も処理させる）
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 1000) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            }
            socketKeptDuringHandler = serverSocketCount(server) > 0;
            handled = true;
            return QJsonValue(true);
        });
        server.dispatcher().registerMethod(QStringLiteral("ping"), [](const QJsonObject&) {
            return QJsonValue(QStringLiteral("pong"));
        });
        QString error;
        QVERIFY2(server.listen(socketPath, &error), qPrintable(error));

        client->connectToServer(socketPath);
        QVERIFY(client->waitForConnected(3000));
        // 2行目は切断後に処理されてはならない
        client->write(R"({"jsonrpc":"2.0","id":1,"method":"nested"})" "\n"
                      R"({"jsonrpc":"2.0","id":2,"method":"nested"})" "\n");
        client->flush();
        QTRY_VERIFY_WITH_TIMEOUT(handled, 5000);
        QVERIFY(socketKeptDuringHandler);
        // ハンドラが戻った後に削除される
        QTRY_COMPARE_WITH_TIMEOUT(serverSocketCount(server), qsizetype(0), 3000);
        handled = false;
        QTest::qWait(100);
        QVERIFY(!handled);

        const QJsonObject pong = parse(request(socketPath, R"({"jsonrpc":"2.0","id":3,"method":"ping"})" "\n"));
        QCOMPARE(pong.value(QStringLiteral("result")).toString(), QStringLiteral("pong"));
        server.close();
    }

    void deferredCallbackMayDeleteItsParent()
    {
        auto* parent = new QObject;
        bool called = false;
        AutomationDeferredCall::schedule([parent, &called]() { delete parent; called = true; }, parent);
        QTRY_VERIFY(called);

        auto* cancelledParent = new QObject;
        bool cancelledCalled = false;
        AutomationDeferredCall::schedule([&cancelledCalled]() { cancelledCalled = true; }, cancelledParent);
        delete cancelledParent;
        QCoreApplication::processEvents();
        QVERIFY(!cancelledCalled);
    }

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
