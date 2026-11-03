/// @file csaverdictwait.cpp
/// @brief サーバーの裁定を待つ間の状態の実装

#include "csaverdictwait.h"

#include <QTimer>

CsaVerdictWait::CsaVerdictWait(int timeoutMs, QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(timeoutMs);
    connect(m_timer, &QTimer::timeout, this, &CsaVerdictWait::onTimeout);
}

void CsaVerdictWait::begin(const QString& failureText)
{
    m_failureText = failureText;
    m_timer->start();
}

QString CsaVerdictWait::take()
{
    const QString text = m_failureText;
    clear();
    return text;
}

void CsaVerdictWait::clear()
{
    m_timer->stop();
    m_failureText.clear();
}

void CsaVerdictWait::onTimeout()
{
    if (!isPending()) return;
    emit timedOut(take());
}
