/// @file shogiclock.cpp
/// @brief 将棋対局用タイマー管理クラスの実装

#include "shogiclock.h"
#include "logcategories.h"
#include <QtGlobal>
#include <QDebug>

// ============================================================
// 構築
// ============================================================

ShogiClock::ShogiClock(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &ShogiClock::updateClock);
}

// ============================================================
// 設定
// ============================================================

void ShogiClock::setLoseOnTimeout(bool v) { m_loseOnTimeout = v; }

void ShogiClock::setPlayerTimes(int player1Seconds, int player2Seconds,
                                int byoyomi1Seconds, int byoyomi2Seconds,
                                int bincSeconds, int wincSeconds,
                                bool isLimitedTime)
{
    qCDebug(lcShogiClock, "setPlayerTimes p1=%d p2=%d byo1=%d byo2=%d inc1=%d inc2=%d limited=%d",
            player1Seconds, player2Seconds, byoyomi1Seconds, byoyomi2Seconds,
            bincSeconds, wincSeconds, isLimitedTime ? 1 : 0);

    // 秒読みとインクリメントは排他
    if (byoyomi1Seconds > 0 || byoyomi2Seconds > 0) {
        m_byoyomi1TimeMs = qMax(0, byoyomi1Seconds) * 1000LL;
        m_byoyomi2TimeMs = qMax(0, byoyomi2Seconds) * 1000LL;
        m_bincMs = 0; m_wincMs = 0;
    } else if (bincSeconds > 0 || wincSeconds > 0) {
        m_byoyomi1TimeMs = 0; m_byoyomi2TimeMs = 0;
        m_bincMs = qMax(0, bincSeconds) * 1000LL;
        m_wincMs = qMax(0, wincSeconds) * 1000LL;
    } else {
        m_byoyomi1TimeMs = m_byoyomi2TimeMs = 0;
        m_bincMs = m_wincMs = 0;
    }

    m_player1TimeMs = qMax(0, player1Seconds) * 1000LL;
    m_player2TimeMs = qMax(0, player2Seconds) * 1000LL;

    m_timeLimitSet = isLimitedTime;
    m_byoyomi1Applied = false;
    m_byoyomi2Applied = false;
    m_clockRunning = false;
    m_gameOver = false;

    m_player1ConsiderationTimeMs = 0;
    m_player2ConsiderationTimeMs = 0;
    m_player1TotalConsiderationTimeMs = 0;
    m_player2TotalConsiderationTimeMs = 0;

    m_p1LastMoveShownSec = m_p2LastMoveShownSec = 0;
    m_p1PrevShownTotalSec = m_p2PrevShownTotalSec = 0;

    m_prevShownSecP1 = remainingDisplaySecP1();
    m_prevShownSecP2 = remainingDisplaySecP2();
    emit timeUpdated();
}

void ShogiClock::setCurrentPlayer(int player)
{
    m_currentPlayer = (player == 2 ? 2 : 1);
}

// ============================================================
// クロック制御
// ============================================================

void ShogiClock::startClock()
{
    if (m_clockRunning) return;

    saveState();

    m_elapsedTimer.restart();
    m_lastTickMs = m_elapsedTimer.elapsed();

    m_prevShownSecP1 = remainingDisplaySecP1();
    m_prevShownSecP2 = remainingDisplaySecP2();

    qCDebug(lcShogiClock, "[DEBUG] startClock: p1Ms=%lld (%d sec), p2Ms=%lld (%d sec), currentPlayer=%d",
            m_player1TimeMs, m_prevShownSecP1,
            m_player2TimeMs, m_prevShownSecP2,
            m_currentPlayer);

    emit timeUpdated();

    m_timer->start();
    m_clockRunning = true;
}

