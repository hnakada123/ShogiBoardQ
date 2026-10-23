/// @file enginelistsettings.cpp
/// @brief 設定ファイルから登録エンジン一覧を読み込むユーティリティの実装

#include "enginelistsettings.h"

#include "settingscommon.h"
#include "settingskeys.h"

#include <QSettings>

EngineListSettings::SelectionKeys EngineListSettings::considerationSelection()
{
    return {SettingsKeys::kConsiderationEngineName, SettingsKeys::kConsiderationEngineIndex, nullptr};
}

EngineListSettings::SelectionKeys EngineListSettings::kifuAnalysisSelection()
{
    return {SettingsKeys::kKifuAnalysisEngineName, SettingsKeys::kKifuAnalysisEngineIndex, nullptr};
}

EngineListSettings::SelectionKeys EngineListSettings::tsumeSearchSelection()
{
    return {SettingsKeys::kTsumeSearchEngineName, nullptr, SettingsKeys::kTsumeSearchEnginePath};
}

EngineListSettings::SelectionKeys EngineListSettings::tsumeshogiGeneratorSelection()
{
    return {SettingsKeys::kTsumeshogiGeneratorEngineName, SettingsKeys::kTsumeshogiGeneratorEngineIndex, nullptr};
}

EngineListSettings::SelectionKeys EngineListSettings::csaGameSelection()
{
    return {SettingsKeys::kCsaEngineName, SettingsKeys::kCsaEngineNumber, nullptr};
}

EngineListSettings::SelectionKeys EngineListSettings::tsumeCollectionSelection()
{
    return {SettingsKeys::kTsumeCollectionEngineName, nullptr, SettingsKeys::kTsumeCollectionEngine};
}

std::optional<QString> EngineListSettings::savedSelectionName(const SelectionKeys& keys)
{
    const QSettings& settings = SettingsCommon::openSettings();
    if (!keys.name || !settings.contains(keys.name)) return std::nullopt;
    return settings.value(keys.name).toString();
}

int EngineListSettings::restoreSelection(const SelectionKeys& keys, const RegisteredEngines& engines)
{
    const QSettings& settings = SettingsCommon::openSettings();
    if (keys.name) {
        const int byName = engines.indexOfName(settings.value(keys.name).toString());
        if (byName >= 0) return byName;
    }
    if (keys.legacyIndex && settings.contains(keys.legacyIndex)) {
        const int index = settings.value(keys.legacyIndex).toInt();
        if (index >= 0 && index < engines.entries().size()) return index;
    }
    if (keys.legacyPath) {
        return engines.indexOfPath(settings.value(keys.legacyPath).toString());
    }
    return -1;
}

void EngineListSettings::saveSelection(const SelectionKeys& keys, const QString& engineName)
{
    QSettings& settings = SettingsCommon::openSettings();
    if (keys.name) settings.setValue(keys.name, engineName);
    if (keys.legacyIndex) settings.remove(keys.legacyIndex);
    if (keys.legacyPath) settings.remove(keys.legacyPath);
}
