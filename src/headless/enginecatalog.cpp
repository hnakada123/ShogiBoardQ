/// @file enginecatalog.cpp
/// @brief 設定ファイルに登録された USI エンジンの名前解決の実装

#include "enginecatalog.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QStringList>

std::optional<EngineEntry> EngineCatalog::findByName(const QString& name, QString* error)
{
    const RegisteredEngines engines = RegisteredEngines::load();
    if (std::optional<EngineEntry> entry = engines.findByName(name)) return entry;
    const QStringList names = engines.names();
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
    const RegisteredEngines engines = RegisteredEngines::load();
    for (const EngineEntry& entry : engines.entries()) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = entry.name;
        obj[QStringLiteral("path")] = entry.path;
        obj[QStringLiteral("author")] = entry.author;
        obj[QStringLiteral("exists")] = QFileInfo::exists(entry.path);
        array.append(obj);
    }
    return array;
}
