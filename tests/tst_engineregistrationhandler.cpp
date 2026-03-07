/// @file tst_engineregistrationhandler.cpp
/// @brief EngineRegistrationHandler のオプション解析と実行時キャンセルテスト

#define private public
#include "engineregistrationhandler.h"
#undef private
#include "engineregistrationdialog.h"

#include <QtTest>

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <signal.h>
#include <unistd.h>

namespace {
constexpr auto kPidEnvName = "SBQ_TEST_ENGINE_PID_FILE";

class ScopedEnvVar
{
public:
    ScopedEnvVar(const char* name, const QByteArray& value)
        : m_name(name)
    {
        const QByteArray existing = qgetenv(name);
        if (!existing.isNull()) {
            m_hadOriginal = true;
            m_originalValue = existing;
        }
        qputenv(m_name.constData(), value);
    }

    ~ScopedEnvVar()
    {
        if (m_hadOriginal) {
            qputenv(m_name.constData(), m_originalValue);
        } else {
            qunsetenv(m_name.constData());
        }
    }

private:
    QByteArray m_name;
    QByteArray m_originalValue;
    bool m_hadOriginal = false;
};

QString createExecutableScript(const QTemporaryDir& dir, const QString& fileName,
                               const QString& body)
{
    const QString path = dir.filePath(fileName);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    file.write(body.toUtf8());
    file.close();
    QFile::Permissions perms = file.permissions();
    perms |= QFileDevice::ExeOwner | QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    perms |= QFileDevice::ExeGroup | QFileDevice::ReadGroup;
    perms |= QFileDevice::ExeOther | QFileDevice::ReadOther;
    file.setPermissions(perms);
    return path;
}

qint64 readPidFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }

    bool ok = false;
    const qint64 pid = QString::fromUtf8(file.readAll()).trimmed().toLongLong(&ok);
    return ok ? pid : -1;
}

bool processExists(qint64 pid)
{
    if (pid <= 0) {
        return false;
    }
    return ::kill(static_cast<pid_t>(pid), 0) == 0;
}
} // namespace

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

    void cancelRegistration_whileWaitingForUsiResponse_stopsProcess()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString pidFile = tmp.filePath(QStringLiteral("cancel-wait.pid"));
        const ScopedEnvVar pidEnv(kPidEnvName, pidFile.toUtf8());
        const QString enginePath = createExecutableScript(
            tmp,
            QStringLiteral("slow_usi_engine.sh"),
            QStringLiteral(
                "#!/usr/bin/env bash\n"
                "set -eu\n"
                "echo $$ > \"${%1}\"\n"
                "while IFS= read -r line; do\n"
                "  if [[ \"$line\" == \"usi\" ]]; then\n"
                "    sleep 10\n"
                "  fi\n"
                "done\n")
                .arg(QString::fromLatin1(kPidEnvName)));
        QVERIFY(!enginePath.isEmpty());

        EngineRegistrationHandler handler;
        QSignalSpy progressSpy(&handler, &EngineRegistrationHandler::registrationInProgressChanged);
        QSignalSpy registeredSpy(&handler, &EngineRegistrationHandler::engineRegistered);
        QSignalSpy errorSpy(&handler, &EngineRegistrationHandler::errorOccurred);

        handler.startRegistration(enginePath);
        QTRY_VERIFY_WITH_TIMEOUT(handler.isRegistrationInProgress(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(pidFile), 2000);

        const qint64 pid = readPidFile(pidFile);
        QVERIFY(pid > 0);
        QVERIFY(processExists(pid));

        handler.cancelRegistration();

        QTRY_VERIFY_WITH_TIMEOUT(!handler.isRegistrationInProgress(), 4000);
        QCOMPARE(registeredSpy.count(), 0);
        QCOMPARE(errorSpy.count(), 0);
        QVERIFY(progressSpy.count() >= 2);
        QCOMPARE(progressSpy.at(0).at(0).toBool(), true);
        QCOMPARE(progressSpy.last().at(0).toBool(), false);
        QTRY_VERIFY_WITH_TIMEOUT(!processExists(pid), 2000);
    }

    void destructor_returnsPromptly_duringQuitWait()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString pidFile = tmp.filePath(QStringLiteral("quit-wait.pid"));
        const ScopedEnvVar pidEnv(kPidEnvName, pidFile.toUtf8());
        const QString enginePath = createExecutableScript(
            tmp,
            QStringLiteral("slow_quit_engine.sh"),
            QStringLiteral(
                "#!/usr/bin/env bash\n"
                "set -eu\n"
                "echo $$ > \"${%1}\"\n"
                "while IFS= read -r line; do\n"
                "  if [[ \"$line\" == \"usi\" ]]; then\n"
                "    echo \"id name SlowQuitEngine\"\n"
                "    echo \"id author Test\"\n"
                "    echo \"usiok\"\n"
                "  elif [[ \"$line\" == \"quit\" ]]; then\n"
                "    sleep 10\n"
                "    exit 0\n"
                "  fi\n"
                "done\n")
                .arg(QString::fromLatin1(kPidEnvName)));
        QVERIFY(!enginePath.isEmpty());

        auto* handler = new EngineRegistrationHandler;
        handler->startRegistration(enginePath);

        QTRY_VERIFY_WITH_TIMEOUT(handler->isRegistrationInProgress(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(pidFile), 2000);

        const qint64 pid = readPidFile(pidFile);
        QVERIFY(pid > 0);
        QVERIFY(processExists(pid));

        QTest::qWait(200);

        QElapsedTimer timer;
        timer.start();
        delete handler;

        QVERIFY2(timer.elapsed() < 4500,
                 qPrintable(QStringLiteral("Destructor took too long: %1 ms").arg(timer.elapsed())));
        QTRY_VERIFY_WITH_TIMEOUT(!processExists(pid), 2000);
    }
};

QTEST_MAIN(TestEngineRegistrationHandler)
#include "tst_engineregistrationhandler.moc"
