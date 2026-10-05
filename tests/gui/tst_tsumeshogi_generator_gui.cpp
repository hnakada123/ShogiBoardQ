#include <QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include "applicationfonts.h"
#include "pvboarddialog.h"
#include "settingscommon.h"
#include "tsumeshogigeneratordialog.h"
#include "tsumeshogisettings.h"

class TestTsumeshogiGeneratorGui : public QObject
{
    Q_OBJECT
    const QString sfen = QStringLiteral("9/9/6R1+R/5k3/9/7+S1/9/9/9 b 2b4g3s4n4l18p 1");
    const QStringList pv{QStringLiteral("3c5c+"), QStringLiteral("4d4e"), QStringLiteral("1c4c"),
                         QStringLiteral("P*4d"), QStringLiteral("4c4d")};
    QString savePath;

    QPushButton* button(TsumeshogiGeneratorDialog& dialog, const char* name)
    { return dialog.findChild<QPushButton*>(QLatin1String(name)); }
    void addResult(TsumeshogiGeneratorDialog& dialog)
    { QVERIFY(QMetaObject::invokeMethod(&dialog, "onPositionFound", Q_ARG(QString, sfen), Q_ARG(QStringList, pv))); }
    void registerEngine()
    {
        auto& settings = SettingsCommon::openSettings();
        settings.beginWriteArray("Engines"); settings.setArrayIndex(0);
        settings.setValue("name", "Audit USI");
        settings.setValue("path", QStringLiteral(AUDIT_DIR "/mock_usi.py"));
        settings.endArray();
    }
    void dismissError()
    {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->accept();
    }
    void chooseSaveFile()
    {
        if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
            dialog->selectFile(savePath);
            static_cast<QDialog*>(dialog)->accept();
        }
    }

