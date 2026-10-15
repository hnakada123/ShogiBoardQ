#ifndef MATCHSEARCHSNAPSHOT_H
#define MATCHSEARCHSNAPSHOT_H

#include "shogiclock.h"
#include "usimatchhandler.h"
#include <limits>

/// 対局時計を通信に渡せる値へ変換する。stop待ちの後も呼び直し、期限を延ばさない。
inline UsiMatchHandler::SearchTiming matchSearchSnapshot(ShogiClock* clock, Turn side,
                                                        const UsiTimingParams& timing, bool pondering)
{
    UsiMatchHandler::SearchTiming result;
    auto& input = result.input;
    input.sideToMove = side;
    input.extraMs = timing.extraMs;
    input.marginMs = timing.marginMs;
    if (!clock) {
        input.timeControl = timing.timeControl;
        input.btimeMs = timing.btimeMs;
        input.wtimeMs = timing.wtimeMs;
        result.timeoutMs = static_cast<int>(qBound(qint64(1), UsiGoTimeCalculator::budgetMs(input),
                                                  qint64(std::numeric_limits<int>::max())));
        return result;
    }
    clock->updateClock();
    result.expired = clock->isGameOver();
    input.timeControl = clock->timeControl();
    const auto tc = input.timeControl.normalized();
    const auto remaining = [&](Turn player) {
        const qint64 increment = tc.usesByoyomi() || (pondering && player == side)
            ? 0 : tc.side(player).incrementMs;
        return qMax<qint64>(0, clock->remainingMainTimeMs(player) - increment);
    };
    input.btimeMs = remaining(Turn::Black);
    input.wtimeMs = remaining(Turn::White);
    if (!pondering) input.moverByoyomiMs = qMax<qint64>(0,
        clock->remainingTurnTimeMs(side) - clock->remainingMainTimeMs(side));
    if (clock->enforcesTimeout()) result.timeoutMs = static_cast<int>(qBound(qint64(1),
        clock->remainingTurnTimeMs(side), qint64(std::numeric_limits<int>::max())));
    return result;
}

#endif // MATCHSEARCHSNAPSHOT_H
