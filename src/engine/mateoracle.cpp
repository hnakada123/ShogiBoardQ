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

void MateOracle::start(const QString& enginePath, const QString& engineName)
{
    m_asking = false;
    m_started = true;
    // 起動の失敗は、戻った後に errorOccurred で届く
    m_session->startAsync(enginePath, engineName);
}

void MateOracle::quit()
{
    m_started = false;
    m_asking = false;
    m_session->quit();
}

bool MateOracle::ask(const UsiPosition& position, int limitMs)
{
    if (!m_started || !m_session->search(UsiSearchRequest::mate(position, limitMs))) return false;
    m_asking = true;
    return true;
}

bool MateOracle::askWithin(const UsiPosition& position, int deadlineMs)
{
    if (!m_started) return false;
    UsiSearchRequest request;
    request.position = position;
    request.go = UsiGo::mate(0);
    request.replyTimeoutMs = qMax(1, deadlineMs);
    if (!m_session->search(request)) return false;
    m_asking = true;
    return true;
}

void MateOracle::stop()
{
    if (!m_asking) return;
    if (m_session->isInitializing()) {
        // まだ送っていない問い合わせを取りやめる（エンジンからは何も届かない）
        m_session->cancelSearch();
        m_asking = false;
        emit answered({MateAnswer::Kind::Unknown, {}});
        return;
    }
    m_session->stop();
}

void MateOracle::onReady()
{
    if (!m_started) return;
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
    m_asking = false;
    emit errorOccurred(message);
}
