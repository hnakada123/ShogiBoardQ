/// @file p2precordkinds.cpp
/// @brief 対局ジャーナルの件の形の実装

#include "p2precordkinds.h"

namespace P2p {

namespace RecordKinds {

CborValue start(const StartAgreement& agreement, const TimeControlTerms& terms, quint8 mySide,
                const QString& opponentDisplayName)
{
    return CborValue::array({CborValue::text("J-START"), agreement.toCbor(), terms.toCbor(), CborValue::uint(mySide),
                             CborValue::text(opponentDisplayName)});
}

CborValue startSignature(quint8 side, const QByteArray& signature)
{
    return CborValue::array({CborValue::text("J-STARTSIG"), CborValue::uint(side), CborValue::bytes(signature)});
}

CborValue state(const SignedState& s)
{
    return CborValue::array({CborValue::text("J-STATE"), s.state.toCbor(), CborValue::bytes(s.blackApproval),
                             CborValue::bytes(s.whiteApproval)});
}

CborValue resultDigest(const FinalResult& result)
{
    return CborValue::array({CborValue::text("J-RESULT"), CborValue::bytes(result.digest()), result.toCbor()});
}

CborValue resultSignature(quint8 side, const QByteArray& signature)
{
    return CborValue::array({CborValue::text("J-RESULTSIG"), CborValue::uint(side), CborValue::bytes(signature)});
}

CborValue conflict(const GameState& first, const QByteArray& firstSignature, const GameState& second,
                   const QByteArray& secondSignature)
{
    return CborValue::array({CborValue::text("J-CONFLICT"), first.toCbor(), CborValue::bytes(firstSignature),
                             second.toCbor(), CborValue::bytes(secondSignature)});
}

CborValue control(bool sent, const CborValue& controlPayload)
{
    return CborValue::array({CborValue::text("J-CONTROL"), CborValue::uint(sent ? 1 : 0), controlPayload});
}

CborValue ending(const QString& localEnding)
{
    return CborValue::array({CborValue::text("J-END"), CborValue::text(localEnding.toLatin1())});
}

} // namespace RecordKinds

} // namespace P2p
