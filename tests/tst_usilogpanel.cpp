#include <QtTest>
#include <QAction>
#include <QClipboard>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>

#include "analysissettings.h"
#include "settingscommon.h"
#include "usicommlogmodel.h"
#include "usilogpanel.h"

class TestUsiLogPanel : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_config.path());
    }

    void init()
    {
        SettingsCommon::openSettings().clear();
    }

    void appendPreservesReadingPositionAndSelection_data()
    {
        QTest::addColumn<bool>("wrap");
        QTest::newRow("no-wrap") << false;
        QTest::newRow("wrapped") << true;
    }

    void appendPreservesReadingPositionAndSelection()
    {
        QFETCH(bool, wrap);
        AnalysisSettings::setUsiLogWrapLines(wrap);
        UsiLogPanel panel;
        std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
        page->resize(800, 300);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        auto* view = page->findChild<QPlainTextEdit*>("usiLogView");
        QVERIFY(view);
        for (int i = 0; i < 100; ++i) {
            panel.appendColoredLog(QStringLiteral("◀ E1: info depth %1 pv ").arg(i)
                                       + QStringLiteral("7g7f 3c3d ").repeated(40), Qt::blue);
        }
        auto* vertical = view->verticalScrollBar();
        auto* horizontal = view->horizontalScrollBar();
        QVERIFY(vertical->maximum() > 0);
        QCOMPARE(vertical->value(), vertical->maximum());

        vertical->setValue(vertical->maximum() / 2);
        horizontal->setValue(horizontal->maximum() / 2);
        const int y = vertical->value();
        const int x = horizontal->value();
        panel.appendColoredLog(QStringLiteral("◀ E1: bestmove 7g7f"), Qt::blue);
        panel.appendStatus(QStringLiteral("E2: エンジンが起動していません"));
        QCOMPARE(vertical->value(), y);
        QCOMPARE(horizontal->value(), x);

        // 末尾まで選択していても、新着の追記で選択範囲が伸びないこと。
        QTextCursor cursor(view->document());
        cursor.movePosition(QTextCursor::End);
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        view->setTextCursor(cursor);
        vertical->setValue(vertical->maximum());
        const QString selected = view->textCursor().selectedText();
        const int selectedY = vertical->value();
        panel.appendColoredLog(QStringLiteral("▶ E1: isready"), Qt::blue);
        QCOMPARE(view->textCursor().selectedText(), selected);
        QCOMPARE(vertical->value(), selectedY);

        page->findChild<QAction*>("usiLogLatest")->trigger();
        QVERIFY(!view->textCursor().hasSelection());
        QCOMPARE(vertical->value(), vertical->maximum());
        panel.appendColoredLog(QStringLiteral("◀ E1: readyok"), Qt::blue);
        QCOMPARE(vertical->value(), vertical->maximum());
        page->findChild<QAction*>("usiLogWrap")->trigger();
        QCOMPARE(vertical->value(), vertical->maximum());
    }

    void copyAndClearKeepRawLog()
    {
        UsiLogPanel panel;
        std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
        auto* view = page->findChild<QPlainTextEdit*>("usiLogView");
        auto* copy = page->findChild<QAction*>("usiLogCopy");
        auto* clear = page->findChild<QAction*>("usiLogClear");
        QVERIFY(!copy->isEnabled());
        QVERIFY(!clear->isEnabled());
        UsiCommLogModel first;
        UsiCommLogModel second;
        panel.setModels(&first, &second);
        first.appendUsiCommLog(QStringLiteral("▶ E1: position startpos moves 7g7f"));
        second.appendUsiCommLog(QStringLiteral("◀ E2: info string <test> & ready"));
        const QString expected = QStringLiteral("▶ E1: position startpos moves 7g7f\n"
                                                "◀ E2: info string <test> & ready");
        QCOMPARE(view->toPlainText(), expected);
        QVERIFY(copy->isEnabled());
        copy->trigger();
        QCOMPARE(QApplication::clipboard()->text(), expected);
        clear->trigger();
        QVERIFY(view->toPlainText().isEmpty());
        QVERIFY(!copy->isEnabled());
        QVERIFY(!clear->isEnabled());
        // 表示の消去後もエンジンからの追記が継続する。
        first.appendUsiCommLog(QStringLiteral("◀ E1: readyok"));
        QCOMPARE(view->toPlainText(), QStringLiteral("◀ E1: readyok"));
    }

    void commandRoutesFromButtonAndEnter()
    {
        UsiLogPanel panel;
        std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
        auto* input = page->findChild<QLineEdit*>("usiCommandInput");
        auto* send = page->findChild<QPushButton*>("usiCommandSend");
        auto* target = page->findChild<QComboBox*>("usiCommandTarget");
        QSignalSpy spy(&panel, &UsiLogPanel::usiCommandRequested);
        QVERIFY(!send->isEnabled());
        input->setText(QStringLiteral("  "));
        QVERIFY(!send->isEnabled());
        QTest::keyClick(input, Qt::Key_Return);
        QCOMPARE(spy.count(), 0);
        for (int i = 0; i < 3; ++i) {
            target->setCurrentIndex(i);
            input->setText(QStringLiteral("  isready  "));
            QVERIFY(send->isEnabled());
            if (i == 1) QTest::keyClick(input, Qt::Key_Return);
            else QTest::mouseClick(send, Qt::LeftButton);
            QCOMPARE(spy.count(), i + 1);
            QCOMPARE(spy.at(i).at(0).toInt(), i);
            QCOMPARE(spy.at(i).at(1).toString(), QStringLiteral("isready"));
            QVERIFY(input->text().isEmpty());
            QVERIFY(!send->isEnabled());
        }
    }

    void restoresWrapAndTarget()
    {
        {
            UsiLogPanel panel;
            std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
            auto* view = page->findChild<QPlainTextEdit*>("usiLogView");
            QCOMPARE(view->lineWrapMode(), QPlainTextEdit::NoWrap);
            page->findChild<QAction*>("usiLogWrap")->trigger();
            QCOMPARE(view->lineWrapMode(), QPlainTextEdit::WidgetWidth);
            page->findChild<QComboBox*>("usiCommandTarget")->setCurrentIndex(1);
        }
        UsiLogPanel restored;
        std::unique_ptr<QWidget> page(restored.buildUi(nullptr));
        auto* view = page->findChild<QPlainTextEdit*>("usiLogView");
        QCOMPARE(view->lineWrapMode(), QPlainTextEdit::WidgetWidth);
        QVERIFY(page->findChild<QAction*>("usiLogWrap")->isChecked());
        QCOMPARE(page->findChild<QComboBox*>("usiCommandTarget")->currentIndex(), 1);
        page->findChild<QAction*>("usiLogWrap")->trigger();
        QCOMPARE(view->lineWrapMode(), QPlainTextEdit::NoWrap);
    }
};

QTEST_MAIN(TestUsiLogPanel)
#include "tst_usilogpanel.moc"
