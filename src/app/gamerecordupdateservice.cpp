/// @file gamerecordupdateservice.cpp
/// @brief 棋譜追記・ライブセッション更新ロジックの実装

#include "gamerecordupdateservice.h"

#include "matchcoordinator.h"
#include "livegamesession.h"
#include "livegamesessionupdater.h"
#include "logcategories.h"
#include <utility>

GameRecordUpdateService::GameRecordUpdateService(Deps deps, QObject* parent)
    : QObject(parent), m_deps(std::move(deps))
{
    Q_ASSERT(m_deps.liveGameSession && m_deps.liveSessionUpdater);
}

void GameRecordUpdateService::appendRecord(const RecordEntry& entry)
{
    if (entry.displayText.isEmpty()) return;
    if (m_deps.match && m_deps.match->gameOverState().moveAppended) return;
    RecordEntry recorded = entry;
    if (recorded.sfen.isEmpty() && m_deps.sfenRecord && !m_deps.sfenRecord->isEmpty())
        recorded.sfen = m_deps.sfenRecord->last();

    auto* updater = m_deps.liveSessionUpdater;
    updater->ensureSessionStarted();
    if (!m_deps.liveGameSession->canAddMove()) return;
    recorded.time = m_deps.liveGameSession->continuedTime(recorded.time);
    if (m_deps.markGameRecordDirty) m_deps.markGameRecordDirty();
    updater->appendRecord(recorded);
}

void GameRecordUpdateService::updateCurrentSfenAfterMove()
{
    // currentSfenStrを現在の局面に更新
    if (m_deps.sfenRecord && m_deps.currentSfenStr && !m_deps.sfenRecord->isEmpty()) {
        *m_deps.currentSfenStr = m_deps.sfenRecord->last();
        qCDebug(lcApp) << "updateCurrentSfenAfterMove:"
                 << "sfenRecord.size()=" << m_deps.sfenRecord->size()
                 << "using last sfen=" << *m_deps.currentSfenStr;
    }
}
