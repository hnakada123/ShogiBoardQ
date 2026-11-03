/// @file csagameseries.cpp
/// @brief 通信対局の対局数と終局後の再接続の実装

#include "csagameseries.h"

#include <QTimer>

CsaGameSeries::CsaGameSeries(QObject* parent)
    : QObject(parent)
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(kReconnectDelayMs);
    connect(m_reconnectTimer, &QTimer::timeout, this, &CsaGameSeries::reconnectDue);
}

void CsaGameSeries::begin(int gameLimit, bool reconnectAfterGame)
{
    m_reconnectTimer->stop();
    m_gameLimit = gameLimit;
    m_completedGames = 0;
    m_reconnectAfterGame = reconnectAfterGame;
    m_lastEndedByServer = false;
}

bool CsaGameSeries::scheduleReconnect()
{
    if (!m_reconnectAfterGame || !m_lastEndedByServer || !moreGamesWanted()) return false;
    m_reconnectTimer->start();
    return true;
}

bool CsaGameSeries::isReconnectPending() const
{
    return m_reconnectTimer->isActive();
}

void CsaGameSeries::stop()
{
    m_reconnectTimer->stop();
}
