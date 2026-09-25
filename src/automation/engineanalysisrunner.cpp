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
    m_thinkTimer.setSingleShot(true);
    m_noResponseTimer.setSingleShot(true);
    connect(&m_thinkTimer, &QTimer::timeout, this, &EngineAnalysisRunner::onThinkTimeout);
    connect(&m_noResponseTimer, &QTimer::timeout, this, &EngineAnalysisRunner::onNoResponse);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &EngineAnalysisRunner::onSessionError);
    connect(m_session->handler(), &UsiProtocolHandler::infoLineReceived,
            this, &EngineAnalysisRunner::onInfoLine);
    connect(m_session->handler(), &UsiProtocolHandler::bestMoveReceived,
            this, &EngineAnalysisRunner::onBestMove);
}

bool EngineAnalysisRunner::start(const Request& request, QString* error)
{
    m_latest.clear();
    m_done = false;
    if (!m_session->start(request.enginePath, request.engineName, error)) return false;

    UsiProtocolHandler* handler = m_session->handler();
    handler->sendSetOption(QStringLiteral("MultiPV"), QString::number(qMax(1, request.multiPv)));
    handler->sendPosition(request.positionCommand);
    handler->sendRaw(QStringLiteral("go infinite"));
    m_elapsed.start();
    m_thinkTimer.start(qMax(100, request.thinkMs));
    return true;
}

void EngineAnalysisRunner::stop()
{
    if (m_done) return;
    m_thinkTimer.stop();
    onThinkTimeout();
}

void EngineAnalysisRunner::onInfoLine(const QString& line)
{
    if (m_done) return;
    const UsiInfoLine info = UsiInfoLineParser::parse(line);
    if (!info.hasPv()) return;
    m_latest.insert(info.multipv, info);
    emit lineUpdated(info);
}

void EngineAnalysisRunner::onThinkTimeout()
{
    if (m_done || !m_session->isRunning()) return;
    m_session->handler()->sendStop();
    m_noResponseTimer.start(kBestmoveGraceMs);
}

void EngineAnalysisRunner::onBestMove()
{
    if (m_done) return;
    UsiProtocolHandler* handler = m_session->handler();
    finish(handler->bestMove(), handler->predictedMove());
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
    m_thinkTimer.stop();
    m_noResponseTimer.stop();
    m_session->quit();
    emit errorOccurred(message);
}

void EngineAnalysisRunner::finish(const QString& bestmove, const QString& ponder)
{
    m_done = true;
    m_thinkTimer.stop();
    m_noResponseTimer.stop();
    m_session->quit();
    emit finished(bestmove, ponder);
}
