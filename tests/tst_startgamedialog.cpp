/// @file tst_startgamedialog.cpp
/// @brief 対局ダイアログの入力・設定復元・保存の回帰テスト
#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>

#include "startgamedialog.h"
#include "settingscommon.h"
#include "timecontrolvalidator.h"

Q_DECLARE_METATYPE(TimeControlValidator::Issue)

class TestStartGameDialog : public QObject
{
    Q_OBJECT

    QTemporaryDir m_config;
    QString m_warningText;      ///< 表示された警告の本文
    int m_warningAttempts = 0;  ///< 警告を待つ残り回数

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

    /// 表示中の警告の本文を記録してから閉じる
    void closeWarning()
    {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) {
            if (--m_warningAttempts > 0)
                QTimer::singleShot(10, this, &TestStartGameDialog::closeWarning);
            return;
        }
        m_warningText = box->text();
        box->button(QMessageBox::Ok)->click();
    }

    /// 対局開始を押し、時間設定の警告が出たらその本文を返す（出なければ空）
    QString acceptExpectingWarning(StartGameDialog& dialog)
    {
        m_warningText.clear();
        m_warningAttempts = 100;
        QTimer::singleShot(0, this, &TestStartGameDialog::closeWarning);
        accept(dialog);
        m_warningAttempts = 0;
        return m_warningText;
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
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
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
        // 後手の既定は登録済みのエンジンなので、時間無制限にならないよう秒読みを設定する。
        widget<QSpinBox>(dialog, "byoyomiSec1")->setValue(30);
        accept(dialog);
        QVERIFY(player == 1 ? dialog.isHuman1() : dialog.isHuman2());
    }

    void engineSelectionSurvivesRegistrationOrderChange()
    {
        setEngines({QStringLiteral("first"), QStringLiteral("second")});
        {
            StartGameDialog dialog;
            widget<QComboBox>(dialog, "comboBoxPlayer2")->setCurrentIndex(2);
            widget<QSpinBox>(dialog, "byoyomiSec1")->setValue(30);
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
        widget<QSpinBox>(dialog, "byoyomiSec1")->setValue(30);
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

    void timeControlValidation_data()
    {
        using TimeControlValidator::Issue;
        QTest::addColumn<QList<int>>("black");  // 時間, 分, 秒読み, 加算
        QTest::addColumn<QList<int>>("white");
        QTest::addColumn<bool>("hasEngine");
        QTest::addColumn<Issue>("expected");
        QTest::newRow("human-unlimited") << QList<int>{0, 0, 0, 0} << QList<int>{0, 0, 0, 0}
                                         << false << Issue::None;
        QTest::newRow("engine-unlimited") << QList<int>{0, 0, 0, 0} << QList<int>{0, 0, 0, 0}
                                          << true << Issue::EngineWithoutLimit;
        QTest::newRow("engine-byoyomi") << QList<int>{0, 0, 5, 0} << QList<int>{0, 0, 5, 0}
                                        << true << Issue::None;
        QTest::newRow("engine-increment") << QList<int>{0, 0, 0, 10} << QList<int>{0, 0, 0, 10}
                                          << true << Issue::None;
        QTest::newRow("hours-and-minutes") << QList<int>{1, 0, 0, 0} << QList<int>{0, 30, 0, 0}
                                           << true << Issue::None;
        QTest::newRow("base-against-byoyomi") << QList<int>{0, 10, 0, 0} << QList<int>{0, 0, 30, 0}
                                              << false << Issue::None;
        QTest::newRow("white-without-time") << QList<int>{0, 0, 5, 0} << QList<int>{0, 0, 0, 0}
                                            << false << Issue::Player2HasNoTime;
        QTest::newRow("black-without-time") << QList<int>{0, 0, 0, 0} << QList<int>{0, 5, 0, 0}
                                            << true << Issue::Player1HasNoTime;
        // 秒読みがあると加算は使わないので、加算だけの側は時間なしと同じ。
        QTest::newRow("increment-ignored-by-byoyomi") << QList<int>{0, 0, 5, 0} << QList<int>{0, 0, 0, 10}
                                                      << false << Issue::Player2HasNoTime;
    }

    void timeControlValidation()
    {
        QFETCH(QList<int>, black);
        QFETCH(QList<int>, white);
        QFETCH(bool, hasEngine);
        QFETCH(TimeControlValidator::Issue, expected);
        const TimeControlValidator::PlayerTime p1{black.at(0), black.at(1), black.at(2), black.at(3)};
        const TimeControlValidator::PlayerTime p2{white.at(0), white.at(1), white.at(2), white.at(3)};
        QCOMPARE(TimeControlValidator::validate(p1, p2, hasEngine), expected);
    }

    void unlimitedEngineGameIsNotStarted()
    {
        setEngines({QStringLiteral("engine")});
        StartGameDialog dialog;
        widget<QComboBox>(dialog, "comboBoxPlayer2")->setCurrentIndex(1);
        const QString warning = acceptExpectingWarning(dialog);
        QVERIFY2(warning.contains(QStringLiteral("エンジン")), qPrintable(warning));
        QVERIFY(dialog.result() != QDialog::Accepted);
        // 開始しなかった設定は保存しない。
        QVERIFY(!SettingsCommon::openSettings().contains(QStringLiteral("GameSettings/isHuman2")));

        widget<QSpinBox>(dialog, "byoyomiSec1")->setValue(30);
        QVERIFY(acceptExpectingWarning(dialog).isEmpty());
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(dialog.isEngine2());
        QCOMPARE(dialog.byoyomiSec2(), 30);
    }

    void humanUnlimitedGameStarts()
    {
        StartGameDialog dialog;
        QVERIFY(acceptExpectingWarning(dialog).isEmpty());
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.byoyomiSec1() + dialog.addEachMoveSec1() + dialog.basicTimeMinutes1(), 0);
    }

    void oneSideWithoutTimeIsNotStarted()
    {
        StartGameDialog dialog;
        widget<QGroupBox>(dialog, "groupBoxSecondPlayerTimeSettings")->setChecked(true);
        widget<QSpinBox>(dialog, "byoyomiSec1")->setValue(5);
        const QString warning = acceptExpectingWarning(dialog);
        QVERIFY2(warning.contains(QStringLiteral("後手／上手")), qPrintable(warning));
        QVERIFY(dialog.result() != QDialog::Accepted);

        widget<QSpinBox>(dialog, "byoyomiSec2")->setValue(10);
        QVERIFY(acceptExpectingWarning(dialog).isEmpty());
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.byoyomiSec2(), 10);
    }
};

QTEST_MAIN(TestStartGameDialog)
#include "tst_startgamedialog.moc"
