/// @file matchtimekeeper.cpp
/// @brief 時計の配線・残り時間の通知・ターンエポック管理の実装

#include "matchtimekeeper.h"

#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

#include <limits>
#include <QDateTime>


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

// ============================================================
// ターンエポック
// ============================================================

void MatchTimekeeper::markTurnEpochNowFor(Turn side, qint64 nowMs)
{
    if (nowMs < 0) nowMs = QDateTime::currentMSecsSinceEpoch();
    if (side == Turn::Black) m_turnEpochP1Ms = nowMs; else m_turnEpochP2Ms = nowMs;
}

qint64 MatchTimekeeper::turnEpochFor(Turn side) const
{
    return (side == Turn::Black) ? m_turnEpochP1Ms : m_turnEpochP2Ms;
}

// ============================================================
// 時計スナップショット
// ============================================================

void MatchTimekeeper::recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const
{
    turnText.clear(); p1.clear(); p2.clear();
    if (!m_clock) return;

    if (m_refs.gc) {
        turnText = (m_refs.gc->currentPlayer() == Turn::Black) ? QObject::tr("先手番") : QObject::tr("後手番");
    }

    const qint64 t1ms = qMax<qint64>(0, m_clock->remainingMs(Turn::Black));
    const qint64 t2ms = qMax<qint64>(0, m_clock->remainingMs(Turn::White));

    auto mmss = [](qint64 ms) {
        const qint64 s = ms / 1000;
        const qint64 m = s / 60;
        const qint64 r = s % 60;
        return QStringLiteral("%1:%2")
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(r, 2, 10, QLatin1Char('0'));
    };

    p1 = mmss(t1ms);
    p2 = mmss(t2ms);
}
