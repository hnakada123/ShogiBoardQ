/// @file startgamedialog_settings.cpp
/// @brief 対局開始ダイアログ - 設定保存・読込・リセット

#include "startgamedialog.h"
#include "ui_startgamedialog.h"
#include "gamesettings.h"

#include <QStandardPaths>

namespace {
namespace Saved = GameSettings::StartGame;

QString defaultKifuSaveDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

int savedPlayerIndex(const Saved::Player& saved, const QList<StartGameDialog::Engine>& engines)
{
    if (saved.isHuman()) return 0;

    const int number = saved.engineNumber();
    const QString name = saved.engineName();
    const QString path = saved.enginePath();
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

/// 対局者の選択を保存する。インデックス0は「人間」、1以上はエンジン
void savePlayer(const Saved::Player& saved, int playerIndex, const QString& humanName,
                const QString& engineName, const QList<StartGameDialog::Engine>& engines)
{
    const bool isHuman = (playerIndex <= 0);
    saved.isHuman.set(isHuman);
    saved.isEngine.set(!isHuman);
    saved.humanName.set(humanName);
    if (!isHuman) {
        saved.engineNumber.set(playerIndex - 1);
        saved.engineName.set(engineName);
        saved.enginePath.set(engines.at(playerIndex - 1).path);
    }
}
} // namespace

// ============================================================
// 設定保存・読込
// ============================================================

void StartGameDialog::saveGameSettings()
{
    // 先手／下手の設定を保存
    savePlayer(Saved::player1, ui->comboBoxPlayer1->currentIndex(), ui->lineEditHumanName1->text(),
               ui->comboBoxPlayer1->currentText(), m_engineList);
    Saved::player1.basicTimeHour.set(ui->basicTimeHour1->value());
    Saved::player1.basicTimeMinutes.set(ui->basicTimeMinutes1->value());
    Saved::player1.byoyomiSec.set(ui->byoyomiSec1->value());
    Saved::player1.addEachMoveSec.set(ui->addEachMoveSec1->value());

    // 後手／上手の設定を保存
    Saved::separateSecondPlayerTime.set(ui->groupBoxSecondPlayerTimeSettings->isChecked());
    savePlayer(Saved::player2, ui->comboBoxPlayer2->currentIndex(), ui->lineEditHumanName2->text(),
               ui->comboBoxPlayer2->currentText(), m_engineList);
    Saved::player2.basicTimeHour.set(ui->basicTimeHour2->value());
    Saved::player2.basicTimeMinutes.set(ui->basicTimeMinutes2->value());
    Saved::player2.byoyomiSec.set(ui->byoyomiSec2->value());
    Saved::player2.addEachMoveSec.set(ui->addEachMoveSec2->value());

    Saved::startingPositionName.set(ui->comboBoxStartingPosition->currentText());
    Saved::startingPositionNumber.set(ui->comboBoxStartingPosition->currentIndex());
    Saved::maxMoves.set(ui->spinBoxMaxMoves->value());
    Saved::consecutiveGames.set(ui->spinBoxConsecutiveGames->value());
    Saved::showHumanInFront.set(ui->checkBoxShowHumanInFront->isChecked());
    Saved::autoSaveKifu.set(ui->checkBoxAutoSaveKifu->isChecked());
    Saved::kifuSaveDir.set(ui->lineEditKifuSaveDir->text());
    Saved::loseOnTimeout.set(ui->checkBoxLoseOnTimeOut->isChecked());
    Saved::switchTurnEachGame.set(ui->checkBoxSwitchTurnEachGame->isChecked());
    Saved::jishogiRule.set(ui->comboBoxJishogi->currentIndex());
}

void StartGameDialog::loadGameSettings()
{
    // 先手／下手の設定を読み込む
    ui->lineEditHumanName1->setText(Saved::player1.humanName.valueOr(tr("You")));
    ui->comboBoxPlayer1->setCurrentIndex(savedPlayerIndex(Saved::player1, m_engineList));
    updatePlayerUI(1, ui->comboBoxPlayer1->currentIndex());

    ui->basicTimeHour1->setValue(Saved::player1.basicTimeHour());
    ui->basicTimeMinutes1->setValue(Saved::player1.basicTimeMinutes());
    ui->byoyomiSec1->setValue(Saved::player1.byoyomiSec());
    ui->addEachMoveSec1->setValue(Saved::player1.addEachMoveSec());

    // 後手／上手の設定を読み込む
    ui->groupBoxSecondPlayerTimeSettings->setChecked(Saved::separateSecondPlayerTime());

    ui->lineEditHumanName2->setText(Saved::player2.humanName.valueOr(tr("You")));
    ui->comboBoxPlayer2->setCurrentIndex(savedPlayerIndex(Saved::player2, m_engineList));
    updatePlayerUI(2, ui->comboBoxPlayer2->currentIndex());

    ui->basicTimeHour2->setValue(Saved::player2.basicTimeHour());
    ui->basicTimeMinutes2->setValue(Saved::player2.basicTimeMinutes());
    ui->byoyomiSec2->setValue(Saved::player2.byoyomiSec());
    ui->addEachMoveSec2->setValue(Saved::player2.addEachMoveSec());

    const int savedStartingIndex = Saved::startingPositionNumber();
    if (savedStartingIndex >= 0 && savedStartingIndex < ui->comboBoxStartingPosition->count()) {
        ui->comboBoxStartingPosition->setCurrentIndex(savedStartingIndex);
    } else {
        ui->comboBoxStartingPosition->setCurrentText(Saved::startingPositionName());
    }
    ui->spinBoxMaxMoves->setValue(Saved::maxMoves());
    ui->spinBoxConsecutiveGames->setValue(Saved::consecutiveGames());
    ui->checkBoxShowHumanInFront->setChecked(Saved::showHumanInFront());
    ui->checkBoxAutoSaveKifu->setChecked(Saved::autoSaveKifu());
    // デフォルトの棋譜保存先はドキュメントフォルダ
    ui->lineEditKifuSaveDir->setText(Saved::kifuSaveDir.valueOr(defaultKifuSaveDir()));
    ui->checkBoxLoseOnTimeOut->setChecked(Saved::loseOnTimeout());
    ui->checkBoxSwitchTurnEachGame->setChecked(Saved::switchTurnEachGame());
    ui->comboBoxJishogi->setCurrentIndex(Saved::jishogiRule());

    updateConsecutiveGamesEnabled();
}

void StartGameDialog::resetSettingsToDefault()
{
    ui->groupBoxSecondPlayerTimeSettings->setChecked(Saved::separateSecondPlayerTime.defaultValue());
    // 先手: 人間をデフォルトに
    ui->comboBoxPlayer1->setCurrentIndex(0);
    updatePlayerUI(1, 0);
    ui->lineEditHumanName1->setText(tr("You"));
    ui->basicTimeHour1->setValue(Saved::player1.basicTimeHour.defaultValue());
    ui->basicTimeMinutes1->setValue(Saved::player1.basicTimeMinutes.defaultValue());
    ui->byoyomiSec1->setValue(Saved::player1.byoyomiSec.defaultValue());
    ui->addEachMoveSec1->setValue(Saved::player1.addEachMoveSec.defaultValue());

    // 後手: 最初のエンジンをデフォルトに（なければ人間）
    if (ui->comboBoxPlayer2->count() > 1) {
        ui->comboBoxPlayer2->setCurrentIndex(1);
        updatePlayerUI(2, 1);
    } else {
        ui->comboBoxPlayer2->setCurrentIndex(0);
        updatePlayerUI(2, 0);
    }
    ui->lineEditHumanName2->setText(tr("You"));
    ui->basicTimeHour2->setValue(Saved::player2.basicTimeHour.defaultValue());
    ui->basicTimeMinutes2->setValue(Saved::player2.basicTimeMinutes.defaultValue());
    ui->byoyomiSec2->setValue(Saved::player2.byoyomiSec.defaultValue());
    ui->addEachMoveSec2->setValue(Saved::player2.addEachMoveSec.defaultValue());

    ui->comboBoxStartingPosition->setCurrentIndex(Saved::startingPositionNumber.defaultValue());
    ui->spinBoxMaxMoves->setValue(Saved::maxMoves.defaultValue());
    ui->spinBoxConsecutiveGames->setValue(Saved::consecutiveGames.defaultValue());
    ui->checkBoxShowHumanInFront->setChecked(Saved::showHumanInFront.defaultValue());
    ui->checkBoxAutoSaveKifu->setChecked(Saved::autoSaveKifu.defaultValue());
    ui->lineEditKifuSaveDir->setText(defaultKifuSaveDir());
    ui->checkBoxLoseOnTimeOut->setChecked(Saved::loseOnTimeout.defaultValue());
    ui->checkBoxSwitchTurnEachGame->setChecked(Saved::switchTurnEachGame.defaultValue());
    ui->comboBoxJishogi->setCurrentIndex(Saved::jishogiRule.defaultValue());
}
