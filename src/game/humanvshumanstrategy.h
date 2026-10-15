#ifndef HUMANVSHUMANSTRATEGY_H
#define HUMANVSHUMANSTRATEGY_H

/// @file humanvshumanstrategy.h
/// @brief 人間 vs 人間モードの Strategy

#include <QElapsedTimer>

#include "gamemodestrategy.h"

/**
 * @brief 人間同士の対局。手番ごとの考慮時間を測って時計に渡す
 */
class HumanVsHumanStrategy : public GameModeStrategy {
public:
    explicit HumanVsHumanStrategy(MatchStrategyHost& host);

    void start() override;
    void onHumanMove(const RecordEntry& played) override;
    void armTurnTimerIfNeeded() override;
    void disarmTurnTimer() override;

private:
    void finishTurnTimer(Turn mover);

    MatchStrategyHost& m_host;
    QElapsedTimer m_turnTimer;
    bool m_turnTimerArmed = false;
};

#endif // HUMANVSHUMANSTRATEGY_H
