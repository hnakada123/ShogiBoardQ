/// @file shogiclock.cpp
/// @brief 将棋対局用タイマー管理クラスの実装

#include "shogiclock.h"
#include "logcategories.h"
#include <QDebug>
#include <QtGlobal>

namespace {
QString hhmmss(int totalSec)
{
    return QString::asprintf("%02d:%02d:%02d", totalSec / 3600, (totalSec % 3600) / 60, totalSec % 60);
}
} // namespace

// ============================================================
// 構築・設定
// ============================================================

ShogiClock::ShogiClock(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &ShogiClock::updateClock);
}

void ShogiClock::setLoseOnTimeout(bool v) { m_loseOnTimeout = v; }

void ShogiClock::setTimeControl(const TimeControl& timeControl)
{
    m_timeControl = timeControl.normalized();
    m_timeLimitSet = !m_timeControl.isUnlimited();
    qCDebug(lcShogiClock, "setTimeControl black{main=%lld byo=%lld inc=%lld} white{main=%lld byo=%lld inc=%lld} limited=%d",
            m_timeControl.black.mainMs, m_timeControl.black.byoyomiMs, m_timeControl.black.incrementMs,
            m_timeControl.white.mainMs, m_timeControl.white.byoyomiMs, m_timeControl.white.incrementMs,
            m_timeLimitSet ? 1 : 0);

    for (const Turn player : {Turn::Black, Turn::White}) {
        SideState& s = side(player);
        s = SideState{};
        // 初手にも加算を与える。以降は着手確定時に次の手の分を加算する。
        s.remainingMs = qMax<qint64>(0, rules(player).mainMs) + qMax<qint64>(0, rules(player).incrementMs);
        enterByoyomiIfExhausted(player);
    }

    m_clockRunning = false;
    m_turnFinished = false;
    m_currentConsiderationCommitted = false;
    m_gameOver = false;
    m_resumedConsiderationMs[0] = m_resumedConsiderationMs[1] = 0;
    m_resumingTurn = false;
    resetDisplayCache();

    // 「待った」用の履歴は、対局開始時の状態から積み直す。
    m_history.clear();
    pushHistory();
    emit timeUpdated();
}

void ShogiClock::setRemainingTimes(qint64 blackMs, qint64 whiteMs)
{
    updateClock();  // tick間の経過時間は合わせる前の値へ精算する
    side(Turn::Black).remainingMs = qMax<qint64>(0, blackMs);
    side(Turn::White).remainingMs = qMax<qint64>(0, whiteMs);
    for (const Turn player : {Turn::Black, Turn::White}) {
        side(player).byoyomiApplied = false;
        enterByoyomiIfExhausted(player);
    }
    if (m_elapsedTimer.isValid()) m_lastTickMs = m_elapsedTimer.elapsed();
    resetDisplayCache();
    emit timeUpdated();
}

void ShogiClock::enterByoyomiIfExhausted(Turn player)
{
    // 持ち時間を使い切っている側（持ち時間0の秒読み対局を含む）は、初めから秒読みに入った状態にする。
    // 着手後と同じく、手番でないあいだも秒読みの秒数を表示する。
    SideState& s = side(player);
    if (rules(player).byoyomiMs > 0 && s.remainingMs <= 0) {
        s.remainingMs = rules(player).byoyomiMs;
        s.byoyomiApplied = true;
    }
}

void ShogiClock::setCurrentPlayer(Turn player)
{
    if (player == m_currentPlayer) return;
    updateClock(); // tick間の経過時間は旧手番へ精算する
    m_currentPlayer = player;
    m_currentConsiderationCommitted = false;
    m_resumedConsiderationMs[index(player)] = 0;
    m_resumingTurn = false;
    side(player).considerationMs = 0;
    m_turnFinished = false;
    if (m_elapsedTimer.isValid()) m_lastTickMs = m_elapsedTimer.elapsed();
}

void ShogiClock::finishTurn()
{
    updateClock();
    m_turnFinished = true;
}

qint64 ShogiClock::remainingTurnTimeMs(Turn player) const
{
    const SideState& s = side(player);
    const qint64 pending = m_clockRunning && !m_turnFinished && player == m_currentPlayer
        ? qMax<qint64>(0, m_elapsedTimer.elapsed() - m_lastTickMs) : 0;
    return qMax<qint64>(0, s.remainingMs + (s.byoyomiApplied ? 0 : rules(player).byoyomiMs) - pending);
}

qint64 ShogiClock::remainingMainTimeMs(Turn player) const
{
    return qMax<qint64>(0, remainingTurnTimeMs(player) - rules(player).byoyomiMs);
}

// ============================================================
// クロック制御
// ============================================================