private slots:
    void init() { SettingsCommon::openSettings().clear(); }
    void presentationAndExport()
    {
        registerEngine();
        TsumeshogiGeneratorDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isActiveWindow());
        auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("generatorResults"));
        auto* progress = dialog.findChild<QProgressBar*>();
        auto* limit = dialog.findChild<QSpinBox*>(QStringLiteral("generatorMaxPositions"));
        QVERIFY(!button(dialog, "generatorSave")->isEnabled());
        QVERIFY(!button(dialog, "generatorCopyAll")->isEnabled());
        QVERIFY(!button(dialog, "generatorCopySelected")->isEnabled());
        QCOMPARE(progress->maximum(), 10);
        QVERIFY(table->height() >= 180);
        QCOMPARE(dialog.findChild<QScrollArea*>()->verticalScrollBar()->maximum(), 0);
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsumeshogi-generator-empty.png")));
        limit->setValue(3);
        addResult(dialog);
        addResult(dialog);
        QCOMPARE(progress->maximum(), 3);
        QCOMPARE(progress->value(), 2);
        QVERIFY(button(dialog, "generatorSave")->isEnabled());
        QVERIFY(button(dialog, "generatorCopyAll")->isEnabled());
        QVERIFY(!button(dialog, "generatorCopySelected")->isEnabled());
        // 逆順に選んでも表示順でコピーし、SFEN全文と手順を保持する。
        table->item(0, 1)->setText(sfen + QStringLiteral(" first"));
        table->selectionModel()->select(table->model()->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        table->selectionModel()->select(table->model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        dialog.findChild<QCheckBox*>(QStringLiteral("generatorIncludePv"))->setChecked(true);
        table->setFocus();
        QTRY_VERIFY(table->hasFocus());
        QTest::keyClick(table, Qt::Key_C, Qt::ControlModifier);
        const QString moves = QStringLiteral(" moves ") + pv.join(QLatin1Char(' '));
        QCOMPARE(QApplication::clipboard()->text(), sfen + " first" + moves + '\n' + sfen + moves);
        table->item(0, 1)->setText(sfen);
        table->setCurrentCell(0, 1);
        QTest::keyClick(table, Qt::Key_Return);
        QTRY_VERIFY(dialog.findChild<PvBoardDialog*>());
        dialog.findChild<PvBoardDialog*>()->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        dialog.activateWindow();
        QTRY_VERIFY(dialog.isActiveWindow());
        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                         table->visualRect(table->model()->index(0, 1)).center());
        QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier,
                          table->visualRect(table->model()->index(0, 1)).center());
        QTRY_COMPARE(dialog.findChildren<PvBoardDialog*>().size(), 1);
        dialog.findChild<PvBoardDialog*>()->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        limit->setValue(100);
        for (int i = 0; i < 50; ++i) addResult(dialog);
        QTest::qWait(30);
        table->verticalScrollBar()->setValue(0);
        addResult(dialog);
        QCOMPARE(table->verticalScrollBar()->value(), 0);
        table->clearSelection();
        QVERIFY(!button(dialog, "generatorCopySelected")->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onProgressUpdated", Q_ARG(int, 12000), Q_ARG(int, 53), Q_ARG(qint64, 96000)));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onGeneratorFinished"));
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("generatorStatus"))->text().contains(QStringLiteral("生成完了")));
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsumeshogi-generator-results.png")));
        dialog.findChild<QToolButton*>(QStringLiteral("generatorHelp"))->setChecked(true);
        // 説明を開いても設定欄はスクロールせずに全項目が見える（必要ならダイアログが高くなる）
        auto* settingsScroll = dialog.findChild<QScrollArea*>(QStringLiteral("generatorSettingsScroll"));
        QTRY_COMPARE(settingsScroll->verticalScrollBar()->maximum(), 0);
        QVERIFY(table->height() >= 180);
        QTest::qWait(50);
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsumeshogi-generator-help.png")));
        // 閉じると設定欄が縮み、結果一覧に高さが戻る
        const int tableHeightWithHelp = table->height();
        dialog.findChild<QToolButton*>(QStringLiteral("generatorHelp"))->setChecked(false);
        QTRY_VERIFY(table->height() > tableHeightWithHelp);
        dialog.findChild<QToolButton*>(QStringLiteral("generatorHelp"))->setChecked(true);
        QTRY_COMPARE(settingsScroll->verticalScrollBar()->maximum(), 0);
        dialog.close();
        QVERIFY(TsumeshogiSettings::tsumeshogiGeneratorHelpExpanded());
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorMaxPositions(), 100);
        TsumeshogiGeneratorDialog restored;
        QCOMPARE(restored.size(), dialog.size());
        QVERIFY(restored.findChild<QToolButton*>(QStringLiteral("generatorHelp"))->isChecked());
    }
    void saveResults()
    {
        QTemporaryDir files;
        QVERIFY(files.isValid());
        savePath = files.filePath(QStringLiteral("positions.txt"));
        TsumeshogiGeneratorDialog dialog;
        dialog.show();
        addResult(dialog);
        dialog.findChild<QCheckBox*>(QStringLiteral("generatorIncludePv"))->setChecked(true);
        QTimer::singleShot(0, this, &TestTsumeshogiGeneratorGui::chooseSaveFile);
        QTest::mouseClick(button(dialog, "generatorSave"), Qt::LeftButton);
        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QVERIFY(bytes.startsWith("#"));
        QVERIFY(bytes.endsWith((sfen + " moves " + pv.join(' ') + '\n').toUtf8()));
        QCOMPARE(TsumeshogiSettings::tsumeshogiGeneratorLastSaveDirectory(), files.path());
    }
    void generationLifecycle()
    {
        registerEngine();
        TsumeshogiGeneratorDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* generator = dialog.findChild<TsumeshogiGenerator*>();
        auto* status = dialog.findChild<QLabel*>(QStringLiteral("generatorStatus"));
        auto* progress = dialog.findChild<QProgressBar*>();
        auto* limit = dialog.findChild<QSpinBox*>(QStringLiteral("generatorMaxPositions"));
        limit->setValue(0);
        QTest::mouseClick(button(dialog, "generatorStart"), Qt::LeftButton);
        QVERIFY(generator->isRunning());
        QCOMPARE(progress->maximum(), 0);
        QVERIFY(!button(dialog, "generatorRestoreDefaults")->isEnabled());
        QVERIFY(!limit->isEnabled());
        QTest::mouseClick(button(dialog, "generatorStop"), Qt::LeftButton);
        QVERIFY(!generator->isRunning());
        QVERIFY(status->text().contains(QStringLiteral("停止しました")));
        QVERIFY(progress->maximum() > 0);
        QVERIFY(button(dialog, "generatorRestoreDefaults")->isEnabled());
        // 再開時は前の件数・検査状況・経過時間をリセットする。
        addResult(dialog);
        limit->setValue(2);
        QTest::mouseClick(button(dialog, "generatorStart"), Qt::LeftButton);
        QCOMPARE(dialog.findChild<QTableWidget*>()->rowCount(), 0);
        QCOMPARE(progress->maximum(), 2);
        QCOMPARE(progress->value(), 0);
        QVERIFY(!button(dialog, "generatorSave")->isEnabled());
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QVERIFY(!generator->isRunning());
        QVERIFY(!dialog.isVisible());
        // errorOccurred に続く finished でエラー表示が消えない。
        QTimer::singleShot(0, this, &TestTsumeshogiGeneratorGui::dismissError);
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onGeneratorError", Q_ARG(QString, QStringLiteral("test failure"))));
        QVERIFY(QMetaObject::invokeMethod(&dialog, "onGeneratorFinished"));
        QVERIFY(status->text().contains(QStringLiteral("test failure")));
    }
    void noEngineAndLargeFont()
    {
        TsumeshogiSettings::setTsumeshogiGeneratorFontSize(16);
        TsumeshogiGeneratorDialog dialog;
        dialog.resize(940, 800);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(!button(dialog, "generatorStart")->isEnabled());
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("generatorStatus"))->text().contains(QStringLiteral("登録")));
        QVERIFY(dialog.findChild<QTableWidget*>()->height() >= 120);
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsumeshogi-generator-large-font.png")));
    }
};

int main(int argc, char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QTemporaryDir data;
    qputenv("XDG_CONFIG_HOME", data.path().toUtf8());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    qputenv("XDG_CACHE_HOME", data.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TsumeshogiGeneratorGuiTest"));
    ApplicationFonts::initialize();
    TestTsumeshogiGeneratorGui test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_tsumeshogi_generator_gui.moc"
