/// @file tst_engineregistrationhandler.cpp
/// @brief EngineRegistrationHandler のオプション解析と実行時キャンセルテスト

// private メンバへは EngineRegistrationHandler の friend 宣言（SHOGIBOARDQ_TESTING 時のみ有効）経由でアクセスする
#include "engine_profile_format_helper.h"
#include "engineprofilestore.h"
#include "engineregistrationhandler.h"
#include "engineregistrationdialog.h"
#include <QSettings>
#include "settingscommon.h"

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

    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        // 登録の試験は設定ファイルに書くので、利用者の設定ファイルを使わない
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    /// 別の実行ファイルが同じ id name を名乗るときは、登録名に番号を付けて別のエンジンとして登録する
    void registeredName_duplicateIdNameGetsNumber()
    {
        EngineRegistrationHandler handler;
        EngineEntry existing;
        existing.name = QStringLiteral("YaneuraOu NNUE");
        existing.path = QStringLiteral("/engines/a/YaneuraOu");
        handler.m_engines.append(existing);
        QSignalSpy registered(&handler, &EngineRegistrationHandler::engineRegistered);

        handler.m_fileName = QStringLiteral("/engines/b/YaneuraOu");
        handler.registerEngine(QStringLiteral("YaneuraOu NNUE"), QStringLiteral("author"),
                               {QStringLiteral("option name Threads type spin default 1 min 1 max 64")});
        QCOMPARE(registered.count(), 1);
        QCOMPARE(registered.first().first().toString(), QStringLiteral("YaneuraOu NNUE (2)"));
        const auto engines = RegisteredEngines::load().entries();
        QCOMPARE(engines.size(), 2);
        QCOMPARE(engines.at(1).name, QStringLiteral("YaneuraOu NNUE (2)"));
        QCOMPARE(engines.at(1).path, QStringLiteral("/engines/b/YaneuraOu"));
        // オプションは登録名ごとに保存する
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(settings.beginReadArray(EngineProfileFormat::engineOptionsGroup(QStringLiteral("YaneuraOu NNUE (2)"))), 2);  // Threads と補った USI_Hash
        settings.endArray();
        QCOMPARE(handler.engines().uniqueName(QStringLiteral("YaneuraOu NNUE")), QStringLiteral("YaneuraOu NNUE (3)"));
        QCOMPARE(handler.engines().uniqueName(QStringLiteral("Other")), QStringLiteral("Other"));
    }

    /// オプションを読めないエンジンは一覧にも設定にも足さない（画面の一覧と行の番号がずれない）
    void registerEngine_invalidOptionsLeavesListUnchanged()
    {
        EngineRegistrationHandler handler;
        const QList<EngineEntry> before = RegisteredEngines::load().entries();
        handler.m_engines = RegisteredEngines(before);
        QSignalSpy registered(&handler, &EngineRegistrationHandler::engineRegistered);
        QSignalSpy failed(&handler, &EngineRegistrationHandler::errorOccurred);
        QSignalSpy changed(&handler, &EngineRegistrationHandler::enginesChanged);

        handler.m_fileName = QStringLiteral("/engines/c/Broken");
        handler.registerEngine(QStringLiteral("Broken"), QStringLiteral("author"),
                               {QStringLiteral("option name Threads type spin default 1 min 1 max 64"),
                                QStringLiteral("option name Threads type spin default 2 min 1 max 64")});
        QCOMPARE(registered.count(), 0);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(changed.count(), 0);   // 一覧は変わらないので画面も作り直さない
        QCOMPARE(handler.engines().entries().size(), before.size());
        QVERIFY(!handler.engines().findByPath(QStringLiteral("/engines/c/Broken")));

        // 次の登録が成功しても、失敗したエンジンは保存されない（エラーの印は startRegistration と同じく戻す）
        handler.m_errorOccurred = false;
        handler.m_fileName = QStringLiteral("/engines/d/Good");
        handler.registerEngine(QStringLiteral("Good"), QStringLiteral("author"),
                               {QStringLiteral("option name Threads type spin default 1 min 1 max 64")});
        QCOMPARE(registered.count(), 1);
        QCOMPARE(changed.count(), 1);
        const auto engines = RegisteredEngines::load().entries();
        QCOMPARE(engines.size(), before.size() + 1);
        QCOMPARE(engines.last().name, QStringLiteral("Good"));

        // 消すと一覧と設定から消え、画面に知らせる
        handler.removeEngineAt(static_cast<int>(handler.engines().entries().size()) - 1);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(RegisteredEngines::load().entries().size(), before.size());
    }

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

    /// combo の選択肢は JSON の配列で保存し（空白を含められる）、読み戻すと同じになる
    void comboOptionValues_roundTripAsJson()
    {
        EngineOption opt;
        opt.name = QStringLiteral("Playing Style");
        opt.type = QStringLiteral("combo");
        opt.defaultValue = QStringLiteral("Normal");
        opt.currentValue = QStringLiteral("Normal");
        opt.valueList = {QStringLiteral("Ultra Aggressive"), QStringLiteral("Solid")};
        EngineProfileStore::saveOptions(QStringLiteral("ComboEngine"), {opt});

        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.beginReadArray(EngineProfileFormat::engineOptionsGroup(QStringLiteral("ComboEngine")));
        settings.setArrayIndex(0);
        const QJsonDocument json = QJsonDocument::fromJson(settings.value(QStringLiteral("valueList")).toString().toUtf8());
        settings.endArray();
        QVERIFY(json.isArray());
        const QJsonArray arr = json.array();
        QCOMPARE(arr.size(), 2);
        QCOMPARE(arr.at(0).toString(), QStringLiteral("Ultra Aggressive"));
        QCOMPARE(arr.at(1).toString(), QStringLiteral("Solid"));

        const QList<EngineOption> loaded = EngineProfileStore::loadOptions(QStringLiteral("ComboEngine"));
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().valueList, opt.valueList);
        QCOMPARE(loaded.first().currentValue, opt.currentValue);
    }

    /// 同じ実行ファイルは、パスを正規化して見つける（登録ダイアログが重複を断る）
    void findByPath_canonicalizedPath_detectsDuplicate()
    {
        EngineRegistrationHandler handler;

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString basePath = tmp.filePath(QStringLiteral("engine"));
        QFile file(basePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("#!/bin/sh\nexit 0\n");
        file.close();

        EngineEntry engine;
        engine.name = QStringLiteral("test-engine");
        engine.path = basePath;
        handler.m_engines.append(engine);

        const QString alternativePath = tmp.filePath(QStringLiteral("./engine"));
        const std::optional<EngineEntry> existing = handler.engines().findByPath(alternativePath);
        QVERIFY(existing);
        QCOMPARE(existing->name, QStringLiteral("test-engine"));
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
