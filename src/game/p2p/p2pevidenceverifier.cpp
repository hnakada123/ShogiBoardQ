/// @file p2pevidenceverifier.cpp
/// @brief 証跡の検証の実装

#include "p2pevidenceverifier.h"

#include "p2psignature.h"

namespace P2p {

namespace EvidenceVerifier {

namespace {

bool isRecordDeterminedCause(Cause cause)
{
    return cause == Cause::Checkmate || cause == Cause::Repetition || cause == Cause::PerpetualCheck
        || cause == Cause::MaxPly;
}

void setError(EvidenceReport& report, const QString& text)
{
    if (report.error.isEmpty()) report.error = text;
}

/// 状態の承認の署名を確かめ、両署名付きの手順（State0 から連続）と時計・合法性を確かめる
void verifyStates(const Evidence& e, EvidenceReport& report, std::vector<GameState>& mutual)
{
    report.signaturesValid = true;
    report.recordLegal = true;
    report.clockArithmeticValid = true;
    const QByteArray gameId = report.gameId;
    bool chainEnded = false;  // 片方の承認しかない状態の後に、ほかの状態を置かない
    for (std::size_t i = 0; i < e.states.size(); ++i) {
        const SignedState& s = e.states[i];
        const int ply = static_cast<int>(i);
        if (s.state.gameId != gameId || s.state.ply != static_cast<quint32>(i)) {
            report.recordLegal = false;
            report.failedPly = ply;
            setError(report, QStringLiteral("state %1 does not belong to this game or is out of order").arg(ply));
            return;
        }
        const CborValue approval = stateApproval(gameId, s.state.ply, s.state.hash());
        const bool blackOk = !s.blackApproval.isEmpty()
            && Crypto::verify(e.agreement.blackSessionKey, approval, s.blackApproval);
        const bool whiteOk = !s.whiteApproval.isEmpty()
            && Crypto::verify(e.agreement.whiteSessionKey, approval, s.whiteApproval);
        if ((!s.blackApproval.isEmpty() && !blackOk) || (!s.whiteApproval.isEmpty() && !whiteOk)) {
            report.signaturesValid = false;
            report.failedPly = ply;
            setError(report, QStringLiteral("invalid approval signature at ply %1").arg(ply));
            return;
        }
        if (chainEnded || (!blackOk && !whiteOk)) {
            report.recordLegal = false;
            report.failedPly = ply;
            setError(report, QStringLiteral("state %1 follows an unconfirmed state or has no approval").arg(ply));
            return;
        }

        // 手順・合法性・時計の算術（片方だけの承認の状態も、前の状態から正しく作られていること）
        if (i == 0) {
            if (s.state != GameRules::initialState(gameId, e.timeControl)) {
                report.recordLegal = false;
                report.failedPly = 0;
                setError(report, QStringLiteral("State0 is not the initial position"));
                return;
            }
        } else {
            switch (GameRules::checkNext(e.states[i - 1].state, s.state, e.timeControl)) {
            case GameRules::StateCheck::Ok:
                break;
            case GameRules::StateCheck::ClockMismatch:
                report.clockArithmeticValid = false;
                report.failedPly = ply;
                setError(report, QStringLiteral("clock arithmetic does not match at ply %1").arg(ply));
                return;
            default:
                report.recordLegal = false;
                report.failedPly = ply;
                setError(report, QStringLiteral("illegal or inconsistent state at ply %1").arg(ply));
                return;
            }
        }

        if (blackOk && whiteOk) {
            mutual.push_back(s.state);
            report.lastMutualPly = ply;
        } else {
            chainEnded = true;
        }
    }
}

} // namespace

EvidenceReport verify(QByteArrayView bytes)
{
    QString error;
    const std::optional<Evidence> evidence = Evidence::decode(bytes, &error);
    if (!evidence) {
        EvidenceReport report;
        report.error = error;
        return report;
    }
    return verify(*evidence);
}

EvidenceReport verify(const Evidence& e)
{
    EvidenceReport report;
    report.formatValid = true;
    report.gameId = e.gameId();
    report.networkId = e.agreement.networkId;
    for (const NetworkInfo& info : allowedNetworks()) {
        if (networkId(QByteArray(info.name)) == report.networkId) report.networkName = QString::fromLatin1(info.name);
    }
    report.blackPlayerKey = e.agreement.blackPlayerKey;
    report.whitePlayerKey = e.agreement.whitePlayerKey;
    report.timeControl = e.timeControl;
    report.stateCount = static_cast<int>(e.states.size());

    report.conditionsMatch = e.agreement.rulesHash == Rules::v1Hash()
        && e.agreement.timeControlHash == e.timeControl.hash()
        && e.agreement.networkPolicyHash == NetworkPolicy::v1().hash();
    if (!report.conditionsMatch) setError(report, QStringLiteral("conditions do not match the start agreement"));

    const CborValue agreement = e.agreement.toCbor();
    report.blackStartSigned = Crypto::verify(e.agreement.blackPlayerKey, agreement, e.blackStartSignature);
    report.whiteStartSigned = Crypto::verify(e.agreement.whitePlayerKey, agreement, e.whiteStartSignature);
    if (!report.blackStartSigned || !report.whiteStartSigned) setError(report, QStringLiteral("missing or invalid start signature"));

    std::vector<GameState> mutual;
    verifyStates(e, report, mutual);
    const bool recordOk = report.signaturesValid && report.recordLegal && report.clockArithmeticValid;
    if (recordOk && !mutual.empty()) report.terminal = GameRules::judge(mutual);

    report.result = e.result;
    if (e.result) {
        const FinalResult& r = *e.result;
        report.resultDigest = r.digest();
        report.resultMatchesRecord = recordOk && !mutual.empty() && r.gameId == report.gameId
            && r.ply == mutual.back().ply && r.finalStateHash == mutual.back().hash()
            && r.transcriptHash == transcriptHash(report.gameId, mutual);
        if (!report.resultMatchesRecord) setError(report, QStringLiteral("final result does not refer to the last confirmed state"));
        const CborValue resultValue = r.toCbor();
        report.blackResultSigned = !e.blackResultSignature.isEmpty()
            && Crypto::verify(e.agreement.blackPlayerKey, resultValue, e.blackResultSignature);
        report.whiteResultSigned = !e.whiteResultSignature.isEmpty()
            && Crypto::verify(e.agreement.whitePlayerKey, resultValue, e.whiteResultSignature);
        if ((!e.blackResultSignature.isEmpty() && !report.blackResultSigned)
            || (!e.whiteResultSignature.isEmpty() && !report.whiteResultSigned)) {
            report.signaturesValid = false;
            setError(report, QStringLiteral("invalid result signature"));
        }
        // 棋譜から決まる終局と、署名された結果の食い違い
        if (report.terminal) {
            report.resultConflictsWithRecord = r.outcome != report.terminal->outcome || r.cause != report.terminal->cause;
        } else {
            report.resultConflictsWithRecord = isRecordDeterminedCause(r.cause);
        }
        if (report.resultConflictsWithRecord) setError(report, QStringLiteral("signed result conflicts with the record"));
    }

    const bool startOk = report.conditionsMatch && report.blackStartSigned && report.whiteStartSigned;
    if (startOk && recordOk && e.result && report.resultMatchesRecord && report.blackResultSigned
        && report.whiteResultSigned) {
        report.category = EvidenceReport::Category::MutuallySigned;
    } else if (startOk && recordOk && report.terminal) {
        report.category = EvidenceReport::Category::DeterminedByRecord;
    } else {
        report.category = EvidenceReport::Category::Unconfirmed;
    }
    return report;
}

} // namespace EvidenceVerifier

} // namespace P2p
