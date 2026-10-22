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
    return m_book.registeredSfen(sfen);
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

void JosekiBookSession::setActivity(Activity activity)
{
    const bool wasBusy = isBusy();
    m_activity = activity;
    if (wasBusy != isBusy()) emit busyChanged(isBusy());
}

bool JosekiBookSession::save(const QString &filePath, QString *errorMessage)
{
    if (isBusy()) return false;
    const JosekiSaveResult result = JosekiBookFile::save(filePath, m_book);
    if (!result.success) {
        if (errorMessage) *errorMessage = result.errorMessage;
        return false;
    }
    markSaved(filePath);
    return true;
}

bool JosekiBookSession::startRegisterMergeMoves(const QList<KifuMergeEntry> &entries)
{
    if (isBusy() || m_filePath.isEmpty()) return false;
    // 保存できるまで編集中のデータと登録済みの印を変えない（写しに登録して保存する）
    JosekiBook updated = m_book;
    for (const KifuMergeEntry &entry : entries) {
        if (!updated.isMergeRegistered(entry.sfen, entry.usiMove)) updated.registerMergeMove(entry.sfen, entry.usiMove);
    }
    m_pendingMerge = updated;
    m_pendingSaveFilePath = m_filePath;
    setActivity(Activity::Saving);
    // ワーカーには値の写しを渡す（保存中は定跡を変えられないので、写しはデータを共有したまま）
    m_saveWatcher.setFuture(QtConcurrent::run(&JosekiBookFile::save, m_filePath, std::move(updated)));
    return true;
}

bool JosekiBookSession::startLoad(const QString &filePath)
{
    if (isBusy()) return false;
    m_pendingLoadFilePath = filePath;
    setActivity(Activity::Loading);
    m_loadWatcher.setFuture(QtConcurrent::run(&JosekiBookFile::load, filePath));
    return true;
}

bool JosekiBookSession::startSave(const QString &filePath)
{
    if (isBusy()) return false;
    m_pendingSaveFilePath = filePath;
    setActivity(Activity::Saving);
    // スナップショットをコピーしてワーカーに渡す
    m_saveWatcher.setFuture(QtConcurrent::run(&JosekiBookFile::save, filePath, m_book));
    return true;
}

void JosekiBookSession::onLoadFinished()
{
    JosekiLoadResult result = m_loadWatcher.result();
    const QString filePath = std::exchange(m_pendingLoadFilePath, QString());
    setActivity(Activity::Idle);
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
    std::optional<JosekiBook> merged = std::exchange(m_pendingMerge, std::nullopt);
    setActivity(Activity::Idle);
    if (!result.success) {
        emit saveFailed(result.errorMessage);
        if (merged) emit mergeFinished(false);
        return;
    }
    if (merged) m_book = std::move(*merged);
    markSaved(filePath);
    emit saved(filePath);
    if (merged) emit mergeFinished(true);
}
