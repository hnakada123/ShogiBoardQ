/// @file p2pprotocol.cpp
/// @brief P2P対局の条件・状態・開始合意・結果の値の実装

#include "p2pprotocol.h"

namespace P2p {

namespace {

CborValue u(quint64 value) { return CborValue::uint(value); }
CborValue b(const QByteArray& value) { return CborValue::bytes(value); }
CborValue t(const char* value) { return CborValue::text(value); }
CborValue t(const QByteArray& value) { return CborValue::text(value); }

} // namespace

QByteArray zeroHash()
{
    return QByteArray(kHashSize, '\0');
}

// ---------------------------------------------------------------- 条件

namespace Rules {

QByteArray initialSfen()
{
    return QByteArrayLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

CborValue v1()
{
    return CborValue::array({t("SBQ-RULES"), u(1), t(initialSfen()), u(kMaxPly), t("fourfold-draw"),
                             t("perpetual-check-loss"), t("no-declaration"), t("human"), t("human")});
}

QByteArray v1Hash()
{
    return Cbor::hash(v1());
}

bool isV1(const CborValue& value)
{
    return value == v1();
}

} // namespace Rules

CborValue TimeControlTerms::toCbor() const
{
    return CborValue::array({t("SBQ-TIME"), u(1), u(mainMs), u(byoyomiMs), u(0), u(0)});
}

bool TimeControlTerms::isValid() const
{
    return mainMs <= kMaxMainMs && byoyomiMs <= kMaxByoyomiMs && (mainMs > 0 || byoyomiMs > 0);
}

std::optional<TimeControlTerms> TimeControlTerms::fromCbor(const CborValue& value)
{
    CborReader r(value, "SBQ-TIME", 6);
    TimeControlTerms terms;
    terms.mainMs = r.uint(2, kMaxMainMs);
    terms.byoyomiMs = r.uint(3, kMaxByoyomiMs);
    r.uint(4, 0);
    r.uint(5, 0);
    if (!r.ok() || !terms.isValid()) return std::nullopt;
    return terms;
}

CborValue NetworkPolicy::toCbor() const
{
    return CborValue::array({t("SBQ-NET"), u(1), u(pingIntervalMs), u(disconnectDetectMs), u(reconnectWaitMs),
                             u(resendMs), u(gameLimitMs), u(toleranceMs), u(disconnectTotalMs)});
}

std::optional<NetworkPolicy> NetworkPolicy::fromCbor(const CborValue& value)
{
    if (value != v1().toCbor()) return std::nullopt;
    return v1();
}

// ---------------------------------------------------------------- 状態

CborValue GameState::toCbor() const
{
    return CborValue::array({t("SBQ-STATE"), u(1), b(gameId), u(ply), b(previousStateHash), t(moveUsi), t(sfen),
                             u(spentMs), u(blackMainMs), u(whiteMainMs)});
}

std::optional<GameState> GameState::fromCbor(const CborValue& value)
{
    CborReader r(value, "SBQ-STATE", 10);
    GameState s;
    s.gameId = r.bytes(2, kHashSize);
    s.ply = static_cast<quint32>(r.uint(3, kMaxPly));
    s.previousStateHash = r.bytes(4, kHashSize);
    s.moveUsi = r.ascii(5, 5);
    s.sfen = r.ascii(6, kMaxSfenBytes);
    s.spentMs = r.uint(7, kMaxMainMs + kMaxByoyomiMs);
    s.blackMainMs = r.uint(8, kMaxMainMs);
    s.whiteMainMs = r.uint(9, kMaxMainMs);
    if (!r.ok()) return std::nullopt;
    // State0 だけは指し手が空。以後は USI の4〜5文字
    if (s.ply == 0 ? !s.moveUsi.isEmpty() : (s.moveUsi.size() < 4 || s.moveUsi.size() > 5)) return std::nullopt;
    if (s.sfen.isEmpty()) return std::nullopt;
    return s;
}

QByteArray transcriptHash(const QByteArray& gameId, const std::vector<GameState>& states)
{
    std::vector<CborValue> items;
    items.reserve(states.size());
    for (const GameState& s : states) items.push_back(s.toCbor());
    return Cbor::hash(CborValue::array({t("SBQ-TRANSCRIPT"), u(1), b(gameId), CborValue::array(std::move(items))}));
}

// ---------------------------------------------------------------- 開始合意

CborValue StartAgreement::toCbor() const
{
    return CborValue::array({t("SBQ-START"), u(1), b(networkId), b(blackNonce), b(whiteNonce),
                             b(blackPlayerKey), b(whitePlayerKey), b(blackSessionKey), b(whiteSessionKey),
                             b(blackPeerIdHash), b(whitePeerIdHash), b(rulesHash), b(timeControlHash),
                             b(networkPolicyHash), u(startBefore)});
}

bool StartAgreement::hasDistinctKeys() const
{
    const QByteArray keys[] = {blackPlayerKey, whitePlayerKey, blackSessionKey, whiteSessionKey};
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            if (keys[i] == keys[j]) return false;
        }
    }
    return blackPeerIdHash != whitePeerIdHash && blackNonce != whiteNonce;
}

