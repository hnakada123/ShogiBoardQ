/// @file p2pframes.cpp
/// @brief 対局フレームの外枠と payload の実装

#include "p2pframes.h"

#include "p2pdisplayname.h"

namespace P2p {

namespace {

CborValue u(quint64 value) { return CborValue::uint(value); }
CborValue b(const QByteArray& value) { return CborValue::bytes(value); }
CborValue t(const char* value) { return CborValue::text(value); }

bool isSide(quint64 side) { return side == kSideBlack || side == kSideWhite; }

std::optional<ErrorCode> errorCodeOf(quint64 value)
{
    if (value < 1 || value > kMaxErrorCode) return std::nullopt;
    return static_cast<ErrorCode>(value);
}

} // namespace

// ---------------------------------------------------------------- Frame

QByteArray Frame::encode() const
{
    return Cbor::encode(CborValue::array({u(1), u(static_cast<quint64>(type)), u(requestId), b(gameId), payload}));
}

namespace {

/**
 * @brief 外枠の規則（protocol/README.md 2章。type は 1〜11）で読む
 *
 * 通信プロセスと同じ判定で、共通の試験入力（protocol/testvectors/frames.txt）で確かめる。本番の読み取り（decode）と
 * 試験（isWellFormedEnvelope）が同じこの関数を通る。
 */
std::optional<Frame> readEnvelope(QByteArrayView bytes, QString* error)
{
    const auto fail = [error](const QString& reason) {
        if (error) *error = reason;
        return std::nullopt;
    };
    if (bytes.isEmpty() || bytes.size() > kMaxFrameBytes) return fail(QStringLiteral("frame size out of range"));
    QString cborError;
    const std::optional<CborValue> root = Cbor::decode(bytes, &cborError);
    if (!root) return fail(QStringLiteral("frame is not deterministic CBOR: %1").arg(cborError));
    CborReader r(*root, nullptr, 5);
    const quint64 version = r.uint(0, 0xFFFF);
    const quint64 type = r.uint(1, 0xFFFF);
    Frame frame;
    frame.requestId = r.uint(2, 0xFFFFFFFFFFFFFFFFULL);
    frame.gameId = r.bytes(3, kHashSize);
    const CborValue& payload = r.item(4);
    if (!r.ok() || !payload.isArray()) return fail(QStringLiteral("malformed frame envelope"));
    if (version != 1) return fail(QStringLiteral("unsupported frame version"));
    if (type < 1 || type > static_cast<quint64>(FrameType::InviteToken)) return fail(QStringLiteral("unknown frame type"));
    frame.type = static_cast<FrameType>(type);
    frame.payload = payload;
    return frame;
}

} // namespace

bool Frame::isWellFormedEnvelope(QByteArrayView bytes)
{
    return readEnvelope(bytes, nullptr).has_value();
}

std::optional<Frame> Frame::decode(QByteArrayView bytes, QString* error)
{
    std::optional<Frame> frame = readEnvelope(bytes, error);
    if (!frame) return std::nullopt;
    // INVITE_TOKEN は通信プロセスが処理し、対局・UI側には届かない
    if (frame->type == FrameType::InviteToken) {
        if (error) *error = QStringLiteral("INVITE_TOKEN is not for the game");
        return std::nullopt;
    }
    return frame;
}

// ---------------------------------------------------------------- HELLO

CborValue HelloPayload::toCbor() const
{
    std::vector<CborValue> conditions;
    if (timeControl) conditions = {Rules::v1(), timeControl->toCbor(), NetworkPolicy::v1().toCbor()};
    return CborValue::array({t("SBQ-HELLO"), u(1), u(role), b(networkId), CborValue::array({u(1)}), b(peerId),
                             b(connectionRandom), b(playerKey), b(sessionKey), CborValue::text(displayName),
                             u(static_cast<quint64>(sidePreference)), b(offerId), b(coin),
                             CborValue::array(std::move(conditions))});
}

std::optional<HelloPayload> HelloPayload::fromCbor(const CborValue& value)
{
    CborReader r(value, "SBQ-HELLO", 14);
    HelloPayload h;
    h.role = static_cast<quint8>(r.uint(2, 2));
    h.networkId = r.bytes(3, kHashSize);
    const std::vector<CborValue>& versions = r.array(4, 8);
    h.peerId = r.bytesUpTo(5, kMaxPeerIdBytes);
    h.connectionRandom = r.bytes(6, kNonceSize);
    h.playerKey = r.bytes(7, kPublicKeySize);
    h.sessionKey = r.bytes(8, kPublicKeySize);
    h.displayName = r.utf8(9, DisplayName::kMaxBytes);
    const quint64 side = r.uint(10, 2);
    h.offerId = r.bytes(11, kHashSize);
    h.coin = r.bytes(12, kHashSize);
    const std::vector<CborValue>& conditions = r.array(13, 3);
    if (!r.ok() || h.role < kRoleApplicant || h.peerId.isEmpty()) return std::nullopt;
    bool supportsV1 = false;
    for (const CborValue& v : versions) supportsV1 = supportsV1 || (v.isUInt() && v.toUInt() == 1);
    // 表示名は受け取ったバイト列のまま判定する（QString にすると先頭の BOM が消える）
    if (!supportsV1 || !DisplayName::isValidUtf8(r.item(9).data())) return std::nullopt;
    h.sidePreference = static_cast<SidePreference>(side);
    if (h.role == kRoleApplicant) {
        if (!conditions.empty()) return std::nullopt;
    } else {
        if (conditions.size() != 3 || !Rules::isV1(conditions[0]) || !NetworkPolicy::fromCbor(conditions[2])) {
            return std::nullopt;
        }
        h.timeControl = TimeControlTerms::fromCbor(conditions[1]);
        if (!h.timeControl) return std::nullopt;
    }
    return h;
}

CborValue ReconnectHello::toCbor() const
{
    return CborValue::array({t("SBQ-RECONNECT"), u(1), u(side), b(connectionRandom)});
}

std::optional<ReconnectHello> ReconnectHello::fromCbor(const CborValue& value)
{
    CborReader r(value, "SBQ-RECONNECT", 4);
    ReconnectHello h;
    const quint64 side = r.uint(2, 2);
    h.connectionRandom = r.bytes(3, kNonceSize);
    if (!r.ok() || !isSide(side)) return std::nullopt;
    h.side = static_cast<quint8>(side);
    return h;
}

// ---------------------------------------------------------------- 開始

CborValue StartAgreementPayload::toCbor() const
{
    return CborValue::array({agreement.toCbor(), Rules::v1(), timeControl.toCbor(), NetworkPolicy::v1().toCbor()});
}

std::optional<StartAgreementPayload> StartAgreementPayload::fromCbor(const CborValue& value)
{
    CborReader r(value, nullptr, 4);
    if (!r.ok()) return std::nullopt;
    const std::optional<StartAgreement> agreement = StartAgreement::fromCbor(r.item(0));
    const std::optional<TimeControlTerms> terms = TimeControlTerms::fromCbor(r.item(2));
    if (!agreement || !terms || !Rules::isV1(r.item(1)) || !NetworkPolicy::fromCbor(r.item(3))) return std::nullopt;
    StartAgreementPayload p;
    p.agreement = *agreement;
    p.timeControl = *terms;
    // 条件の配列のハッシュが合意と一致すること
    if (p.agreement.rulesHash != Rules::v1Hash() || p.agreement.timeControlHash != p.timeControl.hash()
        || p.agreement.networkPolicyHash != NetworkPolicy::v1().hash()) {
        return std::nullopt;
    }
    return p;
}

CborValue startSignaturePayload(const SideSignature& s)
{
    return CborValue::array({u(s.side), b(s.signature)});
}

std::optional<SideSignature> parseStartSignature(const CborValue& value)
{
    CborReader r(value, nullptr, 2);
    SideSignature s;
    const quint64 side = r.uint(0, 2);
    s.signature = r.bytes(1, kSignatureSize);
    if (!r.ok() || !isSide(side)) return std::nullopt;
    s.side = static_cast<quint8>(side);
    return s;
}

// ---------------------------------------------------------------- 着手

CborValue StateProposePayload::toCbor() const
{
    return CborValue::array({state.toCbor(), b(signature)});
}

std::optional<StateProposePayload> StateProposePayload::fromCbor(const CborValue& value)
{
    CborReader r(value, nullptr, 2);
    if (!r.ok()) return std::nullopt;
    const std::optional<GameState> state = GameState::fromCbor(r.item(0));
    StateProposePayload p;
    p.signature = r.bytes(1, kSignatureSize);
    if (!r.ok() || !state) return std::nullopt;
    p.state = *state;
    return p;
}

CborValue StateAcceptPayload::toCbor() const
{
    return CborValue::array({u(ply), b(stateHash), b(signature)});
}

std::optional<StateAcceptPayload> StateAcceptPayload::fromCbor(const CborValue& value)
{
    CborReader r(value, nullptr, 3);
    StateAcceptPayload p;
    p.ply = static_cast<quint32>(r.uint(0, kMaxPly));
    p.stateHash = r.bytes(1, kHashSize);
    p.signature = r.bytes(2, kSignatureSize);
    if (!r.ok()) return std::nullopt;
    return p;
}

// ---------------------------------------------------------------- 制御

CborValue ControlPayload::toCbor() const
{
    return CborValue::array({control.toCbor(), body, b(signature)});
}

std::optional<ControlPayload> ControlPayload::fromCbor(const CborValue& value)
{
    CborReader r(value, nullptr, 3);
    if (!r.ok()) return std::nullopt;
    const std::optional<Control> control = Control::fromCbor(r.item(0));
    ControlPayload p;
    p.body = r.item(1);
    p.signature = r.bytes(2, kSignatureSize);
    if (!r.ok() || !control || !p.body.isArray() || Cbor::hash(p.body) != control->bodyHash) return std::nullopt;
    p.control = *control;
    return p;
}

CborValue SyncRequestPayload::toCbor() const
{
    return CborValue::array({u(fromPly), b(previousStateHash)});
}

std::optional<SyncRequestPayload> SyncRequestPayload::fromCbor(const CborValue& value)
{
    CborReader r(value, nullptr, 2);
    SyncRequestPayload p;
    p.fromPly = static_cast<quint32>(r.uint(0, kMaxPly));
    p.previousStateHash = r.bytes(1, kHashSize);
    if (!r.ok() || p.fromPly < 1) return std::nullopt;
    return p;
}

CborValue SyncChunkPayload::toCbor() const
{
    std::vector<CborValue> items;
    items.reserve(states.size());
    for (const SignedState& s : states) {
        items.push_back(CborValue::array({s.state.toCbor(), b(s.blackApproval), b(s.whiteApproval)}));
    }
    return CborValue::array({CborValue::array(std::move(items)), u(final ? 1 : 0)});
}

std::optional<SyncChunkPayload> SyncChunkPayload::fromCbor(const CborValue& value)
{
    CborReader r(value, nullptr, 2);
    const std::vector<CborValue>& items = r.array(0, kMaxSyncChunkStates);
    const quint64 final = r.uint(1, 1);
    if (!r.ok()) return std::nullopt;
    SyncChunkPayload p;
    p.final = final == 1;
    for (const CborValue& item : items) {
        CborReader sr(item, nullptr, 3);
        const std::optional<GameState> state = GameState::fromCbor(sr.item(0));
        SignedState s;
        s.blackApproval = sr.bytes(1, kSignatureSize);
        s.whiteApproval = sr.bytes(2, kSignatureSize);
        if (!sr.ok() || !state) return std::nullopt;
        s.state = *state;
        p.states.push_back(std::move(s));
    }
    return p;
}

// ---------------------------------------------------------------- 結果

CborValue ResultSignaturePayload::toCbor() const
{
    return CborValue::array({result.toCbor(), u(signature.side), b(signature.signature)});
}

std::optional<ResultSignaturePayload> ResultSignaturePayload::fromCbor(const CborValue& value)
{
    CborReader r(value, nullptr, 3);
    if (!r.ok()) return std::nullopt;
    const std::optional<FinalResult> result = FinalResult::fromCbor(r.item(0));
    ResultSignaturePayload p;
    const quint64 side = r.uint(1, 2);
    p.signature.signature = r.bytes(2, kSignatureSize);
    if (!r.ok() || !result || !isSide(side)) return std::nullopt;
    p.result = *result;
    p.signature.side = static_cast<quint8>(side);
    return p;
}

CborValue rejectPayload(ErrorCode code)
{
    return CborValue::array({u(static_cast<quint64>(code))});
}

std::optional<ErrorCode> parseReject(const CborValue& value)
{
    CborReader r(value, nullptr, 1);
    const quint64 code = r.uint(0, 0xFF);
    if (!r.ok()) return std::nullopt;
    return errorCodeOf(code);
}

Frame rejectFrame(ErrorCode code)
{
    Frame frame;
    frame.type = FrameType::Reject;
    frame.requestId = 1;
    frame.gameId = zeroHash();
    frame.payload = rejectPayload(code);
    return frame;
}

// ---------------------------------------------------------------- CONTROL の body

namespace ControlBody {

CborValue ready(const QByteArray& state0Hash) { return CborValue::array({b(state0Hash)}); }
CborValue resume(quint32 latestPly, const QByteArray& latestStateHash)
{
    return CborValue::array({u(latestPly), b(latestStateHash)});
}
CborValue abort(const QByteArray& latestStateHash, ErrorCode reason)
{
    return CborValue::array({b(latestStateHash), u(static_cast<quint64>(reason))});
}
CborValue error(ErrorCode code, quint32 relatedPly)
{
    return CborValue::array({u(static_cast<quint64>(code)), u(relatedPly)});
}
CborValue decline(const QByteArray& proposalBodyHash) { return CborValue::array({b(proposalBodyHash)}); }

std::optional<QByteArray> parseReady(const CborValue& body)
{
    CborReader r(body, nullptr, 1);
    QByteArray hash = r.bytes(0, kHashSize);
    if (!r.ok()) return std::nullopt;
    return hash;
}

std::optional<Resume> parseResume(const CborValue& body)
{
    CborReader r(body, nullptr, 2);
    Resume resume;
    resume.latestPly = static_cast<quint32>(r.uint(0, kMaxPly));
    resume.latestStateHash = r.bytes(1, kHashSize);
    if (!r.ok()) return std::nullopt;
    return resume;
}

std::optional<ErrorCode> parseAbort(const CborValue& body)
{
    CborReader r(body, nullptr, 2);
    r.bytes(0, kHashSize);   // latestStateHash
    const std::optional<ErrorCode> reason = errorCodeOf(r.uint(1, 0xFF));
    if (!r.ok()) return std::nullopt;
    return reason;
}

std::optional<ErrorCode> parseError(const CborValue& body)
{
    CborReader r(body, nullptr, 2);
    const std::optional<ErrorCode> code = errorCodeOf(r.uint(0, 0xFF));
    r.uint(1, kMaxPly);   // relatedPly
    if (!r.ok()) return std::nullopt;
    return code;
}

std::optional<QByteArray> parseDecline(const CborValue& body)
{
    CborReader r(body, nullptr, 1);
    QByteArray hash = r.bytes(0, kHashSize);
    if (!r.ok()) return std::nullopt;
    return hash;
}

} // namespace ControlBody

} // namespace P2p
