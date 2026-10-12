/// @file csagamedialog_settings.cpp
/// @brief CSA対局ダイアログ - 接続履歴と設定の読み書き

#include "csagamedialog.h"
#include "ui_csagamedialog.h"
#include "settingscommon.h"
#include "networksettings.h"
#include <QSettings>

namespace {
constexpr auto kCsaServerHistoryArray = "CsaServerHistory";
constexpr auto kCsaGameSettingsGroup = "CsaGameSettings";
constexpr auto kKeyHost = "host";
constexpr auto kKeyPort = "port";
constexpr auto kKeyId = "id";
constexpr auto kKeyIsHuman = "isHuman";
constexpr auto kKeyEngineNumber = "engineNumber";
constexpr auto kKeyEngineName = "engineName";
constexpr auto kKeyPassword = "password";
} // namespace

// 設定ファイルからサーバー接続履歴を読み込む
void CsaGameDialog::loadServerHistory()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    // [CsaServerHistory]グループ内の履歴数を取得する
    int size = settings.beginReadArray(kCsaServerHistoryArray);

    // 履歴リストをクリアする
    m_serverHistory.clear();

    // 履歴リストに追加する
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        ServerHistory history;
        history.host = settings.value(kKeyHost).toString();
        history.port = settings.value(kKeyPort, 4081).toInt();
        history.id = settings.value(kKeyId).toString();
        m_serverHistory.append(history);
    }

    settings.endArray();
}

// 現在の設定をサーバー接続履歴として保存する
void CsaGameDialog::saveServerHistory()
{
    // 現在の設定を取得
    ServerHistory current;
    current.host = ui->lineEditHost->text().trimmed();
    current.port = ui->spinBoxPort->value();
    current.id = ui->lineEditId->text().trimmed();

    // 空の場合は保存しない
    if (current.host.isEmpty() || current.id.isEmpty()) {
        return;
    }

    // 同じ設定が既にあるか確認し、あれば削除（最新を先頭に移動するため）
    for (int i = 0; i < m_serverHistory.size(); ++i) {
        const ServerHistory& h = m_serverHistory.at(i);
        if (h.host == current.host && h.port == current.port && h.id == current.id) {
            m_serverHistory.removeAt(i);
            break;
        }
    }

    // 先頭に追加
    m_serverHistory.prepend(current);

    // 最大10件まで保持
    while (m_serverHistory.size() > 10) {
        m_serverHistory.removeLast();
    }

    // 設定ファイルに保存
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    settings.beginWriteArray(kCsaServerHistoryArray);
    for (int i = 0; i < m_serverHistory.size(); ++i) {
        settings.setArrayIndex(i);
        const ServerHistory& h = m_serverHistory.at(i);
        settings.setValue(kKeyHost, h.host);
        settings.setValue(kKeyPort, h.port);
        settings.setValue(kKeyId, h.id);
    }
    settings.endArray();
}

// UIにサーバー履歴を反映する
void CsaGameDialog::populateUIWithServerHistory()
{
    // 履歴コンボボックスをクリアする
    ui->comboBoxHistory->clear();

    ui->comboBoxHistory->addItem(tr("新しい接続先"));
    ui->comboBoxHistory->setEnabled(!m_serverHistory.isEmpty());

    // 履歴リストからサーバー履歴をコンボボックスに追加する
    for (const ServerHistory& history : std::as_const(m_serverHistory)) {
        QString displayText = formatServerHistoryDisplay(history);
        ui->comboBoxHistory->addItem(displayText);
    }
}

// サーバー履歴の表示文字列を生成する
QString CsaGameDialog::formatServerHistoryDisplay(const ServerHistory& history) const
{
    return QString("%1:%2 %3").arg(history.host).arg(history.port).arg(history.id);
}

// 設定ファイルから対局設定を読み込む
void CsaGameDialog::loadGameSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    settings.beginGroup(kCsaGameSettingsGroup);

    // 対局者設定
    bool isHuman = settings.value(kKeyIsHuman, true).toBool();
    ui->radioButtonHuman->setChecked(isHuman);
    ui->radioButtonEngine->setChecked(!isHuman);

    int engineIndex = ui->comboBoxEngine->findText(settings.value(kKeyEngineName).toString());
    if (engineIndex < 0) engineIndex = settings.value(kKeyEngineNumber, 0).toInt();
    if (engineIndex >= 0 && engineIndex < ui->comboBoxEngine->count()) {
        ui->comboBoxEngine->setCurrentIndex(engineIndex);
    }

    // サーバー設定
    ui->lineEditHost->setText(settings.value(kKeyHost, "").toString());
    ui->spinBoxPort->setValue(settings.value(kKeyPort, 4081).toInt());
    ui->lineEditId->setText(settings.value(kKeyId, "").toString());
    ui->lineEditPassword->clear();
    settings.remove(kKeyPassword);

    settings.endGroup();
}

