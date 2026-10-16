/// @file engineanalysisrunner.cpp
/// @brief 登録エンジンで 1 局面を一定時間解析する実行器の実装

#include "engineanalysisrunner.h"

#include "usienginesession.h"
#include "usiprotocolhandler.h"

namespace {
constexpr int kBestmoveGraceMs = 10000; ///< stop 送信後に bestmove を待つ上限
}

EngineAnalysisRunner::EngineAnalysisRunner(QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
{
    connect(m_session, &UsiEngineSession::ready, this, &EngineAnalysisRunner::onSessionReady);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &EngineAnalysisRunner::onSessionError);
    connect(m_session, &UsiEngineSession::searchTimedOut, this, &EngineAnalysisRunner::onNoResponse);
    connect(m_session->handler(), &UsiProtocolHandler::infoReceived, this, &EngineAnalysisRunner::onInfo);
    connect(m_session->handler(), &UsiProtocolHandler::bestMoveReceived, this, &EngineAnalysisRunner::onBestMove);
}

bool EngineAnalysisRunner::start(const Request& request, QString* error)
{
    m_latest.clear();
    m_done = false;
    m_request = request;
    m_searchStarted = false;
    m_elapsed.invalidate();
    return m_session->startAsync(request.enginePath, request.engineName, error);
}

void EngineAnalysisRunner::onSessionReady()
{
    if (m_done) return;
    m_searchStarted = true;
    m_session->handler()->sendSetOption(QStringLiteral("MultiPV"), QString::number(qMax(1, m_request.multiPv)));
    UsiSearchRequest search;
    search.position = m_request.position;
    search.go = UsiGo::infinite();
    search.stopAfterMs = qMax(100, m_request.thinkMs);
    search.replyTimeoutMs = kBestmoveGraceMs;
    m_session->search(search);
    m_elapsed.start();
}

void EngineAnalysisRunner::stop()
{
    if (m_done) return;
    if (!m_searchStarted) {
        finish({}, {});
        return;
    }
    m_session->stop();
}

void EngineAnalysisRunner::onInfo(const UsiInfo& info)
{
    if (m_done || !info.hasPv()) return;
    m_latest.insert(info.multipv, info);
    emit lineUpdated(info);
}

void EngineAnalysisRunner::onBestMove(const UsiBestMove& result)
{
    if (m_done) return;
    finish(result.move, result.ponder);
}

void EngineAnalysisRunner::onNoResponse()
{
    if (m_done) return;
    m_done = true;
    m_session->quit();
    emit errorOccurred(QStringLiteral("The engine did not answer bestmove after stop"));
}

void EngineAnalysisRunner::onSessionError(const QString& message)
{
    if (m_done) return;
    m_done = true;
    m_session->quit();
    emit errorOccurred(message);
}

void EngineAnalysisRunner::finish(const QString& bestmove, const QString& ponder)
{
    m_done = true;
    m_session->quit();
    emit finished(bestmove, ponder);
}
