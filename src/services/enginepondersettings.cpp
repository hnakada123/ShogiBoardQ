/// @file enginepondersettings.cpp
/// @brief GUI先読み設定の保存・復元と旧USI_Ponder設定の引き継ぎ

#include "enginepondersettings.h"
#include "enginesettingsconstants.h"
#include "settingscommon.h"

#include <QSettings>

using namespace EngineSettingsConstants;

namespace EnginePonderSettings {

Preferences load(const QString& engineName)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    Preferences prefs;
    const int count = settings.beginReadArray(engineName);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        if (settings.value(EngineOptionNameKey).toString() == QLatin1String("USI_Ponder")) {
            prefs.defaultEnabled = settings.value(EngineOptionDefaultKey, false).toBool();
            prefs.enabled = settings.value(EngineOptionValueKey, prefs.defaultEnabled).toBool();
            break;
        }
    }
    settings.endArray();

    settings.beginGroup(engineName);
    prefs.enabled = settings.value(EngineGuiPonderEnabledKey, prefs.enabled).toBool();
    prefs.sendUnreportedOption = settings.value(EngineSendUnreportedPonderKey, true).toBool();
    settings.endGroup();
    return prefs;
}

void save(const QString& engineName, bool enabled, bool sendUnreportedOption)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(engineName);
    settings.setValue(EngineGuiPonderEnabledKey, enabled);
    settings.setValue(EngineSendUnreportedPonderKey, sendUnreportedOption);
    settings.endGroup();

    // 既存オプションだけを同期する。未報告のオプション行は捏造しない。
    const int count = settings.beginReadArray(engineName);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        if (settings.value(EngineOptionNameKey).toString() == QLatin1String("USI_Ponder")) {
            settings.setValue(EngineOptionValueKey, enabled ? QStringLiteral("true") : QStringLiteral("false"));
        }
    }
    settings.endArray();
}

} // namespace EnginePonderSettings