// 対局設定を設定ファイルに保存する
void CsaGameDialog::saveGameSettings()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    settings.beginGroup(kCsaGameSettingsGroup);

    // 対局者設定
    settings.setValue(kKeyIsHuman, ui->radioButtonHuman->isChecked());
    settings.setValue(kKeyEngineName, ui->comboBoxEngine->currentText());
    settings.setValue(kKeyEngineNumber, ui->comboBoxEngine->currentIndex());

    // サーバー設定
    settings.setValue(kKeyHost, ui->lineEditHost->text().trimmed());
    settings.setValue(kKeyPort, ui->spinBoxPort->value());
    settings.setValue(kKeyId, ui->lineEditId->text().trimmed());
    settings.remove(kKeyPassword);

    settings.endGroup();
}

void CsaGameDialog::loadAdvancedSettings()
{
    ui->checkBoxKeepAlive->setChecked(NetworkSettings::csaKeepAliveEnabled());
    ui->spinBoxKeepAliveInterval->setValue(NetworkSettings::csaKeepAliveIntervalSec());
    ui->checkBoxTcpKeepAlive->setChecked(NetworkSettings::csaTcpKeepAliveEnabled());
    ui->spinBoxThinkingMargin->setValue(NetworkSettings::csaThinkingMarginSec());

    const int mode = NetworkSettings::csaGameCountMode();
    ui->radioButtonGamesSingle->setChecked(mode == static_cast<int>(GameCountMode::Single));
    ui->radioButtonGamesCount->setChecked(mode == static_cast<int>(GameCountMode::Count));
    ui->radioButtonGamesUnlimited->setChecked(mode == static_cast<int>(GameCountMode::Unlimited));
    ui->spinBoxGameCount->setValue(NetworkSettings::csaGameCount());
    ui->checkBoxReconnect->setChecked(NetworkSettings::csaReconnectAfterGame());
    ui->checkBoxAutoFlip->setChecked(NetworkSettings::csaAutoFlipBoard());

    ui->checkBoxAutoSave->setChecked(NetworkSettings::csaAutoSaveEnabled());
    ui->lineEditAutoSaveDir->setText(NetworkSettings::csaAutoSaveDir());
    const int formatIndex = ui->comboBoxAutoSaveFormat->findData(NetworkSettings::csaAutoSaveFormat());
    ui->comboBoxAutoSaveFormat->setCurrentIndex(qMax(0, formatIndex));

    ui->lineEditGameName->setText(NetworkSettings::csaGameName());
    ui->comboBoxTurnPreference->setCurrentIndex(qBound(0, NetworkSettings::csaTurnPreference(), 2));
}

void CsaGameDialog::saveAdvancedSettings()
{
    NetworkSettings::setCsaKeepAliveEnabled(ui->checkBoxKeepAlive->isChecked());
    NetworkSettings::setCsaKeepAliveIntervalSec(ui->spinBoxKeepAliveInterval->value());
    NetworkSettings::setCsaTcpKeepAliveEnabled(ui->checkBoxTcpKeepAlive->isChecked());
    NetworkSettings::setCsaThinkingMarginSec(ui->spinBoxThinkingMargin->value());
    NetworkSettings::setCsaGameCountMode(static_cast<int>(m_gameCountMode));
    NetworkSettings::setCsaGameCount(ui->spinBoxGameCount->value());
    NetworkSettings::setCsaReconnectAfterGame(ui->checkBoxReconnect->isChecked());
    NetworkSettings::setCsaAutoFlipBoard(ui->checkBoxAutoFlip->isChecked());
    NetworkSettings::setCsaAutoSaveEnabled(ui->checkBoxAutoSave->isChecked());
    NetworkSettings::setCsaAutoSaveDir(ui->lineEditAutoSaveDir->text().trimmed());
    NetworkSettings::setCsaAutoSaveFormat(ui->comboBoxAutoSaveFormat->currentData().toString());
    NetworkSettings::setCsaGameName(ui->lineEditGameName->text().trimmed());
    NetworkSettings::setCsaTurnPreference(ui->comboBoxTurnPreference->currentIndex());
}
