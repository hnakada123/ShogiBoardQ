/// @file registeredengines.cpp
/// @brief 登録エンジンの一覧と検索の実装

#include "registeredengines.h"

#include "engineprofilestore.h"

#include <QFileInfo>

namespace {
QString canonicalPath(const QString& file)
{
    const QString resolved = QFileInfo(file).canonicalFilePath();
    return resolved.isEmpty() ? QFileInfo(file).absoluteFilePath() : resolved;
}
} // namespace

RegisteredEngines RegisteredEngines::load()
{
    return RegisteredEngines(EngineProfileStore::loadEngines());
}

QStringList RegisteredEngines::names() const
{
    QStringList result;
    for (const EngineEntry& entry : m_entries) result.append(entry.name);
    return result;
}

int RegisteredEngines::indexOfName(const QString& name) const
{
    if (name.isEmpty()) return -1;
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).name == name) return static_cast<int>(i);
    }
    return -1;
}

int RegisteredEngines::indexOfPath(const QString& path) const
{
    if (path.isEmpty()) return -1;
    const QString wanted = canonicalPath(path);
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (canonicalPath(m_entries.at(i).path) == wanted) return static_cast<int>(i);
    }
    return -1;
}

std::optional<EngineEntry> RegisteredEngines::findByName(const QString& name) const
{
    const int index = indexOfName(name);
    if (index < 0) return std::nullopt;
    return m_entries.at(index);
}

std::optional<EngineEntry> RegisteredEngines::findByPath(const QString& path) const
{
    const int index = indexOfPath(path);
    if (index < 0) return std::nullopt;
    return m_entries.at(index);
}
