/// @file enginelistsettings.cpp
/// @brief 設定ファイルから登録エンジン一覧を読み込むユーティリティの実装

#include "enginelistsettings.h"

#include "engineprofilestore.h"
#include "settingscommon.h"
#include "settingskeys.h"

#include <QFileInfo>
#include <QSettings>

QList<EngineListSettings::EngineEntry> EngineListSettings::loadEngines()
{
    return EngineProfileStore::loadEngines();
}

int EngineListSettings::indexOfName(const QList<EngineEntry>& engines, const QString& name)
{
    if (name.isEmpty()) return -1;
    for (qsizetype i = 0; i < engines.size(); ++i) {
        if (engines.at(i).name == name) return static_cast<int>(i);
    }
    return -1;
}

int EngineListSettings::indexOfPath(const QList<EngineEntry>& engines, const QString& path)
{
    if (path.isEmpty()) return -1;
    const auto canonical = [](const QString& file) {
        const QString resolved = QFileInfo(file).canonicalFilePath();
        return resolved.isEmpty() ? QFileInfo(file).absoluteFilePath() : resolved;
    };
    const QString wanted = canonical(path);
    for (qsizetype i = 0; i < engines.size(); ++i) {
        if (canonical(engines.at(i).path) == wanted) return static_cast<int>(i);
    }
    return -1;
}

QString EngineListSettings::pathForName(const QString& name)
{
    const QList<EngineEntry> engines = loadEngines();
    const int index = indexOfName(engines, name);
    return index >= 0 ? engines.at(index).path : QString();
}

QString EngineListSettings::nameForPath(const QString& path)
{
    const QList<EngineEntry> engines = loadEngines();
    const int index = indexOfPath(engines, path);
    return index >= 0 ? engines.at(index).name : QString();
}

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

int EngineListSettings::restoreSelection(const SelectionKeys& keys, const QList<EngineEntry>& engines)
{
    const QSettings& settings = SettingsCommon::openSettings();
    if (keys.name) {
        const int byName = indexOfName(engines, settings.value(keys.name).toString());
        if (byName >= 0) return byName;
    }
    if (keys.legacyIndex && settings.contains(keys.legacyIndex)) {
        const int index = settings.value(keys.legacyIndex).toInt();
        if (index >= 0 && index < engines.size()) return index;
    }
    if (keys.legacyPath) {
        return indexOfPath(engines, settings.value(keys.legacyPath).toString());
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
