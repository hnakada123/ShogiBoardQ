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

CborValue start(const StartAgreement& agreement, const RulesTerms& rules, const TimeControlTerms& terms, quint8 mySide,
                const QString& opponentDisplayName)
{
    std::vector<CborValue> items{CborValue::text(kStart), agreement.toCbor(), terms.toCbor(), CborValue::uint(mySide),
                                 CborValue::text(opponentDisplayName)};
    if (!rules.isHumanOnly()) items.push_back(rules.toCbor());
    return CborValue::array(std::move(items));
}

CborValue startSignature(quint8 side, const QByteArray& signature)
{
    return CborValue::array({CborValue::text(kStartSignature), CborValue::uint(side), CborValue::bytes(signature)});
}

CborValue state(const SignedState& s)
{
    // 件の種類の語の後に、証跡と同じ [State, blackApproval, whiteApproval] の3つの欄を並べる
    const CborValue fields = s.toCbor();
    std::vector<CborValue> items{CborValue::text(kState)};
    items.insert(items.end(), fields.items().begin(), fields.items().end());
    return CborValue::array(std::move(items));
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

CborValue ending(Settlement settlement)
{
    return CborValue::array({CborValue::text(kEnding), CborValue::text(settlementName(settlement).toLatin1())});
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
        if (kind == kStart && (items.size() == 5 || items.size() == 6)) {
            const std::optional<StartAgreement> agreement = StartAgreement::fromCbor(items[1]);
            const std::optional<TimeControlTerms> terms = TimeControlTerms::fromCbor(items[2]);
            // 対局者の種別の無い件は、双方人間の対局
            const std::optional<RulesTerms> rules = items.size() == 6 ? RulesTerms::fromCbor(items[5]) : RulesTerms{};
            if (!agreement || !terms || !rules) return std::nullopt;
            e.agreement = *agreement;
            e.rules = *rules;
            e.timeControl = *terms;
            const std::optional<Turn> mine = items[3].isUInt() ? sideOfCode(items[3].toUInt()) : std::nullopt;
            out.mySide = sideCode(mine.value_or(Turn::Black));
            out.opponentDisplayName = items[4].isText() ? items[4].toText() : QString();
            started = true;
        } else if (!started) {
            continue;
        } else if (kind == kStartSignature && items.size() == 3 && items[1].isUInt() && items[2].isBytes()) {
            if (const std::optional<Turn> side = sideOfCode(items[1].toUInt())) e.startSignature(*side) = items[2].data();
        } else if (kind == kState && items.size() == 4) {
            const std::optional<SignedState> s = SignedState::fromCbor(CborValue::array({items[1], items[2], items[3]}),
                                                                       SignedState::Approvals::Optional);
            if (!s) continue;
            const quint32 ply = s->state.ply;
            // 同じ手数の記録は新しいもの（署名が足されたもの）で置き換える
            if (ply < e.states.size()) {
                e.states[ply] = *s;
                e.states.resize(ply + 1);
            } else if (ply == e.states.size()) {
                e.states.push_back(*s);
            }
        } else if (kind == kResult && items.size() == 3) {
            e.result = FinalResult::fromCbor(items[2]);
        } else if (kind == kResultSignature && items.size() == 3 && items[1].isUInt() && items[2].isBytes()) {
            if (const std::optional<Turn> side = sideOfCode(items[1].toUInt())) e.resultSignature(*side) = items[2].data();
        } else if (kind == kConflict) {
            out.hasConflictProof = true;
        } else if (kind == kEnding && items.size() == 2 && items[1].isText()) {
            out.settlement = settlementFromName(items[1].toText());
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
