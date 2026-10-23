/// @file humanvshumanstrategy.cpp
/// @brief 人間 vs 人間モードの Strategy 実装

#include "humanvshumanstrategy.h"

#include "logcategories.h"

HumanVsHumanStrategy::HumanVsHumanStrategy(MatchStrategyHost& host)
    : m_host(host)
{
}

void HumanVsHumanStrategy::start()
{
    qCDebug(lcGame) << "[Match] Start HvH";
}

void HumanVsHumanStrategy::onHumanMove(const RecordEntry& played)
{
    m_host.completeHumanMove(played);
}
