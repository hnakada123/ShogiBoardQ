/// @file enginecatalog.cpp
/// @brief 設定ファイルに登録された USI エンジンの名前解決の実装

#include "enginecatalog.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QStringList>

std::optional<EngineListSettings::EngineEntry> EngineCatalog::findByName(const QString& name, QString* error)
{
    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    QStringList names;
    for (const auto& entry : std::as_const(engines)) {
        if (entry.name == name) return entry;
        names.append(entry.name);
    }
    if (error) {
        if (names.isEmpty()) {
            *error = QStringLiteral("No USI engines are registered. Register one in ShogiBoardQ "
                                    "(Settings > Engine Settings) first.");
        } else {
            *error = QStringLiteral("Unknown engine \"%1\". Registered engines: %2")
                         .arg(name, names.join(QStringLiteral(", ")));
        }
    }
    return std::nullopt;
}

QJsonArray EngineCatalog::toJson()
{
    QJsonArray array;
    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    for (const auto& entry : std::as_const(engines)) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = entry.name;
        obj[QStringLiteral("path")] = entry.path;
        obj[QStringLiteral("author")] = entry.author;
        obj[QStringLiteral("exists")] = QFileInfo::exists(entry.path);
        array.append(obj);
    }
    return array;
}
