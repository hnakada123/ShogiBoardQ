/// @file matchruntimequeryservice.cpp
/// @brief 対局実行時の判定・取得クエリを集約するサービスの実装

#include "matchruntimequeryservice.h"
#include "playmodepolicyservice.h"
#include "timecontrolcontroller.h"
#include "logcategories.h"

void MatchRuntimeQueryService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

// --- 手番・対局判定（PlayModePolicyService に委譲）---

bool MatchRuntimeQueryService::isHumanTurnNow() const
{
    if (m_deps.playModePolicy) {
        return m_deps.playModePolicy->isHumanTurnNow();
    }
    return true;
}

bool MatchRuntimeQueryService::isGameActivelyInProgress() const
{
    if (m_deps.playModePolicy) {
        return m_deps.playModePolicy->isGameActivelyInProgress();
    }
    return false;
}

bool MatchRuntimeQueryService::isHvH() const
{
    if (m_deps.playModePolicy) {
        return m_deps.playModePolicy->isHvH();
    }
    return false;
}

bool MatchRuntimeQueryService::isHumanSide(ShogiGameController::Player p) const
{
    if (m_deps.playModePolicy) {
        return m_deps.playModePolicy->isHumanSide(p);
    }
    return true;
}

// --- 時間取得（TimeControlController に委譲）---

qint64 MatchRuntimeQueryService::getRemainingMsFor(MatchCoordinator::Player p) const
{
    if (!m_deps.timeController) {
        qCDebug(lcApp) << "getRemainingMsFor: timeController=null";
        return 0;
    }
    const int player = (p == MatchCoordinator::P1) ? 1 : 2;
    return m_deps.timeController->getRemainingMs(player);
}

qint64 MatchRuntimeQueryService::getIncrementMsFor(MatchCoordinator::Player p) const
{
    if (!m_deps.timeController) {
        qCDebug(lcApp) << "getIncrementMsFor: timeController=null";
        return 0;
    }
    const int player = (p == MatchCoordinator::P1) ? 1 : 2;
    return m_deps.timeController->getIncrementMs(player);
}

qint64 MatchRuntimeQueryService::getByoyomiMs() const
{
    if (!m_deps.timeController) {
        qCDebug(lcApp) << "getByoyomiMs: timeController=null";
        return 0;
    }
    return m_deps.timeController->getByoyomiMs();
}

// --- SFEN アクセサ（MatchCoordinator に委譲）---

QStringList* MatchRuntimeQueryService::sfenRecord()
{
    MatchCoordinator* match = m_deps.match ? *m_deps.match : nullptr;
    return match ? match->sfenRecordPtr() : nullptr;
}

const QStringList* MatchRuntimeQueryService::sfenRecord() const
{
    MatchCoordinator* match = m_deps.match ? *m_deps.match : nullptr;
    return match ? match->sfenRecordPtr() : nullptr;
}
