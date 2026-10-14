#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QTemporaryDir>

#include "automationcommands.h"
#include "automationcontext.h"
#include "automationdispatcher.h"
#include "automationwidgets.h"
#include "csagamedialog.h"
#include "networksettings.h"

class TestAutomationSecurity : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    static QJsonObject call(AutomationDispatcher& dispatcher, const QString& method, const QJsonObject& params)
    {
        return dispatcher.handleRequest({{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 1}, {QStringLiteral("method"), method}, {QStringLiteral("params"), params}});
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void manuallyRevealedPasswordRemainsPrivate()
    {
        QMainWindow main;
        CsaGameDialog dialog(&main);
        auto* password = dialog.findChild<QLineEdit*>(QStringLiteral("lineEditPassword"));
        auto* reveal = dialog.findChild<QCheckBox*>(QStringLiteral("checkBoxShowPassword"));
        QVERIFY(password);
        QVERIFY(reveal);
        password->setText(QStringLiteral("audit-secret"));
        dialog.show();
        reveal->setChecked(true); // A user may reveal it locally; RPC still must not expose it.
        QCOMPARE(password->echoMode(), QLineEdit::Normal);
        AutomationContext context;
        context.mainWindow = &main;
        AutomationDispatcher dispatcher;
        AutomationCommands::registerUiCommands(dispatcher, context);
        AutomationCommands::registerWidgetCommands(dispatcher, context);
        const QString target = AutomationWidgets::selector(&dialog);
        const auto text = call(dispatcher, QStringLiteral("widget.text"),
            {{QStringLiteral("dialog"), target}, {QStringLiteral("widget"), QStringLiteral("lineEditPassword")}});
        QVERIFY(!text.contains(QStringLiteral("error")));
        QCOMPARE(text[QStringLiteral("result")].toObject()[QStringLiteral("widgets")].toArray().first()
                     .toObject()[QStringLiteral("text")].toString(), QStringLiteral("*****"));
        const auto screenshot = call(dispatcher, QStringLiteral("screenshot.capture"),
            {{QStringLiteral("target"), target}, {QStringLiteral("output_dir"), m_config.path()}});
        QCOMPARE(screenshot[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(),
                 AutomationErrorCode::NotAllowed);
        const auto click = call(dispatcher, QStringLiteral("widget.click"),
            {{QStringLiteral("target"), target}, {QStringLiteral("widget"), QStringLiteral("checkBoxShowPassword")}});
        QCOMPARE(click[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(),
                 AutomationErrorCode::NotAllowed);
    }

    void passwordAcceptsWriteOnlyInput()
    {
        QMainWindow main;
        CsaGameDialog dialog(&main);
        dialog.show();
        auto* password = dialog.findChild<QLineEdit*>(QStringLiteral("lineEditPassword"));
        QVERIFY(password);
        AutomationContext context;
        context.mainWindow = &main;
        AutomationDispatcher dispatcher;
        AutomationCommands::registerWidgetCommands(dispatcher, context);
        const auto response = call(dispatcher, QStringLiteral("widget.setValue"),
            {{QStringLiteral("target"), AutomationWidgets::selector(&dialog)},
             {QStringLiteral("widget"), QStringLiteral("lineEditPassword")},
             {QStringLiteral("value"), QStringLiteral("audit-secret")}});
        QVERIFY(!response.contains(QStringLiteral("error")));
        QVERIFY(!QJsonDocument(response).toJson().contains("audit-secret"));
        QTRY_COMPARE(password->text(), QStringLiteral("audit-secret"));
        QCOMPARE(password->echoMode(), QLineEdit::Password);
    }

    void tlsSettingDefaultsAndRoundTrips()
    {
        NetworkSettings::csaUseTls.remove();
        QVERIFY(!NetworkSettings::csaUseTls());
        CsaGameDialog local;
        QVERIFY(!local.useTls());
        auto* transport = local.findChild<QComboBox*>(QStringLiteral("comboBoxTransport"));
        auto* warning = local.findChild<QLabel*>(QStringLiteral("labelPlaintextWarning"));
        QVERIFY(transport);
        QVERIFY(warning);
        QCOMPARE(transport->currentIndex(), 0);
        QVERIFY(!warning->isHidden());
        transport->setCurrentIndex(1);
        QVERIFY(local.useTls());
        QVERIFY(warning->isHidden());
        transport->setCurrentIndex(0);
        QVERIFY(!local.useTls());
        QVERIFY(!warning->isHidden());
        NetworkSettings::csaUseTls.set(true);
        CsaGameDialog tls;
        QVERIFY(tls.useTls());
    }
};

QTEST_MAIN(TestAutomationSecurity)
#include "tst_automation_security.moc"
