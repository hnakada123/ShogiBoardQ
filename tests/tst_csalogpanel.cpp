#include <QtTest>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>

#include "csalogpanel.h"

class TestCsaLogPanel : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_config.path());
    }

    void sendingFollowsConnectionAndInput()
    {
        CsaLogPanel panel;
        std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
        auto* input = page->findChild<QLineEdit*>("csaCommandInput");
        auto* send = page->findChild<QPushButton*>("csaSendButton");
        QSignalSpy spy(&panel, &CsaLogPanel::csaRawCommandRequested);
        QVERIFY(!input->isEnabled());
        QVERIFY(!send->isEnabled());
        panel.setConnected(true);
        QVERIFY(input->isEnabled());
        input->setText(QStringLiteral("   "));
        QVERIFY(!send->isEnabled());
        for (bool enter : {false, true}) {
            input->setText(QStringLiteral("  %%WHO  "));
            QVERIFY(send->isEnabled());
            if (enter) QTest::keyClick(input, Qt::Key_Return);
            else QTest::mouseClick(send, Qt::LeftButton);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("%%WHO"));
            QVERIFY(input->text().isEmpty());
            QVERIFY(!send->isEnabled());
        }
        input->setText(QStringLiteral("LOGOUT"));
        panel.setConnected(false);
        QVERIFY(!send->isEnabled());
        QMetaObject::invokeMethod(input, "returnPressed");
        QCOMPARE(spy.count(), 0);
        QCOMPARE(input->text(), QStringLiteral("LOGOUT"));
    }

    void incomingLogPreservesReadingPosition()
    {
        CsaLogPanel panel;
        std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
        page->resize(640, 280);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        auto* view = page->findChild<QPlainTextEdit*>("csaLogView");
        for (int i = 0; i < 100; ++i) panel.append(QStringLiteral("%1: +7776FU,T1").arg(i));
        auto* scroll = view->verticalScrollBar();
        QVERIFY(scroll->maximum() > 0);
        QCOMPARE(scroll->value(), scroll->maximum());
        QTextCursor cursor(view->document());
        cursor.movePosition(QTextCursor::NextBlock);
        cursor.select(QTextCursor::LineUnderCursor);
        view->setTextCursor(cursor);
        scroll->setValue(scroll->maximum() / 2);
        const int position = scroll->value();
        const QString selected = view->textCursor().selectedText();
        panel.append(QStringLiteral("-3334FU,T1"));
        QCOMPARE(scroll->value(), position);
        QCOMPARE(view->textCursor().selectedText(), selected);

        view->moveCursor(QTextCursor::End);
        panel.append(QStringLiteral("+2726FU,T1"));
        QCOMPARE(scroll->value(), scroll->maximum());
        for (int i = 0; i < 5000; ++i) panel.append(QString::number(i));
        QCOMPARE(view->document()->blockCount(), 5000);
    }
};

QTEST_MAIN(TestCsaLogPanel)
#include "tst_csalogpanel.moc"
