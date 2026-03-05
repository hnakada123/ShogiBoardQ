/// @file shogiclock_format.cpp
/// @brief 将棋時計の表示文字列関連実装

#include "shogiclock.h"

#include <QtGlobal>

QString ShogiClock::getPlayer1TimeString() const
{
    const int rs = remainingDisplaySecP1();
    const int h = rs / 3600;
    const int m = (rs % 3600) / 60;
    const int s = rs % 60;
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
}

QString ShogiClock::getPlayer2TimeString() const
{
    const int rs = remainingDisplaySecP2();
    const int h = rs / 3600;
    const int m = (rs % 3600) / 60;
    const int s = rs % 60;
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
    const int h = totalSec / 3600;
    const int m = (totalSec % 3600) / 60;
    const int s = totalSec % 60;
    return QString::asprintf("%02d:%02d:%02d", h, m, s);
}

QString ShogiClock::getPlayer2TotalConsiderationTime() const
{
    const int totalSec = static_cast<int>(qMax<qint64>(0, m_player2TotalConsiderationTimeMs) / 1000);
    const int h = totalSec / 3600;
    const int m = (totalSec % 3600) / 60;
    const int s = totalSec % 60;
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
