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
    qCDebug(lcShogiClock) << "setPlayerTimes p1=" << player1Seconds
                          << "p2=" << player2Seconds
                          << "byo1=" << byoyomi1Seconds
                          << "byo2=" << byoyomi2Seconds
                          << "inc1=" << bincSeconds
                          << "inc2=" << wincSeconds
                          << "limited=" << isLimitedTime;

    // byoyomi と increment は排他
    if (byoyomi1Seconds > 0 || byoyomi2Seconds > 0) {
        m_byoyomi1TimeMs = qMax(0, byoyomi1Seconds) * 1000LL;
        m_byoyomi2TimeMs = qMax(0, byoyomi2Seconds) * 1000LL;
        m_bincMs = 0;
        m_wincMs = 0;
    } else if (bincSeconds > 0 || wincSeconds > 0) {
        m_byoyomi1TimeMs = 0;
        m_byoyomi2TimeMs = 0;
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

    m_gameOver = false;  // ★ 対局開始時/再設定時は終局フラグを下ろす

    // 表示キャッシュを初期化して即反映
    m_prevShownSecP1 = qMax<qint64>(0, (m_player1TimeMs + 999) / 1000);
    m_prevShownSecP2 = qMax<qint64>(0, (m_player2TimeMs + 999) / 1000);
    emit timeUpdated();
}

void ShogiClock::setCurrentPlayer(int player) { m_currentPlayer = (player == 2 ? 2 : 1); }

void ShogiClock::startClock()
{
    if (m_clockRunning) return;

    saveState();
    m_elapsedTimer.restart();
    m_lastTickMs = m_elapsedTimer.elapsed();

    // 秒キャッシュをセットし、一度描画
    m_prevShownSecP1 = qMax<qint64>(0, (m_player1TimeMs + 999) / 1000);
    m_prevShownSecP2 = qMax<qint64>(0, (m_player2TimeMs + 999) / 1000);
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
                m_player1TimeMs -= elapsed;
                m_player1TimeMs = qMax<qint64>(0, m_player1TimeMs);
                m_player1ConsiderationTimeMs += elapsed;
            } else {
                m_player2TimeMs -= elapsed;
                m_player2TimeMs = qMax<qint64>(0, m_player2TimeMs);
                m_player2ConsiderationTimeMs += elapsed;
            }
        } else {
            if (m_currentPlayer == 1) {
                m_player1ConsiderationTimeMs += elapsed;
            } else {
                m_player2ConsiderationTimeMs += elapsed;
            }
        }
    }

    m_timer->stop();
    m_clockRunning = false;
    emit timeUpdated();
}

void ShogiClock::applyByoyomiAndResetConsideration1()
{
    // ★ 終局後は“秒読み/加算はしない”が、考慮時間は総計に反映
    if (m_gameOver) {
        m_player1TotalConsiderationTimeMs += m_player1ConsiderationTimeMs;
        emit timeUpdated();
        return;
    }

    const bool shouldUseByoyomi =
        (m_byoyomi1TimeMs > 0) && (m_player1TimeMs <= 0 || m_byoyomi1Applied);

    if (shouldUseByoyomi) {
        m_player1TimeMs   = m_byoyomi1TimeMs;
        m_byoyomi1Applied = true;
    } else if (m_byoyomi1TimeMs == 0 && m_bincMs > 0) {
        if (m_player1TimeMs > 0) m_player1TimeMs += m_bincMs;  // 0なら加算しない
    }

    m_player1TotalConsiderationTimeMs += m_player1ConsiderationTimeMs;
    emit timeUpdated();
}

void ShogiClock::applyByoyomiAndResetConsideration2()
{
    if (m_gameOver) {
        m_player2TotalConsiderationTimeMs += m_player2ConsiderationTimeMs;
        emit timeUpdated();
        return;
    }

    const bool shouldUseByoyomi =
        (m_byoyomi2TimeMs > 0) && (m_player2TimeMs <= 0 || m_byoyomi2Applied);

    if (shouldUseByoyomi) {
        m_player2TimeMs   = m_byoyomi2TimeMs;
        m_byoyomi2Applied = true;
    } else if (m_byoyomi2TimeMs == 0 && m_wincMs > 0) {
        if (m_player2TimeMs > 0) m_player2TimeMs += m_wincMs;
    }

    m_player2TotalConsiderationTimeMs += m_player2ConsiderationTimeMs;
    emit timeUpdated();
}

