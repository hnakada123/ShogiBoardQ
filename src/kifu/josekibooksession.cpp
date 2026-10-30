/// @file josekibooksession.cpp
/// @brief 開いている定跡ファイルの状態と非同期の読み書きの実装

#include "josekibooksession.h"

#include "josekisettings.h"

#include <QFileInfo>
#include <QtConcurrentRun>
#include <algorithm>
#include <utility>

namespace {

/// filePath の定跡を book（今の定跡の写し）に足す（ワーカースレッドで呼ぶ）
JosekiBookMergeResult mergeBookFile(JosekiBook book, const QString &filePath, JosekiBook::MergeOptions options)
{
    JosekiBookMergeResult result;
    const JosekiLoadResult loaded = JosekiBookFile::load(filePath);
    if (!loaded.success) {
        result.errorMessage = loaded.errorMessage;
        return result;
    }
    result.stats = book.mergeFrom(loaded.book, options);
    result.book = std::move(book);
    result.success = true;
    return result;
}

} // namespace

JosekiBookSession::JosekiBookSession(QObject *parent)
    : QObject(parent)
{
    connect(&m_loadWatcher, &QFutureWatcher<JosekiLoadResult>::finished,
            this, &JosekiBookSession::onLoadFinished);
    connect(&m_saveWatcher, &QFutureWatcher<JosekiSaveResult>::finished,
            this, &JosekiBookSession::onSaveFinished);
    connect(&m_mergeBookWatcher, &QFutureWatcher<JosekiBookMergeResult>::finished,
            this, &JosekiBookSession::onMergeBookFinished);
}

JosekiBookSession::~JosekiBookSession() = default;

// ============================================================
// 定跡データの読み取り
// ============================================================

QString JosekiBookSession::registeredSfen(const QString &sfen) const
{
    return m_book.registeredSfen(sfen);
}

// ============================================================
// 定跡データの変更
// ============================================================

bool JosekiBookSession::addMove(const QString &sfen, const JosekiMove &move)
{
    if (!canEdit()) return false;
    m_book.putMove(sfen, move);
    setModified(true);
    emit contentChanged();
    return true;
}

bool JosekiBookSession::updateMove(const QString &sfen, const JosekiMove &move, JosekiBook::NextMoveUpdate next)
{
    if (!canEdit() || !m_book.updateMove(sfen, move, next)) return false;
    setModified(true);
    emit contentChanged();
    return true;
}

bool JosekiBookSession::removeMove(const QString &sfen, const QString &usiMove)
{
    if (!canEdit() || !m_book.removeMove(sfen, usiMove)) return false;
    setModified(true);
    emit contentChanged();
    return true;
}

bool JosekiBookSession::moveMoveTo(const QString &sfen, const QString &usiMove, int index)
{
    if (!canEdit() || !m_book.moveTo(sfen, usiMove, index)) return false;
    setModified(true);
    emit contentChanged();
    return true;
}

bool JosekiBookSession::sortMoves(const QString &sfen, JosekiBook::SortKey key)
{
    if (!canEdit() || !m_book.sortMoves(sfen, key)) return false;
    setModified(true);
    emit contentChanged();
    return true;
}

JosekiBookSession::ImportStats JosekiBookSession::importMoves(const QList<JosekiImport::Move> &moves)
{
    ImportStats stats;
    if (!canEdit() || moves.isEmpty()) return stats;
    for (const JosekiImport::Move &move : moves) {
        if (m_book.addPlayedMove(move.sfen, move.usiMove, move.count, move.comment)) ++stats.added;
        else ++stats.updated;
    }
    setModified(true);
    emit contentChanged();
    return stats;
}

int JosekiBookSession::valueOf(const UsiScore &score)
{
    // やねうら王の VALUE_MATE。n 手で詰ます側は VALUE_MATE - n、詰まされる側はその符号を変えた値
    constexpr int kMate = 32000;
    if (score.isCp()) return score.value;
    if (!score.isMate()) return 0;
    const int plies = score.mateCountKnown ? qAbs(score.value) : 0;
    return score.mateWin ? kMate - plies : -(kMate - plies);
}

JosekiBookSession::EngineRegistration JosekiBookSession::registerEnginePv(const QString &sfen, const QStringList &pv,
                                                                          const UsiScore &score, int depth)
{
    if (!canEdit() || sfen.isEmpty() || pv.isEmpty() || pv.constFirst().isEmpty()) return EngineRegistration::Invalid;
    JosekiMove move;
    move.move = pv.constFirst();
    // 読み筋が1手だけなら、登録済みの手の予想応手は変えない
    if (pv.size() > 1) move.nextMove = pv.at(1);
    const JosekiBook::NextMoveUpdate next =
        pv.size() > 1 ? JosekiBook::NextMoveUpdate::Replace : JosekiBook::NextMoveUpdate::Keep;
    move.value = valueOf(score);
    move.depth = qMax(depth, 0);
    const QList<JosekiMove> existing = m_book.movesAt(sfen);
    for (const JosekiMove &current : existing) {
        if (current.move != move.move) continue;
        // 出現頻度とコメントは残す
        move.frequency = current.frequency;
        move.comment = current.comment;
        (void)updateMove(sfen, move, next);
        return EngineRegistration::Updated;
    }
    move.frequency = 1;
    addMove(sfen, move);
    return EngineRegistration::Added;
}

