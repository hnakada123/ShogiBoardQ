/// @file p2pnegotiation.cpp
/// @brief 開始までの交渉の実装

#include "p2pnegotiation.h"

#include <tuple>

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
    // offerId は予約の項目（公開募集をやめたので、常にゼロ32バイト）
    hello.offerId = zeroHash();
    if (m_config.role == kRoleApplicant) {
        // 申込側は、相手の版を知る前に送るので、常に版 1 の形（種別は START_AGREEMENT の Rules で伝える）
        hello.coin = coinCommit(m_myNonce);
    } else {
        hello.coin = m_myNonce;
        hello.timeControl = m_terms;
        hello.playerKind = m_config.myKind;
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
    // 友だちの申し込みは、在席の知らせを預けたその友だちの ID を名乗らなければ受けない
    if (!m_config.expectedPlayerKey.isEmpty() && hello.playerKey != m_config.expectedPlayerKey) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    if (hello.offerId != zeroHash()) return fail(ErrorCode::Expired, true);
    if (!sidesCompatible(hello.sidePreference, m_config.mySide)) return fail(ErrorCode::ConditionMismatch, true);
    if (!Crypto::isValidPublicKey(hello.playerKey) || !Crypto::isValidPublicKey(hello.sessionKey)
        || hello.playerKey == m_playerKey->publicKey() || hello.sessionKey == m_sessionKey.publicKey()
        || hello.connectionRandom == m_myRandom) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    // エンジンが指す対局は、対局者の種別を扱える相手（版 2）とだけ始める。自分の公開鍵を知らせる前に断る
    if (m_config.myKind != PlayerKind::Human && !hello.supportsPlayerKinds) {
        return fail(ErrorCode::UnsupportedVersion, true);
    }
    m_opponent = OpponentHello{hello.displayName, hello.playerKey, hello.sessionKey, hello.peerId, hello.sidePreference,
                               hello.supportsPlayerKinds};
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
    if (hello.role != kRoleHost || hello.peerId != m_config.opponentPeerId || hello.offerId != zeroHash()
        || !hello.timeControl) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    if (!Crypto::isValidPublicKey(hello.playerKey) || !Crypto::isValidPublicKey(hello.sessionKey)
        || hello.playerKey == m_playerKey->publicKey() || hello.sessionKey == m_sessionKey.publicKey()
        || hello.connectionRandom == m_myRandom || hello.coin == m_myNonce) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    // 友だちへの申し込みは、相手がその友だちの ID を名乗らなければ進めない
    if (!m_config.expectedPlayerKey.isEmpty() && hello.playerKey != m_config.expectedPlayerKey) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    // エンジンが指す対局は、対局者の種別を扱える相手（版 2）とだけ始める
    if (m_config.myKind != PlayerKind::Human && !hello.supportsPlayerKinds) {
        return fail(ErrorCode::UnsupportedVersion, true);
    }
    m_opponent = OpponentHello{hello.displayName, hello.playerKey, hello.sessionKey, hello.peerId, hello.sidePreference,
                               hello.supportsPlayerKinds};
    m_opponentRandom = hello.connectionRandom;
    m_opponentKind = hello.playerKind;
    m_terms = *hello.timeControl;

    const std::optional<bool> applicantBlack = decideApplicantBlack(m_myNonce, hello.coin);
    if (!applicantBlack) return fail(ErrorCode::ConditionMismatch, true);
    m_iAmBlack = *applicantBlack;
    m_sidesByCoin = m_config.mySide == SidePreference::None && hello.sidePreference == SidePreference::None;

    // 自分と相手の値を、決まった先後の順に入れる
    const Turn me = turnOf(m_iAmBlack);
    StartAgreement a;
    a.networkId = m_config.networkId;
    std::tie(a.blackNonce, a.whiteNonce) = inSideOrder(me, m_myNonce, hello.coin);
    std::tie(a.blackPlayerKey, a.whitePlayerKey) = inSideOrder(me, m_playerKey->publicKey(), hello.playerKey);
    std::tie(a.blackSessionKey, a.whiteSessionKey) = inSideOrder(me, m_sessionKey.publicKey(), hello.sessionKey);
    std::tie(a.blackPeerIdHash, a.whitePeerIdHash) =
        inSideOrder(me, Cbor::sha256(m_config.myPeerId), Cbor::sha256(hello.peerId));
    // 対局者の種別は、決まった先後に従って Rules に入れる（双方人間なら版 1 と同じ値）
    m_rules = RulesTerms::forSides(m_iAmBlack, m_config.myKind, hello.playerKind);
    a.rulesHash = m_rules.hash();
    a.timeControlHash = m_terms.hash();
    a.networkPolicyHash = NetworkPolicy::v1().hash();
    a.startBefore = now() + kStartWindowSeconds;
    if (!a.hasDistinctKeys()) return fail(ErrorCode::ConditionMismatch, true);
    m_agreement = a;
    StartAgreementPayload payload;
    payload.agreement = a;
    payload.rules = m_rules;
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
    // Rules の自分の側は HELLO で伝えた自分の種別。相手の側は申込側の種別で、エンジンと書けるのは版 2 に対応する相手だけ
    const PlayerKind applicantKind = payload.rules.kind(!hostBlack);
    if (payload.rules.kind(hostBlack) != m_config.myKind
        || (applicantKind != PlayerKind::Human && !m_opponent->supportsPlayerKinds)) {
        return fail(ErrorCode::ConditionMismatch, true);
    }
    const QByteArray applicantPeerId = m_opponent->peerId;
    const bool matches = myNonceInAgreement == m_myNonce
        && a.playerKey(hostBlack) == m_playerKey->publicKey() && a.sessionKey(hostBlack) == m_sessionKey.publicKey()
        && a.playerKey(!hostBlack) == m_opponent->playerKey && a.sessionKey(!hostBlack) == m_opponent->sessionKey
        && a.peerIdHash(hostBlack) == Cbor::sha256(m_config.myPeerId)
        && a.peerIdHash(!hostBlack) == Cbor::sha256(applicantPeerId) && a.rulesHash == payload.rules.hash()
        && a.timeControlHash == m_terms.hash() && a.networkPolicyHash == NetworkPolicy::v1().hash();
    if (!matches || !a.hasDistinctKeys()) return fail(ErrorCode::ConditionMismatch, true);
    // 期限は今から3分以内（相手の時計のずれを少し見込む）
    const quint64 t = now();
    if (a.startBefore <= t || a.startBefore > t + kStartWindowSeconds + kClockSkewSeconds) {
        return fail(ErrorCode::Expired, true);
    }
    m_agreement = a;
    m_rules = payload.rules;
    m_opponentKind = applicantKind;
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
    s.side = sideCode(turnOf(m_iAmBlack));
    s.signature = m_myStartSignature;
    send(FrameType::StartSignature, startSignaturePayload(s), gameId());
}

void Negotiation::handleStartSignature(const SideSignature& signature)
{
    if (signature.side != sideCode(turnOf(!m_iAmBlack)) || opponentVerified()) return fail(ErrorCode::InvalidSignature, true);
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

MatchSetup Negotiation::matchSetup() const
{
    MatchSetup setup;
    if (m_agreement) setup.agreement = *m_agreement;
    setup.rules = m_rules;
    setup.terms = m_terms;
    setup.iAmBlack = m_iAmBlack;
    std::tie(setup.blackStartSignature, setup.whiteStartSignature) =
        inSideOrder(turnOf(m_iAmBlack), m_myStartSignature, m_opponentStartSignature);
    setup.myConnectionRandom = m_myRandom;
    setup.opponentConnectionRandom = m_opponent ? m_opponentRandom : QByteArray();
    setup.nextRequestId = m_requestId;
    if (m_opponent) setup.opponentDisplayName = m_opponent->displayName;
    return setup;
}

} // namespace P2p
