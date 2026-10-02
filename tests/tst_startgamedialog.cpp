/// @file tst_startgamedialog.cpp
/// @brief 対局ダイアログの入力・設定復元・保存の回帰テスト
#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>

#include "startgamedialog.h"
#include "settingscommon.h"

class TestStartGameDialog : public QObject
{
    Q_OBJECT

    QTemporaryDir m_config;

    template<typename T>
    static T* widget(StartGameDialog& dialog, const char* name)
    {
        auto* result = dialog.findChild<T*>(QLatin1String(name));
        Q_ASSERT(result);
        return result;
    }

    static void accept(StartGameDialog& dialog)
    {
        widget<QDialogButtonBox>(dialog, "buttonBox")->button(QDialogButtonBox::Ok)->click();
    }

    static void setEngines(const QStringList& names)
    {
        auto& settings = SettingsCommon::openSettings();
        settings.remove(QStringLiteral("Engines"));
        settings.beginWriteArray(QStringLiteral("Engines"));
        for (qsizetype i = 0; i < names.size(); ++i) {
            settings.setArrayIndex(static_cast<int>(i));
            settings.setValue(QStringLiteral("name"), names.at(i));
            settings.setValue(QStringLiteral("path"), QStringLiteral("/engines/") + names.at(i));
        }
        settings.endArray();
        settings.sync();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("StartGameDialogTest"));
    }

    void init()
    {
        auto& settings = SettingsCommon::openSettings();
        settings.clear();
        settings.sync();
    }

    void missingEngineFallsBackToHuman()
    {
        StartGameDialog dialog;
        QCOMPARE(widget<QComboBox>(dialog, "comboBoxPlayer2")->currentIndex(), 0);
        accept(dialog);
        QVERIFY(dialog.isHuman2());
        QVERIFY(!dialog.isEngine2());
        QCOMPARE(dialog.engineNumber2(), -1);
    }

    void deletedEngineFallsBackToHuman_data()
    {
        QTest::addColumn<int>("player");
        QTest::newRow("black") << 1;
        QTest::newRow("white") << 2;
    }

    void deletedEngineFallsBackToHuman()
    {
        QFETCH(int, player);
        setEngines({QStringLiteral("remaining")});
        auto& settings = SettingsCommon::openSettings();
        const QString suffix = QString::number(player);
        settings.setValue(QStringLiteral("GameSettings/isHuman") + suffix, false);
        settings.setValue(QStringLiteral("GameSettings/engineNumber") + suffix, 7);
        settings.setValue(QStringLiteral("GameSettings/engineName") + suffix, QStringLiteral("deleted"));
        settings.sync();
        StartGameDialog dialog;
        accept(dialog);
        QVERIFY(player == 1 ? dialog.isHuman1() : dialog.isHuman2());
    }

    void engineSelectionSurvivesRegistrationOrderChange()
    {
        setEngines({QStringLiteral("first"), QStringLiteral("second")});
        {
            StartGameDialog dialog;
            widget<QComboBox>(dialog, "comboBoxPlayer2")->setCurrentIndex(2);
            accept(dialog);
        }
        setEngines({QStringLiteral("second")});
        StartGameDialog reopened;
        accept(reopened);
        QCOMPARE(reopened.engineName2(), QStringLiteral("second"));
        QCOMPARE(reopened.engineNumber2(), 0);
    }

    void resetRestoresCommonTime()
    {
        StartGameDialog dialog;
        widget<QGroupBox>(dialog, "groupBoxSecondPlayerTimeSettings")->setChecked(true);
        widget<QPushButton>(dialog, "pushButtonResetToDefault")->click();
        QVERIFY(!widget<QGroupBox>(dialog, "groupBoxSecondPlayerTimeSettings")->isChecked());
        widget<QSpinBox>(dialog, "basicTimeMinutes1")->setValue(5);
        accept(dialog);
        QCOMPARE(dialog.basicTimeMinutes2(), 5);
    }

    void minuteOrLongerTimeSurvivesSave_data()
    {
        QTest::addColumn<bool>("increment");
        QTest::newRow("byoyomi") << false;
        QTest::newRow("increment") << true;
    }

    void minuteOrLongerTimeSurvivesSave()
    {
        QFETCH(bool, increment);
        const char* first = increment ? "addEachMoveSec1" : "byoyomiSec1";
        const char* second = increment ? "addEachMoveSec2" : "byoyomiSec2";
        {
            StartGameDialog dialog;
            widget<QGroupBox>(dialog, "groupBoxSecondPlayerTimeSettings")->setChecked(true);
            widget<QSpinBox>(dialog, first)->setValue(60);
            widget<QSpinBox>(dialog, second)->setValue(120);
            widget<QCheckBox>(dialog, "checkBoxLoseOnTimeOut")->setChecked(false);
            accept(dialog);
            QCOMPARE(increment ? dialog.addEachMoveSec1() : dialog.byoyomiSec1(), 60);
            QCOMPARE(increment ? dialog.addEachMoveSec2() : dialog.byoyomiSec2(), 120);
            QVERIFY(!dialog.isLoseOnTimeout());
        }
        StartGameDialog reopened;
        QCOMPARE(widget<QSpinBox>(reopened, first)->value(), 60);
        QCOMPARE(widget<QSpinBox>(reopened, second)->value(), 120);
        QVERIFY(!widget<QCheckBox>(reopened, "checkBoxLoseOnTimeOut")->isChecked());
    }

    void saveOnlyAndCancelHaveSeparateEffects()
    {
        StartGameDialog dialog;
        dialog.show();
        widget<QSpinBox>(dialog, "basicTimeMinutes1")->setValue(5);
        widget<QPushButton>(dialog, "pushButtonSaveSettingsOnly")->click();
        QVERIFY(dialog.isVisible());
        QVERIFY(!widget<QLabel>(dialog, "labelSaveStatus")->text().isEmpty());
        widget<QSpinBox>(dialog, "basicTimeMinutes1")->setValue(12);
        widget<QDialogButtonBox>(dialog, "buttonBox")->button(QDialogButtonBox::Cancel)->click();
        StartGameDialog reopened;
        QCOMPARE(widget<QSpinBox>(reopened, "basicTimeMinutes1")->value(), 5);
    }

    void autoSaveControlsFollowSettingAndRestore()
    {
        const QString path = m_config.path();
        {
            StartGameDialog dialog;
            auto* toggle = widget<QCheckBox>(dialog, "checkBoxAutoSaveKifu");
            auto* directory = widget<QLineEdit>(dialog, "lineEditKifuSaveDir");
            auto* browse = widget<QPushButton>(dialog, "pushButtonSelectKifuDir");
            QVERIFY(!directory->isEnabled());
            QVERIFY(!browse->isEnabled());
            toggle->setChecked(true);
            QVERIFY(directory->isEnabled());
            QVERIFY(browse->isEnabled());
            directory->setText(path);
            toggle->setChecked(false);
            QCOMPARE(directory->text(), path);
            toggle->setChecked(true);
            accept(dialog);
            QVERIFY(dialog.isAutoSaveKifu());
            QCOMPARE(dialog.kifuSaveDir(), path);
        }
        StartGameDialog reopened;
        QVERIFY(widget<QPushButton>(reopened, "pushButtonSelectKifuDir")->isEnabled());
        QCOMPARE(widget<QLineEdit>(reopened, "lineEditKifuSaveDir")->text(), path);
        widget<QPushButton>(reopened, "pushButtonResetToDefault")->click();
        QVERIFY(!widget<QPushButton>(reopened, "pushButtonSelectKifuDir")->isEnabled());
    }

    void turnSwitchRequiresMultipleEngineGames()
    {
        setEngines({QStringLiteral("engine")});
        StartGameDialog dialog;
        auto* count = widget<QSpinBox>(dialog, "spinBoxConsecutiveGames");
        auto* toggle = widget<QCheckBox>(dialog, "checkBoxSwitchTurnEachGame");
        QVERIFY(!count->isEnabled());
        QVERIFY(!toggle->isEnabled());
        widget<QComboBox>(dialog, "comboBoxPlayer1")->setCurrentIndex(1);
        QVERIFY(count->isEnabled());
        count->setValue(0);
        QCOMPARE(count->value(), 1);
        QVERIFY(!toggle->isEnabled());
        count->setValue(2);
        QVERIFY(toggle->isEnabled());
        toggle->setChecked(true);
        widget<QPushButton>(dialog, "pushButtonSaveSettingsOnly")->click();
        {
            StartGameDialog reopened;
            accept(reopened);
            QCOMPARE(reopened.consecutiveGames(), 2);
            QVERIFY(reopened.isSwitchTurnEachGame());
        }
        count->setValue(1);
        QVERIFY(!toggle->isEnabled());
        accept(dialog);
        QVERIFY(!dialog.isSwitchTurnEachGame());
        count->setValue(2);
        QVERIFY(toggle->isChecked()); // 一時的に無効にしても選択は保持する。
        widget<QComboBox>(dialog, "comboBoxPlayer2")->setCurrentIndex(0);
        QCOMPARE(count->value(), 1);
        QVERIFY(!toggle->isEnabled());
        accept(dialog);
        QCOMPARE(dialog.consecutiveGames(), 1);
        QVERIFY(!dialog.isSwitchTurnEachGame());
    }
};

QTEST_MAIN(TestStartGameDialog)
#include "tst_startgamedialog.moc"
