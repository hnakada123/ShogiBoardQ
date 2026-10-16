/// @file matesearchrunner.cpp
/// @brief `go mate` による詰み探索実行器の実装

#include "matesearchrunner.h"

#include "usienginesession.h"
#include "usiprotocolhandler.h"

MateSearchRunner::MateSearchRunner(QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
{
    connect(m_session, &UsiEngineSession::ready, this, &MateSearchRunner::onSessionReady);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &MateSearchRunner::onSessionError);
    connect(m_session, &UsiEngineSession::searchTimedOut, this, &MateSearchRunner::onNoResponse);
    connect(m_session->handler(), &UsiProtocolHandler::checkmateReceived, this, &MateSearchRunner::onCheckmate);
}

QString MateSearchRunner::statusName(Status status)
{
    switch (status) {
    case Status::Mate: return QStringLiteral("mate");
    case Status::NoMate: return QStringLiteral("nomate");
    case Status::Unknown: return QStringLiteral("unknown");
    case Status::NotImplemented: return QStringLiteral("notimplemented");
    }
    return QStringLiteral("unknown");
}

bool MateSearchRunner::start(const Request& request, QString* error)
{
    m_done = false;
    m_request = request;
    m_searchStarted = false;
    m_elapsed.invalidate();
    return m_session->startAsync(request.enginePath, request.engineName, error);
}

void MateSearchRunner::onSessionReady()
{
    if (m_done) return;
    m_searchStarted = true;
    m_session->search(UsiSearchRequest::mate(m_request.position, qMax(100, m_request.timeMs)));
    m_elapsed.start();
}

void MateSearchRunner::stop()
{
    if (m_done) return;
    if (!m_searchStarted) {
        finish(Status::Unknown, {});
        return;
    }
    m_session->stop();
}

void MateSearchRunner::onCheckmate(const UsiCheckmate& result)
{
    switch (result.result) {
    case UsiCheckmate::Result::Mate: finish(Status::Mate, result.pv); return;
    case UsiCheckmate::Result::NoMate: finish(Status::NoMate, {}); return;
    case UsiCheckmate::Result::NotImplemented: finish(Status::NotImplemented, {}); return;
    case UsiCheckmate::Result::Unknown: finish(Status::Unknown, {}); return;
    }
}

void MateSearchRunner::onNoResponse()
{
    finish(Status::Unknown, {});
}

void MateSearchRunner::onSessionError(const QString& message)
{
    if (m_done) return;
    m_done = true;
    m_session->quit();
    emit errorOccurred(message);
}

void MateSearchRunner::finish(Status status, const QStringList& pv)
{
    if (m_done) return;
    m_done = true;
    m_session->quit();
    emit finished(status, pv);
}
