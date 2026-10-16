/// @file tsumeverificationrunner.cpp
/// @brief 余詰検査実行器の実装

#include "tsumeverificationrunner.h"

#include "usienginesession.h"
#include "usiprotocolhandler.h"

#include <algorithm>

namespace {
constexpr int kQueryMaxMs = 1000;   ///< 1 問い合わせの上限（難問 1 つで総予算を使い切らない）
constexpr int kSafetyMarginMs = 5000;
constexpr int kStopResponseMs = 3000;  ///< stop 送信後に応答を待つ上限
}

TsumeVerificationRunner::TsumeVerificationRunner(QObject* parent)
    : QObject(parent)
    , m_session(new UsiEngineSession(this))
{
    m_stepTimer.setSingleShot(true);
    connect(&m_stepTimer, &QTimer::timeout, this, &TsumeVerificationRunner::step);
    connect(m_session, &UsiEngineSession::ready, this, &TsumeVerificationRunner::onSessionReady);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &TsumeVerificationRunner::onSessionError);
    connect(m_session, &UsiEngineSession::searchTimedOut, this, &TsumeVerificationRunner::onNoResponse);
    connect(m_session->handler(), &UsiProtocolHandler::checkmateReceived, this, &TsumeVerificationRunner::onCheckmate);
}

QString TsumeVerificationRunner::statusName(TsumeshogiVerifier::Status status)
{
    switch (status) {
    case TsumeshogiVerifier::Status::Running: return QStringLiteral("running");
    case TsumeshogiVerifier::Status::Unique: return QStringLiteral("unique");
    case TsumeshogiVerifier::Status::Multiple: return QStringLiteral("multiple");
    case TsumeshogiVerifier::Status::NoMate: return QStringLiteral("nomate");
    case TsumeshogiVerifier::Status::WrongLength: return QStringLiteral("wrong_length");
    case TsumeshogiVerifier::Status::Unknown: return QStringLiteral("unknown");
    case TsumeshogiVerifier::Status::Invalid: return QStringLiteral("invalid");
    case TsumeshogiVerifier::Status::Surplus: return QStringLiteral("surplus");
    }
    return QStringLiteral("unknown");
}

bool TsumeVerificationRunner::start(const Request& request, QString* error)
{
    m_request = request;
    m_queries = 0;
    m_awaiting = false;
    m_done = false;
    m_verifier.start(request.sfen, request.targetMoves, {request.allowFinalMoveAlternatives});
    if (m_verifier.result().status == TsumeshogiVerifier::Status::Invalid) {
        if (error) *error = QStringLiteral("The SFEN is not a valid tsume position");
        return false;
    }
    m_elapsed.invalidate();
    return m_session->startAsync(request.enginePath, request.engineName, error);
}

void TsumeVerificationRunner::onSessionReady()
{
    if (m_done) return;
    m_elapsed.start();
    m_stepTimer.start(0);
}

void TsumeVerificationRunner::abort()
{
    if (m_done) return;
    m_verifier.abort();
    if (m_awaiting) {
        m_session->stop();
        return; // 応答を待って finish する
    }
    finish();
}

void TsumeVerificationRunner::step()
{
    if (m_done || m_awaiting) return;
    if (m_elapsed.elapsed() >= m_request.timeoutMs) m_verifier.abort();

    const QString sfen = m_verifier.nextPosition();
    if (m_verifier.result().status != TsumeshogiVerifier::Status::Running) {
        finish();
        return;
    }
    if (sfen.isEmpty()) {
        m_stepTimer.start(0);
        return;
    }
    const qint64 remaining = m_request.timeoutMs - m_elapsed.elapsed();
    if (remaining <= 0) {
        m_verifier.abort();
        finish();
        return;
    }
    const int timeout = static_cast<int>(std::min<qint64>(remaining, kQueryMaxMs));
    m_awaiting = true;
    emit progress(++m_queries);
    // 制限時間を過ぎても応答がなければ stop を送り、その応答（打ち切った探索の結果）は不明として扱う
    UsiSearchRequest search;
    search.position = QStringLiteral("position sfen ") + sfen;
    search.go = UsiGo::mate(timeout);
    search.stopAfterMs = timeout + kSafetyMarginMs;
    search.replyTimeoutMs = kStopResponseMs;
    m_session->search(search);
}

void TsumeVerificationRunner::submit(TsumeshogiVerifier::Reply reply, const QStringList& pv)
{
    if (m_done || !m_awaiting) return;
    m_awaiting = false;
    m_verifier.submit(reply, pv);
    m_stepTimer.start(0);
}

void TsumeVerificationRunner::onCheckmate(const UsiCheckmate& result)
{
    // 無応答で stop を送った後の応答（打ち切った探索の結果）は結果として扱わない
    if (result.stopped) {
        submit(TsumeshogiVerifier::Reply::Unknown);
        return;
    }
    switch (result.result) {
    case UsiCheckmate::Result::Mate: submit(TsumeshogiVerifier::Reply::Mate, result.pv); return;
    case UsiCheckmate::Result::NoMate: submit(TsumeshogiVerifier::Reply::NoMate); return;
    case UsiCheckmate::Result::Unknown: submit(TsumeshogiVerifier::Reply::Unknown); return;
    case UsiCheckmate::Result::NotImplemented:
        if (m_done) return;
        m_done = true;
        m_session->quit();
        emit errorOccurred(QStringLiteral("The engine does not support go mate"));
        return;
    }
}

void TsumeVerificationRunner::onNoResponse()
{
    // stop にも応答しない → エンジンが固まっているとみなす
    if (m_done) return;
    m_done = true;
    m_stepTimer.stop();
    m_session->quit();
    emit errorOccurred(QStringLiteral("The engine stopped responding during verification"));
}

void TsumeVerificationRunner::onSessionError(const QString& message)
{
    if (m_done) return;
    m_done = true;
    m_stepTimer.stop();
    m_session->quit();
    emit errorOccurred(message);
}

void TsumeVerificationRunner::finish()
{
    if (m_done) return;
    m_done = true;
    m_stepTimer.stop();
    m_session->quit();
    const auto& result = m_verifier.result();
    emit finished(result.status, result.pv, m_queries, m_elapsed.elapsed());
}
