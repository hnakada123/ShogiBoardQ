/// @file usi.cpp
/// @brief GUI から使う USI エンジン1台の窓口の実装

#include "usi.h"

#include "engineprocessmanager.h"
#include "thinkinginfoformatter.h"
#include "usienginesession.h"

Usi::Usi(QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
    , m_matchHandler(new UsiMatchHandler(m_session->handler(), this))
    , m_formatter(new ThinkingInfoFormatter(this))
{
    UsiProtocolHandler* handler = m_session->handler();
    connect(m_session, &UsiEngineSession::ready, this, &Usi::engineInitialized);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &Usi::onSessionError);
    connect(m_session, &UsiEngineSession::searchTimedOut, this, &Usi::searchTimedOut);
    connect(handler, &UsiProtocolHandler::searchCandidateChanged, this, &Usi::searchCandidateChanged);

    // 思考の表に出す行と情報欄の値
    connect(handler, &UsiProtocolHandler::positionChanged, m_formatter, &ThinkingInfoFormatter::setSearchPosition);
    connect(handler, &UsiProtocolHandler::searchStarted, m_formatter, &ThinkingInfoFormatter::requestClearThinkingInfo);
    connect(handler, &UsiProtocolHandler::infoReceived, m_formatter, &ThinkingInfoFormatter::onInfoReceived);
    connect(handler, &UsiProtocolHandler::checkmateReceived, m_formatter, &ThinkingInfoFormatter::presentCheckmate);
    // bestmove の直前に届いた info を、bestmove を受け取る側より先に表示の行にしておく
    connect(handler, &UsiProtocolHandler::bestMoveReceived, m_formatter, &ThinkingInfoFormatter::flushInfoBuffer);

    // 対局
    connect(m_matchHandler, &UsiMatchHandler::moveReady, this, &Usi::matchMoveReady);
    connect(m_matchHandler, &UsiMatchHandler::resigned, this, &Usi::resigned);
    connect(m_matchHandler, &UsiMatchHandler::declaredWin, this, &Usi::declaredWin);
    connect(m_matchHandler, &UsiMatchHandler::timedOut, this, &Usi::onMatchTimeout);
    connect(m_matchHandler, &UsiMatchHandler::errorOccurred, this, &Usi::onSessionError);
}

Usi::~Usi()
{
    m_session->quit();
}

// ============================================================
// 識別・起動・終了
// ============================================================

void Usi::setLogIdentity(const QString& engineTag, const QString& sideTag, const QString& engineName)
{
    m_engineTag = engineTag;
    m_engineName = engineName.isEmpty() ? QStringLiteral("Engine") : engineName;
    m_session->process()->setLogIdentity(engineTag, sideTag, engineName);
}

bool Usi::startEngine(const QString& engineFile, const QString& engineName)
{
    if (engineFile.isEmpty()) {
        quit();
        emit errorOccurred(tr("Engine file path is empty."));
        return false;
    }
    m_matchHandler->cancelAsync();
    QString error;
    if (!m_session->startAsync(engineFile, engineName, &error)) {
        emit errorOccurred(error);
        return false;
    }
    return true;
}

bool Usi::isInitializing() const
{
    return m_session->isInitializing();
}

bool Usi::isRunning() const
{
    return m_session->isRunning();
}

void Usi::quit(bool clearThinking)
{
    m_matchHandler->cancelAsync();
    m_session->quit();
    if (clearThinking) m_formatter->requestClearThinkingInfo();
}

void Usi::cancelCurrentOperation()
{
    m_matchHandler->cancelAsync();
    m_session->cancelCurrentOperation();
}

void Usi::onSessionError(const QString& message)
{
    quit();
    emit errorOccurred(message);
}

void Usi::onMatchTimeout()
{
    cancelCurrentOperation();
    emit errorOccurred(tr("Timeout waiting for bestmove."));
}

// ============================================================
// 対局
// ============================================================

void Usi::setMatchTimeSource(MatchTimeSource* source)
{
    m_matchHandler->setTimeSource(source);
}

void Usi::requestMatchMove(const UsiPosition& position)
{
    m_session->whenReady([this, position]() { m_matchHandler->requestMove(position); });
}

void Usi::forgetPonder()
{
    m_matchHandler->forgetPonder();
}

void Usi::requestImmediateMove()
{
    // 準備中に stop を送ると保留中の position・go まで取りやめになり、エンジンが指さずに対局が止まる。
    // 保留中の処理の後ろに並べ、探索を始めた直後に止めて指させる
    m_session->whenReady([this]() { m_session->stop(); });
}

void Usi::sendGameOverCommand(GameOverResult result)
{
    m_session->handler()->sendGameOver(result);
}

// ============================================================
// 検討・棋譜解析・詰み探索
// ============================================================

void Usi::startConsideration(const UsiPosition& position, int byoyomiMilliSec, int multiPV)
{
    // 候補手の数は前の設定を残さないよう毎回送る
    sendSetOptionCommand(QStringLiteral("MultiPV"), QString::number(multiPV));
    UsiSearchRequest request;
    request.position = position;
    request.go = UsiGo::infinite();
    request.stopAfterMs = qMax(0, byoyomiMilliSec);
    m_session->search(request);
}

void Usi::search(const UsiSearchRequest& request)
{
    m_session->search(request);
}

void Usi::startMateSearch(const UsiPosition& position, int timeLimitMs)
{
    // 応答しないエンジンを待ち続けない（CLI の詰み探索と同じ打ち切り方）
    m_session->search(UsiSearchRequest::mate(position, timeLimitMs));
}

void Usi::stop()
{
    m_session->stop();
}

void Usi::sendSetOptionCommand(const QString& name, const QString& value)
{
    m_session->whenReady([this, name, value]() { m_session->handler()->sendSetOption(name, value); });
}

void Usi::sendConsoleCommand(const QString& command)
{
    m_session->whenReady([this, command]() { m_session->handler()->sendConsoleCommand(command); });
}

// ============================================================
// 読み筋の表示
// ============================================================

const UsiProtocolHandler::SearchCandidate& Usi::searchCandidate() const
{
    return m_session->handler()->searchCandidate();
}

const EngineProcessManager* Usi::process() const
{
    return m_session->process();
}

const UsiProtocolHandler* Usi::protocol() const
{
    return m_session->handler();
}
