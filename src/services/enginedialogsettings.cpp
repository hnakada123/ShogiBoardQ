/// @file enginedialogsettings.cpp
/// @brief エンジン設定ダイアログの永続化実装

#include "enginedialogsettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace EngineDialogSettings {

int engineSettingsFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeEngineSettings, 12).toInt();
}

void setEngineSettingsFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeEngineSettings, size);
}

QSize engineSettingsDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEngineSettingsDialogSize, QSize(800, 800)).toSize();
}

void setEngineSettingsDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEngineSettingsDialogSize, size);
}

int engineRegistrationFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeEngineRegistration, 12).toInt();
}

void setEngineRegistrationFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeEngineRegistration, size);
}

QSize engineRegistrationDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEngineRegistrationDialogSize, QSize(647, 421)).toSize();
}

void setEngineRegistrationDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEngineRegistrationDialogSize, size);
}

} // namespace EngineDialogSettings
