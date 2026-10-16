/// @file josekibooksession.cpp
/// @brief 開いている定跡ファイルの状態と非同期の読み書きの実装

#include "josekibooksession.h"
#include "josekirepository.h"
#include "sfenutils.h"

#include <QtConcurrentRun>
#include <algorithm>
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

// ============================================================
// 定跡データの読み取り
// ============================================================

QString JosekiBookSession::positionKey(const QString &sfen)
{
    return SfenUtils::normalizeSfenKey(sfen);
}

bool JosekiBookSession::isEmpty() const
{
    return m_repository->isEmpty();
}

int JosekiBookSession::positionCount() const
{
    return m_repository->positionCount();
}

QList<JosekiMove> JosekiBookSession::movesAt(const QString &sfen) const
{
    return m_repository->movesForPosition(positionKey(sfen));
}

QString JosekiBookSession::registeredSfen(const QString &sfen) const
{
    return m_repository->sfenWithPly(positionKey(sfen));
}

bool JosekiBookSession::hasPosition(const QString &sfen) const
{
    return m_repository->containsPosition(positionKey(sfen));
}

bool JosekiBookSession::hasMove(const QString &sfen, const QString &usiMove) const
{
    const QList<JosekiMove> &moves = m_repository->movesForPosition(positionKey(sfen));
    return std::any_of(moves.cbegin(), moves.cend(), [&usiMove](const JosekiMove &m) { return m.move == usiMove; });
}

QSet<QString> JosekiBookSession::mergeRegisteredMoves() const
{
    return m_repository->mergeRegisteredMoves();
}

// ============================================================
// 定跡データの変更
// ============================================================

void JosekiBookSession::addMove(const QString &sfen, const JosekiMove &move)
{
    const QString key = positionKey(sfen);
    // 局面を手数付きで覚える（手数のない SFEN は1手目とする）
    const qsizetype fields = sfen.split(QLatin1Char(' '), Qt::SkipEmptyParts).size();
    m_repository->ensureSfenWithPly(key, fields == 3 ? sfen + QStringLiteral(" 1") : sfen);
    m_repository->removeMoveByUsi(key, move.move);
    m_repository->addMove(key, move);
    setModified(true);
}

bool JosekiBookSession::updateMove(const QString &sfen, const JosekiMove &move)
{
    if (!hasMove(sfen, move.move)) return false;
    m_repository->updateMove(positionKey(sfen), move.move, move.value, move.depth, move.frequency, move.comment,
                             move.nextMove);
    setModified(true);
    return true;
}

bool JosekiBookSession::removeMove(const QString &sfen, const QString &usiMove)
{
    const QString key = positionKey(sfen);
    const QList<JosekiMove> &moves = m_repository->movesForPosition(key);
    const auto it = std::find_if(moves.cbegin(), moves.cend(), [&usiMove](const JosekiMove &m) { return m.move == usiMove; });
    if (it == moves.cend()) return false;
    // 最後の定跡手を消したら局面も消す
    m_repository->deleteMove(key, static_cast<int>(std::distance(moves.cbegin(), it)));
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
    m_repository->clear();
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
    if (!m_repository->saveToFile(filePath, errorMessage)) return false;
    markSaved(filePath);
    return true;
}

bool JosekiBookSession::registerMergeMoves(const QList<KifuMergeEntry> &entries, QString *errorMessage)
{
    if (m_busy || m_filePath.isEmpty()) return false;
    // 保存が成功するまで編集中のデータと登録済みの印を変えない
    JosekiRepository updated = *m_repository;
    for (const KifuMergeEntry &entry : entries) {
        const QString key = positionKey(entry.sfen);
        if (!updated.mergeRegisteredMoves().contains(key + QLatin1Char(':') + entry.usiMove))
            updated.registerMergeMove(key, entry.sfen, entry.usiMove);
    }
    if (!updated.saveToFile(m_filePath, errorMessage)) return false;
    *m_repository = std::move(updated);
    markSaved(m_filePath);
    return true;
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
