/// @file tst_param_specs.cpp
/// @brief 引数の表（ParamSpec）の検査・CLI の変換・自動化 API のメソッドとの対応

#include <QtTest>
#include <QJsonArray>

#include "automationcommands.h"
#include "automationcontext.h"
#include "automationdispatcher.h"
#include "automationmethodspecs.h"
#include "clicommandspecs.h"
#include "paramspec.h"

using namespace ParamSpecs;

class TestParamSpecs : public QObject
{
    Q_OBJECT

private:
    static OperationSpec sample()
    {
        return {QStringLiteral("sample"), QString(),
                {integer(QStringLiteral("count"), QString(), 1, 10).withDefault(3),
                 choice(QStringLiteral("mode"), QString(), {QStringLiteral("a"), QStringLiteral("b")}).require(),
                 boolean(QStringLiteral("flag"), QString()),
                 stringArray(QStringLiteral("names"), QString()).atMost(2),
                 object(QStringLiteral("rect"), QString(), {integer(QStringLiteral("w"), QString(), 1, 5).require()})}};
    }

private slots:
    void validateFillsDefaultsAndRejectsMistakes()
    {
        const OperationSpec spec = sample();
        QString error;
        const auto ok = validate(spec, {{QStringLiteral("mode"), QStringLiteral("a")}}, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(ok->value(QStringLiteral("count")).toInt(), 3);
        QCOMPARE(ok->value(QStringLiteral("flag")).toBool(), false);
        QVERIFY(!ok->contains(QStringLiteral("names")));

        const QList<QPair<QJsonObject, QString>> bad = {
            {{}, QStringLiteral("\"mode\" is required")},
            {{{QStringLiteral("mode"), QStringLiteral("c")}}, QStringLiteral("must be one of a, b")},
            {{{QStringLiteral("mode"), QStringLiteral("a")}, {QStringLiteral("count"), 11}}, QStringLiteral("between 1 and 10")},
            {{{QStringLiteral("mode"), QStringLiteral("a")}, {QStringLiteral("count"), 1.5}}, QStringLiteral("must be an integer")},
            {{{QStringLiteral("mode"), QStringLiteral("a")}, {QStringLiteral("extra"), 1}}, QStringLiteral("Unknown parameter \"extra\"")},
            {{{QStringLiteral("mode"), QStringLiteral("a")}, {QStringLiteral("names"), QJsonArray{QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("z")}}},
             QStringLiteral("number of items")},
            {{{QStringLiteral("mode"), QStringLiteral("a")}, {QStringLiteral("rect"), QJsonObject{{QStringLiteral("w"), 9}}}},
             QStringLiteral("\"rect\" is invalid")},
        };
        for (const auto& [params, message] : bad) {
            error.clear();
            QVERIFY(!validate(spec, params, &error));
            QVERIFY2(error.contains(message), qPrintable(error));
        }
    }

    void schemaDescribesTheTable()
    {
        const QJsonObject schema = inputSchema(sample());
        QCOMPARE(schema.value(QStringLiteral("additionalProperties")).toBool(true), false);
        QCOMPARE(schema.value(QStringLiteral("required")).toArray(), QJsonArray{QStringLiteral("mode")});
        const QJsonObject count = schema.value(QStringLiteral("properties")).toObject().value(QStringLiteral("count")).toObject();
        QCOMPARE(count.value(QStringLiteral("type")).toString(), QStringLiteral("integer"));
        QCOMPARE(count.value(QStringLiteral("minimum")).toInt(), 1);
        QCOMPARE(count.value(QStringLiteral("maximum")).toInt(), 10);
        QCOMPARE(count.value(QStringLiteral("default")).toInt(), 3);
    }

    /// CLI のオプションは表から読み、範囲と既定値も表で決まる
    void cliOptionsFollowTheTable()
    {
        const OperationSpec* analyze = CliCommandSpecs::find(QStringLiteral("analyze"));
        QVERIFY(analyze);
        QString error;
        const auto params = CliCommandSpecs::parse(*analyze, {QStringLiteral("--engine"), QStringLiteral("E"),
                                                              QStringLiteral("--moves"), QStringLiteral("7g7f  3c3d"),
                                                              QStringLiteral("--stdin-control")}, &error);
        QVERIFY2(params, qPrintable(error));
        QCOMPARE(params->value(QStringLiteral("moves")).toArray(), (QJsonArray{QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
        QCOMPARE(params->value(QStringLiteral("stdin_control")).toBool(), true);
        QCOMPARE(params->value(QStringLiteral("seconds")).toInt(), analyze->param(QStringLiteral("seconds"))->defaultValue.toInt());
        QCOMPARE(params->value(QStringLiteral("sfen")).toString(), QStringLiteral("startpos"));

        QVERIFY(!CliCommandSpecs::parse(*analyze, {QStringLiteral("--engine"), QStringLiteral("E"),
                                                   QStringLiteral("--multipv"), QStringLiteral("11")}, &error));
        QCOMPARE(error, QStringLiteral("--multipv must be an integer between 1 and 10"));
        QVERIFY(!CliCommandSpecs::parse(*analyze, {QStringLiteral("--seconds"), QStringLiteral("3")}, &error));
        QCOMPARE(error, QStringLiteral("--engine is required"));
        QVERIFY(!CliCommandSpecs::parse(*analyze, {QStringLiteral("--engine"), QStringLiteral("E"), QStringLiteral("extra")}, &error));
        QVERIFY(CliCommandSpecs::usage().contains(QStringLiteral("--max-positions n")));
    }

    /// 自動化 API に登録したメソッドと表のメソッドは同じ
    void everyRegisteredMethodHasASpec()
    {
        AutomationDispatcher dispatcher;
        AutomationCommands::registerAll(dispatcher, AutomationContext{});
        QStringList specified;
        for (const OperationSpec& spec : AutomationMethodSpecs::all()) specified << spec.name;
        specified.sort();
        QCOMPARE(dispatcher.methodNames(), specified);
    }

    /// 表にあるメソッドは、呼ぶ前に引数を検査し既定値を補う
    void dispatcherValidatesBeforeCalling()
    {
        AutomationDispatcher dispatcher;
        QJsonObject received;
        dispatcher.registerMethod(QStringLiteral("clipboard.get"), [&received](const QJsonObject& params) {
            received = params;
            return QJsonValue(true);
        });
        const auto call = [&dispatcher](const QJsonObject& params) {
            return dispatcher.handleRequest({{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), 1},
                                             {QStringLiteral("method"), QStringLiteral("clipboard.get")},
                                             {QStringLiteral("params"), params}});
        };
        QVERIFY(call({}).contains(QStringLiteral("result")));
        QCOMPARE(received.value(QStringLiteral("max_chars")).toInt(), 30000);
        const QJsonObject unknown = call({{QStringLiteral("max_char"), 5}});
        QCOMPARE(unknown.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt(),
                 AutomationErrorCode::InvalidParams);
        QVERIFY(unknown.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("max_char")));
    }
};

QTEST_MAIN(TestParamSpecs)
#include "tst_param_specs.moc"
