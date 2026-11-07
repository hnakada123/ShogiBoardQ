#include <QtTest>
#include <QAbstractButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>

#include "csalogpanel.h"
#include "fontsizecontrol.h"
#include "networksettings.h"

#include <QAction>

class TestCsaLogPanel : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_config.path());
    }

    void sendingFollowsConnectionAndInput()
    {
        CsaLogPanel panel;
        QWidget* page = &panel;
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
        QWidget* page = &panel;
        page->resize(640, 280);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page));
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

    /// 通信ログの操作（折り返し・全てコピー・消去）は USI 通信ログと同じ LogPanel の部品
    void sharesLogActionsWithUsiLog()
    {
        CsaLogPanel panel;
        auto* copy = panel.findChild<QAction*>("csaLogCopy");
        auto* clear = panel.findChild<QAction*>("csaLogClear");
        auto* wrap = panel.findChild<QAction*>("csaLogWrap");
        auto* view = panel.findChild<QPlainTextEdit*>("csaLogView");
        QVERIFY(copy && clear && wrap && view);
        QVERIFY(!copy->isEnabled() && !clear->isEnabled());
        panel.append(QStringLiteral("LOGIN:user OK"));
        QVERIFY(copy->isEnabled() && clear->isEnabled());
        wrap->trigger();
        QCOMPARE(view->lineWrapMode(), QPlainTextEdit::WidgetWidth);
        QVERIFY(NetworkSettings::csaLogWrapLines());
        wrap->trigger();
        clear->trigger();
        QVERIFY(view->document()->isEmpty());
        QVERIFY(!copy->isEnabled());
    }

    /// ドックと通信対局の待機ダイアログのログは同じ設定を使い、片方で変えた文字の大きさと折り返しに
    /// もう片方も合わせる（作ったときの古い値から計算して保存し直さない）。別の設定のパネルは変えない
    void panelsSharingSettingsFollowEachOther()
    {
        NetworkSettings::csaLogFontSize.set(10);
        NetworkSettings::csaLogWrapLines.set(false);
        const BoundedSetting<int> otherFont{"FontSize/testOtherLog", 10, 8, 24};
        const Setting<bool> otherWrap{"TestOtherLog/wrapLines", false};
        CsaLogPanel dock;
        LogPanel other(otherFont, otherWrap, LogPanel::Names{}, nullptr);
        const auto button = [](QWidget& panel, const char* role) {
            const auto buttons = panel.findChildren<QAbstractButton*>();
            for (QAbstractButton* candidate : buttons) {
                if (candidate->property(FontSizeControl::kRoleProperty).toString() == QLatin1String(role))
                    return candidate;
            }
            return static_cast<QAbstractButton*>(nullptr);
        };
        const auto logSize = [](QWidget& panel) {
            const auto views = panel.findChildren<QPlainTextEdit*>();
            return views.isEmpty() ? -1 : views.first()->font().pointSize();
        };
        auto* dockWrap = dock.findChild<QAction*>("csaLogWrap");
        auto* dockView = dock.findChild<QPlainTextEdit*>("csaLogView");
        {
            CsaLogPanel waiting;
            auto* increase = button(waiting, "increase");
            QVERIFY(increase);
            increase->click();
            increase->click();
            QCOMPARE(NetworkSettings::csaLogFontSize(), 12);
            QCOMPARE(logSize(dock), 12);
            QCOMPARE(logSize(waiting), 12);
            waiting.findChild<QAction*>("csaLogWrap")->trigger();
            QVERIFY(NetworkSettings::csaLogWrapLines());
            QVERIFY(dockWrap->isChecked());
            QCOMPARE(dockView->lineWrapMode(), QPlainTextEdit::WidgetWidth);
        }
        // 待機ダイアログを閉じた後のドックの A+ は、合わせた大きさから1つ上げる
        button(dock, "increase")->click();
        QCOMPARE(NetworkSettings::csaLogFontSize(), 13);
        QCOMPARE(logSize(dock), 13);
        dockWrap->trigger();
        QVERIFY(!NetworkSettings::csaLogWrapLines());
        // 別の設定を使うパネル（USI 通信ログ）は変わらない
        QCOMPARE(otherFont(), 10);
        QCOMPARE(logSize(other), 10);
        QVERIFY(!otherWrap());
        NetworkSettings::csaLogFontSize.set(10);
    }
};

QTEST_MAIN(TestCsaLogPanel)
#include "tst_csalogpanel.moc"
