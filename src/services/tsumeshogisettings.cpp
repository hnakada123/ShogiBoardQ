/// @file tsumeshogisettings.cpp
/// @brief 詰将棋設定の永続化実装

#include "tsumeshogisettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace TsumeshogiSettings {

void migrateCollectionEngineSelection()
{
    // 以前の版は判定エンジンをパスで保存し、内蔵エンジンは "@hayanagi" と書いていた。
    // パスは選択の復元（EngineListSettings::restoreSelection）が読むので、内蔵の印だけを空の名前に直す
    QSettings& s = SettingsCommon::openSettings();
    if (s.contains(SettingsKeys::kTsumeCollectionEngineName)) return;
    if (s.value(SettingsKeys::kTsumeCollectionEngine).toString() == QLatin1String("@hayanagi")) {
        s.setValue(SettingsKeys::kTsumeCollectionEngineName, QString());
        s.remove(SettingsKeys::kTsumeCollectionEngine);
    }
}

} // namespace TsumeshogiSettings
