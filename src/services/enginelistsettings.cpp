/// @file enginelistsettings.cpp
/// @brief 機能ごとに選んだエンジンの保存の実装

#include "enginelistsettings.h"

#include "settingscommon.h"

#include <QSettings>

// 機能ごとのキー（名前のキーと、以前の版が保存していた番号・パスのキー。以前の版のキーは読むだけ）

EngineListSettings::SelectionKeys EngineListSettings::considerationSelection()
{
    return {"Consideration/engineName", "Consideration/engineIndex", nullptr};
}

EngineListSettings::SelectionKeys EngineListSettings::kifuAnalysisSelection()
{
    return {"KifuAnalysis/engineName", "KifuAnalysis/engineIndex", nullptr};
}

EngineListSettings::SelectionKeys EngineListSettings::tsumeSearchSelection()
{
    return {"TsumeSearch/engineName", nullptr, "TsumeSearch/enginePath"};
}

EngineListSettings::SelectionKeys EngineListSettings::tsumeshogiGeneratorSelection()
{
    return {"TsumeshogiGenerator/engineName", "TsumeshogiGenerator/engineIndex", nullptr};
}

EngineListSettings::SelectionKeys EngineListSettings::csaGameSelection()
{
    return {"CsaGameSettings/engineName", "CsaGameSettings/engineNumber", nullptr};
}

EngineListSettings::SelectionKeys EngineListSettings::tsumeCollectionSelection()
{
    // 判定エンジンの名前（内蔵エンジンなら空の文字列）。以前の版は判定エンジンをパスで保存し、
    // 内蔵の Hayanagi は "@hayanagi" と書いていた
    return {"TsumeCollection/engineName", nullptr, "TsumeCollection/engine", "@hayanagi"};
}

std::optional<QString> EngineListSettings::savedSelectionName(const SelectionKeys& keys)
{
    const QSettings& settings = SettingsCommon::openSettings();
    if (!keys.name) return std::nullopt;
    if (settings.contains(keys.name)) return settings.value(keys.name).toString();
    if (keys.legacyPath && keys.legacyBuiltinPath
        && settings.value(keys.legacyPath).toString() == QLatin1String(keys.legacyBuiltinPath)) {
        return QString();
    }
    return std::nullopt;
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
