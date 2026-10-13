/// @file startgamedialog_settings.cpp
/// @brief 対局開始ダイアログ - 設定保存・読込・リセット

#include "startgamedialog.h"
#include "ui_startgamedialog.h"
#include "settingscommon.h"

#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr auto kGameSettingsGroup = "GameSettings";
constexpr auto kKeyStartingPositionName = "startingPositionName";
constexpr auto kKeyStartingPositionNumber = "startingPositionNumber";

/// 対局条件の既定値。初回の読み込みと「既定に戻す」の両方で使う
namespace Defaults {
constexpr int kBasicTimeHour = 0;
constexpr int kBasicTimeMinutes = 0;
constexpr int kByoyomiSec = 0;
constexpr int kAddEachMoveSec = 0;
constexpr int kStartingPositionIndex = 1;  // 平手
constexpr int kMaxMoves = 1000;
constexpr int kConsecutiveGames = 1;
constexpr bool kShowHumanInFront = true;
constexpr bool kAutoSaveKifu = false;
constexpr bool kLoseOnTimeout = true;
constexpr bool kSwitchTurnEachGame = false;
constexpr int kJishogiRule = 0;
constexpr bool kSeparateSecondPlayerTime = false;
} // namespace Defaults

QString defaultKifuSaveDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

int savedPlayerIndex(const QSettings& settings, const QList<StartGameDialog::Engine>& engines,
                     int player, bool defaultHuman)
{
    const QString suffix = QString::number(player);
    if (settings.value(QStringLiteral("isHuman") + suffix, defaultHuman).toBool()) return 0;

    const int number = settings.value(QStringLiteral("engineNumber") + suffix, 0).toInt();
    const QString name = settings.value(QStringLiteral("engineName") + suffix).toString();
    const QString path = settings.value(QStringLiteral("enginePath") + suffix).toString();
    const auto matches = [&](const StartGameDialog::Engine& engine) {
        return !path.isEmpty() ? engine.path == path : engine.name == name;
    };
    if (number >= 0 && number < engines.size()
        && ((name.isEmpty() && path.isEmpty()) || matches(engines.at(number)))) {
        return number + 1;
    }
    // 登録順が変わっても同じエンジンを選び、削除済みなら人間へ戻す。
    for (qsizetype i = 0; i < engines.size(); ++i) {
        if (matches(engines.at(i))) return static_cast<int>(i) + 1;
    }
    return 0;
}
} // namespace

// ============================================================
// 設定保存・読込
// ============================================================

