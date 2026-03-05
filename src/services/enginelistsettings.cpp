/// @file enginelistsettings.cpp
/// @brief 設定ファイルから登録エンジン一覧を読み込むユーティリティの実装

#include "enginelistsettings.h"

#include "enginesettingsconstants.h"
#include "settingscommon.h"

#include <QSettings>

QList<EngineListSettings::EngineEntry> EngineListSettings::loadEngines()
{
    using namespace EngineSettingsConstants;

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    const int size = settings.beginReadArray(EnginesGroupName);

    QList<EngineEntry> engines;
    engines.reserve(size);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        EngineEntry entry;
        entry.name = settings.value(EngineNameKey).toString();
        entry.path = settings.value(EnginePathKey).toString();
        entry.author = settings.value(EngineAuthorKey).toString();
        engines.append(entry);
    }
    settings.endArray();
    return engines;
}
