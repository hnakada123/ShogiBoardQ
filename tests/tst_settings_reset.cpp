/// @file tst_settings_reset.cpp
/// @brief 設定初期化の確認・終了キャンセル・終了時保存との順序を検証する

#include <QtTest>
#include <QCloseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include "appsettings.h"
#include "settingscommon.h"
#include "settingsresetcontroller.h"

namespace {
class SettingsWindow : public QWidget
{
public:
    bool allowClose = true;
    int closeAttempts = 0;

    ~SettingsWindow() override
    {
        // 実際の各ウィジェットと同様、破棄時にも設定を書き込む。
        AppSettings::setToolbarVisible(false);
    }

protected:
    void closeEvent(QCloseEvent* event) override
    {
        ++closeAttempts;
        event->setAccepted(allowClose);
    }
};
}

class TestSettingsReset : public QObject
{
    Q_OBJECT

private:
    QMessageBox::StandardButton m_answer = QMessageBox::Cancel;
    bool m_confirmationSeen = false;

    void stopEventLoop()
    {
        QCoreApplication::exit(0);
    }

    void answerConfirmation()
    {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) return;
        m_confirmationSeen = true;
        QCOMPARE(box->defaultButton(), box->button(QMessageBox::Cancel));
        box->button(m_answer)->click();
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void init()
    {
        QVERIFY(SettingsCommon::resetAllSettings());
        AppSettings::setLanguage(QStringLiteral("en"));
        AppSettings::setPieceSoundVolume(80);
        // エンジン設定など共有インスタンスを使わない書き込みも対象にする。
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("Engines/size"), 1);
        settings.setValue(QStringLiteral("Engines/1/name"), QStringLiteral("Test engine"));
        settings.setValue(QStringLiteral("Legacy/unknownKey"), 42);
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
        m_confirmationSeen = false;
    }

    void cleanupTestCase()
    {
        QVERIFY(SettingsCommon::resetAllSettings());
    }

    void normalExitKeepsSettings()
    {
        QCOMPARE(SettingsResetController::finalizeExit(0), 0);
        QCOMPARE(SettingsResetController::finalizeExit(1), 1);
        QCOMPARE(AppSettings::language(), QStringLiteral("en"));
        QCOMPARE(AppSettings::pieceSoundVolume(), 80);
        QVERIFY(SettingsCommon::openSettings().contains(QStringLiteral("Engines/1/name")));
    }

    void confirmationAndShutdown_data()
    {
        QTest::addColumn<bool>("confirmReset");
        QTest::addColumn<bool>("allowClose");
        QTest::newRow("cancel-reset") << false << true;
        QTest::newRow("cancel-unsaved-close") << true << false;
        QTest::newRow("reset-after-destruction") << true << true;
    }

    void confirmationAndShutdown()
    {
        QFETCH(bool, confirmReset);
        QFETCH(bool, allowClose);
        m_answer = confirmReset ? QMessageBox::Yes : QMessageBox::Cancel;
        QTimer answerTimer;
        connect(&answerTimer, &QTimer::timeout, this, &TestSettingsReset::answerConfirmation);
        answerTimer.start(1);

        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, this, &TestSettingsReset::stopEventLoop);
        watchdog.start(5000);

        int exitCode;
        {
            SettingsWindow window;
            window.allowClose = allowClose;
            window.show();
            SettingsResetController controller(&window);
            QTimer::singleShot(0, &controller, [&controller, &window]() {
                controller.confirmAndQuit();
                if (window.isVisible()) QCoreApplication::exit(0);
            });
            exitCode = QCoreApplication::exec();
            QVERIFY(m_confirmationSeen);
            QVERIFY(watchdog.isActive());
            QCOMPARE(window.closeAttempts, confirmReset ? 1 : 0);
            // 初期化は全ウィンドウの破棄が完了するまで実行されない。
            QCOMPARE(AppSettings::pieceSoundVolume(), 80);
        }
        watchdog.stop();
        answerTimer.stop();
        QVERIFY(!AppSettings::toolbarVisible());
        QCOMPARE(SettingsResetController::finalizeExit(exitCode), 0);

        QSettings stored(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        if (confirmReset && allowClose) {
            QVERIFY(stored.allKeys().isEmpty());
            QVERIFY(SettingsCommon::openSettings().allKeys().isEmpty());
            QCOMPARE(AppSettings::language(), QStringLiteral("system"));
            QCOMPARE(AppSettings::pieceSoundVolume(), 30);
            QVERIFY(AppSettings::toolbarVisible());
        } else {
            QCOMPARE(exitCode, 0);
            QCOMPARE(AppSettings::language(), QStringLiteral("en"));
            QCOMPARE(AppSettings::pieceSoundVolume(), 80);
            QVERIFY(stored.contains(QStringLiteral("Engines/1/name")));
        }
    }
};

QTEST_MAIN(TestSettingsReset)
#include "tst_settings_reset.moc"
