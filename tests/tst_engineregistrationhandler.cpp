/// @file tst_engineregistrationhandler.cpp
/// @brief EngineRegistrationHandler のオプション解析と実行時キャンセルテスト

#define private public
#include "engineregistrationhandler.h"
#undef private
#include "engineregistrationdialog.h"

#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <limits>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

namespace {
constexpr auto kPidEnvName = "SBQ_TEST_ENGINE_PID_FILE";
constexpr auto kModeEnvName = "SBQ_TEST_ENGINE_MODE";
constexpr auto kSlowUsiMode = "slow-usi";
constexpr auto kSlowQuitMode = "slow-quit";

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
#ifdef Q_OS_WIN
    if (static_cast<quint64>(pid) > std::numeric_limits<DWORD>::max()) {
        return false;
    }

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION,
                                       FALSE,
                                       static_cast<DWORD>(pid));
    if (processHandle == nullptr) {
        return false;
    }

    DWORD exitCode = 0;
    const bool isRunning = GetExitCodeProcess(processHandle, &exitCode) != 0
                           && exitCode == STILL_ACTIVE;
    CloseHandle(processHandle);
    return isRunning;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

QString engineRegistrationTestHelperPath()
{
#ifdef Q_OS_WIN
    const QString suffix = QStringLiteral(".exe");
#else
    const QString suffix;
#endif
    return QCoreApplication::applicationDirPath()
           + QDir::separator()
           + QStringLiteral("engineregistration_test_engine")
           + suffix;
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

        const QString enginePath = engineRegistrationTestHelperPath();
        QVERIFY2(QFileInfo::exists(enginePath), qPrintable(enginePath));

        const QString pidFile = tmp.filePath(QStringLiteral("cancel-wait.pid"));
        const ScopedEnvVar pidEnv(kPidEnvName, pidFile.toUtf8());
        const ScopedEnvVar modeEnv(kModeEnvName, QByteArray(kSlowUsiMode));

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
        QTRY_VERIFY_WITH_TIMEOUT(!processExists(pid), 4000);
    }

    void destructor_returnsPromptly_duringQuitWait()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString enginePath = engineRegistrationTestHelperPath();
        QVERIFY2(QFileInfo::exists(enginePath), qPrintable(enginePath));

        const QString pidFile = tmp.filePath(QStringLiteral("quit-wait.pid"));
        const ScopedEnvVar pidEnv(kPidEnvName, pidFile.toUtf8());
        const ScopedEnvVar modeEnv(kModeEnvName, QByteArray(kSlowQuitMode));

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
        QTRY_VERIFY_WITH_TIMEOUT(!processExists(pid), 4000);
    }
};

QTEST_MAIN(TestEngineRegistrationHandler)
#include "tst_engineregistrationhandler.moc"
