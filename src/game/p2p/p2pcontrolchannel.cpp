/// @file p2pcontrolchannel.cpp
/// @brief 対局中の制御メッセージの通し番号・署名・1局の件数の実装

#include "p2pcontrolchannel.h"

namespace P2p {

void ControlChannel::restart()
{
    m_connectionId.clear();
    m_sent = 0;
    m_received = 0;
}

ControlPayload ControlChannel::make(ControlKind kind, const CborValue& body, const SigningKey& sessionKey)
{
    ControlPayload payload;
    payload.control.gameId = m_gameId;
    payload.control.connectionId = m_connectionId;
    payload.control.sequence = ++m_sent;
    payload.control.kind = kind;
    payload.control.bodyHash = Cbor::hash(body);
    payload.body = body;
    payload.signature = sessionKey.sign(payload.control.toCbor());
    ++m_count;
    return payload;
}

ControlChannel::Receipt ControlChannel::receive(const ControlPayload& payload, const QByteArray& opponentSessionKey)
{
    const Control& c = payload.control;
    if (c.gameId != m_gameId || c.connectionId != m_connectionId) return Receipt::Ignored;
    // 同じ接続での再送（同じ番号）は1回だけ扱う
    if (c.sequence <= m_received) return Receipt::Ignored;
    if (c.sequence != m_received + 1) return Receipt::OutOfOrder;
    if (!Crypto::verify(opponentSessionKey, c.toCbor(), payload.signature)) return Receipt::BadSignature;
    m_received = c.sequence;
    return ++m_count > kMaxMessages ? Receipt::OverLimit : Receipt::Accepted;
}

} // namespace P2p
