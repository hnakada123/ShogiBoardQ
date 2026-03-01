/// @file startgamedialog_settings.cpp
/// @brief 対局開始ダイアログ - 設定保存・読込・リセット

#include "startgamedialog.h"
#include "ui_startgamedialog.h"
#include "settingscommon.h"

#include <QSettings>
#include <QStandardPaths>

// ============================================================
// 設定保存・読込
// ============================================================

void StartGameDialog::saveGameSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup("GameSettings");

    // 先手／下手の設定を保存
    // インデックス0は「人間」、1以上はエンジン
    int player1Index = ui->comboBoxPlayer1->currentIndex();
    bool isHuman1 = (player1Index == 0);
    settings.setValue("isHuman1", isHuman1);
    settings.setValue("isEngine1", !isHuman1);
    settings.setValue("humanName1", ui->lineEditHumanName1->text());
    if (!isHuman1) {
        // エンジンの場合、インデックスから1を引いてエンジン番号を算出
        settings.setValue("engineNumber1", player1Index - 1);
        settings.setValue("engineName1", ui->comboBoxPlayer1->currentText());
    }
    settings.setValue("basicTimeHour1", ui->basicTimeHour1->value());
    settings.setValue("basicTimeMinutes1", ui->basicTimeMinutes1->value());
    settings.setValue("byoyomiSec1", ui->byoyomiSec1->value());
    settings.setValue("addEachMoveSec1", ui->addEachMoveSec1->value());

    // 後手／上手の設定を保存
    settings.setValue("isGroupBoxSecondPlayerTimeSettingsChecked", ui->groupBoxSecondPlayerTimeSettings->isChecked());

    int player2Index = ui->comboBoxPlayer2->currentIndex();
    bool isHuman2 = (player2Index == 0);
    settings.setValue("isHuman2", isHuman2);
    settings.setValue("isEngine2", !isHuman2);
    settings.setValue("humanName2", ui->lineEditHumanName2->text());
    if (!isHuman2) {
        // エンジンの場合、インデックスから1を引いてエンジン番号を算出
        settings.setValue("engineNumber2", player2Index - 1);
        settings.setValue("engineName2", ui->comboBoxPlayer2->currentText());
    }
    settings.setValue("basicTimeHour2", ui->basicTimeHour2->value());
    settings.setValue("basicTimeMinutes2", ui->basicTimeMinutes2->value());
    settings.setValue("byoyomiSec2", ui->byoyomiSec2->value());
    settings.setValue("addEachMoveSec2", ui->addEachMoveSec2->value());

    settings.setValue("startingPositionName", ui->comboBoxStartingPosition->currentText());
    settings.setValue("startingPositionNumber", ui->comboBoxStartingPosition->currentIndex());
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
    settings.beginGroup("GameSettings");

    // 先手／下手の設定を読み込む
    bool isHuman1 = settings.value("isHuman1", true).toBool();
    ui->lineEditHumanName1->setText(settings.value("humanName1", tr("You")).toString());
    if (isHuman1) {
        ui->comboBoxPlayer1->setCurrentIndex(0);
    } else {
        // エンジン番号+1がコンボボックスのインデックスになる
        int engineNumber = settings.value("engineNumber1", 0).toInt();
        ui->comboBoxPlayer1->setCurrentIndex(engineNumber + 1);
    }
    updatePlayerUI(1, ui->comboBoxPlayer1->currentIndex());

    ui->basicTimeHour1->setValue(settings.value("basicTimeHour1", 0).toInt());
    ui->basicTimeMinutes1->setValue(settings.value("basicTimeMinutes1", 0).toInt());
    ui->byoyomiSec1->setValue(settings.value("byoyomiSec1", 0).toInt());
    ui->addEachMoveSec1->setValue(settings.value("addEachMoveSec1", 0).toInt());

    // 後手／上手の設定を読み込む
    bool isChecked = settings.value("isGroupBoxSecondPlayerTimeSettingsChecked", false).toBool();
    ui->groupBoxSecondPlayerTimeSettings->setChecked(isChecked);

    bool isHuman2 = settings.value("isHuman2", false).toBool();
    ui->lineEditHumanName2->setText(settings.value("humanName2", tr("You")).toString());
    if (isHuman2) {
        ui->comboBoxPlayer2->setCurrentIndex(0);
    } else {
        // エンジン番号+1がコンボボックスのインデックスになる
        int engineNumber = settings.value("engineNumber2", 0).toInt();
        ui->comboBoxPlayer2->setCurrentIndex(engineNumber + 1);
    }
    updatePlayerUI(2, ui->comboBoxPlayer2->currentIndex());

    ui->basicTimeHour2->setValue(settings.value("basicTimeHour2", 0).toInt());
    ui->basicTimeMinutes2->setValue(settings.value("basicTimeMinutes2", 0).toInt());
    ui->byoyomiSec2->setValue(settings.value("byoyomiSec2", 0).toInt());
    ui->addEachMoveSec2->setValue(settings.value("addEachMoveSec2", 0).toInt());

    ui->comboBoxStartingPosition->setCurrentText(settings.value("startingPositionName", "平手").toString());
    ui->comboBoxStartingPosition->setCurrentIndex(settings.value("startingPositionNumber", 1).toInt());
    ui->spinBoxMaxMoves->setValue(settings.value("maxMoves", 1000).toInt());
    ui->spinBoxConsecutiveGames->setValue(settings.value("consecutiveGames", 1).toInt());
    ui->checkBoxShowHumanInFront->setChecked(settings.value("isShowHumanInFront", true).toBool());
    ui->checkBoxAutoSaveKifu->setChecked(settings.value("isAutoSaveKifu", false).toBool());

    // デフォルトの棋譜保存先はドキュメントフォルダ
    QString defaultKifuDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    ui->lineEditKifuSaveDir->setText(settings.value("kifuSaveDir", defaultKifuDir).toString());

    ui->checkBoxLoseOnTimeOut->setChecked(settings.value("isLoseOnTimeout", true).toBool());
    ui->checkBoxSwitchTurnEachGame->setChecked(settings.value("isSwitchTurnEachGame", false).toBool());
    ui->comboBoxJishogi->setCurrentIndex(settings.value("jishogiRule", 0).toInt());

    settings.endGroup();

    updateConsecutiveGamesEnabled();
}

