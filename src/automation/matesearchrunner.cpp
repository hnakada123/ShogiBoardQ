/// @file matesearchrunner.cpp
/// @brief `go mate` による詰み探索実行器の実装

#include "matesearchrunner.h"

#include "usienginesession.h"
#include "usiprotocolhandler.h"

namespace {
constexpr int kSafetyMarginMs = 5000;
constexpr int kStopResponseMs = 3000;
}

MateSearchRunner::MateSearchRunner(QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
{
    m_safetyTimer.setSingleShot(true);
    connect(&m_safetyTimer, &QTimer::timeout, this, &MateSearchRunner::onSafetyTimeout);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &MateSearchRunner::onSessionError);
    UsiProtocolHandler* handler = m_session->handler();
    connect(handler, &UsiProtocolHandler::checkmateSolved, this, &MateSearchRunner::onSolved);
    connect(handler, &UsiProtocolHandler::checkmateNoMate, this, &MateSearchRunner::onNoMate);
    connect(handler, &UsiProtocolHandler::checkmateNotImplemented, this, &MateSearchRunner::onNotImplemented);
    connect(handler, &UsiProtocolHandler::checkmateUnknown, this, &MateSearchRunner::onUnknown);
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
    m_stopSent = false;
    if (!m_session->start(request.enginePath, request.engineName, error)) return false;
    UsiProtocolHandler* handler = m_session->handler();
    handler->sendPosition(request.positionCommand);
    handler->sendGoMate(qMax(100, request.timeMs));
    m_elapsed.start();
    m_safetyTimer.start(qMax(100, request.timeMs) + kSafetyMarginMs);
    return true;
}

void MateSearchRunner::stop()
{
    if (m_done || m_stopSent) return;
    m_stopSent = true;
    m_session->handler()->sendStop();
    m_safetyTimer.start(kStopResponseMs);
}

void MateSearchRunner::onSolved(const QStringList& pv) { finish(Status::Mate, pv); }
void MateSearchRunner::onNoMate() { finish(Status::NoMate, {}); }
void MateSearchRunner::onNotImplemented() { finish(Status::NotImplemented, {}); }
void MateSearchRunner::onUnknown() { finish(Status::Unknown, {}); }

void MateSearchRunner::onSafetyTimeout()
{
    if (m_done) return;
    if (!m_stopSent) {
        // 制限時間を過ぎても応答が無い。stop を送って短く待つ
        stop();
        return;
    }
    finish(Status::Unknown, {});
}

void MateSearchRunner::onSessionError(const QString& message)
{
    if (m_done) return;
    m_done = true;
    m_safetyTimer.stop();
    m_session->quit();
    emit errorOccurred(message);
}

void MateSearchRunner::finish(Status status, const QStringList& pv)
{
    if (m_done) return;
    m_done = true;
    m_safetyTimer.stop();
    m_session->quit();
    emit finished(status, pv);
}
