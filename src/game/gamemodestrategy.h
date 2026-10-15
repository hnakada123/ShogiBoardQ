#ifndef GAMEMODESTRATEGY_H
#define GAMEMODESTRATEGY_H

/// @file gamemodestrategy.h
/// @brief 対局者の組（人間・エンジン）別の手番の進め方（Strategy）と、Strategy が司令塔に頼む操作

#include <QString>

#include "recordentry.h"
#include "shogitypes.h"
#include "usimove.h"

class EngineLifecycleManager;
class ShogiClock;
class ShogiGameController;
class Usi;

/**
 * @brief Strategy が司令塔（MatchCoordinator）に頼む操作
 *
 * 一手の確定（時計・棋譜・千日手・最大手数）とエンジンへの思考の依頼は司令塔が行う。
 * Strategy は手番ごとに誰が指すかだけを決める。
 */
class MatchStrategyHost {
public:
    virtual ShogiGameController* gameController() const = 0;
    virtual ShogiClock* clock() const = 0;
    virtual EngineLifecycleManager& engines() = 0;
    virtual bool isGameOver() const = 0;
    /// 対局の手順の手数（開始局面からの指した手の数）
    virtual int currentPly() const = 0;

    /// 盤と手番の表示を、いまの局面に合わせるよう知らせる
    virtual void refreshBoardAndTurn() = 0;
    /// side の手番のエンジンに、いまの局面で思考させる
    virtual void requestEngineMove(Turn side) = 0;
    /// side の手番のエンジンが返した先読みの局面を覚える（次の思考で使う）
    virtual void setEnginePonder(Turn side, const QString& ponder) = 0;

    /**
     * @brief エンジンの指し手を盤に指し、一手を確定する
     * @return 対局が続くなら true（反則手・千日手・最大手数・終局なら false）
     */
    virtual bool applyEngineMove(const UsiMove& move, Turn mover, qint64 elapsedMs) = 0;

    /**
     * @brief 盤で指した人間の手（ShogiGameController が指した後）で一手を確定する
     * @return 対局が続くなら true
     */
    virtual bool completeHumanMove(const RecordEntry& played) = 0;

protected:
    ~MatchStrategyHost() = default;
};

/**
 * @brief 対局者の組ごとの開始・着手処理（人間対人間・人間対エンジン・エンジン対エンジン）
 */
class GameModeStrategy {
public:
    virtual ~GameModeStrategy() = default;

    /// 対局を開始する（エンジンを作って起動し、表示を整える）
    virtual void start() = 0;

    /// 人間が盤で指した（ShogiGameController が指した後）
    virtual void onHumanMove(const RecordEntry& played) = 0;

    /// 手番がエンジンなら思考を始める（対局の開始・再開の直後に呼ぶ）
    virtual void startInitialMoveIfNeeded() {}

    /// 人間の考慮時間の計測を始める・打ち切る（人間対人間だけが使う）
    virtual void armTurnTimerIfNeeded() {}
    virtual void disarmTurnTimer() {}
};

#endif // GAMEMODESTRATEGY_H