std::optional<StartAgreement> StartAgreement::fromCbor(const CborValue& value)
{
    CborReader r(value, "SBQ-START", 15);
    StartAgreement a;
    a.networkId = r.bytes(2, kHashSize);
    a.blackNonce = r.bytes(3, kNonceSize);
    a.whiteNonce = r.bytes(4, kNonceSize);
    a.blackPlayerKey = r.bytes(5, kPublicKeySize);
    a.whitePlayerKey = r.bytes(6, kPublicKeySize);
    a.blackSessionKey = r.bytes(7, kPublicKeySize);
    a.whiteSessionKey = r.bytes(8, kPublicKeySize);
    a.blackPeerIdHash = r.bytes(9, kHashSize);
    a.whitePeerIdHash = r.bytes(10, kHashSize);
    a.rulesHash = r.bytes(11, kHashSize);
    a.timeControlHash = r.bytes(12, kHashSize);
    a.networkPolicyHash = r.bytes(13, kHashSize);
    a.startBefore = r.uint(14, 0xFFFFFFFFFFFFFFFFULL);
    if (!r.ok() || !a.hasDistinctKeys()) return std::nullopt;
    return a;
}

// ---------------------------------------------------------------- 署名の対象

CborValue stateApproval(const QByteArray& gameId, quint32 ply, const QByteArray& stateHash)
{
    return CborValue::array({t("SBQ-APPROVE"), u(1), b(gameId), u(ply), b(stateHash)});
}

CborValue Control::toCbor() const
{
    return CborValue::array({t("SBQ-CONTROL"), u(1), b(gameId), b(connectionId), u(sequence),
                             u(static_cast<quint64>(kind)), b(bodyHash)});
}

std::optional<Control> Control::fromCbor(const CborValue& value)
{
    CborReader r(value, "SBQ-CONTROL", 7);
    Control c;
    c.gameId = r.bytes(2, kHashSize);
    c.connectionId = r.bytes(3, kHashSize);
    c.sequence = r.uint(4, 0xFFFFFFFFFFFFFFFFULL);
    const quint64 kind = r.uint(5, 6);
    c.bodyHash = r.bytes(6, kHashSize);
    if (!r.ok() || kind < 1 || c.sequence == 0) return std::nullopt;
    c.kind = static_cast<ControlKind>(kind);
    return c;
}

bool isAllowedOutcome(Cause cause, Outcome outcome)
{
    switch (cause) {
    case Cause::Resign:
    case Cause::Checkmate:
    case Cause::Timeout:
    case Cause::PerpetualCheck:
        return outcome == Outcome::BlackWins || outcome == Outcome::WhiteWins;
    case Cause::Repetition:
    case Cause::AgreedDraw:
    case Cause::MaxPly:
        return outcome == Outcome::Draw;
    case Cause::AgreedAbort:
        return outcome == Outcome::Aborted;
    }
    return false;
}

CborValue FinalResult::toCbor() const
{
    return CborValue::array({t("SBQ-RESULT"), u(1), b(gameId), b(finalStateHash), b(transcriptHash), u(ply),
                             u(static_cast<quint64>(outcome)), u(static_cast<quint64>(cause))});
}

std::optional<FinalResult> FinalResult::fromCbor(const CborValue& value)
{
    CborReader r(value, "SBQ-RESULT", 8);
    FinalResult f;
    f.gameId = r.bytes(2, kHashSize);
    f.finalStateHash = r.bytes(3, kHashSize);
    f.transcriptHash = r.bytes(4, kHashSize);
    f.ply = static_cast<quint32>(r.uint(5, kMaxPly));
    const quint64 outcome = r.uint(6, 4);
    const quint64 cause = r.uint(7, 8);
    if (!r.ok() || outcome < 1 || cause < 1) return std::nullopt;
    f.outcome = static_cast<Outcome>(outcome);
    f.cause = static_cast<Cause>(cause);
    if (!isAllowedOutcome(f.cause, f.outcome)) return std::nullopt;
    return f;
}

// ---------------------------------------------------------------- 識別子

const std::vector<NetworkInfo>& allowedNetworks()
{
    static const std::vector<NetworkInfo> networks = {
        {"sbq-local-1", true},
        {"sbq-limited-1", false},
    };
    return networks;
}

std::optional<NetworkInfo> findNetwork(const QString& name)
{
    for (const NetworkInfo& info : allowedNetworks()) {
        if (name == QLatin1String(info.name)) return info;
    }
    return std::nullopt;
}

QByteArray networkId(const QByteArray& networkName)
{
    return Cbor::hash(CborValue::array({t("SBQ-NETWORK"), u(1), t(networkName)}));
}

QByteArray coinCommit(const QByteArray& nonceA)
{
    return Cbor::hash(CborValue::array({t("SBQ-COMMIT"), u(1), b(nonceA)}));
}

bool applicantIsBlack(const QByteArray& nonceA, const QByteArray& nonceB)
{
    const QByteArray coin = Cbor::hash(CborValue::array({t("SBQ-COIN"), u(1), b(nonceA), b(nonceB)}));
    return (static_cast<quint8>(coin.back()) & 1u) == 0;
}

QByteArray connectionId(const QByteArray& gameId, const QByteArray& blackRandom, const QByteArray& whiteRandom)
{
    return Cbor::hash(CborValue::array({t("SBQ-CONN"), u(1), b(gameId), b(blackRandom), b(whiteRandom)}));
}

QStringList verificationCode(const QByteArray& keyA, const QByteArray& keyB)
{
    const bool aFirst = keyA < keyB;
    const QByteArray digest =
        Cbor::hash(CborValue::array({t("SBQ-VERIFY"), u(1), b(aFirst ? keyA : keyB), b(aFirst ? keyB : keyA)}));
    QStringList groups;
    for (int group = 0; group < 6; ++group) {
        quint64 value = 0;
        for (int i = 0; i < 5; ++i) value = (value << 8) | static_cast<quint8>(digest.at(group * 5 + i));
        groups.append(QStringLiteral("%1").arg(value % 100000, 5, 10, QLatin1Char('0')));
    }
    return groups;
}

} // namespace P2p
