/// @file p2pknownpeers.cpp
/// @brief 既知の相手の記録の実装

#include "p2pknownpeers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "p2pcbor.h"
#include "p2pdisplayname.h"
#include "p2pownerfile.h"
#include "p2pprotocol.h"

namespace {

using P2p::CborValue;

constexpr qint64 kMaxFileBytes = 8LL * 1024 * 1024;

} // namespace

P2pKnownPeers::P2pKnownPeers(const QString& path)
    : m_path(path)
{
}

bool P2pKnownPeers::load()
{
    m_peers.clear();
    QFile file(m_path);
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) return false;
    const std::optional<CborValue> value = P2p::Cbor::decode(file.read(kMaxFileBytes));
    if (!value) return false;
    P2p::CborReader r(*value, "SBQ-PEERS", 3);
    const std::vector<CborValue>& items = r.array(2, kMaxPeers);
    if (!r.ok()) return false;
    for (const CborValue& item : items) {
        P2p::CborReader p(item, nullptr, 7);
        P2pKnownPeer peer;
        peer.playerKey = p.bytes(0, P2p::kPublicKeySize);
        peer.localName = p.utf8(1, 256);
        peer.lastDisplayName = p.utf8(2, 96);
        peer.verified = p.uint(3, 1) == 1;
        peer.blocked = p.uint(4, 1) == 1;
        peer.games = static_cast<quint32>(p.uint(5, 0xFFFFFFFFu));
        peer.unconfirmed = static_cast<quint32>(p.uint(6, 0xFFFFFFFFu));
        if (p.ok()) m_peers.append(peer);
    }
    return true;
}

bool P2pKnownPeers::save() const
{
    std::vector<CborValue> items;
    items.reserve(static_cast<std::size_t>(m_peers.size()));
    for (const P2pKnownPeer& peer : m_peers) {
        items.push_back(CborValue::array({CborValue::bytes(peer.playerKey), CborValue::text(peer.localName),
                                          CborValue::text(peer.lastDisplayName), CborValue::uint(peer.verified ? 1 : 0),
                                          CborValue::uint(peer.blocked ? 1 : 0), CborValue::uint(peer.games),
                                          CborValue::uint(peer.unconfirmed)}));
    }
    const QByteArray bytes = P2p::Cbor::encode(
        CborValue::array({CborValue::text("SBQ-PEERS"), CborValue::uint(1), CborValue::array(std::move(items))}));
    return P2pOwnerFile::writeAtomically(m_path, bytes);
}

const P2pKnownPeer* P2pKnownPeers::find(const QByteArray& playerKey) const
{
    for (const P2pKnownPeer& peer : m_peers) {
        if (peer.playerKey == playerKey) return &peer;
    }
    return nullptr;
}

P2pKnownPeer& P2pKnownPeers::entry(const QByteArray& playerKey)
{
    for (P2pKnownPeer& peer : m_peers) {
        if (peer.playerKey == playerKey) return peer;
    }
    P2pKnownPeer peer;
    peer.playerKey = playerKey;
    m_peers.append(peer);
    return m_peers.last();
}

bool P2pKnownPeers::recordGameStarted(const QByteArray& playerKey, const QString& displayName)
{
    P2pKnownPeer& peer = entry(playerKey);
    peer.lastDisplayName = displayName;
    ++peer.games;
    return save();
}

bool P2pKnownPeers::recordUnconfirmed(const QByteArray& playerKey)
{
    ++entry(playerKey).unconfirmed;
    return save();
}

bool P2pKnownPeers::setLocalName(const QByteArray& playerKey, const QString& name)
{
    entry(playerKey).localName = name.trimmed().left(kMaxLocalNameLength);
    return save();
}

bool P2pKnownPeers::setVerified(const QByteArray& playerKey, bool verified)
{
    entry(playerKey).verified = verified;
    return save();
}

bool P2pKnownPeers::setBlocked(const QByteArray& playerKey, bool blocked)
{
    entry(playerKey).blocked = blocked;
    return save();
}

bool P2pKnownPeers::remove(const QByteArray& playerKey)
{
    m_peers.removeIf([&playerKey](const P2pKnownPeer& peer) { return peer.playerKey == playerKey; });
    return save();
}

QList<P2pKnownPeer> P2pKnownPeers::sameNameDifferentId(const QString& displayName, const QByteArray& playerKey) const
{
    QList<P2pKnownPeer> result;
    const QString key = P2p::DisplayName::comparisonKey(displayName);
    for (const P2pKnownPeer& peer : m_peers) {
        if (peer.playerKey == playerKey || peer.localName.isEmpty()) continue;
        if (P2p::DisplayName::comparisonKey(peer.lastDisplayName) == key) result.append(peer);
    }
    return result;
}
