/// @file registeredengines.cpp
/// @brief 登録エンジンの一覧と検索の実装

#include "registeredengines.h"

#include "engineprofilestore.h"

#include <QFileInfo>

QString RegisteredEngines::canonicalPath(const QString& path)
{
    const QString resolved = QFileInfo(path).canonicalFilePath();
    return resolved.isEmpty() ? QFileInfo(path).absoluteFilePath() : resolved;
}

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

QString RegisteredEngines::uniqueName(const QString& name) const
{
    if (indexOfName(name) < 0) return name;
    for (int number = 2;; ++number) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(name).arg(number);
        if (indexOfName(candidate) < 0) return candidate;
    }
}

void RegisteredEngines::removeAt(int index)
{
    if (index >= 0 && index < m_entries.size()) m_entries.removeAt(index);
}