void StartGameDialog::saveGameSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(kGameSettingsGroup);

    // 先手／下手の設定を保存
    // インデックス0は「人間」、1以上はエンジン
    int player1Index = ui->comboBoxPlayer1->currentIndex();
    bool isHuman1 = (player1Index <= 0);
    settings.setValue("isHuman1", isHuman1);
    settings.setValue("isEngine1", !isHuman1);
    settings.setValue("humanName1", ui->lineEditHumanName1->text());
    if (!isHuman1) {
        // エンジンの場合、インデックスから1を引いてエンジン番号を算出
        settings.setValue("engineNumber1", player1Index - 1);
        settings.setValue("engineName1", ui->comboBoxPlayer1->currentText());
        settings.setValue("enginePath1", m_engineList.at(player1Index - 1).path);
    }
    settings.setValue("basicTimeHour1", ui->basicTimeHour1->value());
    settings.setValue("basicTimeMinutes1", ui->basicTimeMinutes1->value());
    settings.setValue("byoyomiSec1", ui->byoyomiSec1->value());
    settings.setValue("addEachMoveSec1", ui->addEachMoveSec1->value());

    // 後手／上手の設定を保存
    settings.setValue("isGroupBoxSecondPlayerTimeSettingsChecked", ui->groupBoxSecondPlayerTimeSettings->isChecked());

    int player2Index = ui->comboBoxPlayer2->currentIndex();
    bool isHuman2 = (player2Index <= 0);
    settings.setValue("isHuman2", isHuman2);
    settings.setValue("isEngine2", !isHuman2);
    settings.setValue("humanName2", ui->lineEditHumanName2->text());
    if (!isHuman2) {
        // エンジンの場合、インデックスから1を引いてエンジン番号を算出
        settings.setValue("engineNumber2", player2Index - 1);
        settings.setValue("engineName2", ui->comboBoxPlayer2->currentText());
        settings.setValue("enginePath2", m_engineList.at(player2Index - 1).path);
    }
    settings.setValue("basicTimeHour2", ui->basicTimeHour2->value());
    settings.setValue("basicTimeMinutes2", ui->basicTimeMinutes2->value());
    settings.setValue("byoyomiSec2", ui->byoyomiSec2->value());
    settings.setValue("addEachMoveSec2", ui->addEachMoveSec2->value());

    settings.setValue(kKeyStartingPositionName, ui->comboBoxStartingPosition->currentText());
    settings.setValue(kKeyStartingPositionNumber, ui->comboBoxStartingPosition->currentIndex());
    settings.setValue("maxMoves", ui->spinBoxMaxMoves->value());
    settings.setValue("consecutiveGames", ui->spinBoxConsecutiveGames->value());
    settings.setValue("isShowHumanInFront", ui->checkBoxShowHumanInFront->isChecked());
    settings.setValue("isAutoSaveKifu", ui->checkBoxAutoSaveKifu->isChecked());
    settings.setValue("kifuSaveDir", ui->lineEditKifuSaveDir->text());
    settings.setValue("isLoseOnTimeout", ui->checkBoxLoseOnTimeOut->isChecked());
    settings.setValue("isSwitchTurnEachGame", ui->checkBoxSwitchTurnEachGame->isChecked());
    settings.setValue("jishogiRule", ui->comboBoxJishogi->currentIndex());

    settings.endGroup();
}

void StartGameDialog::loadGameSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(kGameSettingsGroup);

    // 先手／下手の設定を読み込む
    ui->lineEditHumanName1->setText(settings.value("humanName1", tr("You")).toString());
    ui->comboBoxPlayer1->setCurrentIndex(savedPlayerIndex(settings, m_engineList, 1, true));
    updatePlayerUI(1, ui->comboBoxPlayer1->currentIndex());

    ui->basicTimeHour1->setValue(settings.value("basicTimeHour1", Defaults::kBasicTimeHour).toInt());
    ui->basicTimeMinutes1->setValue(settings.value("basicTimeMinutes1", Defaults::kBasicTimeMinutes).toInt());
    ui->byoyomiSec1->setValue(settings.value("byoyomiSec1", Defaults::kByoyomiSec).toInt());
    ui->addEachMoveSec1->setValue(settings.value("addEachMoveSec1", Defaults::kAddEachMoveSec).toInt());

    // 後手／上手の設定を読み込む
    bool isChecked = settings.value("isGroupBoxSecondPlayerTimeSettingsChecked",
                                    Defaults::kSeparateSecondPlayerTime).toBool();
    ui->groupBoxSecondPlayerTimeSettings->setChecked(isChecked);

    ui->lineEditHumanName2->setText(settings.value("humanName2", tr("You")).toString());
    ui->comboBoxPlayer2->setCurrentIndex(savedPlayerIndex(settings, m_engineList, 2, false));
    updatePlayerUI(2, ui->comboBoxPlayer2->currentIndex());

    ui->basicTimeHour2->setValue(settings.value("basicTimeHour2", Defaults::kBasicTimeHour).toInt());
    ui->basicTimeMinutes2->setValue(settings.value("basicTimeMinutes2", Defaults::kBasicTimeMinutes).toInt());
    ui->byoyomiSec2->setValue(settings.value("byoyomiSec2", Defaults::kByoyomiSec).toInt());
    ui->addEachMoveSec2->setValue(settings.value("addEachMoveSec2", Defaults::kAddEachMoveSec).toInt());

    const int savedStartingIndex = settings.value(kKeyStartingPositionNumber, Defaults::kStartingPositionIndex).toInt();
    if (savedStartingIndex >= 0 && savedStartingIndex < ui->comboBoxStartingPosition->count()) {
        ui->comboBoxStartingPosition->setCurrentIndex(savedStartingIndex);
    } else {
        ui->comboBoxStartingPosition->setCurrentText(
            settings.value(kKeyStartingPositionName, QStringLiteral("平手")).toString());
    }
    ui->spinBoxMaxMoves->setValue(settings.value("maxMoves", Defaults::kMaxMoves).toInt());
    ui->spinBoxConsecutiveGames->setValue(settings.value("consecutiveGames", Defaults::kConsecutiveGames).toInt());
    ui->checkBoxShowHumanInFront->setChecked(settings.value("isShowHumanInFront", Defaults::kShowHumanInFront).toBool());
    ui->checkBoxAutoSaveKifu->setChecked(settings.value("isAutoSaveKifu", Defaults::kAutoSaveKifu).toBool());
    // デフォルトの棋譜保存先はドキュメントフォルダ
    ui->lineEditKifuSaveDir->setText(settings.value("kifuSaveDir", defaultKifuSaveDir()).toString());
    ui->checkBoxLoseOnTimeOut->setChecked(settings.value("isLoseOnTimeout", Defaults::kLoseOnTimeout).toBool());
    ui->checkBoxSwitchTurnEachGame->setChecked(settings.value("isSwitchTurnEachGame", Defaults::kSwitchTurnEachGame).toBool());
    ui->comboBoxJishogi->setCurrentIndex(settings.value("jishogiRule", Defaults::kJishogiRule).toInt());

    settings.endGroup();

    updateConsecutiveGamesEnabled();
}