void ShogiClock::startClock()
{
    if (m_clockRunning) return;

    m_elapsedTimer.restart();
    m_turnFinished = false;
    m_lastTickMs = m_elapsedTimer.elapsed();
    resetDisplayCache();

    qCDebug(lcShogiClock, "startClock: blackMs=%lld whiteMs=%lld currentPlayer=%s",
            side(Turn::Black).remainingMs, side(Turn::White).remainingMs,
            m_currentPlayer == Turn::Black ? "black" : "white");

    emit timeUpdated();

    m_timer->start();
    m_clockRunning = true;
}

void ShogiClock::stopClock()
{
    if (!m_clockRunning) return;

    updateClock();
    m_timer->stop();
    m_clockRunning = false;
    emit timeUpdated();
}

void ShogiClock::updateClock()
{
    // 前回tickからの経過時間を手番側へ課金し、秒表示が変わったときだけ通知する。
    if (!m_clockRunning) return;
    if (m_gameOver)      return;

    const qint64 now = m_elapsedTimer.elapsed();
    const qint64 elapsed = now - m_lastTickMs;
    if (elapsed <= 0) return;
    m_lastTickMs = now;

    if (m_turnFinished) return;

    if (elapsed > 60) {
        qCDebug(lcShogiClock, "updateClock elapsed=%lldms (expected ~50ms) - Timer delayed!", elapsed);
    }

    if (m_timeLimitSet) {
        if (chargeElapsed(m_currentPlayer, elapsed)) return;
    } else {
        side(m_currentPlayer).considerationMs += elapsed;
    }

    checkInvariants();

    // 秒表示が変わった時のみ通知（残りは切り上げ、累積消費時間は切り捨て）
    const int sec1 = remainingDisplaySec(Turn::Black);
    const int sec2 = remainingDisplaySec(Turn::White);
    if (sec1 != m_prevShownSec[0] || sec2 != m_prevShownSec[1]) {
        m_prevShownSec[0] = sec1;
        m_prevShownSec[1] = sec2;
        emit timeUpdated();
    }
}

bool ShogiClock::chargeElapsed(Turn player, qint64 elapsed)
{
    SideState& s = side(player);
    const qint64 byoMs = rules(player).byoyomiMs;

    s.remainingMs -= elapsed;
    s.considerationMs += elapsed;
    if (s.remainingMs > 0) return false;

    if (byoMs > 0 && !s.byoyomiApplied) {
        // 持ち時間から秒読みへ。超過分は秒読みから引く
        s.remainingMs = byoMs + s.remainingMs;
        s.byoyomiApplied = true;
        if (s.remainingMs > 0) return false;
    }

    s.remainingMs = 0;
    if (m_loseOnTimeout) {
        declareTimeout(player);
    } else if (byoMs <= 0) {
        // 時間切れを負けにしない設定（CSA の表示用時計など）で秒読みがなければ、0 のまま表示を続ける
        return false;
    }
    emit timeUpdated();
    return true;
}

void ShogiClock::declareTimeout(Turn player)
{
    m_gameOver = true;
    m_timer->stop();
    m_clockRunning = false;
    emit playerTimeOut(player);
}

// ============================================================
// 着手確定・待った
// ============================================================

void ShogiClock::commitMove(Turn player)
{
    SideState& s = side(player);
    // 累積へ加算した現在手の時間を、時計表示で二重に数えない。
    if (m_currentPlayer == player) m_currentConsiderationCommitted = true;
    s.totalConsiderationMs += s.considerationMs;
    updateShownConsideration(player);

    // 終局後は秒読み/加算は行わず、表示更新のみ
    if (m_gameOver) {
        emit timeUpdated();
        return;
    }

    const TimeControlSide& r = rules(player);
    if (r.byoyomiMs > 0) {
        // 残り0以下 or 既に秒読み中なら秒読み時間に戻す
        if (s.remainingMs <= 0 || s.byoyomiApplied) {
            s.remainingMs = r.byoyomiMs;
            s.byoyomiApplied = true;
        }
    } else if (r.incrementMs > 0 && s.remainingMs > 0) {
        // 次の手番の分の加算（使い切った後は加算しない）
        s.remainingMs += r.incrementMs;
    }

    // 着手を確定した状態を、次の手番の開始状態として「待った」用に残す。
    pushHistory();
    emit timeUpdated();
}

void ShogiClock::pushHistory()
{
    HistoryEntry entry;
    entry.side[0] = m_side[0];
    entry.side[1] = m_side[1];
    m_history.append(entry);
}

void ShogiClock::undo()
{
    // 「待った」は2手分の状態を巻き戻す。履歴の末尾は現在の手番の開始状態で、
    // 2つ取り除いた末尾が、取り消す自分の手を指し始めたときの状態になる。
    if (m_history.size() < 3) return;
    m_history.removeLast();
    m_history.removeLast();
    const HistoryEntry& entry = m_history.constLast();
    m_side[0] = entry.side[0];
    m_side[1] = entry.side[1];

    // 手番は2手前と同じ。考慮時間は「待った」の時点から数え直す。
    side(m_currentPlayer).considerationMs = 0;
    m_currentConsiderationCommitted = false;
    m_resumedConsiderationMs[0] = m_resumedConsiderationMs[1] = 0;
    m_resumingTurn = false;
    m_turnFinished = false;
    if (m_elapsedTimer.isValid()) m_lastTickMs = m_elapsedTimer.elapsed();
    resetDisplayCache();
    emit timeUpdated();
}

