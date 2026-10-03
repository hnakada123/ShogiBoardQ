#include <QtTest>
#include <QApplication>
#include <QAbstractButton>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTranslator>
#include <QTimer>
#include <QTableWidget>
#include <QLibraryInfo>
#include <memory>

#include "applicationfonts.h"
#include "appsettings.h"
#include "settingscommon.h"
#include "dialogfontscale.h"
#include "boardcolordialog.h"
#include "changeenginesettingsdialog.h"
#include "considerationdialog.h"
#include "csagamedialog.h"
#include "csawaitingdialog.h"
#include "engineregistrationdialog.h"
#include "fontsettingsdialog.h"
#include "jishogiscoredialog.h"
#include "josekimergedialog.h"
#include "josekimovedialog.h"
#include "kifuanalysisdialog.h"
#include "kifupastedialog.h"
#include "piecesoundsettingsdialog.h"
#include "promotedialog.h"
#include "pvboarddialog.h"
#include "sfencollectiondialog.h"
#include "sfenutils.h"
#include "startgamedialog.h"
#include "tsumecollectiondialog.h"
#include "tsumeplaydialog.h"
#include "tsumeshogigeneratordialog.h"
#include "tsumeshogisearchdialog.h"
#include "versiondialog.h"
#include "evaluationchartconfigurator.h"
#include "longlongspinbox.h"

class TestDialogAppearance : public QObject
{
    Q_OBJECT

    std::unique_ptr<QDialog> create(const QString& name)
    {
        if (name == QLatin1String("boardColors")) return std::make_unique<BoardColorDialog>();
        if (name == QLatin1String("engineOptions")) {
            auto& settings = SettingsCommon::openSettings();
            settings.beginWriteArray(QStringLiteral("Test engine"), 2);
            settings.setArrayIndex(0);
            settings.setValue(QStringLiteral("name"), QStringLiteral("Threads"));
            settings.setValue(QStringLiteral("type"), QStringLiteral("spin"));
            settings.setValue(QStringLiteral("default"), QStringLiteral("1"));
            settings.setValue(QStringLiteral("value"), QStringLiteral("4"));
            settings.setValue(QStringLiteral("min"), QStringLiteral("1"));
            settings.setValue(QStringLiteral("max"), QStringLiteral("1024"));
            settings.setArrayIndex(1);
            settings.setValue(QStringLiteral("name"), QStringLiteral("BookFile"));
            settings.setValue(QStringLiteral("type"), QStringLiteral("combo"));
            settings.setValue(QStringLiteral("default"), QStringLiteral("standard_book.db"));
            settings.setValue(QStringLiteral("value"), QStringLiteral("standard_book.db"));
            settings.setValue(QStringLiteral("valueList"), QStringLiteral("[\"standard_book.db\",\"long_book_filename_for_selection.db\"]"));
            settings.endArray();
            settings.sync();
            auto dialog = std::make_unique<ChangeEngineSettingsDialog>();
            dialog->setEngineName(QStringLiteral("Test engine"));
            dialog->setupEngineOptionsDialog();
            return dialog;
        }
        if (name == QLatin1String("consideration")) return std::make_unique<ConsiderationDialog>();
        if (name == QLatin1String("csaGame")) return std::make_unique<CsaGameDialog>();
        if (name == QLatin1String("csaWaiting")) return std::make_unique<CsaWaitingDialog>(nullptr);
        if (name == QLatin1String("engines")) return std::make_unique<EngineRegistrationDialog>();
        if (name == QLatin1String("fontSettings")) return std::make_unique<FontSettingsDialog>();
        if (name == QLatin1String("jishogi")) return std::make_unique<JishogiScoreDialog>(JishogiCalculator::JishogiResult{}, false, false);
        if (name == QLatin1String("josekiMerge")) return std::make_unique<JosekiMergeDialog>();
        if (name == QLatin1String("josekiMove")) return std::make_unique<JosekiMoveDialog>();
        if (name == QLatin1String("analysis")) return std::make_unique<KifuAnalysisDialog>();
        if (name == QLatin1String("paste")) return std::make_unique<KifuPasteDialog>();
        if (name == QLatin1String("sound")) return std::make_unique<PieceSoundSettingsDialog>(nullptr);
        if (name == QLatin1String("promotion")) return std::make_unique<PromoteDialog>();
        if (name == QLatin1String("pv")) return std::make_unique<PvBoardDialog>(SfenUtils::hirateSfen(), QStringList{QStringLiteral("7g7f")});
        if (name == QLatin1String("sfen")) return std::make_unique<SfenCollectionDialog>();
        if (name == QLatin1String("startGame")) return std::make_unique<StartGameDialog>();
        if (name == QLatin1String("tsumeCollection")) return std::make_unique<TsumeCollectionDialog>();
        if (name == QLatin1String("tsumePlay")) return std::make_unique<TsumePlayDialog>();
        if (name == QLatin1String("generator")) return std::make_unique<TsumeshogiGeneratorDialog>();
        if (name == QLatin1String("tsumeSearch")) return std::make_unique<TsumeShogiSearchDialog>();
        if (name == QLatin1String("version")) return std::make_unique<VersionDialog>();
        auto dialog = std::make_unique<QInputDialog>();
        dialog->setLabelText(QStringLiteral("しおり名:"));
        dialog->setTextValue(QStringLiteral("重要な局面"));
        dialog->ensurePolished();
        DialogFontScale::install(dialog.get(), QStringLiteral("input"), true);
        return dialog;
    }