void ShogiClock::stopClock()
{
    if (!m_clockRunning) return;

    // 停止前に経過分を手番側の残時間・考慮時間に反映
    const qint64 now = m_elapsedTimer.elapsed();
    const qint64 elapsed = now - m_lastTickMs;
    if (elapsed > 0) {
        m_lastTickMs = now;
        if (m_timeLimitSet) {
            if (m_currentPlayer == 1) {
                m_player1TimeMs = qMax<qint64>(0, m_player1TimeMs - elapsed);
                m_player1ConsiderationTimeMs += elapsed;
            } else {
                m_player2TimeMs = qMax<qint64>(0, m_player2TimeMs - elapsed);
                m_player2ConsiderationTimeMs += elapsed;
            }
        } else {
            if (m_currentPlayer == 1) m_player1ConsiderationTimeMs += elapsed;
            else                      m_player2ConsiderationTimeMs += elapsed;
        }
    }

    m_timer->stop();
    m_clockRunning = false;
    emit timeUpdated();
}

int ShogiClock::remainingDisplaySecP1() const
{
    qint64 ms = qMax<qint64>(0, m_player1TimeMs);
    return static_cast<int>((ms + 999) / 1000);
}

int ShogiClock::remainingDisplaySecP2() const
{
    qint64 ms = qMax<qint64>(0, m_player2TimeMs);
    return static_cast<int>((ms + 999) / 1000);
}

void ShogiClock::updateShownConsiderationForPlayer(int player)
{
    // 総考慮ms → 秒（切り捨て）の差分から直近手の考慮秒数を算出
    if (player == 1) {
        const int totalSec = static_cast<int>(qMax<qint64>(0, m_player1TotalConsiderationTimeMs) / 1000);
        const int diff = totalSec - m_p1PrevShownTotalSec;
        m_p1LastMoveShownSec = qMax(0, diff);
        m_p1PrevShownTotalSec = totalSec;
    } else {
        const int totalSec = static_cast<int>(qMax<qint64>(0, m_player2TotalConsiderationTimeMs) / 1000);
        const int diff = totalSec - m_p2PrevShownTotalSec;
        m_p2LastMoveShownSec = qMax(0, diff);
        m_p2PrevShownTotalSec = totalSec;
    }
}

// ============================================================
// 着手確定時処理
// ============================================================

void ShogiClock::applyByoyomiAndResetConsideration1()
{
    // 終局後は秒読み/加算は行わず、表示更新のみ
    if (m_gameOver) {
        m_player1TotalConsiderationTimeMs += m_player1ConsiderationTimeMs;
        updateShownConsiderationForPlayer(1);
        emit timeUpdated();
        return;
    }

    m_player1TotalConsiderationTimeMs += m_player1ConsiderationTimeMs;
    updateShownConsiderationForPlayer(1);

    // 秒読み適用: 残り0以下 or 既に秒読み中なら秒読み時間をリセット
    if (m_byoyomi1TimeMs > 0) {
        if (m_player1TimeMs <= 0 || m_byoyomi1Applied) {
            m_player1TimeMs = m_byoyomi1TimeMs;
            m_byoyomi1Applied = true;
        }
    } else if (m_bincMs > 0) {
        // インクリメント加算（残り0なら加算しない）
        if (m_player1TimeMs > 0) m_player1TimeMs += m_bincMs;
    }

    emit timeUpdated();
}

void ShogiClock::applyByoyomiAndResetConsideration2()
{
    if (m_gameOver) {
        m_player2TotalConsiderationTimeMs += m_player2ConsiderationTimeMs;
        updateShownConsiderationForPlayer(2);
        emit timeUpdated();
        return;
    }

    m_player2TotalConsiderationTimeMs += m_player2ConsiderationTimeMs;
    updateShownConsiderationForPlayer(2);

    if (m_byoyomi2TimeMs > 0) {
        if (m_player2TimeMs <= 0 || m_byoyomi2Applied) {
            m_player2TimeMs = m_byoyomi2TimeMs;
            m_byoyomi2Applied = true;
        }
    } else if (m_wincMs > 0) {
        if (m_player2TimeMs > 0) m_player2TimeMs += m_wincMs;
    }

    emit timeUpdated();
}

// ============================================================
// タイマー更新
// ============================================================