void StartGameDialog::resetSettingsToDefault()
{
    // 先手: 人間をデフォルトに
    ui->comboBoxPlayer1->setCurrentIndex(0);
    updatePlayerUI(1, 0);
    ui->lineEditHumanName1->setText(tr("You"));
    ui->basicTimeHour1->setValue(0);
    ui->basicTimeMinutes1->setValue(30);
    ui->byoyomiSec1->setValue(30);
    ui->addEachMoveSec1->setValue(0);

    // 後手: 最初のエンジンをデフォルトに（なければ人間）
    if (ui->comboBoxPlayer2->count() > 1) {
        ui->comboBoxPlayer2->setCurrentIndex(1);
        updatePlayerUI(2, 1);
    } else {
        ui->comboBoxPlayer2->setCurrentIndex(0);
        updatePlayerUI(2, 0);
    }
    ui->lineEditHumanName2->setText(tr("You"));
    ui->basicTimeHour2->setValue(0);
    ui->basicTimeMinutes2->setValue(30);
    ui->byoyomiSec2->setValue(30);
    ui->addEachMoveSec2->setValue(0);

    // 開始局面: 平手
    ui->comboBoxStartingPosition->setCurrentIndex(1);
    ui->spinBoxMaxMoves->setValue(1000);
    ui->spinBoxConsecutiveGames->setValue(1);
    ui->checkBoxShowHumanInFront->setChecked(true);
    ui->checkBoxAutoSaveKifu->setChecked(false);
    ui->lineEditKifuSaveDir->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    ui->checkBoxLoseOnTimeOut->setChecked(true);
    ui->checkBoxSwitchTurnEachGame->setChecked(false);
    ui->comboBoxJishogi->setCurrentIndex(0);
}
