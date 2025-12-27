#include "shogiclock.h"
#include <QtGlobal>
#include <QDebug>

Q_LOGGING_CATEGORY(lcShogiClock, "shogi.clock")

ShogiClock::ShogiClock(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &ShogiClock::updateClock);
}

void ShogiClock::setLoseOnTimeout(bool v) { m_loseOnTimeout = v; }

void ShogiClock::setPlayerTimes(int player1Seconds, int player2Seconds,
                                int byoyomi1Seconds, int byoyomi2Seconds,
                                int bincSeconds, int wincSeconds,
                                bool isLimitedTime)
{
    qCDebug(lcShogiClock, "setPlayerTimes p1=%d p2=%d byo1=%d byo2=%d inc1=%d inc2=%d limited=%d",
            player1Seconds, player2Seconds, byoyomi1Seconds, byoyomi2Seconds,
            bincSeconds, wincSeconds, isLimitedTime ? 1 : 0);

    // byoyomi と increment は排他扱い
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

    // 考慮の初期化
    m_player1ConsiderationTimeMs = 0;
    m_player2ConsiderationTimeMs = 0;
    m_player1TotalConsiderationTimeMs = 0;
    m_player2TotalConsiderationTimeMs = 0;

    // GUI表示用の直近/前回値も初期化
    m_p1LastMoveShownSec = m_p2LastMoveShownSec = 0;
    m_p1PrevShownTotalSec = m_p2PrevShownTotalSec = 0;

    // 表示キャッシュを初期化して即反映（残りは切り上げ秒）
    m_prevShownSecP1 = remainingDisplaySecP1();
    m_prevShownSecP2 = remainingDisplaySecP2();
    emit timeUpdated();
}

void ShogiClock::setCurrentPlayer(int player)
{
    m_currentPlayer = (player == 2 ? 2 : 1);
}

void ShogiClock::startClock()
{
    if (m_clockRunning) return;

    saveState(); // undo用

    m_elapsedTimer.restart();
    m_lastTickMs = m_elapsedTimer.elapsed();

    // 残り時間の秒表示キャッシュをセットして一旦描画
    m_prevShownSecP1 = remainingDisplaySecP1();
    m_prevShownSecP2 = remainingDisplaySecP2();
    emit timeUpdated();

    m_timer->start();
    m_clockRunning = true;
}

void ShogiClock::stopClock()
{
    if (!m_clockRunning) return;

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

// ---- 残り秒（切り上げ） ----
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

// ---- 直近考慮(秒)の更新（着手確定時に呼ぶ） ----
void ShogiClock::updateShownConsiderationForPlayer(int player)
{
    if (player == 1) {
        // 総考慮ms → 秒（切り捨て）
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

void ShogiClock::applyByoyomiAndResetConsideration1()
{
    // 終局後：秒読み/加算は行わず、表示更新のみ
    if (m_gameOver) {
        m_player1TotalConsiderationTimeMs += m_player1ConsiderationTimeMs;
        updateShownConsiderationForPlayer(1);
        emit timeUpdated();
        return;
    }

    // 総考慮（実測ms）に今手分を加算 → 表示用の直近考慮(秒)を確定
    m_player1TotalConsiderationTimeMs += m_player1ConsiderationTimeMs;
    updateShownConsiderationForPlayer(1);

    // 秒読み or インクリメント適用（先手が指し終えた直後）
    if (m_byoyomi1TimeMs > 0) {
        if (m_player1TimeMs <= 0 || m_byoyomi1Applied) {
            m_player1TimeMs = m_byoyomi1TimeMs;
            m_byoyomi1Applied = true;
        }
    } else if (m_bincMs > 0) {
        if (m_player1TimeMs > 0) m_player1TimeMs += m_bincMs; // 0なら加算しない
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

void ShogiClock::updateClock()
{
    if (!m_clockRunning) return;
    if (m_gameOver)      return;

    const qint64 now = m_elapsedTimer.elapsed();
    const qint64 elapsed = now - m_lastTickMs;
    if (elapsed <= 0) return;
    m_lastTickMs = now;

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
                        // メイン→秒読みへ
                        remMs = byoMs - overshoot;
                        byoApplied = true;
                    } else {
                        // 既に秒読み中で0を割った→秒読み尽き
                        remMs = 0;
                    }
                    if (remMs <= 0) {
                        // 秒読みも尽きた
                        if (m_loseOnTimeout) {
                            m_gameOver = true;
                            m_timer->stop();
                            m_clockRunning = false;
                            if (player == 1) emit player1TimeOut();
                            else             emit player2TimeOut();
                            emit resignationTriggered();
                        } else {
                            remMs = 0; // 表示は0のまま継続
                        }
                        emit timeUpdated();
                        return true; // 停止 or 0で維持
                    }
                } else {
                    // 秒読みなし
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
        m_prevShownSecP1 = sec1;
        m_prevShownSecP2 = sec2;
        emit timeUpdated();
    }
}

// ---- GUI文字列 ----
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

// 直近考慮（MM:SS）＝ 総考慮(秒)差分（切り捨て）
QString ShogiClock::getPlayer1ConsiderationTime() const
{
    int totalSec = static_cast<int>(qMax<qint64>(0, m_player1TotalConsiderationTimeMs) / 1000);
    // m_p1LastMoveShownSec は「直近手の差分(秒)」を保持
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

// 総考慮（HH:MM:SS）＝ 実測総考慮ms → 秒は切り捨て
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

// ---- undo用 ----
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
    // あなたの既存実装に合わせて「2手戻す」
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

void ShogiClock::debugCheckInvariants() const
{
#ifndef NDEBUG
    Q_ASSERT(m_player1TimeMs >= 0);
    Q_ASSERT(m_player2TimeMs >= 0);
    if (m_byoyomi1TimeMs > 0 || m_byoyomi2TimeMs > 0) {
        Q_ASSERT(m_bincMs == 0 && m_wincMs == 0);
    }
    if (!m_byoyomi1TimeMs) Q_ASSERT(!m_byoyomi1Applied);
    if (!m_byoyomi2TimeMs) Q_ASSERT(!m_byoyomi2Applied);
#endif
}

// ---- 互換API ----
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
