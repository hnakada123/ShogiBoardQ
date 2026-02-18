#ifndef HUMANVSHUMANSTRATEGY_H
#define HUMANVSHUMANSTRATEGY_H

/// @file humanvshumanstrategy.h
/// @brief 人間 vs 人間モードの Strategy 実装

#include "gamemodestrategy.h"

#include <QElapsedTimer>

class MatchCoordinator;

/**
 * @brief 人間 vs 人間モードの対局ロジック
 *
 * MatchCoordinator から HvH 固有の処理（開始・着手後処理・ターンタイマー）
 * を分離した Strategy 実装。共通処理は MatchCoordinator のメソッドを呼ぶ。
 */
class HumanVsHumanStrategy : public GameModeStrategy {
public:
    explicit HumanVsHumanStrategy(MatchCoordinator* coordinator);

    void start() override;
    void onHumanMove(const QPoint& from, const QPoint& to,
                     const QString& prettyMove) override;
    bool needsEngine() const override { return false; }

    void armTurnTimerIfNeeded() override;
    void finishTurnTimerAndSetConsideration(int moverPlayer) override;

private:
    MatchCoordinator* m_coordinator;
    QElapsedTimer m_turnTimer;
    bool m_turnTimerArmed = false;
};

#endif // HUMANVSHUMANSTRATEGY_H
