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
    auto& input = result.input;
    input.sideToMove = side;
    m_clock->updateClock();
    result.expired = m_clock->isGameOver();
    input.timeControl = m_clock->timeControl();
    const auto tc = input.timeControl.normalized();
    // 時計は手番の始めに加算を足しているので、USI の btime/wtime の意味（加算を含まない）に戻す
    const auto remaining = [&](Turn player) {
        const qint64 increment = tc.usesByoyomi() || (pondering && player == side)
            ? 0 : tc.side(player).incrementMs;
        return qMax<qint64>(0, m_clock->remainingMainTimeMs(player) - increment);
    };
    input.btimeMs = remaining(Turn::Black);
    input.wtimeMs = remaining(Turn::White);
    if (!pondering) input.moverByoyomiMs = qMax<qint64>(0,
        m_clock->remainingTurnTimeMs(side) - m_clock->remainingMainTimeMs(side));
    if (m_clock->enforcesTimeout()) result.timeoutMs = boundedMs(m_clock->remainingTurnTimeMs(side));
    return result;
}

qint64 ClockTimeSource::elapsedThisTurn(Turn side) const
{
    return m_clock->considerationMs(side);
}

bool ClockTimeSource::finishTurn()
{
    m_clock->finishTurn();
    return !m_clock->isGameOver();
}
