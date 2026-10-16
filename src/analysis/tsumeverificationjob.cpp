/// @file tsumeverificationjob.cpp
/// @brief 唯一解の検査を総予算つきの問い合わせとして進める処理の実装

#include "tsumeverificationjob.h"

#include <algorithm>

TsumeVerificationJob::TsumeVerificationJob(const QString& sfen, int targetMoves,
                                           const TsumeshogiVerifier::Options& options, int budgetMs)
    : m_sfen(sfen)
    , m_budgetMs(budgetMs)
{
    m_verifier.start(sfen, targetMoves, options);
}

std::optional<MateQuery> TsumeVerificationJob::nextQuery()
{
    if (m_spentMs >= m_budgetMs) m_verifier.abort();
    const QString sfen = m_verifier.nextPosition();
    if (isDone() || sfen.isEmpty()) return std::nullopt;
    const qint64 remaining = m_budgetMs - m_spentMs;
    if (remaining <= 0) {
        m_verifier.abort();
        return std::nullopt;
    }
    ++m_queries;
    return MateQuery{sfen, static_cast<int>(std::min<qint64>(remaining, kQueryMaxMs))};
}

void TsumeVerificationJob::submit(const MateAnswer& answer, qint64 elapsedMs)
{
    m_spentMs += std::max<qint64>(0, elapsedMs);
    switch (answer.kind) {
    case MateAnswer::Kind::Mate: m_verifier.submit(TsumeshogiVerifier::Reply::Mate, answer.pv); return;
    case MateAnswer::Kind::NoMate: m_verifier.submit(TsumeshogiVerifier::Reply::NoMate); return;
    case MateAnswer::Kind::Unknown:
    case MateAnswer::Kind::Unsupported: m_verifier.submit(TsumeshogiVerifier::Reply::Unknown); return;
    }
}

bool TsumeVerificationJob::isDone() const
{
    return m_verifier.result().status != TsumeshogiVerifier::Status::Running;
}
