/// @file shogiclock_snapshot.cpp
/// @brief 中断時の時計状態の保存・復元
#include "shogiclock.h"

ShogiClock::Snapshot ShogiClock::pauseAndSnapshot()
{
    stopClock();
    Snapshot state;
    state.timeLimitSet = m_timeLimitSet;
    state.loseOnTimeout = m_loseOnTimeout;
    state.currentPlayer = m_currentPlayer;
    state.player1TimeMs = m_player1TimeMs;
    state.player2TimeMs = m_player2TimeMs;
    state.byoyomi1TimeMs = m_byoyomi1TimeMs;
    state.byoyomi2TimeMs = m_byoyomi2TimeMs;
    state.bincMs = m_bincMs;
    state.wincMs = m_wincMs;
    state.player1ConsiderationTimeMs = m_player1ConsiderationTimeMs;
    state.player2ConsiderationTimeMs = m_player2ConsiderationTimeMs;
    state.player1TotalConsiderationTimeMs = m_player1TotalConsiderationTimeMs;
    state.player2TotalConsiderationTimeMs = m_player2TotalConsiderationTimeMs;
    state.p1LastMoveShownSec = m_p1LastMoveShownSec;
    state.p2LastMoveShownSec = m_p2LastMoveShownSec;
    state.p1PrevShownTotalSec = m_p1PrevShownTotalSec;
    state.p2PrevShownTotalSec = m_p2PrevShownTotalSec;
    state.prevShownSecP1 = m_prevShownSecP1;
    state.prevShownSecP2 = m_prevShownSecP2;
    state.byoyomi1Applied = m_byoyomi1Applied;
    state.byoyomi2Applied = m_byoyomi2Applied;
    state.player1TimeHistory = m_player1TimeHistory;
    state.player2TimeHistory = m_player2TimeHistory;
    state.player1ConsiderationHistory = m_player1ConsiderationHistory;
    state.player2ConsiderationHistory = m_player2ConsiderationHistory;
    state.player1TotalConsiderationHistory = m_player1TotalConsiderationHistory;
    state.player2TotalConsiderationHistory = m_player2TotalConsiderationHistory;
    state.byoyomi1AppliedHistory = m_byoyomi1AppliedHistory;
    state.byoyomi2AppliedHistory = m_byoyomi2AppliedHistory;
    state.p1LastMoveShownSecHistory = m_p1LastMoveShownSecHistory;
    state.p2LastMoveShownSecHistory = m_p2LastMoveShownSecHistory;
    state.p1PrevShownTotalSecHistory = m_p1PrevShownTotalSecHistory;
    state.p2PrevShownTotalSecHistory = m_p2PrevShownTotalSecHistory;
    return state;
}

void ShogiClock::restoreSnapshot(const Snapshot& state)
{
    m_timer->stop();
    m_clockRunning = false;
    m_turnFinished = false;
    m_gameOver = false;
    m_timeLimitSet = state.timeLimitSet;
    m_loseOnTimeout = state.loseOnTimeout;
    m_currentPlayer = state.currentPlayer;
    m_player1TimeMs = state.player1TimeMs;
    m_player2TimeMs = state.player2TimeMs;
    m_byoyomi1TimeMs = state.byoyomi1TimeMs;
    m_byoyomi2TimeMs = state.byoyomi2TimeMs;
    m_bincMs = state.bincMs;
    m_wincMs = state.wincMs;
    m_player1ConsiderationTimeMs = state.player1ConsiderationTimeMs;
    m_player2ConsiderationTimeMs = state.player2ConsiderationTimeMs;
    m_player1TotalConsiderationTimeMs = state.player1TotalConsiderationTimeMs;
    m_player2TotalConsiderationTimeMs = state.player2TotalConsiderationTimeMs;
    m_p1LastMoveShownSec = state.p1LastMoveShownSec;
    m_p2LastMoveShownSec = state.p2LastMoveShownSec;
    m_p1PrevShownTotalSec = state.p1PrevShownTotalSec;
    m_p2PrevShownTotalSec = state.p2PrevShownTotalSec;
    m_prevShownSecP1 = state.prevShownSecP1;
    m_prevShownSecP2 = state.prevShownSecP2;
    m_byoyomi1Applied = state.byoyomi1Applied;
    m_byoyomi2Applied = state.byoyomi2Applied;
    m_player1TimeHistory = state.player1TimeHistory;
    m_player2TimeHistory = state.player2TimeHistory;
    m_player1ConsiderationHistory = state.player1ConsiderationHistory;
    m_player2ConsiderationHistory = state.player2ConsiderationHistory;
    m_player1TotalConsiderationHistory = state.player1TotalConsiderationHistory;
    m_player2TotalConsiderationHistory = state.player2TotalConsiderationHistory;
    m_byoyomi1AppliedHistory = state.byoyomi1AppliedHistory;
    m_byoyomi2AppliedHistory = state.byoyomi2AppliedHistory;
    m_p1LastMoveShownSecHistory = state.p1LastMoveShownSecHistory;
    m_p2LastMoveShownSecHistory = state.p2LastMoveShownSecHistory;
    m_p1PrevShownTotalSecHistory = state.p1PrevShownTotalSecHistory;
    m_p2PrevShownTotalSecHistory = state.p2PrevShownTotalSecHistory;
    m_resumedConsiderationMs[0] = m_resumedConsiderationMs[1] = 0;
    m_resumedConsiderationMs[m_currentPlayer - 1] = m_currentPlayer == 1
        ? m_player1ConsiderationTimeMs : m_player2ConsiderationTimeMs;
    m_resumingTurn = true;
    emit timeUpdated();
}

void ShogiClock::setMeasuredConsiderationTime(int player, qint64 elapsedMs)
{
    const qint64 total = qMax<qint64>(0, elapsedMs) + m_resumedConsiderationMs[player - 1];
    m_resumedConsiderationMs[player - 1] = 0;
    if (player == 1) m_player1ConsiderationTimeMs = total;
    else m_player2ConsiderationTimeMs = total;
}
