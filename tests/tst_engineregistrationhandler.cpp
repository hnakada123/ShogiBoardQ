/// @file tst_engineregistrationhandler.cpp
/// @brief EngineRegistrationHandler のオプション解析テスト

#define private public
#include "engineregistrationhandler.h"
#undef private
#include "engineregistrationdialog.h"

#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

class TestEngineRegistrationHandler : public QObject
{
    Q_OBJECT

private slots:
    void parseOptionLine_multiWordName_parsesSpinOption()
    {
        EngineRegistrationHandler handler;

        handler.parseOptionLine(
            QStringLiteral("option name Skill Level type spin default 20 min 0 max 20"));

        QCOMPARE(handler.m_engineOptions.size(), 1);
        const EngineOption opt = handler.m_engineOptions.first();
        QCOMPARE(opt.name, QStringLiteral("Skill Level"));
        QCOMPARE(opt.type, QStringLiteral("spin"));
        QCOMPARE(opt.defaultValue, QStringLiteral("20"));
        QCOMPARE(opt.currentValue, QStringLiteral("20"));
        QCOMPARE(opt.min, QStringLiteral("0"));
        QCOMPARE(opt.max, QStringLiteral("20"));
    }

    void parseOptionLine_comboVarWithSpaces_preservesChoices()
    {
        EngineRegistrationHandler handler;

        handler.parseOptionLine(
            QStringLiteral("option name Playing Style type combo default Normal var Ultra Aggressive var Solid"));

        QCOMPARE(handler.m_engineOptions.size(), 1);
        const EngineOption opt = handler.m_engineOptions.first();
        QCOMPARE(opt.name, QStringLiteral("Playing Style"));
        QCOMPARE(opt.type, QStringLiteral("combo"));
        QCOMPARE(opt.defaultValue, QStringLiteral("Normal"));
        QCOMPARE(opt.valueList.size(), 2);
        QCOMPARE(opt.valueList.at(0), QStringLiteral("Ultra Aggressive"));
        QCOMPARE(opt.valueList.at(1), QStringLiteral("Solid"));
    }

    void parseOptionLine_unknownAttribute_ignoresTail()
    {
        EngineRegistrationHandler handler;

        handler.parseOptionLine(
            QStringLiteral("option name Test type spin unknown 1"));

        QVERIFY(!handler.m_errorOccurred);
        QCOMPARE(handler.m_engineOptions.size(), 1);
        const EngineOption opt = handler.m_engineOptions.first();
        QCOMPARE(opt.name, QStringLiteral("Test"));
        QCOMPARE(opt.type, QStringLiteral("spin"));
    }

    void parseOptionLine_stringDefaultContainingKeywords_preservesTail()
    {
        EngineRegistrationHandler handler;

        handler.parseOptionLine(
            QStringLiteral("option name BookPath type string default max depth default"));

        QVERIFY(!handler.m_errorOccurred);
        QCOMPARE(handler.m_engineOptions.size(), 1);
        const EngineOption opt = handler.m_engineOptions.first();
        QCOMPARE(opt.name, QStringLiteral("BookPath"));
        QCOMPARE(opt.type, QStringLiteral("string"));
        QCOMPARE(opt.defaultValue, QStringLiteral("max depth default"));
        QCOMPARE(opt.currentValue, QStringLiteral("max depth default"));
    }

    void parseOptionLine_filenameDefaultWithSpaces_preservesTail()
    {
        EngineRegistrationHandler handler;

        handler.parseOptionLine(
            QStringLiteral("option name EvalDir type filename default /tmp/max depth folder"));

        QVERIFY(!handler.m_errorOccurred);
        QCOMPARE(handler.m_engineOptions.size(), 1);
        const EngineOption opt = handler.m_engineOptions.first();
        QCOMPARE(opt.name, QStringLiteral("EvalDir"));
        QCOMPARE(opt.type, QStringLiteral("filename"));
        QCOMPARE(opt.defaultValue, QStringLiteral("/tmp/max depth folder"));
        QCOMPARE(opt.currentValue, QStringLiteral("/tmp/max depth folder"));
    }

    void concatenateComboOptionValues_jsonSerialization()
    {
        EngineRegistrationHandler handler;

        EngineOption opt;
        opt.name = QStringLiteral("Playing Style");
        opt.type = QStringLiteral("combo");
        opt.defaultValue = QStringLiteral("Normal");
        opt.currentValue = QStringLiteral("Normal");
        opt.valueList = {QStringLiteral("Ultra Aggressive"), QStringLiteral("Solid")};
        handler.m_engineOptions.append(opt);

        handler.concatenateComboOptionValues();

        QCOMPARE(handler.m_concatenatedOptionValuesList.size(), 1);
        const QByteArray raw = handler.m_concatenatedOptionValuesList.first().toUtf8();
        const QJsonDocument json = QJsonDocument::fromJson(raw);
        QVERIFY(json.isArray());
        const QJsonArray arr = json.array();
        QCOMPARE(arr.size(), 2);
        QCOMPARE(arr.at(0).toString(), QStringLiteral("Ultra Aggressive"));
        QCOMPARE(arr.at(1).toString(), QStringLiteral("Solid"));
    }

    void isDuplicatePath_canonicalizedPath_detectsDuplicate()
    {
        EngineRegistrationHandler handler;

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString basePath = tmp.filePath(QStringLiteral("engine"));
        QFile file(basePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("#!/bin/sh\nexit 0\n");
        file.close();

        Engine engine;
        engine.name = QStringLiteral("test-engine");
        engine.path = basePath;
        handler.m_engineList.append(engine);

        QString existingName;
        const QString alternativePath = tmp.filePath(QStringLiteral("./engine"));
        QVERIFY(handler.isDuplicatePath(alternativePath, existingName));
        QCOMPARE(existingName, QStringLiteral("test-engine"));
    }
};

QTEST_MAIN(TestEngineRegistrationHandler)
#include "tst_engineregistrationhandler.moc"
