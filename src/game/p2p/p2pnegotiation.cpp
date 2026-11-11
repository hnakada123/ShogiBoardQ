/// @file p2pnegotiation.cpp
/// @brief 開始までの交渉の実装

#include "p2pnegotiation.h"

namespace P2p {

namespace {

/// 時計のずれの許容（開始合意の期限の確認）
constexpr quint64 kClockSkewSeconds = 30;

} // namespace

Negotiation::Negotiation(NegotiationConfig config, const SigningKey* playerKey)
    : m_config(std::move(config))
    , m_playerKey(playerKey)
    , m_sessionKey(SigningKey::generate())
    , m_myRandom(Crypto::randomBytes(kNonceSize))
    , m_myNonce(Crypto::randomBytes(kNonceSize))
{
    if (!m_playerKey || !m_playerKey->isValid() || !m_sessionKey.isValid() || m_myRandom.size() != kNonceSize) {
        m_stage = Stage::Failed;
        m_failure = ErrorCode::StorageError;
    }
    if (m_config.role == kRoleHost) m_terms = m_config.hostTerms;
}

bool Negotiation::sidesCompatible(SidePreference applicant, SidePreference host)
{
    if (applicant == SidePreference::None || host == SidePreference::None) return true;
    return applicant != host;
}

std::vector<Frame> Negotiation::takeOutgoing()
{
    std::vector<Frame> out;
    out.swap(m_outgoing);
    return out;
}

void Negotiation::send(FrameType type, const CborValue& payload, const QByteArray& gameId)
{
    Frame frame;
    frame.type = type;
    frame.requestId = m_requestId++;
    frame.gameId = gameId;
    frame.payload = payload;
    m_outgoing.push_back(std::move(frame));
}

void Negotiation::fail(ErrorCode code, bool sendReject)
{
    if (m_stage == Stage::Failed || m_stage == Stage::Started) return;
    if (sendReject) send(FrameType::Reject, rejectPayload(code), zeroHash());
    m_failure = code;
    m_stage = Stage::Failed;
}

HelloPayload Negotiation::myHello() const
{
    HelloPayload hello;
    hello.role = m_config.role;
    hello.networkId = m_config.networkId;
    hello.peerId = m_config.myPeerId;
    hello.connectionRandom = m_myRandom;
    hello.playerKey = m_playerKey ? m_playerKey->publicKey() : QByteArray();
    hello.sessionKey = m_sessionKey.publicKey();
    hello.displayName = m_config.myDisplayName;
    hello.sidePreference = m_config.mySide;
    hello.offerId = m_config.offerId;
    if (m_config.role == kRoleApplicant) {
        hello.coin = coinCommit(m_myNonce);
    } else {
        hello.coin = m_myNonce;
        hello.timeControl = m_terms;
    }
    return hello;
}

void Negotiation::begin()
{
    if (m_stage != Stage::AwaitingHello || m_config.role != kRoleApplicant) return;
    send(FrameType::Hello, myHello().toCbor(), zeroHash());
}

void Negotiation::receive(const Frame& frame)
{
    if (m_stage == Stage::Failed) return;
    if (frame.type == FrameType::Reject) {
        // 両方の開始署名がそろった後の REJECT は無視する
        if (m_stage == Stage::Started) return;
        const std::optional<ErrorCode> code = parseReject(frame.payload);
        m_failure = code.value_or(ErrorCode::ConditionMismatch);
        m_failedByPeer = true;
        m_stage = Stage::Failed;
        return;
    }
    switch (m_stage) {
    case Stage::AwaitingHello: {
        if (frame.type != FrameType::Hello || frame.gameId != zeroHash()) return fail(ErrorCode::ConditionMismatch, true);
        const std::optional<HelloPayload> hello = HelloPayload::fromCbor(frame.payload);
        if (!hello) return fail(ErrorCode::ConditionMismatch, true);
        if (hello->networkId != m_config.networkId) return fail(ErrorCode::WrongNetwork, true);
        if (m_config.role == kRoleHost) handleHelloAsHost(*hello);
        else handleHelloAsApplicant(*hello);
        return;
    }
    case Stage::AwaitingAgreement: {
        if (frame.type != FrameType::StartAgreement || frame.gameId != zeroHash()) {
            return fail(ErrorCode::ConditionMismatch, true);
        }
        const std::optional<StartAgreementPayload> payload = StartAgreementPayload::fromCbor(frame.payload);
        if (!payload) return fail(ErrorCode::ConditionMismatch, true);
        handleAgreement(*payload);
        return;
    }
    case Stage::Confirming:
    case Stage::AwaitingSignature: {
        if (frame.type != FrameType::StartSignature || frame.gameId != gameId()) {
            return fail(ErrorCode::ConditionMismatch, true);
        }
        const std::optional<SideSignature> signature = parseStartSignature(frame.payload);
        if (!signature) return fail(ErrorCode::InvalidSignature, true);
        handleStartSignature(*signature);
        return;
    }
    case Stage::Started:
    case Stage::Failed:
        return;
    }
}

void Negotiation::handleHelloAsHost(const HelloPayload& hello)
{
    if (hello.role != kRoleApplicant || hello.peerId != m_config.opponentPeerId) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    // ブロックした相手には、ブロックしたことを知らせずに BUSY（名乗った ID で判定してよい。実装仕様 3.3節）
    if (m_config.isBlocked && m_config.isBlocked(hello.playerKey)) return fail(ErrorCode::Busy, true);
    if (hello.offerId != m_config.offerId) return fail(ErrorCode::Expired, true);
    if (!sidesCompatible(hello.sidePreference, m_config.mySide)) return fail(ErrorCode::ConditionMismatch, true);
    if (!Crypto::isValidPublicKey(hello.playerKey) || !Crypto::isValidPublicKey(hello.sessionKey)
        || hello.playerKey == m_playerKey->publicKey() || hello.sessionKey == m_sessionKey.publicKey()
        || hello.connectionRandom == m_myRandom) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    m_opponent = OpponentHello{hello.displayName, hello.playerKey, hello.sessionKey, hello.peerId, hello.sidePreference};
    m_opponentRandom = hello.connectionRandom;
    m_commitA = hello.coin;
    send(FrameType::Hello, myHello().toCbor(), zeroHash());
    m_stage = Stage::AwaitingAgreement;
}

std::optional<bool> Negotiation::decideApplicantBlack(const QByteArray& nonceA, const QByteArray& nonceB) const
{
    const SidePreference applicant = m_config.role == kRoleApplicant ? m_config.mySide : m_opponent->sidePreference;
    const SidePreference host = m_config.role == kRoleHost ? m_config.mySide : m_opponent->sidePreference;
    if (!sidesCompatible(applicant, host)) return std::nullopt;
    if (applicant == SidePreference::Black || host == SidePreference::White) return true;
    if (applicant == SidePreference::White || host == SidePreference::Black) return false;
    return applicantIsBlack(nonceA, nonceB);
}

void Negotiation::handleHelloAsApplicant(const HelloPayload& hello)
{
    if (hello.role != kRoleHost || hello.peerId != m_config.opponentPeerId || hello.offerId != m_config.offerId
        || !hello.timeControl) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    // 公開募集では、見た募集と同じ条件・表示名・先後の希望であること
    if ((m_config.expectedTerms && *m_config.expectedTerms != *hello.timeControl)
        || (m_config.expectedDisplayName && *m_config.expectedDisplayName != hello.displayName)
        || (m_config.expectedHostSide && *m_config.expectedHostSide != hello.sidePreference)) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    if (!Crypto::isValidPublicKey(hello.playerKey) || !Crypto::isValidPublicKey(hello.sessionKey)
        || hello.playerKey == m_playerKey->publicKey() || hello.sessionKey == m_sessionKey.publicKey()
        || hello.connectionRandom == m_myRandom || hello.coin == m_myNonce) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    m_opponent = OpponentHello{hello.displayName, hello.playerKey, hello.sessionKey, hello.peerId, hello.sidePreference};
    m_opponentRandom = hello.connectionRandom;
    m_terms = *hello.timeControl;

    const std::optional<bool> applicantBlack = decideApplicantBlack(m_myNonce, hello.coin);
    if (!applicantBlack) return fail(ErrorCode::ConditionMismatch, true);
    m_iAmBlack = *applicantBlack;
    m_sidesByCoin = m_config.mySide == SidePreference::None && hello.sidePreference == SidePreference::None;

    StartAgreement a;
    a.networkId = m_config.networkId;
    a.blackNonce = m_iAmBlack ? m_myNonce : hello.coin;
    a.whiteNonce = m_iAmBlack ? hello.coin : m_myNonce;
    a.blackPlayerKey = m_iAmBlack ? m_playerKey->publicKey() : hello.playerKey;
    a.whitePlayerKey = m_iAmBlack ? hello.playerKey : m_playerKey->publicKey();
    a.blackSessionKey = m_iAmBlack ? m_sessionKey.publicKey() : hello.sessionKey;
    a.whiteSessionKey = m_iAmBlack ? hello.sessionKey : m_sessionKey.publicKey();
    a.blackPeerIdHash = Cbor::sha256(m_iAmBlack ? m_config.myPeerId : hello.peerId);
    a.whitePeerIdHash = Cbor::sha256(m_iAmBlack ? hello.peerId : m_config.myPeerId);
    a.rulesHash = Rules::v1Hash();
    a.timeControlHash = m_terms.hash();
    a.networkPolicyHash = NetworkPolicy::v1().hash();
    a.startBefore = now() + kStartWindowSeconds;
    if (!a.hasDistinctKeys()) return fail(ErrorCode::ConditionMismatch, true);
    m_agreement = a;
    StartAgreementPayload payload;
    payload.agreement = a;
    payload.timeControl = m_terms;
    send(FrameType::StartAgreement, payload.toCbor(), zeroHash());
    m_stage = Stage::Confirming;
}

void Negotiation::handleAgreement(const StartAgreementPayload& payload)
{
    const StartAgreement& a = payload.agreement;
    if (payload.timeControl != m_terms || a.networkId != m_config.networkId) return fail(ErrorCode::ConditionMismatch, true);
    // 先後: 自分の鍵が置かれた側
    const bool hostBlack = a.blackPlayerKey == m_playerKey->publicKey();
    const QByteArray myNonceInAgreement = hostBlack ? a.blackNonce : a.whiteNonce;
    const QByteArray applicantNonce = hostBlack ? a.whiteNonce : a.blackNonce;
    // 乱数の開示が commit と一致すること（一致しなければ交渉を打ち切る）
    if (coinCommit(applicantNonce) != m_commitA) return fail(ErrorCode::ConditionMismatch, true);
    const std::optional<bool> applicantBlack = decideApplicantBlack(applicantNonce, m_myNonce);
    if (!applicantBlack || *applicantBlack == hostBlack) return fail(ErrorCode::ConditionMismatch, true);
    const QByteArray applicantPeerId = m_opponent->peerId;
    const bool matches = myNonceInAgreement == m_myNonce
        && a.playerKey(hostBlack) == m_playerKey->publicKey() && a.sessionKey(hostBlack) == m_sessionKey.publicKey()
        && a.playerKey(!hostBlack) == m_opponent->playerKey && a.sessionKey(!hostBlack) == m_opponent->sessionKey
        && a.peerIdHash(hostBlack) == Cbor::sha256(m_config.myPeerId)
        && a.peerIdHash(!hostBlack) == Cbor::sha256(applicantPeerId) && a.rulesHash == Rules::v1Hash()
        && a.timeControlHash == m_terms.hash() && a.networkPolicyHash == NetworkPolicy::v1().hash();
    if (!matches || !a.hasDistinctKeys()) return fail(ErrorCode::ConditionMismatch, true);
    // 期限は今から3分以内（相手の時計のずれを少し見込む）
    const quint64 t = now();
    if (a.startBefore <= t || a.startBefore > t + kStartWindowSeconds + kClockSkewSeconds) {
        return fail(ErrorCode::Expired, true);
    }
    m_agreement = a;
    m_iAmBlack = hostBlack;
    m_sidesByCoin = m_config.mySide == SidePreference::None && m_opponent->sidePreference == SidePreference::None;
    m_stage = Stage::Confirming;
}

void Negotiation::confirm()
{
    if (m_stage != Stage::Confirming || m_confirmed) return;
    if (now() >= m_agreement->startBefore) return fail(ErrorCode::Expired, true);
    m_confirmed = true;
    if (m_config.role == kRoleApplicant) {
        // 申込側が先に署名する
        sendMyStartSignature();
        m_stage = Stage::AwaitingSignature;
        return;
    }
    // 募集側は、申込側の開始署名を確かめてから送る
    if (opponentVerified()) {
        sendMyStartSignature();
        m_stage = Stage::Started;
    } else {
        m_stage = Stage::AwaitingSignature;
    }
}

void Negotiation::sendMyStartSignature()
{
    m_myStartSignature = m_playerKey->sign(m_agreement->toCbor());
    SideSignature s;
    s.side = m_iAmBlack ? kSideBlack : kSideWhite;
    s.signature = m_myStartSignature;
    send(FrameType::StartSignature, startSignaturePayload(s), gameId());
}

void Negotiation::handleStartSignature(const SideSignature& signature)
{
    const quint8 opponentSide = m_iAmBlack ? kSideWhite : kSideBlack;
    if (signature.side != opponentSide || opponentVerified()) return fail(ErrorCode::InvalidSignature, true);
    if (!Crypto::verify(m_agreement->playerKey(!m_iAmBlack), m_agreement->toCbor(), signature.signature)) {
        return fail(ErrorCode::InvalidSignature, true);
    }
    if (now() >= m_agreement->startBefore) return fail(ErrorCode::Expired, true);
    m_opponentStartSignature = signature.signature;
    if (m_config.role == kRoleApplicant) {
        // 申込側は自分の署名を送った後でしか相手の署名を受けない
        if (!m_confirmed) return fail(ErrorCode::InvalidSignature, true);
        m_stage = Stage::Started;
        return;
    }
    if (m_confirmed) {
        sendMyStartSignature();
        m_stage = Stage::Started;
    }
}

void Negotiation::cancel()
{
    fail(ErrorCode::Cancelled, true);
}

} // namespace P2p
