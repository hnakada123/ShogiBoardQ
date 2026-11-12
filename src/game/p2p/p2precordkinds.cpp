/// @file p2precordkinds.cpp
/// @brief 対局ジャーナルの件の形の実装

#include "p2precordkinds.h"

namespace P2p {

namespace RecordKinds {

namespace {

// 件のラベル（書き手と読み手で共有する）
constexpr char kStart[] = "J-START";
constexpr char kStartSignature[] = "J-STARTSIG";
constexpr char kState[] = "J-STATE";
constexpr char kResult[] = "J-RESULT";
constexpr char kResultSignature[] = "J-RESULTSIG";
constexpr char kConflict[] = "J-CONFLICT";
constexpr char kControl[] = "J-CONTROL";
constexpr char kEnding[] = "J-END";

} // namespace

CborValue start(const StartAgreement& agreement, const TimeControlTerms& terms, quint8 mySide,
                const QString& opponentDisplayName)
{
    return CborValue::array({CborValue::text(kStart), agreement.toCbor(), terms.toCbor(), CborValue::uint(mySide),
                             CborValue::text(opponentDisplayName)});
}

CborValue startSignature(quint8 side, const QByteArray& signature)
{
    return CborValue::array({CborValue::text(kStartSignature), CborValue::uint(side), CborValue::bytes(signature)});
}

CborValue state(const SignedState& s)
{
    return CborValue::array({CborValue::text(kState), s.state.toCbor(), CborValue::bytes(s.blackApproval),
                             CborValue::bytes(s.whiteApproval)});
}

CborValue resultDigest(const FinalResult& result)
{
    return CborValue::array({CborValue::text(kResult), CborValue::bytes(result.digest()), result.toCbor()});
}

CborValue resultSignature(quint8 side, const QByteArray& signature)
{
    return CborValue::array({CborValue::text(kResultSignature), CborValue::uint(side), CborValue::bytes(signature)});
}

CborValue conflict(const GameState& first, const QByteArray& firstSignature, const GameState& second,
                   const QByteArray& secondSignature)
{
    return CborValue::array({CborValue::text(kConflict), first.toCbor(), CborValue::bytes(firstSignature),
                             second.toCbor(), CborValue::bytes(secondSignature)});
}

CborValue control(bool sent, const CborValue& controlPayload)
{
    return CborValue::array({CborValue::text(kControl), CborValue::uint(sent ? 1 : 0), controlPayload});
}

CborValue ending(const QString& localEnding)
{
    return CborValue::array({CborValue::text(kEnding), CborValue::text(localEnding.toLatin1())});
}

std::optional<JournalReplay> replay(const std::vector<CborValue>& records)
{
    JournalReplay out;
    Evidence& e = out.evidence;
    bool started = false;
    for (const CborValue& record : records) {
        if (!record.isArray() || record.items().empty() || !record.items()[0].isText()) continue;
        const QByteArray& kind = record.items()[0].data();
        const std::vector<CborValue>& items = record.items();
        if (kind == kStart && items.size() == 5) {
            const std::optional<StartAgreement> agreement = StartAgreement::fromCbor(items[1]);
            const std::optional<TimeControlTerms> terms = TimeControlTerms::fromCbor(items[2]);
            if (!agreement || !terms) return std::nullopt;
            e.agreement = *agreement;
            e.timeControl = *terms;
            out.mySide = items[3].isUInt() && items[3].toUInt() == kSideWhite ? kSideWhite : kSideBlack;
            out.opponentDisplayName = items[4].isText() ? items[4].toText() : QString();
            started = true;
        } else if (!started) {
            continue;
        } else if (kind == kStartSignature && items.size() == 3 && items[1].isUInt() && items[2].isBytes()) {
            (items[1].toUInt() == kSideBlack ? e.blackStartSignature : e.whiteStartSignature) = items[2].data();
        } else if (kind == kState && items.size() == 4 && items[2].isBytes() && items[3].isBytes()) {
            const std::optional<GameState> state = GameState::fromCbor(items[1]);
            if (!state) continue;
            const SignedState s{*state, items[2].data(), items[3].data()};
            // 同じ手数の記録は新しいもの（署名が足されたもの）で置き換える
            if (state->ply < e.states.size()) {
                e.states[state->ply] = s;
                e.states.resize(state->ply + 1);
            } else if (state->ply == e.states.size()) {
                e.states.push_back(s);
            }
        } else if (kind == kResult && items.size() == 3) {
            e.result = FinalResult::fromCbor(items[2]);
        } else if (kind == kResultSignature && items.size() == 3 && items[1].isUInt() && items[2].isBytes()) {
            (items[1].toUInt() == kSideBlack ? e.blackResultSignature : e.whiteResultSignature) = items[2].data();
        } else if (kind == kConflict) {
            out.hasConflictProof = true;
        } else if (kind == kEnding && items.size() == 2 && items[1].isText()) {
            out.localEnding = items[1].toText();
        }
    }
    if (!started) return std::nullopt;
    if (!e.result) {
        e.blackResultSignature.clear();
        e.whiteResultSignature.clear();
    }
    return out;
}

} // namespace RecordKinds

} // namespace P2p
