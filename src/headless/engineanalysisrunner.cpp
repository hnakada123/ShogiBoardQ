/// @file engineanalysisrunner.cpp
/// @brief 登録エンジンで局面を順に一定時間ずつ解析する実行器の実装

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

void EngineAnalysisRunner::start(const Request& request)
{
    m_latest.clear();
    m_done = false;
    m_stopRequested = false;
    m_index = 0;
    m_request = request;
    m_elapsed.invalidate();
    // 起動の失敗は errorOccurred で届く
    m_session->startAsync(request.enginePath, request.engineName);
}

void EngineAnalysisRunner::onSessionReady()
{
    if (m_done) return;
    m_session->handler()->sendSetOption(QStringLiteral("MultiPV"), QString::number(qMax(1, m_request.multiPv)));
    if (m_request.positions.isEmpty()) {
        finish(false);
        return;
    }
    searchCurrent();
}

void EngineAnalysisRunner::searchCurrent()
{
    m_latest.clear();
    emit positionStarted(m_index);
    UsiSearchRequest search;
    search.position = m_request.positions.at(m_index);
    search.go = UsiGo::infinite();
    search.stopAfterMs = qMax(100, m_request.thinkMs);
    search.replyTimeoutMs = kBestmoveGraceMs;
    m_elapsed.start();
    if (!m_session->search(search)) fail(QStringLiteral("No position to analyse"));
}

void EngineAnalysisRunner::stop()
{
    if (m_done || m_stopRequested) return;
    m_stopRequested = true;
    // 初期化が終わる前（探索を送る前）なら、そのまま終える
    if (m_session->isInitializing()) {
        finish(true);
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
    emit positionFinished(m_index, result.move, result.ponder);
    if (m_done) return;
    if (m_stopRequested || m_index + 1 >= m_request.positions.size()) {
        finish(m_stopRequested);
        return;
    }
    ++m_index;
    searchCurrent();
}

void EngineAnalysisRunner::onNoResponse()
{
    fail(QStringLiteral("The engine did not answer bestmove after stop"));
}

void EngineAnalysisRunner::onSessionError(const QString& message)
{
    fail(message);
}

void EngineAnalysisRunner::fail(const QString& message)
{
    if (m_done) return;
    m_done = true;
    m_session->quit();
    emit errorOccurred(message);
}

void EngineAnalysisRunner::finish(bool stopped)
{
    m_done = true;
    m_session->quit();
    emit finished(stopped);
}
