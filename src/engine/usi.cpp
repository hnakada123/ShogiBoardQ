/// @file usi.cpp
/// @brief GUI から使う USI エンジン1台の窓口の実装

#include "usi.h"

#include "engineprocessmanager.h"
#include "thinkinginfoformatter.h"
#include "usienginesession.h"

Usi::Usi(QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
    , m_matchHandler(new UsiMatchHandler(m_session, this))
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

void Usi::startEngine(const QString& engineFile, const QString& engineName)
{
    // 起動の失敗は、この呼び出しから戻った後に errorOccurred で知らせる（UsiEngineSession::startAsync）
    m_matchHandler->cancelAsync();
    m_session->startAsync(engineFile, engineName);
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

void Usi::requestBookMove(const UsiPosition& position, const UsiMove& move, int delayMs, const QString& label)
{
    m_session->whenReady([this, position, move, delayMs, label]() {
        m_formatter->presentBookMove(label);
        m_matchHandler->requestBookMove(position, move, delayMs);
    });
}

void Usi::forgetPonder()
{
    m_matchHandler->forgetPonder();
}

void Usi::requestImmediateMove()
{
    if (m_matchHandler->playBookMoveNow()) return;
    // 準備中に頼まれた探索の依頼（requestMatchMove）は初期化の後で届くので、その後ろに並べて止める
    // （始めた直後に stop を送り、指させる）
    m_session->whenReady([this]() { m_session->stop(); });
}

void Usi::sendGameOverCommand(GameOverResult result)
{
    m_session->handler()->sendGameOver(result);
}

// ============================================================
// 検討・棋譜解析・詰み探索
// ============================================================

bool Usi::startConsideration(const UsiPosition& position, int byoyomiMilliSec, int multiPV)
{
    UsiSearchRequest request;
    request.position = position;
    request.go = UsiGo::infinite();
    request.stopAfterMs = qMax(0, byoyomiMilliSec);
    // 候補手の数は前の設定を残さないよう毎回送る（前の探索を止めた後、position の前に）
    request.options = {{QStringLiteral("MultiPV"), QString::number(multiPV)}};
    return m_session->search(request);
}

bool Usi::search(const UsiSearchRequest& request)
{
    return m_session->search(request);
}

bool Usi::startMateSearch(const UsiPosition& position, int timeLimitMs)
{
    // 応答しないエンジンを待ち続けない（CLI の詰み探索と同じ打ち切り方）
    return m_session->search(UsiSearchRequest::mate(position, timeLimitMs));
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