    QList<QAbstractButton*> fontButtons(QDialog* dialog)
    {
        QAbstractButton* down = nullptr;
        QAbstractButton* up = nullptr;
        for (auto* button : dialog->findChildren<QAbstractButton*>()) {
            if (button->window() != dialog) continue;
            if (button->text() == QLatin1String("A-") || button->text() == QStringLiteral("A−")) down = button;
            if (button->text() == QLatin1String("A+")) up = button;
        }
        return {down, up};
    }

    void snapshot(QDialog* dialog, const QString& name)
    {
        const QString directory = qEnvironmentVariable("SHOGIBOARDQ_DIALOG_SCREENSHOTS");
        if (directory.isEmpty()) return;
        QDir().mkpath(directory);
        QVERIFY(dialog->grab().save(directory + QLatin1Char('/') + name + QStringLiteral(".png")));
    }

private slots:
    void init() { QVERIFY(SettingsCommon::resetAllSettings()); }

    void dialogs_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("english");
        for (const auto* name : {"boardColors", "engineOptions", "consideration", "csaGame", "csaWaiting", "engines",
             "fontSettings", "jishogi", "josekiMerge", "josekiMove", "analysis", "paste", "sound", "promotion",
             "pv", "sfen", "startGame", "tsumeCollection", "tsumePlay", "generator", "tsumeSearch", "version", "input"}) {
            for (bool english : {false, true})
                QTest::newRow(qPrintable(QString::fromLatin1(name) + (english ? "-en" : "-ja"))) << QString::fromLatin1(name) << english;
        }
    }

    void dialogs()
    {
        QFETCH(QString, name);
        QFETCH(bool, english);
        QTranslator translator;
        QTranslator qtTranslator;
        if (qtTranslator.load(english ? QStringLiteral("qtbase_en") : QStringLiteral("qtbase_ja"),
                              QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            qApp->installTranslator(&qtTranslator);
        if (english) {
            QVERIFY(translator.load(QStringLiteral(TRANSLATIONS_DIR "/ShogiBoardQ_en.qm")));
            qApp->installTranslator(&translator);
        }
        auto dialog = create(name);
        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog.get()));
        QTest::qWait(30);
        const auto buttons = fontButtons(dialog.get());
        QVERIFY2(buttons[0] && buttons[1], qPrintable(name + " has no font controls"));
        snapshot(dialog.get(), QString::fromLatin1(QTest::currentDataTag()));
        const int initial = dialog->font().pointSize();
        for (int i = 0; i < 32 && buttons[1]->isEnabled(); ++i) {
            QTest::mouseClick(buttons[1], Qt::LeftButton);
            QCoreApplication::processEvents();
        }
        QVERIFY2(!buttons[1]->isEnabled(), "Increase must be disabled at the upper limit");
        QTest::qWait(30);
        QVERIFY(dialog->isVisible());
        for (auto* button : buttons) {
            QVERIFY(button->isVisible());
            QVERIFY(!button->accessibleName().isEmpty());
            QVERIFY(button->focusPolicy() != Qt::NoFocus);
            QVERIFY(button->width() >= button->fontMetrics().horizontalAdvance(button->text()) + 12);
            QVERIFY(dialog->rect().contains(QRect(button->mapTo(dialog.get(), QPoint()), button->size())));
            QVERIFY2(button->visibleRegion().boundingRect().contains(button->rect()), "Font controls must remain fully visible");
        }
        snapshot(dialog.get(), QString::fromLatin1(QTest::currentDataTag()) + "-large");
        for (int i = 0; i < 32 && buttons[0]->isEnabled(); ++i) {
            QTest::mouseClick(buttons[0], Qt::LeftButton);
            QCoreApplication::processEvents();
        }
        QVERIFY(!buttons[0]->isEnabled());
        if (name != QLatin1String("jishogi")) QVERIFY(dialog->font().pointSize() <= initial);
        dialog->reject();
    }

    void soundNumericInput()
    {
        PieceSoundSettingsDialog dialog(nullptr);
        for (const auto* name : {"pieceSoundVolume", "pieceSoundPitch", "pieceSoundLow", "pieceSoundMid", "pieceSoundHigh"}) {
            auto* slider = dialog.findChild<QSlider*>(QString::fromLatin1(name));
            auto* input = dialog.findChild<QSpinBox*>(QString::fromLatin1(name) + "Value");
            QVERIFY(slider && input);
            input->setValue(input->maximum());
            QCOMPARE(slider->value(), input->maximum());
            slider->setValue(slider->minimum());
            QCOMPARE(input->value(), slider->minimum());
        }
    }

    void footerReflowsWithoutHidingActions()
    {
        PieceSoundSettingsDialog dialog(nullptr);
        dialog.resize(1000, 500);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* box = dialog.findChild<QDialogButtonBox*>();
        auto* scale = dialog.findChild<QWidget*>(QStringLiteral("dialogFontScale"));
        QVERIFY(box && scale);
        QTRY_VERIFY(qAbs(box->geometry().center().y() - scale->geometry().center().y()) <= 2);
        for (int i = 0; i < 14; ++i) QTest::mouseClick(fontButtons(&dialog)[1], Qt::LeftButton);
        dialog.resize(qMax(box->minimumSizeHint().width(), scale->minimumSizeHint().width()) + 32, 750);
        QTRY_VERIFY(box->geometry().top() >= scale->geometry().bottom());
        QTest::qWait(50);
        for (auto* button : box->buttons()) {
            QVERIFY(button->visibleRegion().boundingRect().contains(button->rect()));
            QVERIFY(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint()), button->size())));
        }
        dialog.resize(1600, 800);
        QTRY_VERIFY(qAbs(box->geometry().center().y() - scale->geometry().center().y()) <= 2);
        dialog.reject();
    }

    void emptyStatesAndMergeSelection()
    {
        EngineRegistrationDialog registration;
        auto* list = registration.findChild<QListWidget*>();
        auto* hint = registration.findChild<QLabel*>(QStringLiteral("engineHintLabel"));
        QVERIFY(list && hint);
        QVERIFY(hint->text().contains(QStringLiteral("追加")));
        list->addItem(QStringLiteral("Test engine"));
        QVERIFY(hint->text().contains(QStringLiteral("ダブルクリック")));
        list->clear();
        QVERIFY(hint->text().contains(QStringLiteral("追加")));

        JosekiMergeDialog merge;
        auto* all = merge.findChild<QPushButton*>(QStringLiteral("registerAllMoves"));
        auto* table = merge.findChild<QTableWidget*>();
        QVERIFY(all && table);
        QVERIFY(!all->isEnabled());
        const QString sfen = SfenUtils::hirateSfen();
        merge.setKifuData({{1, sfen, QStringLiteral("7g7f"), QStringLiteral("▲７六歩"), true}});
        QVERIFY(all->isEnabled());
        merge.setRegisteredMoves({sfen.section(QLatin1Char(' '), 0, 2) + QStringLiteral(":7g7f")});
        QVERIFY(!all->isEnabled());
        QVERIFY(!table->cellWidget(0, 2)->isEnabled());
        merge.setRegisteredMoves({});
        QVERIFY(all->isEnabled());
        merge.show();
        QVERIFY(QTest::qWaitForWindowExposed(&merge));
        for (int i = 0; i < 12; ++i) QTest::mouseClick(fontButtons(&merge)[1], Qt::LeftButton);
        QTest::qWait(30);
        QVERIFY(table->rowHeight(0) >= table->cellWidget(0, 2)->sizeHint().height());
        snapshot(&merge, QStringLiteral("joseki-merge-populated-large"));
        merge.reject();

        SfenCollectionDialog collection;
        auto* file = collection.findChild<QLabel*>(QStringLiteral("collectionFileLabel"));
        QVERIFY(file);
        QVERIFY(file->text().contains(QStringLiteral("ファイルを開く")));
    }

    void engineSelectionAndKeyboard()
    {
        EngineRegistrationDialog registration;
        auto* list = registration.findChild<QListWidget*>();
        auto* configure = registration.findChild<QPushButton*>(QStringLiteral("configureEngineButton"));
        auto* remove = registration.findChild<QPushButton*>(QStringLiteral("removeEngineButton"));
        QVERIFY(list && configure && remove);
        QVERIFY(!configure->isEnabled());
        QVERIFY(!remove->isEnabled());
        list->addItem(QStringLiteral("Test engine"));
        list->setCurrentRow(0);
        QVERIFY(configure->isEnabled());
        QVERIFY(remove->isEnabled());
        list->clearSelection();
        QVERIFY(!configure->isEnabled());

        auto options = create(QStringLiteral("engineOptions"));
        auto* threads = options->findChild<LongLongSpinBox*>(QStringLiteral("Threads"));
        auto* book = options->findChild<QComboBox*>(QStringLiteral("BookFile"));
        QVERIFY(threads && book);
        QVERIFY(threads->focusPolicy() & Qt::TabFocus);
        QCOMPARE(threads->value(), 4);
        QCOMPARE(book->currentText(), QStringLiteral("standard_book.db"));
        QVERIFY(book->maximumWidth() > 200);
    }

    void savedFontAndSize()
    {
        {
            PromoteDialog dialog;
            dialog.show();
            QVERIFY(QTest::qWaitForWindowExposed(&dialog));
            QTest::mouseClick(fontButtons(&dialog)[1], Qt::LeftButton);
            dialog.resize(540, 280);
            dialog.reject();
            QCOMPARE(AppSettings::dialogFontSize(QStringLiteral("promotion"), 10), dialog.font().pointSize());
            QCOMPARE(AppSettings::auxiliaryDialogSize(QStringLiteral("promotion")), dialog.size());
        }
        PromoteDialog reopened;
        QCOMPARE(reopened.size(), QSize(540, 280));
        QCOMPARE(reopened.font().pointSize(), AppSettings::dialogFontSize(QStringLiteral("promotion"), 10));
    }

    void colorPickerAndCommunicationLog()
    {
        BoardColorDialog appearance;
        auto* picker = appearance.findChild<QColorDialog*>();
        QVERIFY(picker);
        picker->show();
        QVERIFY(QTest::qWaitForWindowExposed(picker));
        const auto colorButtons = fontButtons(picker);
        QVERIFY(colorButtons[0] && colorButtons[1]);
        const int colorSize = picker->font().pointSize();
        QTest::mouseClick(colorButtons[1], Qt::LeftButton);
        QCOMPARE(picker->font().pointSize(), colorSize + 1);
        snapshot(picker, QStringLiteral("color-picker"));
        picker->reject();

        CsaWaitingDialog waiting(nullptr);
        waiting.show();
        auto* log = waiting.findChild<QDialog*>(QStringLiteral("csaWaitingLogWindow"));
        QVERIFY(log);
        log->show();
        QVERIFY(QTest::qWaitForWindowExposed(log));
        const auto logButtons = fontButtons(log);
        QVERIFY(logButtons[0] && logButtons[1]);
        QTest::mouseClick(logButtons[1], Qt::LeftButton);
        const int logSize = logButtons[1]->font().pointSize();
        QTest::mouseClick(fontButtons(&waiting)[1], Qt::LeftButton);
        QCOMPARE(logButtons[1]->font().pointSize(), logSize);
        snapshot(log, QStringLiteral("csa-log"));
        log->reject();
        waiting.reject();
    }

    void evaluationSettings()
    {
        EvaluationChartConfigurator configurator;
        QTimer::singleShot(0, this, &TestDialogAppearance::checkEvaluationSettings);
        QVERIFY(QMetaObject::invokeMethod(&configurator, "showSettings", Qt::DirectConnection));
    }

private:
    void checkEvaluationSettings()
    {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        // 検査に失敗しても、モーダルループが残らないようにする。
        QTimer::singleShot(1000, dialog, &QDialog::reject);
        const auto buttons = fontButtons(dialog);
        QVERIFY(buttons[0] && buttons[1]);
        const int initial = dialog->font().pointSize();
        QTest::mouseClick(buttons[1], Qt::LeftButton);
        QCOMPARE(dialog->font().pointSize(), initial + 1);
        snapshot(dialog, QStringLiteral("evaluation-settings"));
        dialog->reject();
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir data;
    qputenv("XDG_CONFIG_HOME", data.path().toUtf8());
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    qputenv("XDG_CACHE_HOME", data.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("DialogAppearanceTest"));
    ApplicationFonts::initialize();
    TestDialogAppearance test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_dialog_appearance.moc"
