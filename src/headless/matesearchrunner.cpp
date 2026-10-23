/// @file matesearchrunner.cpp
/// @brief `go mate` による詰み探索実行器の実装

#include "matesearchrunner.h"

MateSearchRunner::MateSearchRunner(QObject* parent)
    : QObject(parent)
    , m_oracle(new MateOracle(this))
{
    connect(m_oracle, &MateOracle::ready, this, &MateSearchRunner::onReady);
    connect(m_oracle, &MateOracle::answered, this, &MateSearchRunner::onAnswered);
    connect(m_oracle, &MateOracle::timedOut, this, &MateSearchRunner::onNoResponse);
    connect(m_oracle, &MateOracle::errorOccurred, this, &MateSearchRunner::onEngineError);
}

QString MateSearchRunner::kindName(MateAnswer::Kind kind)
{
    switch (kind) {
    case MateAnswer::Kind::Mate: return QStringLiteral("mate");
    case MateAnswer::Kind::NoMate: return QStringLiteral("nomate");
    case MateAnswer::Kind::Unknown: return QStringLiteral("unknown");
    case MateAnswer::Kind::Unsupported: return QStringLiteral("notimplemented");
    }
    return QStringLiteral("unknown");
}

void MateSearchRunner::start(const Request& request)
{
    m_done = false;
    m_elapsed.invalidate();
    // 起動の失敗は errorOccurred で届く。問い合わせは初期化が終わってから送られる
    m_oracle->start(request.enginePath, request.engineName);
    m_oracle->ask(request.position, qMax(100, request.timeMs));
}

void MateSearchRunner::onReady()
{
    m_elapsed.start();
}

void MateSearchRunner::stop()
{
    if (!m_done) m_oracle->stop();
}

void MateSearchRunner::onAnswered(const MateAnswer& answer)
{
    finish(answer);
}

void MateSearchRunner::onNoResponse()
{
    finish({MateAnswer::Kind::Unknown, {}});
}

void MateSearchRunner::onEngineError(const QString& message)
{
    if (m_done) return;
    m_done = true;
    m_oracle->quit();
    emit errorOccurred(message);
}

void MateSearchRunner::finish(const MateAnswer& answer)
{
    if (m_done) return;
    m_done = true;
    m_oracle->quit();
    emit finished(answer);
}
