#ifndef HUMANVSENGINESTRATEGY_H
#define HUMANVSENGINESTRATEGY_H

/// @file humanvsenginestrategy.h
/// @brief 人間 vs エンジンモードの Strategy 実装

#include "gamemodestrategy.h"
#include "matchcoordinator.h"

#include <QElapsedTimer>
#include <QString>

/**
 * @brief 人間 vs エンジンモードの対局ロジック
 *
 * MatchCoordinator から HvE 固有の処理（開始・着手後処理・人間ターンタイマー・
 * エンジン初手・エンジン返し手）を分離した Strategy 実装。
 * 共通処理は StrategyContext 経由で呼ぶ。
 */
class HumanVsEngineStrategy : public GameModeStrategy {
public:
    HumanVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                          bool engineIsP1,
                          const QString& enginePath,
                          const QString& engineName);

    void start() override;
    void onHumanMove(const QPoint& from, const QPoint& to,
                     const QString& prettyMove) override;
    bool needsEngine() const override { return true; }

    void armTurnTimerIfNeeded() override;
    void finishTurnTimerAndSetConsideration(int moverPlayer) override;
    void disarmTurnTimer() override;
    void startInitialMoveIfNeeded() override;

private:
    /// エンジン返し手の処理（人間着手後の2手目以降）
    void onHumanMoveEngineReply(const QPoint& humanFrom, const QPoint& humanTo);

    /// 指定したエンジン側で初手を指す
    void startInitialEngineMoveFor(int engineSide);

    /// 人間タイマーを停止し考慮時間を確定する
    void finishHumanTimerInternal();

    MatchCoordinator::StrategyContext& m_ctx;
    bool m_engineIsP1;
    QString m_enginePath;
    QString m_engineName;
    QElapsedTimer m_humanTurnTimer;
    bool m_humanTimerArmed = false;
};

#endif // HUMANVSENGINESTRATEGY_H
