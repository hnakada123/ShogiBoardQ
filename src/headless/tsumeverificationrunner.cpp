/// @file tsumeverificationrunner.cpp
/// @brief 余詰検査実行器の実装

#include "tsumeverificationrunner.h"

#include "mateoracle.h"
#include "matequeryloop.h"
#include "tsumeverificationjob.h"

TsumeVerificationRunner::TsumeVerificationRunner(QObject* parent)
    : QObject(parent)
    , m_oracle(new MateOracle(this))
    , m_loop(new MateQueryLoop(m_oracle, this))
{
    connect(m_oracle, &MateOracle::ready, this, &TsumeVerificationRunner::onReady);
    connect(m_oracle, &MateOracle::errorOccurred, this, &TsumeVerificationRunner::onEngineError);
    connect(m_oracle, &MateOracle::timedOut, this, &TsumeVerificationRunner::onNoResponse);
    connect(m_loop, &MateQueryLoop::queryStarted, this, &TsumeVerificationRunner::onQueryStarted);
    connect(m_loop, &MateQueryLoop::finished, this, &TsumeVerificationRunner::onFinished);
    connect(m_loop, &MateQueryLoop::unsupported, this, &TsumeVerificationRunner::onUnsupported);
}

TsumeVerificationRunner::~TsumeVerificationRunner() = default;

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

bool TsumeVerificationRunner::start(const Request& request)
{
    m_done = false;
    m_job = std::make_unique<TsumeVerificationJob>(
        request.sfen, request.targetMoves, TsumeshogiVerifier::Options{request.allowFinalMoveAlternatives, false},
        request.timeoutMs);
    if (m_job->result().status == TsumeshogiVerifier::Status::Invalid) return false;
    m_elapsed.invalidate();
    // エンジンの起動の失敗は errorOccurred で届く
    m_oracle->start(request.enginePath, request.engineName);
    return true;
}

void TsumeVerificationRunner::onReady()
{
    if (m_done || !m_job) return;
    m_elapsed.start();
    m_loop->run(m_job.get());
}

void TsumeVerificationRunner::abort()
{
    if (m_done || !m_job) return;
    m_job->abort();
    // 答えを待っている問い合わせがあれば、その答え（Unknown）が届いてから終える
    if (!m_oracle->isAsking()) {
        m_loop->cancel();
        onFinished();
        return;
    }
    m_oracle->stop();
}

void TsumeVerificationRunner::onQueryStarted()
{
    if (m_job) emit progress(m_job->queries());
}

void TsumeVerificationRunner::onFinished()
{
    if (m_done || !m_job) return;
    m_done = true;
    m_oracle->quit();
    const TsumeshogiVerifier::Result result = m_job->result();
    emit finished(result.status, result.pv, m_job->queries(), m_elapsed.isValid() ? m_elapsed.elapsed() : 0);
}

void TsumeVerificationRunner::onUnsupported()
{
    fail(QStringLiteral("The engine does not support go mate"));
}

void TsumeVerificationRunner::onNoResponse()
{
    // stop にも応答しない → エンジンが固まっているとみなす
    fail(QStringLiteral("The engine stopped responding during verification"));
}

void TsumeVerificationRunner::onEngineError(const QString& message)
{
    fail(message);
}

void TsumeVerificationRunner::fail(const QString& message)
{
    if (m_done) return;
    m_done = true;
    m_loop->cancel();
    m_oracle->quit();
    emit errorOccurred(message);
}