void ShogiClock::updateClock()
{
    if (!m_clockRunning) return;
    if (m_gameOver) return;

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
                        remMs = byoMs - overshoot;  // メイン→秒読みへ
                        byoApplied = true;
                    } else {
                        // 既に秒読み中で 0 を割った→秒読みも尽きた
                        remMs = 0;
                    }
                    if (remMs <= 0) {
                        // 秒読みにも達した → 投了 OR 0で止める
                        if (m_loseOnTimeout) {
                            m_gameOver = true;       // ★ 終局フラグON
                            m_timer->stop();
                            m_clockRunning = false;
                            if (player == 1) emit player1TimeOut();
                            else             emit player2TimeOut();
                            emit resignationTriggered();
                        } else {
                            remMs = 0; // 表示は 0 のまま継続
                        }
                        emit timeUpdated();
                        return true;
                    }
                } else {
                    // 秒読みなし
                    remMs = 0;
                    if (m_loseOnTimeout) {
                        m_gameOver = true;           // ★ 終局フラグON
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
            return false; // 継続
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

    // Debug 不変条件
    debugCheckInvariants();

    // 秒が変わったときだけ通知
    const qint64 sec1 = qMax<qint64>(0, (m_player1TimeMs + 999) / 1000);
    const qint64 sec2 = qMax<qint64>(0, (m_player2TimeMs + 999) / 1000);
    if (sec1 != m_prevShownSecP1 || sec2 != m_prevShownSecP2) {
        m_prevShownSecP1 = sec1;
        m_prevShownSecP2 = sec2;
        emit timeUpdated();
    }
}

/*
void ShogiClock::updateClock()
{
    if (!m_clockRunning) return;

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
                        remMs = byoMs - overshoot;  // メイン→秒読みへ
                        byoApplied = true;
                    } else {
                        // 既に秒読み中で 0 を割った→秒読みも尽きた
                        remMs = 0;
                    }
                    if (remMs <= 0) {
                        // 秒読みにも達した → 投了 OR 0で止める
                        if (m_loseOnTimeout) {
                            m_timer->stop();
                            m_clockRunning = false;
                            if (player == 1) emit player1TimeOut();
                            else             emit player2TimeOut();
                            emit resignationTriggered();
                        } else {
                            remMs = 0; // 表示は 0 のまま継続
                        }
                        emit timeUpdated();
                        return true;
                    }
                } else {
                    // 秒読みなし
                    remMs = 0;
                    if (m_loseOnTimeout) {
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
            return false; // 継続
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

    // Debug 不変条件
    debugCheckInvariants();

    // 秒が変わったときだけ通知
    const qint64 sec1 = qMax<qint64>(0, (m_player1TimeMs + 999) / 1000);
    const qint64 sec2 = qMax<qint64>(0, (m_player2TimeMs + 999) / 1000);
    if (sec1 != m_prevShownSecP1 || sec2 != m_prevShownSecP2) {
        m_prevShownSecP1 = sec1;
        m_prevShownSecP2 = sec2;
        emit timeUpdated();
    }
}
*/

QString ShogiClock::getPlayer1TimeString() const
{
    const qint64 totalSeconds = qMax<qint64>(0, (m_player1TimeMs + 999) / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString::asprintf("%02lld:%02lld:%02lld",
                             static_cast<long long>(hours),
                             static_cast<long long>(minutes),
                             static_cast<long long>(seconds));
}

QString ShogiClock::getPlayer2TimeString() const
{
    const qint64 totalSeconds = qMax<qint64>(0, (m_player2TimeMs + 999) / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString::asprintf("%02lld:%02lld:%02lld",
                             static_cast<long long>(hours),
                             static_cast<long long>(minutes),
                             static_cast<long long>(seconds));
}

qint64 ShogiClock::getPlayer1TimeIntMs() const { return m_player1TimeMs; }
qint64 ShogiClock::getPlayer2TimeIntMs() const { return m_player2TimeMs; }

QString ShogiClock::getPlayer1ConsiderationTime() const
{
    const qint64 totalSeconds = qMax<qint64>(0, m_player1ConsiderationTimeMs / 1000);
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString::asprintf("%02lld:%02lld",
                             static_cast<long long>(minutes),
                             static_cast<long long>(seconds));
}

QString ShogiClock::getPlayer2ConsiderationTime() const
{
    const qint64 totalSeconds = qMax<qint64>(0, m_player2ConsiderationTimeMs / 1000);
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString::asprintf("%02lld:%02lld",
                             static_cast<long long>(minutes),
                             static_cast<long long>(seconds));
}

QString ShogiClock::getPlayer1TotalConsiderationTime() const
{
    const qint64 totalSeconds = qMax<qint64>(0, m_player1TotalConsiderationTimeMs / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString::asprintf("%02lld:%02lld:%02lld",
                             static_cast<long long>(hours),
                             static_cast<long long>(minutes),
                             static_cast<long long>(seconds));
}

QString ShogiClock::getPlayer2TotalConsiderationTime() const
{
    const qint64 totalSeconds = qMax<qint64>(0, m_player2TotalConsiderationTimeMs / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    return QString::asprintf("%02lld:%02lld:%02lld",
                             static_cast<long long>(hours),
                             static_cast<long long>(minutes),
                             static_cast<long long>(seconds));
}

QString ShogiClock::getPlayer1ConsiderationAndTotalTime() const
{
    return getPlayer1ConsiderationTime() + "/" + getPlayer1TotalConsiderationTime();
}

QString ShogiClock::getPlayer2ConsiderationAndTotalTime() const
{
    return getPlayer2ConsiderationTime() + "/" + getPlayer2TotalConsiderationTime();
}

void ShogiClock::saveState()
{
    qCDebug(lcShogiClock) << "saveState p1=" << m_player1TimeMs
                          << "p2=" << m_player2TimeMs
                          << "c1=" << m_player1ConsiderationTimeMs
                          << "c2=" << m_player2ConsiderationTimeMs
                          << "t1=" << m_player1TotalConsiderationTimeMs
                          << "t2=" << m_player2TotalConsiderationTimeMs
                          << "byo1Applied=" << m_byoyomi1Applied
                          << "byo2Applied=" << m_byoyomi2Applied;

    m_player1TimeHistory.push(m_player1TimeMs);
    m_player2TimeHistory.push(m_player2TimeMs);
    m_player1ConsiderationHistory.push(m_player1ConsiderationTimeMs);
    m_player2ConsiderationHistory.push(m_player2ConsiderationTimeMs);
    m_player1TotalConsiderationHistory.push(m_player1TotalConsiderationTimeMs);
    m_player2TotalConsiderationHistory.push(m_player2TotalConsiderationTimeMs);
    m_byoyomi1AppliedHistory.push(m_byoyomi1Applied);
    m_byoyomi2AppliedHistory.push(m_byoyomi2Applied);
}

void ShogiClock::undo()
{
    // 2手戻す（あなたの元実装に合わせる）
    if (m_player1TimeHistory.size() >= 3 &&
        m_player2TimeHistory.size() >= 3 &&
        m_player1ConsiderationHistory.size() >= 3 &&
        m_player2ConsiderationHistory.size() >= 3 &&
        m_player1TotalConsiderationHistory.size() >= 3 &&
        m_player2TotalConsiderationHistory.size() >= 3 &&
        m_byoyomi1AppliedHistory.size() >= 3 &&
        m_byoyomi2AppliedHistory.size() >= 3) {

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

        emit timeUpdated();
    }
}

void ShogiClock::debugCheckInvariants() const
{
#ifndef NDEBUG
    // 時間は負にならない
    Q_ASSERT(m_player1TimeMs >= 0);
    Q_ASSERT(m_player2TimeMs >= 0);

    // byoyomi 方式のとき increment は 0 のはず（setPlayerTimes の仕様）
    if (m_byoyomi1TimeMs > 0 || m_byoyomi2TimeMs > 0) {
        Q_ASSERT(m_bincMs == 0 && m_wincMs == 0);
    }

    // byoyomiApplied は「byoyomi 秒数が設定されているプレイヤー」に対してのみ true になり得る
    if (!m_byoyomi1TimeMs) Q_ASSERT(!m_byoyomi1Applied);
    if (!m_byoyomi2TimeMs) Q_ASSERT(!m_byoyomi2Applied);

    // 表示キャッシュの整合
    Q_ASSERT(m_prevShownSecP1 >= -1);
    Q_ASSERT(m_prevShownSecP2 >= -1);
#endif
}

void ShogiClock::setPlayer1ConsiderationTime(int newPlayer1ConsiderationTimeMs)
{
    m_player1ConsiderationTimeMs = static_cast<qint64>(qMax(0, newPlayer1ConsiderationTimeMs));
}

void ShogiClock::setPlayer2ConsiderationTime(int newPlayer2ConsiderationTimeMs)
{
    m_player2ConsiderationTimeMs = static_cast<qint64>(qMax(0, newPlayer2ConsiderationTimeMs));
}
