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
#include "josekiplayoptionsdialog.h"
#include "josekisettings.h"
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

    /// 定跡の条件のダイアログで手数を20にして閉じる
    void setJosekiMaxPly20()
    {
        auto* dialog = qobject_cast<JosekiPlayOptionsDialog*>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->findChild<QSpinBox*>(QStringLiteral("josekiMaxPly"))->setValue(20);
        dialog->accept();
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
        QVERIFY(dialog.data().players[1].isHuman());
        QVERIFY(dialog.data().players[1].engine.name.isEmpty());
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
        QVERIFY(dialog.data().players[player == 1 ? 0 : 1].isHuman());
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
        QCOMPARE(reopened.data().players[1].engine.name, QStringLiteral("second"));
        QCOMPARE(reopened.data().players[1].engine.path, QStringLiteral("/engines/second"));
    }

    /// 「定跡を使う」はエンジンの対局者だけに出し、定跡を開いていなければ押せない（チェックは残す）。
    /// 条件はダイアログで選んで保存し、先後の入れ替えで一緒に入れ替える
    void josekiOptionsPerEngine()
    {
        setEngines({QStringLiteral("first")});
        {
            StartGameDialog dialog;
            auto* use1 = widget<QCheckBox>(dialog, "checkBoxUseJoseki1");
            auto* use2 = widget<QCheckBox>(dialog, "checkBoxUseJoseki2");
            QVERIFY(!use1->isVisibleTo(&dialog));
            QVERIFY(use2->isVisibleTo(&dialog));
            QVERIFY(!use2->isEnabled());
            use2->setChecked(true);
            QVERIFY(!dialog.data().players[1].joseki.enabled);
            dialog.setJosekiBookName(QStringLiteral("book.db"));
            QVERIFY(use2->isEnabled());
            QVERIFY(use2->text().contains(QStringLiteral("book.db")));
            QVERIFY(dialog.data().players[1].joseki.enabled);
            QTimer::singleShot(0, this, &TestStartGameDialog::setJosekiMaxPly20);
            widget<QPushButton>(dialog, "pushButtonJosekiConditions2")->click();
            QCOMPARE(dialog.data().players[1].joseki.maxPly, 20);
            widget<QSpinBox>(dialog, "byoyomiSec1")->setValue(30);
            accept(dialog);
        }
        QVERIFY(JosekiSettings::playOptions2().enabled);
        QCOMPARE(JosekiSettings::playOptions2().maxPly, 20);
        StartGameDialog reopened;
        QVERIFY(widget<QCheckBox>(reopened, "checkBoxUseJoseki2")->isChecked());
        widget<QPushButton>(reopened, "pushButtonSwapSides")->click();
        QVERIFY(widget<QCheckBox>(reopened, "checkBoxUseJoseki1")->isChecked());
        QVERIFY(!widget<QCheckBox>(reopened, "checkBoxUseJoseki2")->isChecked());
        reopened.setJosekiBookName(QStringLiteral("book.db"));
        QCOMPARE(reopened.data().players[0].joseki.maxPly, 20);
    }

    void resetRestoresCommonTime()
    {
        StartGameDialog dialog;
        widget<QGroupBox>(dialog, "groupBoxSecondPlayerTimeSettings")->setChecked(true);
        widget<QPushButton>(dialog, "pushButtonResetToDefault")->click();
        QVERIFY(!widget<QGroupBox>(dialog, "groupBoxSecondPlayerTimeSettings")->isChecked());
        widget<QSpinBox>(dialog, "basicTimeMinutes1")->setValue(5);
        accept(dialog);
        QCOMPARE(dialog.data().players[1].time.mainMs, 5 * 60 * 1000);
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
            const auto seconds = [increment](const PlayerChoice& player) {
                return (increment ? player.time.incrementMs : player.time.byoyomiMs) / 1000;
            };
            QCOMPARE(seconds(dialog.data().players[0]), 60);
            QCOMPARE(seconds(dialog.data().players[1]), 120);
            QVERIFY(!dialog.data().isLoseOnTimeout);
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
            QVERIFY(dialog.data().autoSaveKifu);
            QCOMPARE(dialog.data().kifuSaveDir, path);
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
            QCOMPARE(reopened.data().consecutiveGames, 2);
            QVERIFY(reopened.data().isSwitchTurnEachGame);
        }
        count->setValue(1);
        QVERIFY(!toggle->isEnabled());
        accept(dialog);
        QVERIFY(!dialog.data().isSwitchTurnEachGame);
        count->setValue(2);
        QVERIFY(toggle->isChecked()); // 一時的に無効にしても選択は保持する。
        widget<QComboBox>(dialog, "comboBoxPlayer2")->setCurrentIndex(0);
        QCOMPARE(count->value(), 1);
        QVERIFY(!toggle->isEnabled());
        accept(dialog);
        QCOMPARE(dialog.data().consecutiveGames, 1);
        QVERIFY(!dialog.data().isSwitchTurnEachGame);
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
        // 列は 時間・分・秒読み（秒）・加算（秒）
        const auto side = [](const QList<int>& values) {
            TimeControlSide time;
            time.mainMs = (values.at(0) * 3600LL + values.at(1) * 60LL) * 1000;
            time.byoyomiMs = values.at(2) * 1000LL;
            time.incrementMs = values.at(3) * 1000LL;
            return time;
        };
        QCOMPARE(TimeControlValidator::validate(side(black), side(white), hasEngine), expected);
    }

    /// 時間を指定する（側は 1=先手、2=後手）
    static void setTime(StartGameDialog& dialog, int side, int minutes, int byoyomi, int increment)
    {
        const QString n = QString::number(side);
        widget<QSpinBox>(dialog, ("basicTimeHour" + n).toLatin1().constData())->setValue(0);
        widget<QSpinBox>(dialog, ("basicTimeMinutes" + n).toLatin1().constData())->setValue(minutes);
        widget<QSpinBox>(dialog, ("byoyomiSec" + n).toLatin1().constData())->setValue(byoyomi);
        widget<QSpinBox>(dialog, ("addEachMoveSec" + n).toLatin1().constData())->setValue(increment);
    }

    /// 初回に開いたときと「既定に戻す」で同じ対局条件になる（時間は全項目0秒）
    void firstLoadMatchesResetDefaults()
    {
        setEngines({QStringLiteral("engine")});
        StartGameDialog fresh;
        const auto snapshot = [](StartGameDialog& d) {
            QList<int> values;
            for (const char* name : {"basicTimeHour1", "basicTimeMinutes1", "byoyomiSec1", "addEachMoveSec1",
                                     "basicTimeHour2", "basicTimeMinutes2", "byoyomiSec2", "addEachMoveSec2",
                                     "spinBoxMaxMoves", "spinBoxConsecutiveGames"}) {
                values << widget<QSpinBox>(d, name)->value();
            }
            values << widget<QComboBox>(d, "comboBoxPlayer1")->currentIndex()
                   << widget<QComboBox>(d, "comboBoxPlayer2")->currentIndex()
                   << widget<QComboBox>(d, "comboBoxStartingPosition")->currentIndex()
                   << int(widget<QGroupBox>(d, "groupBoxSecondPlayerTimeSettings")->isChecked());
            return values;
        };
        const QList<int> firstLoad = snapshot(fresh);
        widget<QPushButton>(fresh, "pushButtonResetToDefault")->click();
        QCOMPARE(snapshot(fresh), firstLoad);
        QCOMPARE(widget<QComboBox>(fresh, "comboBoxPlayer2")->currentIndex(), 1);
        // 既定の時間は全項目0秒（仕様書「対局開始時の時間設定の確認」）。後手がエンジンのまま
        // 開始しようとすると、エンジンが参加する時間無制限として警告する
        const QString warning = acceptExpectingWarning(fresh);
        QVERIFY2(warning.contains(QStringLiteral("エンジン")), qPrintable(warning));
        QVERIFY(fresh.result() != QDialog::Accepted);
    }

    void unlimitedEngineGameIsNotStarted()
    {
        setEngines({QStringLiteral("engine")});
        StartGameDialog dialog;
        widget<QComboBox>(dialog, "comboBoxPlayer2")->setCurrentIndex(1);
        setTime(dialog, 1, 0, 0, 0);
        const QString warning = acceptExpectingWarning(dialog);
        QVERIFY2(warning.contains(QStringLiteral("エンジン")), qPrintable(warning));
        QVERIFY(dialog.result() != QDialog::Accepted);
        // 開始しなかった設定は保存しない。
        QVERIFY(!SettingsCommon::openSettings().contains(QStringLiteral("GameSettings/isHuman2")));

        widget<QSpinBox>(dialog, "byoyomiSec1")->setValue(30);
        QVERIFY(acceptExpectingWarning(dialog).isEmpty());
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(dialog.data().players[1].isEngine());
        QCOMPARE(dialog.data().players[1].time.byoyomiMs, 30 * 1000);
    }

    void humanUnlimitedGameStarts()
    {
        StartGameDialog dialog;
        setTime(dialog, 1, 0, 0, 0);
        QVERIFY(acceptExpectingWarning(dialog).isEmpty());
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QVERIFY(!dialog.data().players[0].time.hasLimit());
    }

    void oneSideWithoutTimeIsNotStarted()
    {
        StartGameDialog dialog;
        widget<QGroupBox>(dialog, "groupBoxSecondPlayerTimeSettings")->setChecked(true);
        setTime(dialog, 1, 0, 5, 0);
        setTime(dialog, 2, 0, 0, 0);
        const QString warning = acceptExpectingWarning(dialog);
        QVERIFY2(warning.contains(QStringLiteral("後手／上手")), qPrintable(warning));
        QVERIFY(dialog.result() != QDialog::Accepted);

        widget<QSpinBox>(dialog, "byoyomiSec2")->setValue(10);
        QVERIFY(acceptExpectingWarning(dialog).isEmpty());
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(dialog.data().players[1].time.byoyomiMs, 10 * 1000);
    }
};

QTEST_MAIN(TestStartGameDialog)
#include "tst_startgamedialog.moc"
