/// @file gamechannels.cpp
/// @brief ローカル対局と通信対局の窓口の組の実装

#include "gamechannels.h"

#include "appactivitystate.h"

GameChannels::GameChannels(const AppActivityState* activity, MatchCoordinator* match, CsaGameCoordinator* csaGame)
    : m_activity(activity)
    , m_local(match ? std::make_unique<LocalGameChannel>(match) : nullptr)
    , m_csa(csaGame ? std::make_unique<CsaGameChannel>(csaGame) : nullptr)
{
}

GameChannel* GameChannels::active() const
{
    if (!m_activity) return nullptr;
    if (m_activity->is(AppActivityState::Activity::DuringCsaGame)
        || m_activity->is(AppActivityState::Activity::DuringCsaConnection))
        return m_csa.get();
    if (m_activity->is(AppActivityState::Activity::DuringGame)) return m_local.get();
    return nullptr;
}
