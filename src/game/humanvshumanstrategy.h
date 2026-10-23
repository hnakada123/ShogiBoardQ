#ifndef HUMANVSHUMANSTRATEGY_H
#define HUMANVSHUMANSTRATEGY_H

/// @file humanvshumanstrategy.h
/// @brief 人間 vs 人間モードの Strategy

#include "gamemodestrategy.h"

/**
 * @brief 人間同士の対局。人間の手を一手として確定する（考慮時間は人間対エンジンの人間の手と同じく対局時計が測る）
 */
class HumanVsHumanStrategy : public GameModeStrategy {
public:
    explicit HumanVsHumanStrategy(MatchStrategyHost& host);

    void start() override;
    void onHumanMove(const RecordEntry& played) override;

private:
    MatchStrategyHost& m_host;
};

#endif // HUMANVSHUMANSTRATEGY_H