void ShogiClock::updateClock()
{
    // 処理フロー:
    // 1. 前回tickからの経過時間を算出
    // 2. 持ち時間制限ありなら手番側の残り時間を減算
    //    - 残り0以下になったら秒読み遷移 or 時間切れ判定
    // 3. 制限なしなら考慮時間のみ加算
    // 4. 秒表示が変わった場合のみtimeUpdatedを通知

    if (!m_clockRunning) return;
    if (m_gameOver)      return;

    const qint64 now = m_elapsedTimer.elapsed();
    const qint64 elapsed = now - m_lastTickMs;
    if (elapsed <= 0) return;
    m_lastTickMs = now;

    if (elapsed > 60) {
        qCDebug(lcShogiClock, "updateClock elapsed=%lldms (expected ~50ms) - Timer delayed!", elapsed);
    }

    if (m_timeLimitSet) {
        auto step = [&](int player)->bool {
            qint64& remMs      = (player == 1) ? m_player1TimeMs : m_player2TimeMs;
            qint64& considerMs = (player == 1) ? m_player1ConsiderationTimeMs : m_player2ConsiderationTimeMs;
            const qint64 byoMs = (player == 1) ? m_byoyomi1TimeMs : m_byoyomi2TimeMs;
            bool& byoApplied   = (player == 1) ? m_byoyomi1Applied : m_byoyomi2Applied;

            remMs      -= elapsed;
            considerMs += elapsed;

            if (remMs <= 0) {
                if (byoMs > 0) {
                    const qint64 overshoot = -remMs;
                    if (!byoApplied) {
                        // メイン時間→秒読みへ遷移
                        remMs = byoMs - overshoot;
                        byoApplied = true;
                    } else {
                        // 既に秒読み中で時間切れ
                        remMs = 0;
                    }
                    if (remMs <= 0) {
                        if (m_loseOnTimeout) {
                            m_gameOver = true;
                            m_timer->stop();
                            m_clockRunning = false;
                            if (player == 1) emit player1TimeOut();
                            else             emit player2TimeOut();
                            emit resignationTriggered();
                        } else {
                            remMs = 0;
                        }
                        emit timeUpdated();
                        return true;
                    }
                } else {
                    remMs = 0;
                    if (m_loseOnTimeout) {
                        m_gameOver = true;
                        m_timer->stop();
                        m_clockRunning = false;
                        if (player == 1) emit player1TimeOut();
                        else             emit player2TimeOut();
                        emit resignationTriggered();
                        emit timeUpdated();
                        return true;
                    }
                }
            }
            return false;
        };

        if (m_currentPlayer == 1) {
            if (step(1)) return;
        } else {
            if (step(2)) return;
        }
    } else {
        if (m_currentPlayer == 1) m_player1ConsiderationTimeMs += elapsed;
        else                      m_player2ConsiderationTimeMs += elapsed;
    }

    debugCheckInvariants();

    // 秒表示が変わった時のみ通知（残りは切り上げ秒）
    const int sec1 = remainingDisplaySecP1();
    const int sec2 = remainingDisplaySecP2();
    if (sec1 != m_prevShownSecP1 || sec2 != m_prevShownSecP2) {
        const int diff1 = m_prevShownSecP1 - sec1;
        const int diff2 = m_prevShownSecP2 - sec2;
        if (diff1 > 1 || diff2 > 1) {
            qCDebug(lcShogiClock, "Display jump detected! P1: %d->%d (diff=%d), P2: %d->%d (diff=%d), currentPlayer=%d, p1Ms=%lld, p2Ms=%lld",
                      m_prevShownSecP1, sec1, diff1,
                      m_prevShownSecP2, sec2, diff2,
                      m_currentPlayer, m_player1TimeMs, m_player2TimeMs);
        }
        m_prevShownSecP1 = sec1;
        m_prevShownSecP2 = sec2;
        emit timeUpdated();
    }
}

// ============================================================
// GUI文字列
// ============================================================

QString ShogiClock::getPlayer1TimeString() const
{
    const int rs = remainingDisplaySecP1();
    const int h = rs / 3600, m = (rs % 3600) / 60, s = rs % 60;
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
}

QString ShogiClock::getPlayer2TimeString() const
{
    const int rs = remainingDisplaySecP2();
    const int h = rs / 3600, m = (rs % 3600) / 60, s = rs % 60;
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
}

