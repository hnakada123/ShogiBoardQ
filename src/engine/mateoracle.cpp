/// @file mateoracle.cpp
/// @brief エンジンへの詰みの問い合わせの実装

#include "mateoracle.h"

#include "usienginesession.h"
#include "usiprotocolhandler.h"

MateAnswer MateAnswer::fromCheckmate(const UsiCheckmate& result)
{
    if (result.stopped) return {Kind::Unknown, {}};
    switch (result.result) {
    case UsiCheckmate::Result::Mate: return {Kind::Mate, result.pv};
    case UsiCheckmate::Result::NoMate: return {Kind::NoMate, {}};
    case UsiCheckmate::Result::NotImplemented: return {Kind::Unsupported, {}};
    case UsiCheckmate::Result::Unknown: break;
    }
    return {Kind::Unknown, {}};
}

MateOracle::MateOracle(QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
{
    connect(m_session, &UsiEngineSession::ready, this, &MateOracle::onReady);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &MateOracle::onError);
    connect(m_session, &UsiEngineSession::searchTimedOut, this, &MateOracle::onTimedOut);
    connect(m_session->handler(), &UsiProtocolHandler::checkmateReceived, this, &MateOracle::onCheckmate);
}

MateOracle::~MateOracle()
{
    quit();
}

bool MateOracle::start(const QString& enginePath, const QString& engineName, QString* error)
{
    m_asking = false;
    m_ready = false;
    m_started = m_session->startAsync(enginePath, engineName, error);
    return m_started;
}

void MateOracle::quit()
{
    m_started = false;
    m_ready = false;
    m_asking = false;
    m_session->quit();
}

bool MateOracle::isReady() const
{
    return m_ready && !m_session->isInitializing();
}

void MateOracle::ask(const UsiPosition& position, int limitMs)
{
    if (!m_started) return;
    m_asking = true;
    m_session->search(UsiSearchRequest::mate(position, limitMs));
}

void MateOracle::askWithin(const UsiPosition& position, int deadlineMs)
{
    if (!m_started) return;
    m_asking = true;
    UsiSearchRequest request;
    request.position = position;
    request.go = UsiGo::mate(0);
    request.replyTimeoutMs = qMax(1, deadlineMs);
    m_session->search(request);
}

void MateOracle::stop()
{
    if (!m_asking) return;
    if (m_session->isInitializing()) {
        // まだ送っていない問い合わせを取りやめる（エンジンからは何も届かない）
        m_session->stop();
        m_asking = false;
        emit answered({MateAnswer::Kind::Unknown, {}});
        return;
    }
    m_session->stop();
}

void MateOracle::onReady()
{
    if (!m_started) return;
    m_ready = true;
    emit ready();
}

void MateOracle::onCheckmate(const UsiCheckmate& result)
{
    if (!m_asking) return;
    m_asking = false;
    emit answered(MateAnswer::fromCheckmate(result));
}

void MateOracle::onTimedOut()
{
    if (!m_asking) return;
    m_asking = false;
    emit timedOut();
}

void MateOracle::onError(const QString& message)
{
    if (!m_started) return;
    m_started = false;
    m_ready = false;
    m_asking = false;
    emit errorOccurred(message);
}
