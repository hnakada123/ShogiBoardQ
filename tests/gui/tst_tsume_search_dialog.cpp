#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QLibraryInfo>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTranslator>

#include "analysissettings.h"
#include "applicationfonts.h"
#include "settingscommon.h"
#include "tsumeshogisearchdialog.h"

class TestTsumeSearchDialog : public QObject
{
    Q_OBJECT

    void registerEngines(bool reversed = false)
    {
        auto& settings = SettingsCommon::openSettings();
        settings.beginWriteArray(QStringLiteral("Engines"), 2);
        for (int index = 0; index < 2; ++index) {
            const int engine = reversed ? 1 - index : index;
            settings.setArrayIndex(index);
            settings.setValue(QStringLiteral("name"), engine == 0
                ? QStringLiteral("Hayanagi 1.5.0")
                : QStringLiteral("YaneuraOu Mate — long engine name for selection and tooltip"));
            settings.setValue(QStringLiteral("path"), QStringLiteral("/test/engine%1").arg(engine));
        }
        settings.endArray();
    }

    QPushButton* startButton(TsumeShogiSearchDialog& dialog)
    {
        return dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
    }

    void verifyLayout(TsumeShogiSearchDialog& dialog)
    {
        for (auto* widget : dialog.findChildren<QWidget*>()) {
            if (!widget->isVisible() || widget->isWindow()) continue;
            const QRect bounds(widget->mapTo(&dialog, QPoint()), widget->size());
            QVERIFY2(dialog.rect().contains(bounds), qPrintable(widget->objectName()));
            if (auto* label = qobject_cast<QLabel*>(widget)) {
                if (label->hasHeightForWidth())
                    QVERIFY2(label->height() >= label->heightForWidth(label->width()),
                             qPrintable(label->objectName()));
            }
        }
    }

private slots:
    void init() { SettingsCommon::openSettings().clear(); }