QString ShogiClock::getPlayer1ConsiderationTime() const
{
    int totalSec = static_cast<int>(qMax<qint64>(0, m_player1TotalConsiderationTimeMs) / 1000);
    Q_UNUSED(totalSec);
    const int mm = m_p1LastMoveShownSec / 60;
    const int ss = m_p1LastMoveShownSec % 60;
    return QString::asprintf("%02d:%02d", mm, ss);
}

QString ShogiClock::getPlayer2ConsiderationTime() const
{
    int totalSec = static_cast<int>(qMax<qint64>(0, m_player2TotalConsiderationTimeMs) / 1000);
    Q_UNUSED(totalSec);
    const int mm = m_p2LastMoveShownSec / 60;
    const int ss = m_p2LastMoveShownSec % 60;
    return QString::asprintf("%02d:%02d", mm, ss);
}

QString ShogiClock::getPlayer1TotalConsiderationTime() const
{
    const int totalSec = static_cast<int>(qMax<qint64>(0, m_player1TotalConsiderationTimeMs) / 1000);
    const int h = totalSec / 3600, m = (totalSec % 3600) / 60, s = totalSec % 60;
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
}

QString ShogiClock::getPlayer2TotalConsiderationTime() const
{
    const int totalSec = static_cast<int>(qMax<qint64>(0, m_player2TotalConsiderationTimeMs) / 1000);
    const int h = totalSec / 3600, m = (totalSec % 3600) / 60, s = totalSec % 60;
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
}

QString ShogiClock::getPlayer1ConsiderationAndTotalTime() const
{
    return getPlayer1ConsiderationTime() + "/" + getPlayer1TotalConsiderationTime();
}

QString ShogiClock::getPlayer2ConsiderationAndTotalTime() const
{
    return getPlayer2ConsiderationTime() + "/" + getPlayer2TotalConsiderationTime();
}

// ============================================================
// undo
// ============================================================

void ShogiClock::saveState()
{
    qCDebug(lcShogiClock, "saveState p1=%lld p2=%lld c1=%lld c2=%lld t1=%lld t2=%lld byo1Applied=%d byo2Applied=%d p1PrevShownTot=%d p2PrevShownTot=%d p1Last=%d p2Last=%d",
            m_player1TimeMs, m_player2TimeMs,
            m_player1ConsiderationTimeMs, m_player2ConsiderationTimeMs,
            m_player1TotalConsiderationTimeMs, m_player2TotalConsiderationTimeMs,
            m_byoyomi1Applied ? 1 : 0, m_byoyomi2Applied ? 1 : 0,
            m_p1PrevShownTotalSec, m_p2PrevShownTotalSec,
            m_p1LastMoveShownSec, m_p2LastMoveShownSec);

    m_player1TimeHistory.push(m_player1TimeMs);
    m_player2TimeHistory.push(m_player2TimeMs);
    m_player1ConsiderationHistory.push(m_player1ConsiderationTimeMs);
    m_player2ConsiderationHistory.push(m_player2ConsiderationTimeMs);
    m_player1TotalConsiderationHistory.push(m_player1TotalConsiderationTimeMs);
    m_player2TotalConsiderationHistory.push(m_player2TotalConsiderationTimeMs);
    m_byoyomi1AppliedHistory.push(m_byoyomi1Applied);
    m_byoyomi2AppliedHistory.push(m_byoyomi2Applied);
    m_p1PrevShownTotalSecHistory.push(m_p1PrevShownTotalSec);
    m_p2PrevShownTotalSecHistory.push(m_p2PrevShownTotalSec);
    m_p1LastMoveShownSecHistory.push(m_p1LastMoveShownSec);
    m_p2LastMoveShownSecHistory.push(m_p2LastMoveShownSec);
}

