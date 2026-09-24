#include "tsumeshogigenerator.h"
#include "usi.h"

#include <algorithm>

void TsumeshogiGenerator::startVerification(const QString& sfen, Phase origin)
{
    m_verificationOrigin = origin;
    m_verificationSfen = sfen;
    m_verificationQueries = 0;
    m_verificationAwaiting = false;
    m_verificationElapsed.start();
    m_verifier.start(sfen, m_settings.targetMoves);
    m_phase = Phase::Verifying;
    emit verificationProgress(0);
    m_verificationStepTimer.start(0);
}

void TsumeshogiGenerator::submitVerification(TsumeshogiVerifier::Reply reply, const QStringList& pv)
{
    if (!m_verificationAwaiting) return;
    m_verificationAwaiting = false;
    m_verifier.submit(reply, pv);
    m_verificationStepTimer.start(0);
}

void TsumeshogiGenerator::continueVerification()
{
    if (m_phase != Phase::Verifying || m_verificationAwaiting) return;
    if (m_verificationElapsed.elapsed() >= m_settings.timeoutMs) m_verifier.abort();

    const QString sfen = m_verifier.nextPosition();
    if (m_verifier.result().status != TsumeshogiVerifier::Status::Running) {
        finishVerification();
        return;
    }
    if (sfen.isEmpty()) {
        m_verificationStepTimer.start(0);
        return;
    }
    const qint64 remaining = m_settings.timeoutMs - m_verificationElapsed.elapsed();
    if (remaining <= 0) {
        m_verifier.abort();
        finishVerification();
        return;
    }
    // 難しい1問だけで総予算を使い切らず、他の攻手の余詰も検出する。
    const int timeout = static_cast<int>(std::min<qint64>(remaining, 1000));
    m_verificationAwaiting = true;
    emit verificationProgress(++m_verificationQueries);
    QString position = QStringLiteral("position sfen ") + sfen;
    m_usi->sendPositionAndGoMateCommands(timeout, position);
    m_safetyTimer.start(timeout + 5000);
}

void TsumeshogiGenerator::finishVerification()
{
    const auto result = m_verifier.result();
    m_phase = m_verificationOrigin;
    if (result.status != TsumeshogiVerifier::Status::Unique) {
        ++m_verificationRejected;
        if (result.status == TsumeshogiVerifier::Status::Unknown)
            ++m_verificationInconclusive;
        emit verificationStatsUpdated(m_verificationRejected, m_verificationInconclusive);
        if (m_phase == Phase::Searching) emit searchPhaseStarted();
        advanceAfterFailure();
        return;
    }

    m_verifiedSfen = m_verificationSfen;
    m_verifiedPv = result.pv;
    if (m_phase == Phase::Searching) {
        startTrimmingPhase(m_verifiedSfen, m_verifiedPv);
    } else {
        m_trimBaseSfen = m_verifiedSfen;
        m_trimBasePv = m_verifiedPv;
        m_trimCandidates = enumerateRemovablePieces(m_trimBaseSfen);
        m_trimCandidateIndex = 0;
        tryNextTrimCandidate();
    }
}
