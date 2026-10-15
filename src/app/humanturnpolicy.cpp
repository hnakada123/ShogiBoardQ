/// @file humanturnpolicy.cpp
/// @brief 盤を人間が操作できるか・対局が進行中かの判定の実装

#include "humanturnpolicy.h"

#include "appactivitystate.h"
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"

void HumanTurnPolicy::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

bool HumanTurnPolicy::isHumanTurnNow() const
{
    // 対局していない待機中は、盤上の着手を表示中の手の次の手（変化）として記録する。
    // 終局後も同じ扱いで、指し継いだ手は分岐ツリーに変化として加わる。
    if (!isGameActivelyInProgress()) {
        return m_deps.canRecordVariationMove && m_deps.canRecordVariationMove();
    }

    switch (m_deps.activity->activity()) {
    case AppActivityState::Activity::DuringGame:
        return m_deps.gameController && m_deps.match->isHumanSide(m_deps.gameController->currentPlayer());
    case AppActivityState::Activity::DuringCsaGame: {
        const CsaGameCoordinator* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr;
        return csa && csa->isHumanPlayer() && csa->isMyTurn();
    }
    default:
        return false;
    }
}

bool HumanTurnPolicy::isGameActivelyInProgress() const
{
    if (!m_deps.activity) return false;
    switch (m_deps.activity->activity()) {
    case AppActivityState::Activity::DuringGame:
        return m_deps.match && !m_deps.match->isGameOver();
    case AppActivityState::Activity::DuringCsaGame: {
        const CsaGameCoordinator* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr;
        return csa && csa->gameState() == CsaGameCoordinator::GameState::InGame;
    }
    default:
        return false;
    }
}
