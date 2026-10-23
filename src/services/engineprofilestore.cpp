/// @file engineprofilestore.cpp
/// @brief 登録エンジンの設定の保存形式の実装

#include "engineprofilestore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSettings>

#include "enginesettingsconstants.h"
#include "settingscommon.h"

using namespace EngineSettingsConstants;

namespace {

/// combo の選択肢は JSON の配列で保存する（選択肢に空白を含められる）
QString serializeValueList(const QStringList& values)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(values)).toJson(QJsonDocument::Compact));
}

/// JSON の配列を優先し、以前の版の空白区切りも読む
QStringList parseValueList(const QString& text)
{
    const QJsonDocument json = QJsonDocument::fromJson(text.toUtf8());
    if (!json.isArray()) return text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList values;
    const QJsonArray array = json.array();
    values.reserve(array.size());
    for (const QJsonValue& value : array) values.append(value.toString());
    return values;
}

} // namespace

namespace EngineProfileStore {

// ============================================================
// エンジン一覧
// ============================================================

QList<EngineEntry> loadEngines()
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    const int size = settings.beginReadArray(EnginesGroupName);
    QList<EngineEntry> engines;
    engines.reserve(size);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        engines.append({settings.value(EngineNameKey).toString(), settings.value(EnginePathKey).toString(),
                        settings.value(EngineAuthorKey).toString()});
    }
    settings.endArray();
    return engines;
}

void saveEngines(const QList<EngineEntry>& engines)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    // 前の一覧の要素を残さない
    settings.remove(QLatin1String(EnginesGroupName));
    settings.beginWriteArray(EnginesGroupName);
    for (qsizetype i = 0; i < engines.size(); ++i) {
        settings.setArrayIndex(static_cast<int>(i));
        settings.setValue(EngineNameKey, engines.at(i).name);
        settings.setValue(EnginePathKey, engines.at(i).path);
        settings.setValue(EngineAuthorKey, engines.at(i).author);
    }
    settings.endArray();
}

// ============================================================
// エンジンごとの設定
// ============================================================

QList<EngineOption> loadOptions(const QString& engineName)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    const int size = settings.beginReadArray(engineOptionsGroup(engineName));
    QList<EngineOption> options;
    options.reserve(size);
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        EngineOption option;
        option.name = settings.value(EngineOptionNameKey).toString();
        option.type = settings.value(EngineOptionTypeKey).toString();
        option.defaultValue = settings.value(EngineOptionDefaultKey).toString();
        option.min = settings.value(EngineOptionMinKey).toString();
        option.max = settings.value(EngineOptionMaxKey).toString();
        option.currentValue = settings.value(EngineOptionValueKey).toString();
        option.valueList = parseValueList(settings.value(EngineOptionValueListKey).toString());
        options.append(option);
    }
    settings.endArray();
    return options;
}

void saveOptions(const QString& engineName, const QList<EngineOption>& options)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    const QString group = engineOptionsGroup(engineName);
    // 前の配列の要素は消す（GUI の設定 gui/... は残す）
    settings.beginGroup(group);
    const QStringList keys = settings.childGroups();
    for (const QString& key : keys) {
        if (key != QLatin1String("gui")) settings.remove(key);
    }
    settings.remove(QStringLiteral("size"));
    settings.endGroup();

    settings.beginWriteArray(group);
    for (qsizetype i = 0; i < options.size(); ++i) {
        const EngineOption& option = options.at(i);
        settings.setArrayIndex(static_cast<int>(i));
        settings.setValue(EngineOptionNameKey, option.name);
        settings.setValue(EngineOptionTypeKey, option.type);
        settings.setValue(EngineOptionDefaultKey, option.defaultValue);
        settings.setValue(EngineOptionMinKey, option.min);
        settings.setValue(EngineOptionMaxKey, option.max);
        settings.setValue(EngineOptionValueKey, option.currentValue);
        settings.setValue(EngineOptionValueListKey, serializeValueList(option.valueList));
    }
    settings.endArray();
}

void removeEngine(const QString& engineName)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.remove(engineOptionsGroup(engineName));
}

PonderPreferences loadPonder(const QString& engineName)
{
    PonderPreferences prefs;
    const QList<EngineOption> options = loadOptions(engineName);
    for (const EngineOption& option : options) {
        if (!isPonderOption(option.name)) continue;
        prefs.defaultEnabled = QVariant(option.defaultValue).toBool();
        prefs.enabled = option.currentValue.isEmpty() ? prefs.defaultEnabled : QVariant(option.currentValue).toBool();
        break;
    }
    // GUI の設定が無ければ、以前の版が保存した USI_Ponder の値を引き継ぐ
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(engineOptionsGroup(engineName));
    prefs.enabled = settings.value(EngineGuiPonderEnabledKey, prefs.enabled).toBool();
    prefs.sendUnreportedOption = settings.value(EngineSendUnreportedPonderKey, true).toBool();
    settings.endGroup();
    return prefs;
}

void savePonder(const QString& engineName, bool enabled, bool sendUnreportedOption)
{
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    const QString group = engineOptionsGroup(engineName);
    settings.beginGroup(group);
    settings.setValue(EngineGuiPonderEnabledKey, enabled);
    settings.setValue(EngineSendUnreportedPonderKey, sendUnreportedOption);
    settings.endGroup();

    // 保存したオプションに USI_Ponder があれば値を合わせる。報告のないオプションの行は作らない
    const int count = settings.beginReadArray(group);
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        if (isPonderOption(settings.value(EngineOptionNameKey).toString()))
            settings.setValue(EngineOptionValueKey, enabled ? QStringLiteral("true") : QStringLiteral("false"));
    }
    settings.endArray();
}

LaunchOptions launchOptions(const QString& engineName)
{
    LaunchOptions launch;
    const PonderPreferences ponder = loadPonder(engineName);
    launch.ponderEnabled = ponder.enabled;
    launch.sendUnreportedPonder = ponder.sendUnreportedOption;
    const QList<EngineOption> options = loadOptions(engineName);
    for (const EngineOption& option : options) {
        // 先読みの可否は GUI の設定から送る
        if (isPonderOption(option.name)) continue;
        if (option.type == QLatin1String(OptionTypeButton)) {
            // button 型は "on" のときだけ値なしで送る
            if (option.currentValue == QLatin1String("on")) launch.options.append({option.name, QString(), true});
        } else {
            launch.options.append({option.name, option.currentValue, false});
        }
    }
    return launch;
}

int migrateLegacyOptionGroups()
{
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

} // namespace EngineProfileStore