void StartGameDialog::resetSettingsToDefault()
{
    ui->groupBoxSecondPlayerTimeSettings->setChecked(Defaults::kSeparateSecondPlayerTime);
    // 先手: 人間をデフォルトに
    ui->comboBoxPlayer1->setCurrentIndex(0);
    updatePlayerUI(1, 0);
    ui->lineEditHumanName1->setText(tr("You"));
    ui->basicTimeHour1->setValue(Defaults::kBasicTimeHour);
    ui->basicTimeMinutes1->setValue(Defaults::kBasicTimeMinutes);
    ui->byoyomiSec1->setValue(Defaults::kByoyomiSec);
    ui->addEachMoveSec1->setValue(Defaults::kAddEachMoveSec);

    // 後手: 最初のエンジンをデフォルトに（なければ人間）
    if (ui->comboBoxPlayer2->count() > 1) {
        ui->comboBoxPlayer2->setCurrentIndex(1);
        updatePlayerUI(2, 1);
    } else {
        ui->comboBoxPlayer2->setCurrentIndex(0);
        updatePlayerUI(2, 0);
    }
    ui->lineEditHumanName2->setText(tr("You"));
    ui->basicTimeHour2->setValue(Defaults::kBasicTimeHour);
    ui->basicTimeMinutes2->setValue(Defaults::kBasicTimeMinutes);
    ui->byoyomiSec2->setValue(Defaults::kByoyomiSec);
    ui->addEachMoveSec2->setValue(Defaults::kAddEachMoveSec);

    ui->comboBoxStartingPosition->setCurrentIndex(Defaults::kStartingPositionIndex);
    ui->spinBoxMaxMoves->setValue(Defaults::kMaxMoves);
    ui->spinBoxConsecutiveGames->setValue(Defaults::kConsecutiveGames);
    ui->checkBoxShowHumanInFront->setChecked(Defaults::kShowHumanInFront);
    ui->checkBoxAutoSaveKifu->setChecked(Defaults::kAutoSaveKifu);
    ui->lineEditKifuSaveDir->setText(defaultKifuSaveDir());
    ui->checkBoxLoseOnTimeOut->setChecked(Defaults::kLoseOnTimeout);
    ui->checkBoxSwitchTurnEachGame->setChecked(Defaults::kSwitchTurnEachGame);
    ui->comboBoxJishogi->setCurrentIndex(Defaults::kJishogiRule);
}
