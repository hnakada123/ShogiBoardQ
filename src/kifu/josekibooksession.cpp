/// @file josekibooksession.cpp
/// @brief 開いている定跡ファイルの状態と非同期の読み書きの実装

#include "josekibooksession.h"
#include "josekirepository.h"

#include <QtConcurrentRun>
#include <utility>

JosekiBookSession::JosekiBookSession(QObject *parent)
    : QObject(parent)
    , m_repository(std::make_unique<JosekiRepository>())
{
    connect(&m_loadWatcher, &QFutureWatcher<JosekiLoadResult>::finished,
            this, &JosekiBookSession::onLoadFinished);
    connect(&m_saveWatcher, &QFutureWatcher<JosekiSaveResult>::finished,
            this, &JosekiBookSession::onSaveFinished);
}

JosekiBookSession::~JosekiBookSession() = default;

void JosekiBookSession::setRecentFiles(const QStringList &files)
{
    m_recentFiles = files.mid(0, kMaxRecentFiles);
    emit recentFilesChanged();
}

void JosekiBookSession::addRecentFile(const QString &filePath)
{
    m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    while (m_recentFiles.size() > kMaxRecentFiles) m_recentFiles.removeLast();
    emit recentFilesChanged();
}

void JosekiBookSession::removeRecentFile(const QString &filePath)
{
    if (m_recentFiles.removeAll(filePath) > 0) emit recentFilesChanged();
}

void JosekiBookSession::clearRecentFiles()
{
    m_recentFiles.clear();
    emit recentFilesChanged();
}

QString JosekiBookSession::lastFilePathForSettings() const
{
    return m_recentFiles.contains(m_filePath) ? m_filePath : QString();
}

void JosekiBookSession::newBook()
{
    m_repository->clear();
    m_filePath.clear();
    setModified(false);
}

void JosekiBookSession::markSaved(const QString &filePath)
{
    m_filePath = filePath;
    addRecentFile(filePath);
    setModified(false);
}

void JosekiBookSession::setModified(bool modified)
{
    m_modified = modified;
    emit modifiedChanged(modified);
}

void JosekiBookSession::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged(busy);
}

bool JosekiBookSession::startLoad(const QString &filePath)
{
    if (m_busy) return false;
    m_pendingLoadFilePath = filePath;
    setBusy(true);
    m_loadWatcher.setFuture(QtConcurrent::run(&JosekiRepository::parseFromFile, filePath));
    return true;
}

bool JosekiBookSession::startSave(const QString &filePath)
{
    if (m_busy) return false;
    m_pendingSaveFilePath = filePath;
    setBusy(true);
    // スナップショットをコピーしてワーカーに渡す
    m_saveWatcher.setFuture(QtConcurrent::run(&JosekiRepository::serializeToFile, filePath,
                                              m_repository->josekiData(), m_repository->sfenWithPlyMap()));
    return true;
}

void JosekiBookSession::onLoadFinished()
{
    JosekiLoadResult result = m_loadWatcher.result();
    const QString filePath = std::exchange(m_pendingLoadFilePath, QString());
    setBusy(false);
    if (!result.success) {
        emit loadFailed(result.errorMessage);
        return;
    }
    m_repository->applyLoadResult(std::move(result));
    m_filePath = filePath;
    // 読み込めたファイルだけを履歴に入れる
    addRecentFile(filePath);
    setModified(false);
    emit loaded(filePath);
}

void JosekiBookSession::onSaveFinished()
{
    const JosekiSaveResult result = m_saveWatcher.result();
    const QString filePath = std::exchange(m_pendingSaveFilePath, QString());
    setBusy(false);
    if (!result.success) {
        emit saveFailed(result.errorMessage);
        return;
    }
    emit saved(filePath);
}