// ============================================================
// 最近使ったファイル
// ============================================================

void JosekiBookSession::addRecentFile(const QString &filePath)
{
    m_recentFiles.add(filePath);
    saveHistory();
    emit recentFilesChanged();
}

void JosekiBookSession::removeRecentFile(const QString &filePath)
{
    if (!m_recentFiles.remove(filePath)) return;
    if (filePath == m_deferredLoadPath) cancelDeferredLoad();
    saveHistory();
    emit recentFilesChanged();
}

void JosekiBookSession::clearRecentFiles()
{
    m_recentFiles.clear();
    // 履歴から消えた（「履歴をクリア」した）前回のファイルは、自動で読み込まない
    cancelDeferredLoad();
    saveHistory();
    emit recentFilesChanged();
}

void JosekiBookSession::restore(const QStringList &recentFiles, const QString &lastFilePath, bool deferLoad)
{
    m_recentFiles.setFiles(recentFiles);
    m_restoredLastFilePath = lastFilePath;
    if (deferLoad && !lastFilePath.isEmpty() && m_recentFiles.files().contains(lastFilePath)
        && QFileInfo::exists(lastFilePath)) {
        m_deferredLoadPath = lastFilePath;
        emit deferredLoadChanged();
    }
    saveHistory();
    emit recentFilesChanged();
}

void JosekiBookSession::restoreSaved()
{
    restore(JosekiSettings::josekiWindowRecentFiles(), JosekiSettings::josekiWindowLastFilePath(), autoLoadEnabled());
}

bool JosekiBookSession::autoLoadEnabled() const
{
    return JosekiSettings::josekiWindowAutoLoadEnabled();
}

void JosekiBookSession::setAutoLoadEnabled(bool enabled)
{
    JosekiSettings::josekiWindowAutoLoadEnabled.set(enabled);
}

void JosekiBookSession::saveHistory() const
{
    JosekiSettings::josekiWindowRecentFiles.set(m_recentFiles.files());
    JosekiSettings::josekiWindowLastFilePath.set(lastFilePath());
}

QString JosekiBookSession::lastFilePath() const
{
    const QStringList &recent = m_recentFiles.files();
    if (!m_deferredLoadPath.isEmpty()) return m_deferredLoadPath;
    if (!m_filePath.isEmpty()) return recent.contains(m_filePath) ? m_filePath : QString();
    // 何も開いていない（自動読込をしない・新規にした）間は、履歴に残っている前回のファイルを覚えておく
    return recent.contains(m_restoredLastFilePath) ? m_restoredLastFilePath : QString();
}

void JosekiBookSession::startDeferredLoad()
{
    if (m_deferredLoadPath.isEmpty() || isBusy()) return;
    const QString filePath = std::exchange(m_deferredLoadPath, QString());
    emit deferredLoadChanged();
    startLoad(filePath);
}

void JosekiBookSession::cancelDeferredLoad()
{
    if (m_deferredLoadPath.isEmpty()) return;
    m_deferredLoadPath.clear();
    saveHistory();
    emit deferredLoadChanged();
}

// ============================================================
// ファイル
// ============================================================

void JosekiBookSession::newBook()
{
    cancelDeferredLoad();
    m_book.clear();
    m_filePath.clear();
    setModified(false);
    saveHistory();
    emit filePathChanged();
    emit contentChanged();
}

void JosekiBookSession::markSaved(const QString &filePath)
{
    cancelDeferredLoad();
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

bool JosekiBookSession::startMergeBook(const QString &filePath, const JosekiBook::MergeOptions &options)
{
    if (!canEdit()) return false;
    m_pendingMergeBookFilePath = filePath;
    setActivity(Activity::Merging);
    // 足している間は定跡を変えられないので、写しはデータを共有したまま渡す（足すときにワーカーで複製する）
    m_mergeBookWatcher.setFuture(QtConcurrent::run(&mergeBookFile, m_book, filePath, options));
    return true;
}

void JosekiBookSession::onMergeBookFinished()
{
    JosekiBookMergeResult result = m_mergeBookWatcher.result();
    const QString filePath = std::exchange(m_pendingMergeBookFilePath, QString());
    setActivity(Activity::Idle);
    if (!result.success) {
        emit bookMergeFailed(result.errorMessage);
        return;
    }
    m_book = std::move(result.book);
    setModified(true);
    emit contentChanged();
    emit bookMerged(filePath, result.stats);
}

bool JosekiBookSession::startLoad(const QString &filePath)
{
    if (isBusy()) return false;
    // 前回のファイルより先に別のファイルを開いたら、前回のファイルは読み込まない
    cancelDeferredLoad();
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
    emit contentChanged();
}

void JosekiBookSession::onSaveFinished()
{
    const JosekiSaveResult result = m_saveWatcher.result();
    const QString filePath = std::exchange(m_pendingSaveFilePath, QString());
    setActivity(Activity::Idle);
    if (!result.success) {
        emit saveFailed(result.errorMessage);
        return;
    }
    markSaved(filePath);
    emit saved(filePath);
}
