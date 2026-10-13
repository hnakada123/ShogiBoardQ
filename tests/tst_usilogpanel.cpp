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
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextCharFormat>

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
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
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
        AnalysisSettings::usiLogWrapLines.set(wrap);
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

    // 末尾への追記では直前の行の文字配置を作り直さない（変わったのは新しい行だけ）。
    // 配置をすべて作り直したときと、Qt の標準の文書で同じ内容を表示したときと、画面と
    // スクロールバーが同じになる。
    void appendKeepsPreviousLineLayout_data()
    {
        QTest::addColumn<bool>("wrap");
        QTest::newRow("no-wrap") << false;
        QTest::newRow("wrapped") << true;
    }

    void appendKeepsPreviousLineLayout()
    {
        QFETCH(bool, wrap);
        AnalysisSettings::usiLogWrapLines.set(wrap);
        UsiLogPanel panel;
        std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
        page->resize(700, 300);
        page->show();
        QVERIFY(QTest::qWaitForWindowExposed(page.get()));
        auto* view = page->findChild<QPlainTextEdit*>("usiLogView");
        QVERIFY(view);
        QTextDocument* document = view->document();
        QCOMPARE(document->defaultFont(), view->font());
        QCOMPARE(document->documentMargin(), 6.0);
        QCOMPARE(document->maximumBlockCount(), 5000);

        QSignalSpy updated(document->documentLayout(), &QAbstractTextDocumentLayout::updateBlock);
        QStringList lines;
        for (int i = 0; i < 80; ++i) {
            const QString line = QStringLiteral("◀ E1: info depth %1 pv ").arg(i)
                + QStringLiteral("7g7f 3c3d ").repeated(5 + i % 30);
            lines.append(line);
            updated.clear();
            panel.appendColoredLog(line, i % 3 ? QColor(Qt::blue) : QColor(Qt::darkGreen));
            if (i == 0) continue;
            QVERIFY2(!updated.isEmpty(), qPrintable(QString::number(i)));
            for (const auto& arguments : std::as_const(updated))
                QCOMPARE(arguments.at(0).value<QTextBlock>().blockNumber(), document->blockCount() - 1);
        }
        QCoreApplication::processEvents();

        auto state = [view]() {
            return QList<int>{view->verticalScrollBar()->maximum(), view->verticalScrollBar()->value(),
                              view->horizontalScrollBar()->maximum(), view->horizontalScrollBar()->value()};
        };
        const QImage appended = view->grab().toImage();
        const QList<int> appendedState = state();
        // 文書全体を変えたことにして、行の配置を捨てて描き直す
        document->markContentsDirty(0, document->characterCount());
        QCoreApplication::processEvents();
        QCOMPARE(state(), appendedState);
        QCOMPARE(view->grab().toImage(), appended);

        // Qt の標準の文書で同じ内容を表示した表と比べる。折り返すときは、Qt が行数を
        // 配置した行の分だけ数える（ほかは1行とする）ので、追記の手順でスクロールバーが変わる。
        if (wrap) return;
        QPlainTextEdit reference(page.get());
        reference.setGeometry(view->geometry());
        reference.setReadOnly(true);
        reference.setMaximumBlockCount(5000);
        reference.setFont(view->font());
        reference.setLineWrapMode(view->lineWrapMode());
        reference.document()->setDocumentMargin(6);
        reference.show();
        QTextCursor cursor(reference.document());
        for (qsizetype i = 0; i < lines.size(); ++i) {
            QTextCharFormat format;
            format.setForeground(i % 3 ? QColor(Qt::blue) : QColor(Qt::darkGreen));
            if (i > 0) cursor.insertBlock();
            cursor.insertText(lines.at(i), format);
        }
        reference.verticalScrollBar()->setValue(view->verticalScrollBar()->value());
        reference.horizontalScrollBar()->setValue(view->horizontalScrollBar()->value());
        QCoreApplication::processEvents();
        QCOMPARE(reference.verticalScrollBar()->maximum(), appendedState.at(0));
        QCOMPARE(reference.horizontalScrollBar()->maximum(), appendedState.at(2));
        reference.raise();
        QCOMPARE(reference.grab().toImage(), appended);
    }

    void logHistoryIsBounded()
    {
        UsiLogPanel panel;
        std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
        auto* view = page->findChild<QPlainTextEdit*>("usiLogView");
        for (int i = 0; i < 12000; ++i)
            panel.appendColoredLog(QStringLiteral("info depth %1 pv 7g7f").arg(i), Qt::blue);
        QVERIFY(view->document()->blockCount() <= 5000);
        QCOMPARE(view->document()->lastBlock().text(), QStringLiteral("info depth 11999 pv 7g7f"));
        QVERIFY(!view->document()->isUndoAvailable());
    }

    void trimmingPreservesRetainedSelection_data()
    {
        QTest::addColumn<bool>("reverse");
        QTest::newRow("forward") << false;
        QTest::newRow("reverse") << true;
    }

    void trimmingPreservesRetainedSelection()
    {
        QFETCH(bool, reverse);
        UsiLogPanel panel;
        std::unique_ptr<QWidget> page(panel.buildUi(nullptr));
        auto* view = page->findChild<QPlainTextEdit*>("usiLogView");
        view->setMaximumBlockCount(8);
        for (int i = 0; i < 8; ++i)
            panel.appendColoredLog(QStringLiteral("line %1").arg(i), Qt::blue);
        QTextCursor cursor(view->document());
        const int start = view->document()->findBlockByNumber(5).position();
        const int end = view->document()->characterCount() - 1;
        cursor.setPosition(reverse ? end : start);
        cursor.setPosition(reverse ? start : end, QTextCursor::KeepAnchor);
        view->setTextCursor(cursor);
        const QString selected = cursor.selectedText();
        for (int i = 8; i < 11; ++i) {
            panel.appendColoredLog(QStringLiteral("line %1").arg(i), Qt::blue);
            QCOMPARE(view->document()->blockCount(), 8);
            QCOMPARE(view->textCursor().selectedText(), selected);
        }
        QCOMPARE(view->document()->firstBlock().text(), QStringLiteral("line 3"));
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
