/// @file enginelistsettings.cpp
/// @brief 設定ファイルから登録エンジン一覧を読み込むユーティリティの実装

#include "enginelistsettings.h"

#include "enginesettingsconstants.h"
#include "settingscommon.h"
#include "settingskeys.h"

#include <QFileInfo>
#include <QRegularExpression>
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

int EngineListSettings::migrateLegacyOptionGroups()
{
    using namespace EngineSettingsConstants;

    QSettings& settings = SettingsCommon::openSettings();
    // 以前の版の配列の要素は "1/name" の形、GUI の設定は "gui/..." の形で置かれていた
    static const QRegularExpression kArrayElement(QStringLiteral("^\\d+/"));
    int migrated = 0;
    const QList<EngineEntry> engines = loadEngines();
    for (const EngineEntry& engine : engines) {
        // エンジン一覧と同じ名前のグループは、以前の版でも一覧と混ざって読めない
        if (engine.name.isEmpty() || engine.name == QLatin1String(EnginesGroupName)) continue;
        const QString target = engineOptionsGroup(engine.name);
        if (settings.contains(target + QStringLiteral("/size"))) continue;

        settings.beginGroup(engine.name);
        const QStringList keys = settings.allKeys();
        QList<QPair<QString, QVariant>> values;
        for (const QString& key : keys) {
            if (key == QLatin1String("size") || kArrayElement.match(key).hasMatch()
                || key.startsWith(QLatin1String("gui/"))) {
                values.append({key, settings.value(key)});
            }
        }
        settings.endGroup();
        if (values.isEmpty()) continue;

        for (const auto& value : std::as_const(values)) {
            settings.setValue(target + QLatin1Char('/') + value.first, value.second);
            settings.remove(engine.name + QLatin1Char('/') + value.first);
        }
        ++migrated;
    }
    if (migrated > 0) settings.sync();
    return migrated;
}
