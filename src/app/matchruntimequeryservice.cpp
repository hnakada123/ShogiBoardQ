/// @file matchruntimequeryservice.cpp
/// @brief 対局実行時の判定・取得クエリを集約するサービスの実装

#include "matchruntimequeryservice.h"
#include "playmodepolicyservice.h"
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
    qCDebug(lcApp) << "isHumanTurnNow: playModePolicy=null";
    return false;
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
    qCDebug(lcApp) << "isHumanSide: playModePolicy=null, player=" << static_cast<int>(p);
    return false;
}

// --- SFEN アクセサ（MatchCoordinator に委譲）---

QStringList* MatchRuntimeQueryService::sfenRecord()
{
    MatchCoordinator* match = m_deps.match ? *m_deps.match : nullptr;
    if (!match) {
        qCDebug(lcApp) << "sfenRecord: match=null";
    }
    return match ? match->sfenRecordPtr() : nullptr;
}

const QStringList* MatchRuntimeQueryService::sfenRecord() const
{
    MatchCoordinator* match = m_deps.match ? *m_deps.match : nullptr;
    if (!match) {
        qCDebug(lcApp) << "sfenRecord const: match=null";
    }
    return match ? match->sfenRecordPtr() : nullptr;
}
