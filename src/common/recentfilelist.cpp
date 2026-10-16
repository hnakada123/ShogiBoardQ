/// @file recentfilelist.cpp
/// @brief 最近使ったファイルの一覧の実装

#include "recentfilelist.h"

RecentFileList::RecentFileList(int limit)
    : m_limit(qMax(1, limit))
{
}

void RecentFileList::setFiles(const QStringList& files)
{
    m_files.clear();
    for (const QString& filePath : files) {
        if (!filePath.isEmpty() && !m_files.contains(filePath)) m_files.append(filePath);
        if (m_files.size() >= m_limit) break;
    }
}

void RecentFileList::add(const QString& filePath)
{
    if (filePath.isEmpty()) return;
    m_files.removeAll(filePath);
    m_files.prepend(filePath);
    while (m_files.size() > m_limit) m_files.removeLast();
}

bool RecentFileList::remove(const QString& filePath)
{
    return m_files.removeAll(filePath) > 0;
}

void RecentFileList::clear()
{
    m_files.clear();
}
