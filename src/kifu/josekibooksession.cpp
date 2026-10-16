/// @file josekibooksession.cpp
/// @brief 開いている定跡ファイルの状態と非同期の読み書きの実装

#include "josekibooksession.h"

#include <QtConcurrentRun>
#include <algorithm>
#include <utility>

JosekiBookSession::JosekiBookSession(QObject *parent)
    : QObject(parent)
{
    connect(&m_loadWatcher, &QFutureWatcher<JosekiLoadResult>::finished,
            this, &JosekiBookSession::onLoadFinished);
    connect(&m_saveWatcher, &QFutureWatcher<JosekiSaveResult>::finished,
            this, &JosekiBookSession::onSaveFinished);
}

JosekiBookSession::~JosekiBookSession() = default;

// ============================================================
// 定跡データの読み取り
// ============================================================

QString JosekiBookSession::positionKey(const QString &sfen)
{
    return JosekiBook::positionKey(sfen);
}

QString JosekiBookSession::registeredSfen(const QString &sfen) const
{
    const JosekiPosition *position = m_book.find(sfen);
    return position ? position->sfen : QString();
}

// ============================================================
// 定跡データの変更
// ============================================================

void JosekiBookSession::addMove(const QString &sfen, const JosekiMove &move)
{
    m_book.putMove(sfen, move);
    setModified(true);
}

bool JosekiBookSession::updateMove(const QString &sfen, const JosekiMove &move)
{
    if (!m_book.updateMove(sfen, move)) return false;
    setModified(true);
    return true;
}

bool JosekiBookSession::removeMove(const QString &sfen, const QString &usiMove)
{
    if (!m_book.removeMove(sfen, usiMove)) return false;
    setModified(true);
    return true;
}

// ============================================================
// 最近使ったファイル
// ============================================================

void JosekiBookSession::setRecentFiles(const QStringList &files)
{
    m_recentFiles.setFiles(files);
    emit recentFilesChanged();
}

void JosekiBookSession::addRecentFile(const QString &filePath)
{
    m_recentFiles.add(filePath);
    emit recentFilesChanged();
}

void JosekiBookSession::removeRecentFile(const QString &filePath)
{
    if (m_recentFiles.remove(filePath)) emit recentFilesChanged();
}

void JosekiBookSession::clearRecentFiles()
{
    m_recentFiles.clear();
    emit recentFilesChanged();
}

QString JosekiBookSession::lastFilePathForSettings() const
{
    return m_recentFiles.files().contains(m_filePath) ? m_filePath : QString();
}

// ============================================================
// ファイル
// ============================================================

void JosekiBookSession::newBook()
{
    m_book.clear();
    m_filePath.clear();
    setModified(false);
    emit filePathChanged();
}

void JosekiBookSession::markSaved(const QString &filePath)
{
    const bool changed = m_filePath != filePath;
    m_filePath = filePath;
    addRecentFile(filePath);
    setModified(false);
    if (changed) emit filePathChanged();
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

bool JosekiBookSession::save(const QString &filePath, QString *errorMessage)
{
    if (m_busy) return false;
    const JosekiSaveResult result = JosekiBookFile::save(filePath, m_book);
    if (!result.success) {
        if (errorMessage) *errorMessage = result.errorMessage;
        return false;
    }
    markSaved(filePath);
    return true;
}

bool JosekiBookSession::registerMergeMoves(const QList<KifuMergeEntry> &entries, QString *errorMessage)
{
    if (m_busy || m_filePath.isEmpty()) return false;
    // 保存が成功するまで編集中のデータと登録済みの印を変えない
    JosekiBook updated = m_book;
    for (const KifuMergeEntry &entry : entries) {
        if (!updated.isMergeRegistered(entry.sfen, entry.usiMove)) updated.registerMergeMove(entry.sfen, entry.usiMove);
    }
    const JosekiSaveResult result = JosekiBookFile::save(m_filePath, updated);
    if (!result.success) {
        if (errorMessage) *errorMessage = result.errorMessage;
        return false;
    }
    m_book = std::move(updated);
    markSaved(m_filePath);
    return true;
}

bool JosekiBookSession::startLoad(const QString &filePath)
{
    if (m_busy) return false;
    m_pendingLoadFilePath = filePath;
    setBusy(true);
    m_loadWatcher.setFuture(QtConcurrent::run(&JosekiBookFile::load, filePath));
    return true;
}

bool JosekiBookSession::startSave(const QString &filePath)
{
    if (m_busy) return false;
    m_pendingSaveFilePath = filePath;
    setBusy(true);
    // スナップショットをコピーしてワーカーに渡す
    m_saveWatcher.setFuture(QtConcurrent::run(&JosekiBookFile::save, filePath, m_book));
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
    m_book = std::move(result.book);
    m_filePath = filePath;
    // 読み込めたファイルだけを履歴に入れる
    addRecentFile(filePath);
    setModified(false);
    emit filePathChanged();
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
    markSaved(filePath);
    emit saved(filePath);
}