// ============================================================
// 中断・再開
// ============================================================

ShogiClock::Snapshot ShogiClock::pauseAndSnapshot()
{
    stopClock();
    Snapshot state;
    state.timeControl = m_timeControl;
    state.timeLimitSet = m_timeLimitSet;
    state.loseOnTimeout = m_loseOnTimeout;
    state.considerationCommitted = m_currentConsiderationCommitted;
    state.currentPlayer = m_currentPlayer;
    state.side[0] = m_side[0];
    state.side[1] = m_side[1];
    state.prevShownSec[0] = m_prevShownSec[0];
    state.prevShownSec[1] = m_prevShownSec[1];
    state.history = m_history;
    return state;
}

void ShogiClock::restoreSnapshot(const Snapshot& state)
{
    m_timer->stop();
    m_clockRunning = false;
    m_turnFinished = false;
    m_gameOver = false;
    m_timeControl = state.timeControl;
    m_timeLimitSet = state.timeLimitSet;
    m_loseOnTimeout = state.loseOnTimeout;
    m_currentConsiderationCommitted = state.considerationCommitted;
    m_currentPlayer = state.currentPlayer;
    m_side[0] = state.side[0];
    m_side[1] = state.side[1];
    m_prevShownSec[0] = state.prevShownSec[0];
    m_prevShownSec[1] = state.prevShownSec[1];
    m_history = state.history;
    m_resumedConsiderationMs[0] = m_resumedConsiderationMs[1] = 0;
    m_resumedConsiderationMs[index(m_currentPlayer)] = side(m_currentPlayer).considerationMs;
    m_resumingTurn = true;
    emit timeUpdated();
}

void ShogiClock::setMeasuredConsiderationTime(Turn player, qint64 elapsedMs)
{
    const qint64 total = qMax<qint64>(0, elapsedMs) + m_resumedConsiderationMs[index(player)];
    m_resumedConsiderationMs[index(player)] = 0;
    side(player).considerationMs = total;
}

void ShogiClock::setConsiderationTimeMs(Turn player, qint64 ms)
{
    side(player).considerationMs = qMax<qint64>(0, ms);
}

// ============================================================
// 表示
// ============================================================

int ShogiClock::remainingDisplaySec(Turn player) const
{
    const SideState& s = side(player);
    if (isUnlimited()) {
        const qint64 currentMs = m_currentPlayer == player && !m_currentConsiderationCommitted
            ? s.considerationMs : 0;
        return static_cast<int>((s.totalConsiderationMs + currentMs) / 1000);
    }
    return static_cast<int>((qMax<qint64>(0, s.remainingMs) + 999) / 1000);
}

void ShogiClock::resetDisplayCache()
{
    m_prevShownSec[0] = remainingDisplaySec(Turn::Black);
    m_prevShownSec[1] = remainingDisplaySec(Turn::White);
}

void ShogiClock::updateShownConsideration(Turn player)
{
    // 総考慮ms → 秒（切り捨て）の差分から直近手の考慮秒数を求める
    SideState& s = side(player);
    const int totalSec = static_cast<int>(qMax<qint64>(0, s.totalConsiderationMs) / 1000);
    s.lastMoveShownSec = qMax(0, totalSec - s.prevShownTotalSec);
    s.prevShownTotalSec = totalSec;
}

QString ShogiClock::timeString(Turn player) const
{
    return hhmmss(remainingDisplaySec(player));
}

QString ShogiClock::considerationTimeString(Turn player) const
{
    const int sec = side(player).lastMoveShownSec;
    return QString::asprintf("%02d:%02d", sec / 60, sec % 60);
}

QString ShogiClock::totalConsiderationTimeString(Turn player) const
{
    return hhmmss(static_cast<int>(qMax<qint64>(0, side(player).totalConsiderationMs) / 1000));
}

QString ShogiClock::considerationAndTotalTime(Turn player) const
{
    return considerationTimeString(player) + QLatin1Char('/') + totalConsiderationTimeString(player);
}

// ============================================================
// 内部ヘルパ
// ============================================================

void ShogiClock::checkInvariants() const
{
    for (const Turn player : {Turn::Black, Turn::White}) {
        if (side(player).remainingMs < 0) {
            qWarning().noquote() << "[ShogiClock] invariant violation: negative time for player" << player;
        }
        if (rules(player).byoyomiMs <= 0 && side(player).byoyomiApplied) {
            qWarning().noquote() << "[ShogiClock] invariant violation: byoyomi applied without byoyomi" << player;
        }
    }
}
