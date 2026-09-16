/// @file tst_enginepondersettings.cpp
/// @brief エンジン先読み設定の旧設定移行・UI・永続化の回帰テスト

#include <QtTest>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>

#include "changeenginesettingsdialog.h"
#include "enginepondersettings.h"
#include "enginesettingsconstants.h"
#include "longlongspinbox.h"
#include "settingscommon.h"

using namespace EngineSettingsConstants;

class TestEnginePonderSettings : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
    const QString m_engine = QStringLiteral("PonderTest");

    template<typename T>
    static T* widget(ChangeEngineSettingsDialog& dialog, const char* name)
    {
        auto* result = dialog.findChild<T*>(QLatin1String(name));
        Q_ASSERT(result);
        return result;
    }

    void setup(ChangeEngineSettingsDialog& dialog)
    {
        dialog.setEngineName(m_engine);
        dialog.setupEngineOptionsDialog();
    }

    static void clickButton(ChangeEngineSettingsDialog& dialog, QDialogButtonBox::StandardButton button)
    {
        widget<QDialogButtonBox>(dialog, "buttonBox")->button(button)->click();
    }

    void saveLegacyOption(bool enabled, bool defaultEnabled)
    {
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.beginWriteArray(m_engine, 2);
        settings.setArrayIndex(0);
        settings.setValue(EngineOptionNameKey, QStringLiteral("Threads"));
        settings.setValue(EngineOptionTypeKey, QStringLiteral("spin"));
        settings.setValue(EngineOptionValueKey, QStringLiteral("4"));
        settings.setValue(EngineOptionDefaultKey, QStringLiteral("1"));
        settings.setValue(EngineOptionMinKey, QStringLiteral("1"));
        settings.setValue(EngineOptionMaxKey, QStringLiteral("16"));
        settings.setArrayIndex(1);
        settings.setValue(EngineOptionNameKey, QStringLiteral("USI_Ponder"));
        settings.setValue(EngineOptionTypeKey, QStringLiteral("check"));
        settings.setValue(EngineOptionValueKey, enabled ? QStringLiteral("true") : QStringLiteral("false"));
        settings.setValue(EngineOptionDefaultKey, defaultEnabled ? QStringLiteral("true") : QStringLiteral("false"));
        settings.endArray();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("EnginePonderSettingsTest"));
    }

    void init()
    {
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.clear();
    }

    void legacySettingIsMigrated_data()
    {
        QTest::addColumn<bool>("enabled");
        QTest::newRow("enabled") << true;
        QTest::newRow("disabled") << false;
    }

    void legacySettingIsMigrated()
    {
        QFETCH(bool, enabled);
        saveLegacyOption(enabled, !enabled);
        ChangeEngineSettingsDialog dialog;
        setup(dialog);
        auto* ponder = widget<QCheckBox>(dialog, "ponderEnabledCheckBox");
        QCOMPARE(ponder->isChecked(), enabled);
        QCOMPARE(EnginePonderSettings::load(m_engine).defaultEnabled, !enabled);
        // エンジンオプションとGUI設定の二重表示を避ける。
        QVERIFY(!dialog.findChild<QCheckBox*>(QStringLiteral("USI_Ponder")));
        ponder->setChecked(!enabled);
        widget<LongLongSpinBox>(dialog, "Threads")->setSpinBoxValue(8);
        clickButton(dialog, QDialogButtonBox::Ok);
        QCOMPARE(EnginePonderSettings::load(m_engine).enabled, !enabled);

        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(settings.beginReadArray(m_engine), 2);
        settings.setArrayIndex(0);
        QCOMPARE(settings.value(EngineOptionValueKey).toString(), QStringLiteral("8"));
        settings.setArrayIndex(1);
        QCOMPARE(settings.value(EngineOptionValueKey).toBool(), !enabled);
        settings.endArray();

        ChangeEngineSettingsDialog reopened;
        setup(reopened);
        QCOMPARE(widget<QCheckBox>(reopened, "ponderEnabledCheckBox")->isChecked(), !enabled);
    }

    void unreportedOptionCanBeEnabledAndSaved()
    {
        ChangeEngineSettingsDialog dialog;
        setup(dialog);
        auto* ponder = widget<QCheckBox>(dialog, "ponderEnabledCheckBox");
        auto* notify = widget<QCheckBox>(dialog, "sendUnreportedPonderCheckBox");
        QVERIFY(!ponder->isChecked());
        QVERIFY(notify->isChecked());
        ponder->setChecked(true);
        notify->setChecked(false);
        clickButton(dialog, QDialogButtonBox::Ok);

        ChangeEngineSettingsDialog reopened;
        setup(reopened);
        QVERIFY(widget<QCheckBox>(reopened, "ponderEnabledCheckBox")->isChecked());
        QVERIFY(!widget<QCheckBox>(reopened, "sendUnreportedPonderCheckBox")->isChecked());
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(settings.beginReadArray(m_engine), 0); // 偽のUSIオプションは追加しない
        settings.endArray();
        QVERIFY(!EnginePonderSettings::load(QStringLiteral("AnotherEngine")).enabled);
        QVERIFY(EnginePonderSettings::load(QStringLiteral("AnotherEngine")).sendUnreportedOption);
    }

    void cancelDoesNotSave()
    {
        saveLegacyOption(false, true);
        ChangeEngineSettingsDialog dialog;
        setup(dialog);
        widget<QCheckBox>(dialog, "ponderEnabledCheckBox")->setChecked(true);
        widget<QCheckBox>(dialog, "sendUnreportedPonderCheckBox")->setChecked(false);
        clickButton(dialog, QDialogButtonBox::Cancel);
        const auto prefs = EnginePonderSettings::load(m_engine);
        QVERIFY(!prefs.enabled);
        QVERIFY(prefs.sendUnreportedOption);
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.beginGroup(m_engine);
        QVERIFY(!settings.contains(EngineGuiPonderEnabledKey));
    }

    void restoreDefaults_data()
    {
        QTest::addColumn<bool>("hasOption");
        QTest::newRow("reported") << true;
        QTest::newRow("unreported") << false;
    }

    void restoreDefaults()
    {
        QFETCH(bool, hasOption);
        if (hasOption) saveLegacyOption(false, true);
        EnginePonderSettings::save(m_engine, !hasOption, false);
        ChangeEngineSettingsDialog dialog;
        setup(dialog);
        widget<QPushButton>(dialog, "restoreButton")->click();
        QCOMPARE(widget<QCheckBox>(dialog, "ponderEnabledCheckBox")->isChecked(), hasOption);
        QVERIFY(widget<QCheckBox>(dialog, "sendUnreportedPonderCheckBox")->isChecked());
        // 「既定値に戻す」だけでは保存しない。
        QCOMPARE(EnginePonderSettings::load(m_engine).enabled, !hasOption);
        clickButton(dialog, QDialogButtonBox::Ok);
        QCOMPARE(EnginePonderSettings::load(m_engine).enabled, hasOption);
        QVERIFY(EnginePonderSettings::load(m_engine).sendUnreportedOption);
    }

    void guiSettingOverridesStaleLegacyValue()
    {
        EnginePonderSettings::save(m_engine, true, false);
        saveLegacyOption(false, false);
        QVERIFY(EnginePonderSettings::load(m_engine).enabled);
        // 登録削除と同じグループ削除でGUI設定も消える。
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.remove(m_engine);
        settings.sync();
        QVERIFY(!EnginePonderSettings::load(m_engine).enabled);
        QVERIFY(EnginePonderSettings::load(m_engine).sendUnreportedOption);
    }
};

QTEST_MAIN(TestEnginePonderSettings)
#include "tst_enginepondersettings.moc"
