/// @file matchtimekeeper.cpp
/// @brief 時計の配線・残り時間の通知の実装

#include "matchtimekeeper.h"

#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

#include <limits>

// ============================================================
// 初期化
// ============================================================

MatchTimekeeper::MatchTimekeeper(QObject* parent)
    : QObject(parent)
{
}

void MatchTimekeeper::setRefs(const Refs& refs)
{
    m_refs = refs;
}

// ============================================================
// 時計管理
// ============================================================

void MatchTimekeeper::setClock(ShogiClock* clock)
{
    if (m_clock == clock) return;
    unwireClock();
    m_clock = clock;
    wireClock();
}

ShogiClock* MatchTimekeeper::clock() const
{
    return m_clock;
}

const ShogiClock* MatchTimekeeper::clockConst() const
{
    return m_clock;
}

void MatchTimekeeper::wireClock()
{
    if (!m_clock) return;
    if (m_clockConn) { QObject::disconnect(m_clockConn); m_clockConn = {}; }

    m_clockConn = connect(m_clock, &ShogiClock::timeUpdated,
                          this, &MatchTimekeeper::onClockTick,
                          Qt::UniqueConnection);
    Q_ASSERT(m_clockConn);
}

void MatchTimekeeper::unwireClock()
{
    if (m_clockConn) { QObject::disconnect(m_clockConn); m_clockConn = {}; }
}

void MatchTimekeeper::onClockTick()
{
    qCDebug(lcGame) << "onClockTick()";
    emitTimeUpdateFromClock();
}

void MatchTimekeeper::pokeTimeUpdateNow()
{
    emitTimeUpdateFromClock();
}

void MatchTimekeeper::emitTimeUpdateFromClock()
{
    if (!m_clock || !m_refs.gc) return;

    const qint64 blackMs = m_clock->remainingMs(Turn::Black);
    const qint64 whiteMs = m_clock->remainingMs(Turn::White);
    const Turn side = m_refs.gc->currentPlayer();

    const bool enableUrgency = !m_clock->hasByoyomi(side) || m_clock->byoyomiApplied(side);

    const qint64 activeMs = (side == Turn::Black) ? blackMs : whiteMs;
    const qint64 urgencyMs = enableUrgency ? activeMs
                                           : std::numeric_limits<qint64>::max();

    qCDebug(lcGame) << "emit timeUpdated blackMs=" << blackMs << " whiteMs=" << whiteMs
             << " sideToMove=" << side << " urgencyMs=" << urgencyMs;

    emit timeUpdated(blackMs, whiteMs, side, urgencyMs);
}

