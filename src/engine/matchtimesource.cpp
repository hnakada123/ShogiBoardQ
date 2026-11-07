/// @file matchtimesource.cpp
/// @brief 対局中のエンジンの思考に使う時間の出どころの実装

#include "matchtimesource.h"

#include "shogiclock.h"

#include <limits>

namespace {

int boundedMs(qint64 ms)
{
    return static_cast<int>(qBound(qint64(1), ms, qint64(std::numeric_limits<int>::max())));
}

} // namespace

MatchSearchTiming MatchSearchTiming::fromInput(const UsiGoTimeInput& input)
{
    MatchSearchTiming result;
    result.input = input;
    result.timeoutMs = boundedMs(UsiGoTimeCalculator::budgetMs(input));
    return result;
}

MatchSearchTiming FixedTimeSource::snapshot(Turn side, bool /*pondering*/) const
{
    UsiGoTimeInput input = m_timing;
    input.sideToMove = side;
    return MatchSearchTiming::fromInput(input);
}

MatchSearchTiming ClockTimeSource::snapshot(Turn side, bool pondering) const
{
    MatchSearchTiming result;
    m_clock->updateClock();
    result.expired = m_clock->isGameOver();
    // 対局時計は着手を確定したときに次の手番の加算を足すので、どちらの残りも加算を含む（先読みを始めるのは自分の
    // 着手の確定より前で、自分の残りに足してあるのは指した手番の加算。fromClock は先読みする側の加算を引かない）。
    // 経過時間は時計が残りから引いている
    const UsiClockSide black{m_clock->remainingMainTimeMs(Turn::Black), true, 0};
    const UsiClockSide white{m_clock->remainingMainTimeMs(Turn::White), true, 0};
    result.input = UsiGoTimeCalculator::fromClock(m_clock->timeControl(), side, pondering, black, white);
    auto& input = result.input;
    if (!pondering) input.moverByoyomiMs = qMax<qint64>(0,
        m_clock->remainingTurnTimeMs(side) - m_clock->remainingMainTimeMs(side));
    if (m_clock->enforcesTimeout()) result.timeoutMs = boundedMs(m_clock->remainingTurnTimeMs(side));
    return result;
}

bool ClockTimeSource::finishTurn()
{
    m_clock->finishTurn();
    return !m_clock->isGameOver();
}