    void noEngine()
    {
        TsumeShogiSearchDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(!startButton(dialog)->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton*>(QStringLiteral("engineSetting"))->isEnabled());
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("engineHint"))->text().contains(QStringLiteral("登録")));
        QSignalSpy accepted(&dialog, &QDialog::accepted);
        dialog.accept();
        QCOMPARE(accepted.count(), 0);
        QVERIFY(dialog.isVisible());
        verifyLayout(dialog);
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-search-no-engine.png")));
        dialog.reject();
    }

    void timeSettingsAndIsolation()
    {
        registerEngines();
        AnalysisSettings::setConsiderationEngineIndex(0);
        AnalysisSettings::setConsiderationUnlimitedTime(true);
        AnalysisSettings::setConsiderationByoyomiSec(87);
        AnalysisSettings::setConsiderationMultiPV(6);
        TsumeShogiSearchDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QVERIFY(!dialog.findChild<QSpinBox*>(QStringLiteral("spinBoxMultiPV")));
        auto* time = dialog.findChild<QSpinBox*>(QStringLiteral("byoyomiSec"));
        auto* limited = dialog.findChild<QRadioButton*>(QStringLiteral("considerationTimeRadioButton"));
        auto* unlimited = dialog.findChild<QRadioButton*>(QStringLiteral("unlimitedTimeRadioButton"));
        QVERIFY(limited->isChecked());
        QCOMPARE(time->value(), 20);
        time->setValue(0);
        QCOMPARE(time->value(), 1);
        QTest::mouseClick(unlimited, Qt::LeftButton, Qt::NoModifier, QPoint(10, unlimited->height() / 2));
        QVERIFY(dialog.unlimitedTimeFlag());
        QVERIFY(!time->isEnabled());
        QTest::mouseClick(limited, Qt::LeftButton, Qt::NoModifier, QPoint(10, limited->height() / 2));
        QVERIFY(time->isEnabled());
        auto* combo = dialog.findChild<QComboBox*>();
        combo->setCurrentIndex(1);
        QCOMPARE(combo->toolTip(), combo->currentText());
        auto* editor = time->findChild<QLineEdit*>();
        time->setFocus();
        editor->selectAll();
        QTest::keyClicks(editor, "35");
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.byoyomiSec(), 35);
        QCOMPARE(AnalysisSettings::tsumeSearchTimeLimitSec(), 35);
        QCOMPARE(AnalysisSettings::tsumeSearchEnginePath(), QStringLiteral("/test/engine1"));
        QVERIFY(!AnalysisSettings::tsumeSearchUnlimitedTime());
        QCOMPARE(AnalysisSettings::considerationEngineIndex(), 0);
        QCOMPARE(AnalysisSettings::considerationByoyomiSec(), 87);
        QCOMPARE(AnalysisSettings::considerationMultiPV(), 6);
        QVERIFY(AnalysisSettings::considerationUnlimitedTime());

        registerEngines(true);
        TsumeShogiSearchDialog restored;
        QCOMPARE(restored.engineNumber(), 0);
        QCOMPARE(restored.engineList().at(restored.engineNumber()).path, QStringLiteral("/test/engine1"));
        QCOMPARE(restored.byoyomiSec(), 35);
        restored.findChild<QRadioButton*>(QStringLiteral("unlimitedTimeRadioButton"))->setChecked(true);
        restored.accept();
        TsumeShogiSearchDialog unlimitedRestored;
        QVERIFY(unlimitedRestored.unlimitedTimeFlag());
        QVERIFY(!unlimitedRestored.findChild<QSpinBox*>()->isEnabled());
        QCOMPARE(unlimitedRestored.byoyomiSec(), 35);
    }

    void cancelAndAppearancePersistence()
    {
        registerEngines();
        AnalysisSettings::setTsumeSearchTimeLimitSec(42);
        AnalysisSettings::setTsumeSearchFontSize(10);
        TsumeShogiSearchDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        dialog.findChild<QComboBox*>()->setCurrentIndex(1);
        dialog.findChild<QSpinBox*>()->setValue(123);
        dialog.findChild<QRadioButton*>(QStringLiteral("unlimitedTimeRadioButton"))->setChecked(true);
        QTest::mouseClick(dialog.findChild<QPushButton*>(QStringLiteral("toolButtonFontIncrease")), Qt::LeftButton);
        dialog.resize(740, 520);
        QTest::qWait(30);
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QCOMPARE(AnalysisSettings::tsumeSearchTimeLimitSec(), 42);
        QVERIFY(!AnalysisSettings::tsumeSearchUnlimitedTime());
        QVERIFY(AnalysisSettings::tsumeSearchEnginePath().isEmpty());
        QCOMPARE(AnalysisSettings::tsumeSearchFontSize(), 11);
        TsumeShogiSearchDialog restored;
        QCOMPARE(restored.size(), dialog.size());
        QCOMPARE(restored.font().pointSize(), 11);
        QCOMPARE(restored.byoyomiSec(), 42);
    }

    void presentation_data()
    {
        QTest::addColumn<bool>("english");
        QTest::addColumn<int>("pointSize");
        QTest::newRow("ja") << false << 10;
        QTest::newRow("ja-large") << false << 24;
        QTest::newRow("en") << true << 10;
        QTest::newRow("en-large") << true << 24;
    }

    void presentation()
    {
        QFETCH(bool, english);
        QFETCH(int, pointSize);
        QTranslator translator;
        QTranslator qtTranslator;
        if (english) {
            QVERIFY(translator.load(QStringLiteral(APP_BUILD "/ShogiBoardQ_en.qm")));
            qApp->installTranslator(&translator);
        } else if (qtTranslator.load(QStringLiteral("qtbase_ja"),
                                    QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            qApp->installTranslator(&qtTranslator);
        }
        registerEngines();
        AnalysisSettings::setTsumeSearchFontSize(pointSize);
        TsumeShogiSearchDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        QCOMPARE(startButton(dialog)->text(), english ? QStringLiteral("Start search") : QStringLiteral("探索開始"));
        verifyLayout(dialog);
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/tsume-search-%1.png")
                                  .arg(QString::fromLatin1(QTest::currentDataTag()))));
        // 広げた後で再び狭くしても、説明や開始ボタンが欠けない。
        dialog.resize(1200, 850);
        dialog.resize(dialog.minimumSizeHint().width(), 100);
        QTest::qWait(30);
        verifyLayout(dialog);
        if (pointSize == 24)
            QVERIFY(!dialog.findChild<QPushButton*>(QStringLiteral("toolButtonFontIncrease"))->isEnabled());
        dialog.reject();
        if (english) qApp->removeTranslator(&translator);
        qApp->removeTranslator(&qtTranslator);
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir data;
    qputenv("XDG_CONFIG_HOME", data.path().toUtf8());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    qputenv("XDG_CACHE_HOME", data.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TsumeSearchDialogTest"));
    ApplicationFonts::initialize();
    TestTsumeSearchDialog test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_tsume_search_dialog.moc"
