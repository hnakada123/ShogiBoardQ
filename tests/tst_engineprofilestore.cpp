/// @file tst_engineprofilestore.cpp
/// @brief 登録エンジンの設定（EngineProfileStore）の保存形式・先読みの設定・旧設定の移行・設定画面のテスト

#include <QtTest>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>

#include "changeenginesettingsdialog.h"
#include "enginelistsettings.h"
#include "engineprofilestore.h"
#include "engine_profile_format_helper.h"
#include "longlongspinbox.h"
#include "settingscommon.h"

using namespace EngineProfileFormat;

class TestEngineProfileStore : public QObject
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

    EngineEntry engine() const { return {m_engine, QString(), QString()}; }

    static void clickButton(ChangeEngineSettingsDialog& dialog, QDialogButtonBox::StandardButton button)
    {
        widget<QDialogButtonBox>(dialog, "buttonBox")->button(button)->click();
    }

    void saveLegacyOption(bool enabled, bool defaultEnabled)
    {
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.beginWriteArray(engineOptionsGroup(m_engine), 2);
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
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("EnginePonderSettingsTest"));
    }

    void init()
    {
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.clear();
    }

    /// 起動時に送る設定: USI_Ponder は先読みの設定で送り、button 型は on のものだけを値なしで送る
    void launchOptionsFollowStoredOptions()
    {
        saveLegacyOption(true, false);
        QList<EngineOption> options = EngineProfileStore::loadOptions(m_engine);
        options.append({QStringLiteral("ClearHash"), QStringLiteral("button"), {}, {}, {}, QStringLiteral("on"), {}});
        options.append({QStringLiteral("Reset"), QStringLiteral("button"), {}, {}, {}, QString(), {}});
        EngineProfileStore::saveOptions(m_engine, options);
        EngineProfileStore::savePonder(m_engine, false, false);

        const EngineProfileStore::LaunchOptions launch = EngineProfileStore::launchOptions(m_engine);
        QCOMPARE(launch.options.size(), 2);
        QCOMPARE(launch.options.at(0).name, QStringLiteral("Threads"));
        QCOMPARE(launch.options.at(0).value, QStringLiteral("4"));
        QVERIFY(!launch.options.at(0).button);
        QCOMPARE(launch.options.at(1).name, QStringLiteral("ClearHash"));
        QVERIFY(launch.options.at(1).button);
        QVERIFY(!launch.ponderEnabled);
        QVERIFY(!launch.sendUnreportedPonder);
    }

    /// オプションを書き直しても、短くなった配列の古い要素と GUI の設定は残らない・消えない
    void savingOptionsReplacesTheArray()
    {
        saveLegacyOption(true, false);
        EngineProfileStore::savePonder(m_engine, true, false);
        EngineProfileStore::saveOptions(m_engine, {EngineProfileStore::loadOptions(m_engine).first()});
        QCOMPARE(EngineProfileStore::loadOptions(m_engine).size(), 1);
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QVERIFY(!settings.contains(engineOptionsGroup(m_engine) + QStringLiteral("/2/name")));
        QVERIFY(!EngineProfileStore::loadPonder(m_engine).sendUnreportedOption);
        // 空白区切りの以前の選択肢も読める
        settings.setValue(engineOptionsGroup(m_engine) + QStringLiteral("/1/valueList"), QStringLiteral("Easy Normal"));
        settings.sync();
        QCOMPARE(EngineProfileStore::loadOptions(m_engine).first().valueList,
                 (QStringList{QStringLiteral("Easy"), QStringLiteral("Normal")}));
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
        ChangeEngineSettingsDialog dialog(engine());
        auto* ponder = widget<QCheckBox>(dialog, "ponderEnabledCheckBox");
        QCOMPARE(ponder->isChecked(), enabled);
        QCOMPARE(EngineProfileStore::loadPonder(m_engine).defaultEnabled, !enabled);
        // エンジンオプションとGUI設定の二重表示を避ける。
        QVERIFY(!dialog.findChild<QCheckBox*>(QStringLiteral("USI_Ponder")));
        ponder->setChecked(!enabled);
        widget<LongLongSpinBox>(dialog, "Threads")->setSpinBoxValue(8);
        clickButton(dialog, QDialogButtonBox::Ok);
        QCOMPARE(EngineProfileStore::loadPonder(m_engine).enabled, !enabled);

        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(settings.beginReadArray(engineOptionsGroup(m_engine)), 2);
        settings.setArrayIndex(0);
        QCOMPARE(settings.value(EngineOptionValueKey).toString(), QStringLiteral("8"));
        settings.setArrayIndex(1);
        QCOMPARE(settings.value(EngineOptionValueKey).toBool(), !enabled);
        settings.endArray();

        ChangeEngineSettingsDialog reopened(engine());
        QCOMPARE(widget<QCheckBox>(reopened, "ponderEnabledCheckBox")->isChecked(), !enabled);
    }

    void unreportedOptionCanBeEnabledAndSaved()
    {
        ChangeEngineSettingsDialog dialog(engine());
        auto* ponder = widget<QCheckBox>(dialog, "ponderEnabledCheckBox");
        auto* notify = widget<QCheckBox>(dialog, "sendUnreportedPonderCheckBox");
        QVERIFY(!ponder->isChecked());
        QVERIFY(notify->isChecked());
        ponder->setChecked(true);
        notify->setChecked(false);
        clickButton(dialog, QDialogButtonBox::Ok);

        ChangeEngineSettingsDialog reopened(engine());
        QVERIFY(widget<QCheckBox>(reopened, "ponderEnabledCheckBox")->isChecked());
        QVERIFY(!widget<QCheckBox>(reopened, "sendUnreportedPonderCheckBox")->isChecked());
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        QCOMPARE(settings.beginReadArray(engineOptionsGroup(m_engine)), 0); // 偽のUSIオプションは追加しない
        settings.endArray();
        QVERIFY(!EngineProfileStore::loadPonder(QStringLiteral("AnotherEngine")).enabled);
        QVERIFY(EngineProfileStore::loadPonder(QStringLiteral("AnotherEngine")).sendUnreportedOption);
    }

    void cancelDoesNotSave()
    {
        saveLegacyOption(false, true);
        ChangeEngineSettingsDialog dialog(engine());
        widget<QCheckBox>(dialog, "ponderEnabledCheckBox")->setChecked(true);
        widget<QCheckBox>(dialog, "sendUnreportedPonderCheckBox")->setChecked(false);
        clickButton(dialog, QDialogButtonBox::Cancel);
        const auto prefs = EngineProfileStore::loadPonder(m_engine);
        QVERIFY(!prefs.enabled);
        QVERIFY(prefs.sendUnreportedOption);
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.beginGroup(engineOptionsGroup(m_engine));
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
        EngineProfileStore::savePonder(m_engine, !hasOption, false);
        ChangeEngineSettingsDialog dialog(engine());
        widget<QPushButton>(dialog, "restoreButton")->click();
        QCOMPARE(widget<QCheckBox>(dialog, "ponderEnabledCheckBox")->isChecked(), hasOption);
        QVERIFY(widget<QCheckBox>(dialog, "sendUnreportedPonderCheckBox")->isChecked());
        // 「既定値に戻す」だけでは保存しない。
        QCOMPARE(EngineProfileStore::loadPonder(m_engine).enabled, !hasOption);
        clickButton(dialog, QDialogButtonBox::Ok);
        QCOMPARE(EngineProfileStore::loadPonder(m_engine).enabled, hasOption);
        QVERIFY(EngineProfileStore::loadPonder(m_engine).sendUnreportedOption);
    }

    void guiSettingOverridesStaleLegacyValue()
    {
        EngineProfileStore::savePonder(m_engine, true, false);
        saveLegacyOption(false, false);
        QVERIFY(EngineProfileStore::loadPonder(m_engine).enabled);
        // 登録削除と同じグループ削除でGUI設定も消える。
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.remove(engineOptionsGroup(m_engine));
        settings.sync();
        QVERIFY(!EngineProfileStore::loadPonder(m_engine).enabled);
        QVERIFY(EngineProfileStore::loadPonder(m_engine).sendUnreportedOption);
    }

    /// エンジンの名前がほかの設定のグループ名と同じでも、"/" を含んでも、設定は EngineOptions の下に置く（B8）
    void engineGroupDoesNotCollideWithOtherSettings_data()
    {
        QTest::addColumn<QString>("engine");
        QTest::newRow("General") << QStringLiteral("General");
        QTest::newRow("SizeRelated") << QStringLiteral("SizeRelated");
        QTest::newRow("slash") << QStringLiteral("Engine/1.0");
        QTest::newRow("percent") << QStringLiteral("100%engine\\x");
    }

    void engineGroupDoesNotCollideWithOtherSettings()
    {
        QFETCH(QString, engine);
        const QString group = engineOptionsGroup(engine);
        QVERIFY(group.startsWith(QStringLiteral("EngineOptions/")));
        QCOMPARE(group.count(QLatin1Char('/')), 1);
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.setValue(engine.section(QLatin1Char('/'), 0, 0) + QStringLiteral("/keep"), 42);
        settings.sync();

        EngineProfileStore::savePonder(engine, true, false);
        QVERIFY(EngineProfileStore::loadPonder(engine).enabled);
        settings.sync();
        QVERIFY(settings.contains(group + QLatin1Char('/') + QLatin1String(EngineGuiPonderEnabledKey)));
        // エンジンの設定を消しても、同じ名前のグループのほかの設定は残る
        settings.remove(group);
        settings.sync();
        QVERIFY(!EngineProfileStore::loadPonder(engine).enabled);
        QCOMPARE(settings.value(engine.section(QLatin1Char('/'), 0, 0) + QStringLiteral("/keep")).toInt(), 42);
    }

    /// 以前の版がエンジン名の最上位グループに置いた設定は、起動時に EngineOptions の下へ移す（B8）
    void legacyEngineGroupIsMovedOnStartup()
    {
        QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
        settings.beginWriteArray(QLatin1String(EnginesGroupName), 2);
        settings.setArrayIndex(0);
        settings.setValue(EngineNameKey, m_engine);
        settings.setArrayIndex(1);
        settings.setValue(EngineNameKey, QStringLiteral("General"));
        settings.endArray();
        // 以前の版の書き方（エンジン名のグループに配列と GUI の設定）
        settings.beginWriteArray(m_engine, 1);
        settings.setArrayIndex(0);
        settings.setValue(EngineOptionNameKey, QStringLiteral("Threads"));
        settings.setValue(EngineOptionValueKey, QStringLiteral("6"));
        settings.endArray();
        settings.setValue(m_engine + QLatin1Char('/') + QLatin1String(EngineGuiPonderEnabledKey), true);
        settings.beginWriteArray(QStringLiteral("General"), 1);
        settings.setArrayIndex(0);
        settings.setValue(EngineOptionNameKey, QStringLiteral("Hash"));
        settings.endArray();
        settings.setValue(QStringLiteral("General/language"), QStringLiteral("en"));
        settings.sync();

        QCOMPARE(EngineProfileStore::migrateLegacyOptionGroups(), 2);
        settings.sync();
        QVERIFY(!settings.contains(m_engine + QStringLiteral("/size")));
        QVERIFY(EngineProfileStore::loadPonder(m_engine).enabled);
        QCOMPARE(settings.beginReadArray(engineOptionsGroup(m_engine)), 1);
        settings.setArrayIndex(0);
        QCOMPARE(settings.value(EngineOptionValueKey).toString(), QStringLiteral("6"));
        settings.endArray();
        // 同じ名前のグループにあるアプリの設定はそのまま残す
        QCOMPARE(settings.value(QStringLiteral("General/language")).toString(), QStringLiteral("en"));
        QVERIFY(!settings.contains(QStringLiteral("General/size")));
        QCOMPARE(settings.beginReadArray(engineOptionsGroup(QStringLiteral("General"))), 1);
        settings.endArray();
        // 2回目は何も移さない
        QCOMPARE(EngineProfileStore::migrateLegacyOptionGroups(), 0);
    }
};

QTEST_MAIN(TestEngineProfileStore)
#include "tst_engineprofilestore.moc"