void ShogiClock::undo()
{
    // 「待った」は2手分の状態を巻き戻す
    auto enough = [&]{
        return m_player1TimeHistory.size() >= 3 &&
               m_player2TimeHistory.size() >= 3 &&
               m_player1ConsiderationHistory.size() >= 3 &&
               m_player2ConsiderationHistory.size() >= 3 &&
               m_player1TotalConsiderationHistory.size() >= 3 &&
               m_player2TotalConsiderationHistory.size() >= 3 &&
               m_byoyomi1AppliedHistory.size() >= 3 &&
               m_byoyomi2AppliedHistory.size() >= 3 &&
               m_p1PrevShownTotalSecHistory.size() >= 3 &&
               m_p2PrevShownTotalSecHistory.size() >= 3 &&
               m_p1LastMoveShownSecHistory.size() >= 3 &&
               m_p2LastMoveShownSecHistory.size() >= 3;
    };
    if (!enough()) return;

    auto pop2 = [](auto& st){ st.pop(); st.pop(); };

    pop2(m_player1TimeHistory);
    m_player1TimeMs = m_player1TimeHistory.top();

    pop2(m_player2TimeHistory);
    m_player2TimeMs = m_player2TimeHistory.top();

    pop2(m_player1ConsiderationHistory);
    m_player1ConsiderationTimeMs = m_player1ConsiderationHistory.top();

    pop2(m_player2ConsiderationHistory);
    m_player2ConsiderationTimeMs = m_player2ConsiderationHistory.top();

    pop2(m_player1TotalConsiderationHistory);
    m_player1TotalConsiderationTimeMs = m_player1TotalConsiderationHistory.top();

    pop2(m_player2TotalConsiderationHistory);
    m_player2TotalConsiderationTimeMs = m_player2TotalConsiderationHistory.top();

    pop2(m_byoyomi1AppliedHistory);
    m_byoyomi1Applied = m_byoyomi1AppliedHistory.top();

    pop2(m_byoyomi2AppliedHistory);
    m_byoyomi2Applied = m_byoyomi2AppliedHistory.top();

    pop2(m_p1PrevShownTotalSecHistory);
    m_p1PrevShownTotalSec = m_p1PrevShownTotalSecHistory.top();

    pop2(m_p2PrevShownTotalSecHistory);
    m_p2PrevShownTotalSec = m_p2PrevShownTotalSecHistory.top();

    pop2(m_p1LastMoveShownSecHistory);
    m_p1LastMoveShownSec = m_p1LastMoveShownSecHistory.top();

    pop2(m_p2LastMoveShownSecHistory);
    m_p2LastMoveShownSec = m_p2LastMoveShownSecHistory.top();

    emit timeUpdated();
}

// ============================================================
// 内部ヘルパ
// ============================================================

void ShogiClock::debugCheckInvariants() const
{
    if (m_player1TimeMs < 0 || m_player2TimeMs < 0) {
        qWarning().noquote() << "[ShogiClock] invariant violation: negative time"
                             << "p1=" << m_player1TimeMs << "p2=" << m_player2TimeMs;
    }
    if ((m_byoyomi1TimeMs > 0 || m_byoyomi2TimeMs > 0)
        && (m_bincMs != 0 || m_wincMs != 0)) {
        qWarning().noquote() << "[ShogiClock] invariant violation: byoyomi and increment both set";
    }
    if (!m_byoyomi1TimeMs && m_byoyomi1Applied) {
        qWarning().noquote() << "[ShogiClock] invariant violation: byoyomi1Applied without byoyomi1";
    }
    if (!m_byoyomi2TimeMs && m_byoyomi2Applied) {
        qWarning().noquote() << "[ShogiClock] invariant violation: byoyomi2Applied without byoyomi2";
    }
}

void ShogiClock::setPlayer1ConsiderationTime(int ms)
{
    m_player1ConsiderationTimeMs = static_cast<qint64>(qMax(0, ms));
}

void ShogiClock::setPlayer2ConsiderationTime(int ms)
{
    m_player2ConsiderationTimeMs = static_cast<qint64>(qMax(0, ms));
}

int ShogiClock::getPlayer1ConsiderationTimeMs() const { return static_cast<int>(m_player1ConsiderationTimeMs); }

int ShogiClock::getPlayer2ConsiderationTimeMs() const { return static_cast<int>(m_player2ConsiderationTimeMs); }
