/// @file p2plobbyservice.cpp
/// @brief P2P対局の通信プロセスの窓口の実装

#include "p2plobbyservice.h"

#include <QDir>
#include <QJsonArray>

#include "logcategories.h"
#include "p2pbuildinfo.h"
#include "p2pidentitystore.h"
#include "p2pknownpeers.h"
#include "p2precordstore.h"
#include "peeripc.h"

namespace {

QString sideName(P2p::SidePreference side)
{
    switch (side) {
    case P2p::SidePreference::Black: return QStringLiteral("black");
    case P2p::SidePreference::White: return QStringLiteral("white");
    case P2p::SidePreference::None: break;
    }
    return QStringLiteral("none");
}

P2p::SidePreference sideOf(const QString& name)
{
    if (name == QLatin1String("black")) return P2p::SidePreference::Black;
    if (name == QLatin1String("white")) return P2p::SidePreference::White;
    return P2p::SidePreference::None;
}

quint64 u64(const QJsonValue& value)
{
    return PeerIpc::parseU64(value).value_or(0);
}

} // namespace

P2pLobbyService::P2pLobbyService(const Deps& deps, const QString& dataRoot, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
    , m_dataRoot(dataRoot)
    , m_store(std::make_unique<P2pRecordStore>(dataRoot))
    , m_knownPeers(std::make_unique<P2pKnownPeers>(QDir(dataRoot).filePath(QStringLiteral("peers.cbor"))))
    , m_identity(new P2pIdentityStore(m_store->identityDirectory(), this))
{
    if (m_deps.activity) {
        connect(m_deps.activity, &AppActivityState::revoked, this, &P2pLobbyService::onActivityRevoked);
    }
    if (m_deps.game) {
        connect(m_deps.game, &P2pGameCoordinator::resultAgreed, this, &P2pLobbyService::onGameReleased);
        connect(m_deps.game, &P2pGameCoordinator::gameFinished, this, &P2pLobbyService::onGameReleased);
        connect(m_deps.game, &P2pGameCoordinator::negotiationAbandoned, this, &P2pLobbyService::onNegotiationAbandoned);
        connect(m_deps.game, &P2pGameCoordinator::applicantVerified, this, &P2pLobbyService::onApplicantVerified);
    }
}

P2pLobbyService::~P2pLobbyService() = default;

qint64 P2pLobbyService::buildExpiresAt()
{
    return kP2pBuildEpochSeconds + kBuildLifetimeDays * 24 * 60 * 60;
}

bool P2pLobbyService::isBuildExpired()
{
    return QDateTime::currentSecsSinceEpoch() > buildExpiresAt();
}

// ---------------------------------------------------------------- 開始と終了

P2pLobbyService::StartFailure P2pLobbyService::prepare()
{
    if (m_automationDisabled) return StartFailure::AutomationEnabled;
    // p2p/ を使えるアプリは同時に1つ（実装仕様 9章）
    if (!m_store->lock()) return StartFailure::Locked;
    m_store->recoverJournals();
    m_knownPeers->load();
    return StartFailure::None;
}

P2pLobbyService::StartFailure P2pLobbyService::begin(const QString& networkName, const QStringList& bootstrap,
                                                     bool relayOnly)
{
    if (m_automationDisabled) return StartFailure::AutomationEnabled;
    if (m_status == Status::Starting || m_status == Status::Online) return StartFailure::None;
    const std::optional<P2p::NetworkInfo> network = P2p::findNetwork(networkName);
    if (!network) return StartFailure::NetworkNotAllowed;
    // 期限を過ぎたビルドは、限定試験用と公開用のネットワークへ参加しない（ローカル検証用は使える）
    if (!network->local && isBuildExpired()) return StartFailure::BuildExpired;
    if (!m_store->isLocked()) {
        const StartFailure prepared = prepare();
        if (prepared != StartFailure::None) return prepared;
    }
    if (!m_identity->isUnlocked()) return StartFailure::NoIdentity;

    m_networkName = networkName;
    m_bootstrap = bootstrap.mid(0, 16);
    m_relayOnly = relayOnly;
    m_localProfile = network->local;
    m_status = Status::Starting;
    m_peerFailure = PeerProcess::Failure::None;
    m_offers.clear();
    m_offerId.clear();
    holdConnectionActivity();

    m_peer = new PeerProcess(this);
    connect(m_peer, &PeerProcess::ready, this, &P2pLobbyService::onPeerReady);
    connect(m_peer, &PeerProcess::replied, this, &P2pLobbyService::onPeerReplied);
    connect(m_peer, &PeerProcess::requestFailed, this, &P2pLobbyService::onPeerRequestFailed);
    connect(m_peer, &PeerProcess::notified, this, &P2pLobbyService::onPeerNotified);
    connect(m_peer, &PeerProcess::failed, this, &P2pLobbyService::onPeerFailed);
    connect(m_peer, &PeerProcess::stopped, this, &P2pLobbyService::onPeerStopped);
    if (m_deps.game) {
        m_deps.game->setEnvironment(this, m_store.get(), m_knownPeers.get(), m_identity->key());
    }
    emit statusChanged();
    m_peer->start();
    return StartFailure::None;
}

void P2pLobbyService::end()
{
    if (m_deps.game) m_deps.game->leaveGame();
    if (m_peer && m_peer->isReady()) {
        if (hasOffer()) m_peer->request(QStringLiteral("withdrawOffer"));
        m_peer->request(QStringLiteral("cancelInvites"));
    }
    m_offerId.clear();
    m_inviteActive = false;
    if (m_peer) m_peer->stop();
    m_status = Status::Off;
    m_lease.reset();
    m_identity->lock();
    m_store->unlock();
    emit statusChanged();
}

void P2pLobbyService::holdConnectionActivity()
{
    if (!m_deps.activity || (m_deps.game && m_deps.game->isInProgress())) return;
    if (m_lease.isCurrent() && m_deps.activity->is(AppActivityState::Activity::DuringP2pConnection)) return;
    m_lease = m_deps.activity->begin(AppActivityState::Activity::DuringP2pConnection, this);
}

void P2pLobbyService::onActivityRevoked(QObject* owner)
{
    if (owner != this) return;
    // 対局が始まって司令塔が「P2P対局中」にしたのは取り上げではない
    if (m_deps.game && m_deps.game->isBusy()) return;
    if (m_status == Status::Online || m_status == Status::Starting) {
        qCInfo(lcGame) << "P2P: activity revoked, ending P2P play";
        end();
    }
}

void P2pLobbyService::onGameReleased()
{
    if (m_status == Status::Online) holdConnectionActivity();
}

// ---------------------------------------------------------------- 通信プロセス

void P2pLobbyService::onPeerReady()
{
    QJsonObject params;
    params[QStringLiteral("networkName")] = m_networkName;
    params[QStringLiteral("rulesHash")] = QString::fromLatin1(P2p::Rules::v1Hash().toHex());
    params[QStringLiteral("bootstrap")] = QJsonArray::fromStringList(m_bootstrap);
    params[QStringLiteral("knownPeers")] = QJsonArray::fromStringList(m_store->knownPeerAddresses());
    params[QStringLiteral("relayOnly")] = m_relayOnly;
    m_peer->request(QStringLiteral("start"), params);
}

void P2pLobbyService::onPeerReplied(quint64 requestId, const QString& method, const QJsonObject& result)
{
    if (method == QLatin1String("start")) {
        m_peerIdText = result.value(QStringLiteral("peerId")).toString();
        m_peerIdBytes = QByteArray::fromBase64(result.value(QStringLiteral("peerIdBytes")).toString().toLatin1());
        m_networkId = QByteArray::fromHex(result.value(QStringLiteral("networkId")).toString().toLatin1());
        // 通信プロセスが返した networkId を、こちらで計算した値と比べる
        if (m_networkId != P2p::networkId(m_networkName.toLatin1())) {
            onPeerFailed(PeerProcess::Failure::HelloMismatch, QStringLiteral("networkId mismatch"));
            return;
        }
        m_status = Status::Online;
        emit statusChanged();
        refreshOffers();
        return;
    }
    if (method == QLatin1String("publishOffer")) {
        m_offerId = QByteArray::fromHex(result.value(QStringLiteral("offerId")).toString().toLatin1());
        emit statusChanged();
        return;
    }
    if (method == QLatin1String("createInvite")) {
        m_inviteActive = true;
        emit inviteCreated(result.value(QStringLiteral("invite")).toString(), u64(result.value(QStringLiteral("expiresAt"))),
                           result.value(QStringLiteral("addressCount")).toInt());
        return;
    }
    if (method == QLatin1String("inspectInvite")) {
        InviteInfo info;
        info.peerId = result.value(QStringLiteral("peerId")).toString();
        info.expiresAt = u64(result.value(QStringLiteral("expiresAt")));
        info.addressCount = result.value(QStringLiteral("addressCount")).toInt();
        info.usableAddressCount = result.value(QStringLiteral("usableAddressCount")).toInt();
        const QJsonArray array1 = result.value(QStringLiteral("privateAddresses")).toArray();
        for (const auto& address : array1) {
            info.privateAddresses << address.toString();
        }
        emit inviteInspected(info);
        return;
    }
    if (method == QLatin1String("connectInvite") || method == QLatin1String("connectOffer")) {
        // 接続の番号を受け取った。開いたら交渉を始める
        const auto it = m_connectRequests.find(requestId);
        if (it == m_connectRequests.end()) return;
        P2pGameCoordinator::NegotiationRequest request = it.value();
        m_connectRequests.erase(it);
        request.connection = u64(result.value(QStringLiteral("connection")));
        m_pendingConnections.insert(request.connection, request);
        return;
    }
}

void P2pLobbyService::onPeerRequestFailed(quint64 requestId, const QString& method, const QString& code)
{
    m_connectRequests.remove(requestId);
    qCInfo(lcGame) << "P2P request failed:" << method << code;
    emit requestFailed(method, code);
}

void P2pLobbyService::onPeerFailed(PeerProcess::Failure failure, const QString& detail)
{
    m_peerFailure = failure;
    m_status = Status::Failed;
    // 通信プロセスの異常終了: 募集・交渉は取消。対局中なら中断（結果未確定）
    if (m_deps.game) {
        m_deps.game->leaveGame();
    }
    m_offerId.clear();
    m_offers.clear();
    m_pendingConnections.clear();
    m_connectRequests.clear();
    emit peerFailed(failure, detail);
    emit offersChanged();
    emit statusChanged();
}

void P2pLobbyService::onPeerStopped()
{
    if (m_peer) {
        m_peer->deleteLater();
        m_peer = nullptr;
    }
    if (m_status != Status::Failed) m_status = Status::Off;
    m_lease.reset();
    emit statusChanged();
}

void P2pLobbyService::onPeerNotified(const QString& name, const QJsonObject& params)
{
    if (name == QLatin1String("networkStatus")) {
        handleNetworkStatus(params);
    } else if (name == QLatin1String("offersChanged")) {
        handleOffersChanged(params);
    } else if (name == QLatin1String("connectionOpened")) {
        handleConnectionOpened(params);
    } else if (name == QLatin1String("frameReceived")) {
        const quint64 connection = connectionOf(params);
        const QByteArray frame = QByteArray::fromBase64(params.value(QStringLiteral("frame")).toString().toLatin1());
        if (m_deps.game) m_deps.game->onFrame(connection, frame);
    } else if (name == QLatin1String("connectionClosed")) {
        const quint64 connection = connectionOf(params);
        if (m_pendingConnections.remove(connection) > 0) {
            emit connectFailed(params.value(QStringLiteral("reason")).toString());
        }
        if (m_deps.game) m_deps.game->onConnectionClosed(connection);
    } else if (name == QLatin1String("connectionRoute")) {
        if (m_deps.game) {
            m_deps.game->onRouteChanged(connectionOf(params),
                                        params.value(QStringLiteral("route")).toString() == QLatin1String("relay"));
        }
    } else if (name == QLatin1String("knownPeers")) {
        QStringList peers;
        const QJsonArray array2 = params.value(QStringLiteral("peers")).toArray();
        for (const auto& peer : array2) peers << peer.toString();
        m_store->saveKnownPeerAddresses(peers);
    }
}

quint64 P2pLobbyService::connectionOf(const QJsonObject& params) const
{
    return u64(params.value(QStringLiteral("connection")));
}

void P2pLobbyService::handleNetworkStatus(const QJsonObject& params)
{
    m_listenAddrs.clear();
    const QJsonArray array3 = params.value(QStringLiteral("listenAddrs")).toArray();
    for (const auto& address : array3) m_listenAddrs << address.toString();
    m_connectedPeers = params.value(QStringLiteral("connectedPeers")).toInt();
    m_reachability = params.value(QStringLiteral("reachability")).toString();
    m_clockSkew = params.value(QStringLiteral("clockSkewSuspected")).toBool();
    emit networkStatusChanged();
}

void P2pLobbyService::handleOffersChanged(const QJsonObject& params)
{
    const QJsonArray array4 = params.value(QStringLiteral("removed")).toArray();
    for (const auto& id : array4) {
        m_offers.remove(QByteArray::fromHex(id.toString().toLatin1()));
    }
    const QJsonArray array5 = params.value(QStringLiteral("added")).toArray();
    for (const auto& item : array5) {
        const QJsonObject o = item.toObject();
        // 通信プロセスが確かめた募集も、形・上限・表示名の規則を確かめ直す（PeerIpc::checkOffer で検査済み）
        Offer offer;
        offer.offerId = QByteArray::fromHex(o.value(QStringLiteral("offerId")).toString().toLatin1());
        offer.peerId = o.value(QStringLiteral("peerId")).toString();
        offer.displayName = o.value(QStringLiteral("displayName")).toString();
        offer.terms.mainMs = u64(o.value(QStringLiteral("mainMs")));
        offer.terms.byoyomiMs = u64(o.value(QStringLiteral("byoyomiMs")));
        offer.side = sideOf(o.value(QStringLiteral("sidePreference")).toString());
        offer.expiresAt = u64(o.value(QStringLiteral("expiresAt")));
        if (!offer.terms.isValid() || o.value(QStringLiteral("rulesHash")).toString() != QString::fromLatin1(P2p::Rules::v1Hash().toHex())) {
            continue;
        }
        m_offers.insert(offer.offerId, offer);
    }
    emit offersChanged();
}

// ---------------------------------------------------------------- 接続の振り分け

void P2pLobbyService::handleConnectionOpened(const QJsonObject& params)
{
    const quint64 connection = connectionOf(params);
    const QString peerId = params.value(QStringLiteral("peerId")).toString();
    const QByteArray peerIdBytes = QByteArray::fromBase64(params.value(QStringLiteral("peerIdBytes")).toString().toLatin1());
    const bool viaRelay = params.value(QStringLiteral("route")).toString() == QLatin1String("relay");
    const QString origin = params.value(QStringLiteral("origin")).toString();
    const bool outbound = params.value(QStringLiteral("direction")).toString() == QLatin1String("outbound");
    if (!m_deps.game) {
        closeConnection(connection);
        return;
    }

    // 対局の相手の再接続
    if (origin == QLatin1String("game")) {
        m_deps.game->onConnectionOpened(connection, peerId, viaRelay);
        if (!m_deps.game->ownsConnection(connection)) closeConnection(connection);
        return;
    }

    if (outbound) {
        // 自分が申し込んだ・招待で接続した
        const auto it = m_pendingConnections.find(connection);
        if (it == m_pendingConnections.end()) {
            closeConnection(connection);
            return;
        }
        P2pGameCoordinator::NegotiationRequest request = it.value();
        m_pendingConnections.erase(it);
        request.opponentPeerId = peerId;
        request.opponentPeerIdBytes = peerIdBytes;
        request.viaRelay = viaRelay;
        if (!m_deps.game->startNegotiation(request)) closeConnection(connection);
        return;
    }

    // 相手からの申込み（募集・招待）。交渉中・対局中なら BUSY、再申込みの拒否の対象も BUSY
    if (m_deps.game->isBusy() || isReapplyBlocked(peerId)) {
        rejectConnection(connection, P2p::ErrorCode::Busy);
        return;
    }
    P2pGameCoordinator::NegotiationRequest request;
    request.connection = connection;
    request.opponentPeerId = peerId;
    request.opponentPeerIdBytes = peerIdBytes;
    request.role = P2p::kRoleHost;
    request.viaRelay = viaRelay;
    if (origin == QLatin1String("offer") && hasOffer()) {
        request.myDisplayName = m_offerDisplayName;
        request.mySide = m_offerSide;
        request.offerId = m_offerId;
        request.hostTerms = m_offerTerms;
    } else if (origin == QLatin1String("invite") && m_inviteActive) {
        request.myDisplayName = m_inviteDisplayName;
        request.mySide = m_inviteSide;
        request.offerId = P2p::zeroHash();
        request.hostTerms = m_inviteTerms;
    } else {
        rejectConnection(connection, P2p::ErrorCode::Expired);
        return;
    }
    if (!m_deps.game->startNegotiation(request)) rejectConnection(connection, P2p::ErrorCode::Busy);
}

void P2pLobbyService::rejectConnection(quint64 connection, P2p::ErrorCode code)
{
    P2p::Frame frame;
    frame.type = P2p::FrameType::Reject;
    frame.requestId = 1;
    frame.gameId = P2p::zeroHash();
    frame.payload = P2p::rejectPayload(code);
    sendFrame(connection, frame.encode());
    closeConnection(connection);
}

bool P2pLobbyService::isReapplyBlocked(const QString& peerId) const
{
    const auto it = m_reapplyBlockedUntil.constFind(peerId);
    return it != m_reapplyBlockedUntil.constEnd() && it.value() > QDateTime::currentSecsSinceEpoch();
}

void P2pLobbyService::onNegotiationAbandoned(const QString& peerId, const QByteArray& verifiedPlayerKey)
{
    Q_UNUSED(verifiedPlayerKey);
    // 募集側だけが受ける申込みを、開始に至らなかった相手から10分間拒否する。繰り返す相手には時間を延ばす
    if (peerId.isEmpty()) return;
    const int count = ++m_reapplyCount[peerId];
    const qint64 seconds = kReapplyBlockSeconds * qMin(8, 1 << qMin(3, count - 1));
    m_reapplyBlockedUntil.insert(peerId, QDateTime::currentSecsSinceEpoch() + seconds);
}

void P2pLobbyService::onApplicantVerified(const QString& peerId)
{
    Q_UNUSED(peerId);
    // 申込側の開始署名を確かめた時点で募集を取り下げる（実装仕様 3.3節）
    if (hasOffer()) withdrawOffer();
}

// ---------------------------------------------------------------- 募集

void P2pLobbyService::publishOffer(const QString& displayName, const P2p::TimeControlTerms& terms,
                                   P2p::SidePreference side)
{
    if (!m_peer || m_status != Status::Online) return;
    m_offerDisplayName = displayName;
    m_offerTerms = terms;
    m_offerSide = side;
    QJsonObject params;
    params[QStringLiteral("displayName")] = displayName;
    params[QStringLiteral("mainMs")] = QString::number(terms.mainMs);
    params[QStringLiteral("byoyomiMs")] = QString::number(terms.byoyomiMs);
    params[QStringLiteral("sidePreference")] = sideName(side);
    m_peer->request(QStringLiteral("publishOffer"), params);
}

void P2pLobbyService::withdrawOffer()
{
    if (m_peer && m_peer->isReady() && hasOffer()) m_peer->request(QStringLiteral("withdrawOffer"));
    m_offerId.clear();
    emit statusChanged();
}

void P2pLobbyService::refreshOffers()
{
    if (m_peer && m_status == Status::Online) m_peer->request(QStringLiteral("listOffers"));
}

bool P2pLobbyService::applyToOffer(const QByteArray& offerId, const QString& myDisplayName, P2p::SidePreference mySide)
{
    const auto it = m_offers.constFind(offerId);
    if (!m_peer || m_status != Status::Online || it == m_offers.constEnd() || (m_deps.game && m_deps.game->isBusy())) {
        return false;
    }
    if (!P2p::Negotiation::sidesCompatible(mySide, it->side)) return false;
    P2pGameCoordinator::NegotiationRequest request;
    request.role = P2p::kRoleApplicant;
    request.myDisplayName = myDisplayName;
    request.mySide = mySide;
    request.offerId = offerId;
    // 申込側は、HELLO の応答の条件・表示名・先後の希望が見た募集と同じであることを確かめる
    request.expectedTerms = it->terms;
    request.expectedDisplayName = it->displayName;
    request.expectedHostSide = it->side;
    QJsonObject params;
    params[QStringLiteral("offerId")] = QString::fromLatin1(offerId.toHex());
    const quint64 id = m_peer->request(QStringLiteral("connectOffer"), params);
    if (id == 0) return false;
    m_connectRequests.insert(id, request);
    return true;
}

// ---------------------------------------------------------------- 招待

void P2pLobbyService::createInvite(const QString& displayName, const P2p::TimeControlTerms& terms,
                                   P2p::SidePreference side)
{
    if (!m_peer || m_status != Status::Online) return;
    m_inviteDisplayName = displayName;
    m_inviteTerms = terms;
    m_inviteSide = side;
    m_peer->request(QStringLiteral("createInvite"));
}

void P2pLobbyService::cancelInvites()
{
    m_inviteActive = false;
    if (m_peer && m_peer->isReady()) m_peer->request(QStringLiteral("cancelInvites"));
}

void P2pLobbyService::inspectInvite(const QString& invite)
{
    if (!m_peer || m_status != Status::Online) return;
    QJsonObject params;
    params[QStringLiteral("invite")] = invite;
    m_peer->request(QStringLiteral("inspectInvite"), params);
}

void P2pLobbyService::connectInvite(const QString& invite, bool allowPrivate, const QString& myDisplayName)
{
    if (!m_peer || m_status != Status::Online || (m_deps.game && m_deps.game->isBusy())) return;
    P2pGameCoordinator::NegotiationRequest request;
    request.role = P2p::kRoleApplicant;
    request.myDisplayName = myDisplayName;
    // 招待では先後の希望を「指定なし」にし、条件は招待した側の HELLO の応答で初めて知る
    request.mySide = P2p::SidePreference::None;
    request.offerId = P2p::zeroHash();
    QJsonObject params;
    params[QStringLiteral("invite")] = invite;
    params[QStringLiteral("allowPrivate")] = allowPrivate;
    const quint64 id = m_peer->request(QStringLiteral("connectInvite"), params);
    if (id != 0) m_connectRequests.insert(id, request);
}

// ---------------------------------------------------------------- P2pTransport

bool P2pLobbyService::sendFrame(quint64 connection, const QByteArray& frame)
{
    if (!m_peer || !m_peer->isReady() || connection == 0) return false;
    QJsonObject params;
    params[QStringLiteral("connection")] = QString::number(connection);
    params[QStringLiteral("frame")] = QString::fromLatin1(frame.toBase64());
    return m_peer->request(QStringLiteral("sendFrame"), params) != 0;
}

void P2pLobbyService::closeConnection(quint64 connection)
{
    if (!m_peer || !m_peer->isReady() || connection == 0) return;
    QJsonObject params;
    params[QStringLiteral("connection")] = QString::number(connection);
    m_peer->request(QStringLiteral("closeConnection"), params);
}

void P2pLobbyService::setGamePeer(const QString& peerId)
{
    if (!m_peer || !m_peer->isReady()) return;
    QJsonObject params;
    params[QStringLiteral("peerId")] = peerId;
    m_peer->request(QStringLiteral("setGamePeer"), params);
}

void P2pLobbyService::reconnectGamePeer(const QString& peerId)
{
    if (!m_peer || !m_peer->isReady() || peerId.isEmpty()) return;
    QJsonObject params;
    params[QStringLiteral("peerId")] = peerId;
    m_peer->request(QStringLiteral("connectPeer"), params);
}
