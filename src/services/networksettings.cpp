/// @file networksettings.cpp
/// @brief CSAネットワーク設定の永続化実装

#include "networksettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace NetworkSettings {

int csaLogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeCsaLog, 10).toInt();
}

void setCsaLogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaLog, size);
}

int csaWaitingDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeCsaWaitingDialog, 10).toInt();
}

void setCsaWaitingDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaWaitingDialog, size);
}

int csaGameDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeCsaGameDialog, 10).toInt();
}

void setCsaGameDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaGameDialog, size);
}

QSize csaLogWindowSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kCsaLogWindowSize, QSize(550, 450)).toSize();
}

void setCsaLogWindowSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kCsaLogWindowSize, size);
}

QSize csaGameDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kCsaGameDialogSize, QSize(450, 380)).toSize();
}

void setCsaGameDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kCsaGameDialogSize, size);
}

} // namespace NetworkSettings
