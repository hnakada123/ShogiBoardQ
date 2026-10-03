#include <QtTest>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>

#include "applicationfonts.h"
#include "appsettings.h"
#include "fontsettingsdialog.h"
#include "settingscommon.h"
#include "settingskeys.h"

class TestFontSettingsDialog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        ApplicationFonts::initialize();
    }

    void init()
    {
        QVERIFY(SettingsCommon::resetAllSettings());
        ApplicationFonts::applyFamily({});
    }

    void cleanupTestCase()
    {
        QVERIFY(SettingsCommon::resetAllSettings());
        ApplicationFonts::applyFamily({});
    }

    void previewCancelApplyAndRestore_data()
    {
        QTest::addColumn<QString>("language");
        for (const auto* language : {"ja_JP", "en", "zh_CN", "zh_TW"})
            QTest::newRow(language) << QString::fromLatin1(language);
    }

    void previewCancelApplyAndRestore()
    {
        QFETCH(QString, language);
        ApplicationFonts::initialize({}, language);
        QStringList families = QFontDatabase::families();
        families.removeAll(ApplicationFonts::defaultFamily());
        if (families.isEmpty()) QSKIP("An alternative font is required.");
        const QString chosen = families.first();
        const QString original = QApplication::font().family();
        FontSettingsDialog dialog;
        dialog.show();
        auto* defaults = dialog.findChild<QCheckBox*>(QStringLiteral("useDefaultFont"));
        auto* combo = dialog.findChild<QFontComboBox*>(QStringLiteral("fontFamilyComboBox"));
        auto* preview = dialog.findChild<QLabel*>(QStringLiteral("fontPreview"));
        QVERIFY(defaults && combo && preview);
        QVERIFY(defaults->isChecked());
        QVERIFY(!combo->isEnabled());
        defaults->setChecked(false);
        combo->setCurrentFont(QFont(chosen));
        QCOMPARE(preview->font().family(), chosen);
        QCOMPARE(QApplication::font().family(), original);
        QVERIFY(AppSettings::uiFontFamily().isEmpty());
        dialog.resize(680, 400);
        const QSize size = dialog.size();
        dialog.reject();
        QCOMPARE(QApplication::font().family(), original);
        QVERIFY(AppSettings::uiFontFamily().isEmpty());
        QCOMPARE(AppSettings::fontSettingsDialogSize(), size);

        dialog.show();
        dialog.accept();
        QCOMPARE(QApplication::font().family(), chosen);
        QCOMPARE(AppSettings::uiFontFamily(), chosen);
        // 独立した QSettings と起動時と同じ経路で復元する。
        SettingsCommon::openSettings().sync();
        QSettings disk(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(disk.value(SettingsKeys::kUiFontFamily).toString(), chosen);
        ApplicationFonts::applyFamily({});
        ApplicationFonts::initialize(AppSettings::uiFontFamily(), language);
        QCOMPARE(QApplication::font().family(), chosen);

        FontSettingsDialog restored;
        QCOMPARE(restored.size(), size);
        QVERIFY(!restored.findChild<QCheckBox*>()->isChecked());
        QCOMPARE(restored.findChild<QFontComboBox*>()->currentFont().family(), chosen);
        auto* buttons = restored.findChild<QDialogButtonBox*>();
        for (auto* button : buttons->buttons()) {
            if (buttons->buttonRole(button) == QDialogButtonBox::ResetRole) button->click();
        }
        QVERIFY(restored.findChild<QCheckBox*>()->isChecked());
        QCOMPARE(QApplication::font().family(), chosen);
        restored.accept();
        QVERIFY(AppSettings::uiFontFamily().isEmpty());
        QCOMPARE(QApplication::font().family(), original);
    }
};

QTEST_MAIN(TestFontSettingsDialog)
#include "tst_fontsettingsdialog.moc"
