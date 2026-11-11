/// @file p2pevidence.cpp
/// @brief 検証用証跡の値と読み書きの実装

#include "p2pevidence.h"

namespace P2p {

namespace {

/// 署名の欄（64バイトか、まだない署名の空）
QByteArray signatureField(CborReader& r, std::size_t index)
{
    const QByteArray value = r.bytesUpTo(index, kSignatureSize);
    if (!value.isEmpty() && value.size() != kSignatureSize) r.fail();
    return value;
}

} // namespace

CborValue Evidence::toCbor() const
{
    std::vector<CborValue> stateItems;
    stateItems.reserve(states.size());
    for (const SignedState& s : states) {
        stateItems.push_back(CborValue::array(
            {s.state.toCbor(), CborValue::bytes(s.blackApproval), CborValue::bytes(s.whiteApproval)}));
    }
    return CborValue::array({
        CborValue::text("SBQ-MATCH"),
        CborValue::uint(1),
        agreement.toCbor(),
        CborValue::bytes(blackStartSignature),
        CborValue::bytes(whiteStartSignature),
        Rules::v1(),
        timeControl.toCbor(),
        NetworkPolicy::v1().toCbor(),
        CborValue::array(std::move(stateItems)),
        result ? result->toCbor() : CborValue::array({}),
        CborValue::bytes(blackResultSignature),
        CborValue::bytes(whiteResultSignature),
    });
}

std::optional<Evidence> Evidence::decode(QByteArrayView bytes, QString* error)
{
    const auto fail = [error](const QString& reason) {
        if (error) *error = reason;
        return std::nullopt;
    };
    if (bytes.size() > kMaxBytes) return fail(QStringLiteral("file too large"));
    QString cborError;
    const std::optional<CborValue> root = Cbor::decode(bytes, &cborError);
    if (!root) return fail(QStringLiteral("not deterministic CBOR: %1").arg(cborError));

    CborReader r(*root, "SBQ-MATCH", 12);
    if (!r.ok()) return fail(QStringLiteral("not an evidence file"));
    Evidence e;
    const std::optional<StartAgreement> agreement = StartAgreement::fromCbor(r.item(2));
    if (!agreement) return fail(QStringLiteral("invalid start agreement"));
    e.agreement = *agreement;
    e.blackStartSignature = signatureField(r, 3);
    e.whiteStartSignature = signatureField(r, 4);
    if (!Rules::isV1(r.item(5))) return fail(QStringLiteral("unsupported rules"));
    const std::optional<TimeControlTerms> terms = TimeControlTerms::fromCbor(r.item(6));
    if (!terms) return fail(QStringLiteral("invalid time control"));
    e.timeControl = *terms;
    if (!NetworkPolicy::fromCbor(r.item(7))) return fail(QStringLiteral("unsupported network policy"));

    const std::vector<CborValue>& stateItems = r.array(8, kMaxStates);
    if (!r.ok()) return fail(QStringLiteral("too many states"));
    e.states.reserve(stateItems.size());
    for (const CborValue& item : stateItems) {
        CborReader sr(item, nullptr, 3);
        const std::optional<GameState> state = GameState::fromCbor(sr.item(0));
        SignedState s;
        s.blackApproval = signatureField(sr, 1);
        s.whiteApproval = signatureField(sr, 2);
        if (!sr.ok() || !state) return fail(QStringLiteral("invalid state %1").arg(e.states.size()));
        s.state = *state;
        e.states.push_back(std::move(s));
    }

    const CborValue& resultItem = r.item(9);
    if (resultItem.isArray() && resultItem.items().empty()) {
        e.result.reset();
    } else {
        e.result = FinalResult::fromCbor(resultItem);
        if (!e.result) return fail(QStringLiteral("invalid final result"));
    }
    e.blackResultSignature = signatureField(r, 10);
    e.whiteResultSignature = signatureField(r, 11);
    if (!r.ok()) return fail(QStringLiteral("invalid signature field"));
    if (!e.result && (!e.blackResultSignature.isEmpty() || !e.whiteResultSignature.isEmpty())) {
        return fail(QStringLiteral("result signature without result"));
    }
    return e;
}

} // namespace P2p
