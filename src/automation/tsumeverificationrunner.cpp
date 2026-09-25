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
    m_safetyTimer.setSingleShot(true);
    connect(&m_stepTimer, &QTimer::timeout, this, &TsumeVerificationRunner::step);
    connect(&m_safetyTimer, &QTimer::timeout, this, &TsumeVerificationRunner::onSafetyTimeout);
    connect(m_session, &UsiEngineSession::errorOccurred, this, &TsumeVerificationRunner::onSessionError);
    UsiProtocolHandler* handler = m_session->handler();
    connect(handler, &UsiProtocolHandler::checkmateSolved, this, &TsumeVerificationRunner::onSolved);
    connect(handler, &UsiProtocolHandler::checkmateNoMate, this, &TsumeVerificationRunner::onNoMate);
    connect(handler, &UsiProtocolHandler::checkmateNotImplemented, this, &TsumeVerificationRunner::onNotImplemented);
    connect(handler, &UsiProtocolHandler::checkmateUnknown, this, &TsumeVerificationRunner::onUnknown);
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
    }
    return QStringLiteral("unknown");
}

bool TsumeVerificationRunner::start(const Request& request, QString* error)
{
    m_request = request;
    m_queries = 0;
    m_awaiting = false;
    m_awaitingStop = false;
    m_done = false;
    m_verifier.start(request.sfen, request.targetMoves, {request.allowFinalMoveAlternatives});
    if (m_verifier.result().status == TsumeshogiVerifier::Status::Invalid) {
        if (error) *error = QStringLiteral("The SFEN is not a valid tsume position");
        return false;
    }
    if (!m_session->start(request.enginePath, request.engineName, error)) return false;
    m_elapsed.start();
    m_stepTimer.start(0);
    return true;
}

void TsumeVerificationRunner::abort()
{
    if (m_done) return;
    m_verifier.abort();
    if (m_awaiting) {
        m_session->handler()->sendStop();
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
    UsiProtocolHandler* handler = m_session->handler();
    handler->sendPosition(QStringLiteral("position sfen ") + sfen);
    handler->sendGoMate(timeout);
    m_safetyTimer.start(timeout + kSafetyMarginMs);
}

void TsumeVerificationRunner::submit(TsumeshogiVerifier::Reply reply, const QStringList& pv)
{
    if (m_done || !m_awaiting) return;
    m_awaiting = false;
    m_safetyTimer.stop();
    if (m_awaitingStop) {
        // 無応答後に届いた応答（stop への応答または遅延した応答）は結果として扱わない
        m_awaitingStop = false;
        m_verifier.submit(TsumeshogiVerifier::Reply::Unknown);
    } else {
        m_verifier.submit(reply, pv);
    }
    m_stepTimer.start(0);
}

void TsumeVerificationRunner::onSolved(const QStringList& pv) { submit(TsumeshogiVerifier::Reply::Mate, pv); }
void TsumeVerificationRunner::onNoMate() { submit(TsumeshogiVerifier::Reply::NoMate); }
void TsumeVerificationRunner::onUnknown() { submit(TsumeshogiVerifier::Reply::Unknown); }

void TsumeVerificationRunner::onNotImplemented()
{
    if (m_done) return;
    m_done = true;
    m_safetyTimer.stop();
    m_session->quit();
    emit errorOccurred(QStringLiteral("The engine does not support go mate"));
}

void TsumeVerificationRunner::onSafetyTimeout()
{
    if (m_done) return;
    if (m_awaitingStop) {
        // stop にも応答しない → エンジンが固まっているとみなす
        m_done = true;
        m_stepTimer.stop();
        m_session->quit();
        emit errorOccurred(QStringLiteral("The engine stopped responding during verification"));
        return;
    }
    // 応答が無い。stop を送って応答を待ち、届いた応答は Unknown として扱う
    m_awaitingStop = true;
    m_session->handler()->sendStop();
    m_safetyTimer.start(kStopResponseMs);
}

void TsumeVerificationRunner::onSessionError(const QString& message)
{
    if (m_done) return;
    m_done = true;
    m_stepTimer.stop();
    m_safetyTimer.stop();
    m_session->quit();
    emit errorOccurred(message);
}

void TsumeVerificationRunner::finish()
{
    if (m_done) return;
    m_done = true;
    m_stepTimer.stop();
    m_safetyTimer.stop();
    m_session->quit();
    const auto& result = m_verifier.result();
    emit finished(result.status, result.pv, m_queries, m_elapsed.elapsed());
}
