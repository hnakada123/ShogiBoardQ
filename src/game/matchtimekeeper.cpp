/// @file matchtimekeeper.cpp
/// @brief 時計管理・USI残時間計算・ターンエポック管理の実装

#include "matchtimekeeper.h"

#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "logcategories.h"

#include <limits>
#include <QDateTime>

using P = MatchCoordinator::Player;

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

void MatchTimekeeper::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
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

    const qint64 p1ms = m_clock->getPlayer1TimeIntMs();
    const qint64 p2ms = m_clock->getPlayer2TimeIntMs();
    const bool p1turn = (m_refs.gc->currentPlayer() == ShogiGameController::Player1);

    const bool hasByoyomi = p1turn ? m_clock->hasByoyomi1()
                                   : m_clock->hasByoyomi2();
    const bool inByoyomi  = p1turn ? m_clock->byoyomi1Applied()
                                  : m_clock->byoyomi2Applied();
    const bool enableUrgency = (!hasByoyomi) || inByoyomi;

    const qint64 activeMs = p1turn ? p1ms : p2ms;
    const qint64 urgencyMs = enableUrgency ? activeMs
                                           : std::numeric_limits<qint64>::max();

    qCDebug(lcGame) << "emit timeUpdated p1ms=" << p1ms << " p2ms=" << p2ms
             << " p1turn=" << p1turn << " urgencyMs=" << urgencyMs;

    emit timeUpdated(p1ms, p2ms, p1turn, urgencyMs);
}

// ============================================================
// 時間制御の設定／照会
// ============================================================

void MatchTimekeeper::setTimeControlConfig(bool useByoyomi,
                                           int byoyomiMs1, int byoyomiMs2,
                                           int incMs1,     int incMs2,
                                           bool loseOnTimeout)
{
    m_tc.useByoyomi       = useByoyomi;
    m_tc.byoyomiMs1       = qMax(0, byoyomiMs1);
    m_tc.byoyomiMs2       = qMax(0, byoyomiMs2);
    m_tc.incMs1           = qMax(0, incMs1);
    m_tc.incMs2           = qMax(0, incMs2);
    m_tc.loseOnTimeout    = loseOnTimeout;
}

const MatchTimekeeper::TimeControl& MatchTimekeeper::timeControl() const
{
    return m_tc;
}

// ============================================================
// GoTimes 計算
// ============================================================

MatchTimekeeper::GoTimes MatchTimekeeper::computeGoTimes() const
{
    GoTimes t;

    const bool hasRemainHook = static_cast<bool>(m_hooks.remainingMsFor);
    const bool hasIncHook    = static_cast<bool>(m_hooks.incrementMsFor);
    const bool hasByoHook    = static_cast<bool>(m_hooks.byoyomiMs);

    const qint64 rawB = hasRemainHook ? qMax<qint64>(0, m_hooks.remainingMsFor(P::P1)) : 0;
    const qint64 rawW = hasRemainHook ? qMax<qint64>(0, m_hooks.remainingMsFor(P::P2)) : 0;

    qCDebug(lcGame).noquote()
        << "computeGoTimes_: hooks{remain=" << hasRemainHook
        << ", inc=" << hasIncHook
        << ", byo=" << hasByoHook
        << "} rawB=" << rawB << " rawW=" << rawW
        << " useByoyomi=" << m_tc.useByoyomi;

    if (m_tc.useByoyomi) {
        const bool bApplied = m_clock ? m_clock->byoyomi1Applied() : false;
        const bool wApplied = m_clock ? m_clock->byoyomi2Applied() : false;

        t.btime = bApplied ? 0 : rawB;
        t.wtime = wApplied ? 0 : rawW;
        t.byoyomi = (hasByoHook ? m_hooks.byoyomiMs() : 0);
        t.binc = t.winc = 0;

        qCDebug(lcGame).noquote()
            << "computeGoTimes_: BYO"
            << " bApplied=" << bApplied << " wApplied=" << wApplied
            << " => btime=" << t.btime << " wtime=" << t.wtime
            << " byoyomi=" << t.byoyomi;
    } else {
        t.btime = rawB;
        t.wtime = rawW;
        t.byoyomi = 0;
        t.binc = m_tc.incMs1;
        t.winc = m_tc.incMs2;

        if (t.binc > 0) t.btime = qMax<qint64>(0, t.btime - t.binc);
        if (t.winc > 0) t.wtime = qMax<qint64>(0, t.wtime - t.winc);

        qCDebug(lcGame).noquote()
            << "computeGoTimes_: FISCHER"
            << " => btime=" << t.btime << " wtime=" << t.wtime
            << " binc=" << t.binc << " winc=" << t.winc;
    }

    return t;
}

void MatchTimekeeper::computeGoTimesForUSI(qint64& outB, qint64& outW) const
{
    const GoTimes t = computeGoTimes();
    outB = t.btime;
    outW = t.wtime;
}

void MatchTimekeeper::refreshGoTimes()
{
    qint64 b = 0, w = 0;
    computeGoTimesForUSI(b, w);
}

// ============================================================
// ターンエポック
// ============================================================

void MatchTimekeeper::markTurnEpochNowFor(Player side, qint64 nowMs)
{
    if (nowMs < 0) nowMs = QDateTime::currentMSecsSinceEpoch();
    if (side == P::P1) m_turnEpochP1Ms = nowMs; else m_turnEpochP2Ms = nowMs;
}

qint64 MatchTimekeeper::turnEpochFor(Player side) const
{
    return (side == P::P1) ? m_turnEpochP1Ms : m_turnEpochP2Ms;
}

// ============================================================
// 時計スナップショット
// ============================================================

void MatchTimekeeper::recomputeClockSnapshot(QString& turnText, QString& p1, QString& p2) const
{
    turnText.clear(); p1.clear(); p2.clear();
    if (!m_clock) return;

    if (m_refs.gc) {
        const bool p1Turn = (m_refs.gc->currentPlayer() == ShogiGameController::Player1);
        turnText = p1Turn ? QObject::tr("先手番") : QObject::tr("後手番");
    }

    const qint64 t1ms = qMax<qint64>(0, m_clock->getPlayer1TimeIntMs());
    const qint64 t2ms = qMax<qint64>(0, m_clock->getPlayer2TimeIntMs());

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
