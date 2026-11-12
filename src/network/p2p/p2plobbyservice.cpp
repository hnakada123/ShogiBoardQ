/// @file p2plobbyservice.cpp
/// @brief P2P対局の通信プロセスの窓口の実装

#include "p2plobbyservice.h"

#include <QDateTime>

#include "logcategories.h"
#include "p2pbuildinfo.h"
#include "p2pidentitystore.h"
#include "p2pplayerprofile.h"
#include "peeripc.h"

P2pLobbyService::P2pLobbyService(const Deps& deps, P2pPlayerProfile* profile, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
    , m_profile(profile)
{
    if (m_deps.activity) {
        connect(m_deps.activity, &AppActivityState::revoked, this, &P2pLobbyService::onActivityRevoked);
    }
    if (m_deps.game) {
        connect(m_deps.game, &P2pGameCoordinator::resultAgreed, this, &P2pLobbyService::onGameReleased);
        connect(m_deps.game, &P2pGameCoordinator::gameFinished, this, &P2pLobbyService::onGameReleased);
    }
}

P2pLobbyService::~P2pLobbyService() = default;

qint64 P2pLobbyService::buildExpiresAt()
{
    return kP2pBuildEpochSeconds + kP2pBuildLifetimeDays * 24 * 60 * 60;
}

bool P2pLobbyService::isBuildExpired()
{
    return QDateTime::currentSecsSinceEpoch() > buildExpiresAt();
}

// ---------------------------------------------------------------- 開始と終了

P2pLobbyService::StartFailure P2pLobbyService::begin(const QString& networkName, const QStringList& bootstrap,
                                                     bool relayOnly)
{
    if (m_automationDisabled) return StartFailure::AutomationEnabled;
    if (m_status == Status::Starting || m_status == Status::Online) return StartFailure::None;
    const std::optional<P2p::NetworkInfo> network = P2p::findNetwork(networkName);
    if (!network) return StartFailure::NetworkNotAllowed;
    // 期限を過ぎたビルドは、限定試験用と公開用のネットワークへ参加しない（ローカル検証用は使える）
    if (!network->local && isBuildExpired()) return StartFailure::BuildExpired;
    // 保存先を使えるアプリは同時に1つ（実装仕様 9章）
    if (!m_profile || !m_profile->open()) return StartFailure::Locked;
    if (!m_profile->identity()->isUnlocked()) return StartFailure::NoIdentity;

    m_networkName = networkName;
    m_bootstrap = bootstrap.mid(0, 16);
    m_relayOnly = relayOnly;
    m_localProfile = network->local;
    m_status = Status::Starting;
    m_peerFailure = PeerProcess::Failure::None;
    m_connectedPeers = 0;
    m_relayReservations = 0;
    holdConnectionActivity();

    m_peer = new PeerProcess(this);
    connect(m_peer, &PeerProcess::ready, this, &P2pLobbyService::onPeerReady);
    connect(m_peer, &PeerProcess::replied, this, &P2pLobbyService::onPeerReplied);
    connect(m_peer, &PeerProcess::requestFailed, this, &P2pLobbyService::onPeerRequestFailed);
    connect(m_peer, &PeerProcess::notified, this, &P2pLobbyService::onPeerNotified);
    connect(m_peer, &PeerProcess::failed, this, &P2pLobbyService::onPeerFailed);
    connect(m_peer, &PeerProcess::stopped, this, &P2pLobbyService::onPeerStopped);
    if (m_deps.game) {
        m_deps.game->setEnvironment(this, m_profile->records(), m_profile->knownPeers(), m_profile->identity()->key());
    }
    emit statusChanged();
    m_peer->start();
    return StartFailure::None;
}

void P2pLobbyService::end()
{
    if (m_deps.game) m_deps.game->leaveGame();
    if (m_peer && m_peer->isReady()) m_peer->request(PeerIpc::Requests::cancelInvites());
    m_inviteActive = false;
    if (m_peer) m_peer->stop();
    m_status = Status::Off;
    m_lease.reset();
    if (m_profile) m_profile->close();
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
    // 知らせは次のイベントループで届く。その前に取り直した（対局が同じ呼び出しの中で終わった）なら古い知らせ
    if (m_lease.isCurrent()) return;
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
    m_peer->request(PeerIpc::Requests::start(m_networkName, m_bootstrap, m_relayOnly));
}

void P2pLobbyService::onPeerReplied(quint64 requestId, const QString& method, const QJsonObject& result)
{
    const std::optional<PeerIpc::Reply> reply = PeerIpc::readReply(method, result);
    if (!reply) return;
    if (const auto* started = std::get_if<PeerIpc::StartResult>(&*reply)) {
        m_peerIdText = started->peerId;
        m_peerIdBytes = started->peerIdBytes;
        m_networkId = started->networkId;
        // 通信プロセスが返した networkId を、こちらで計算した値と比べる
        if (m_networkId != P2p::networkId(m_networkName.toLatin1())) {
            onPeerFailed(PeerProcess::Failure::HelloMismatch, QStringLiteral("networkId mismatch"));
            return;
        }
        m_status = Status::Online;
        emit statusChanged();
    } else if (const auto* created = std::get_if<PeerIpc::CreatedInvite>(&*reply)) {
        m_inviteActive = true;
        emit inviteCreated(created->invite, created->expiresAt, created->addressCount);
    } else if (const auto* inspected = std::get_if<PeerIpc::InspectedInvite>(&*reply)) {
        emit inviteInspected(*inspected);
    } else if (const auto* connecting = std::get_if<PeerIpc::ConnectionStarted>(&*reply)) {
        // 招待で接続を始めた（接続し直しの connectPeer は振り分けない）。開いたら交渉を始める
        const auto it = m_connectRequests.find(requestId);
        if (it == m_connectRequests.end()) return;
        P2pGameCoordinator::NegotiationRequest request = it.value();
        m_connectRequests.erase(it);
        request.connection = connecting->connection;
        m_pendingConnections.insert(request.connection, request);
    }
}

void P2pLobbyService::onPeerRequestFailed(quint64 requestId, const QString& method, const QString& code)
{
    m_connectRequests.remove(requestId);
    qCInfo(lcGame) << "P2P request failed:" << method << code;
    if (const std::optional<PeerIpc::InviteRejection> rejection = PeerIpc::inviteRejection(method, code)) {
        emit inviteRejected(*rejection);
    }
}

void P2pLobbyService::onPeerFailed(PeerProcess::Failure failure, const QString& detail)
{
    m_peerFailure = failure;
    m_status = Status::Failed;
    // 通信プロセスの異常終了: 招待・交渉は取消。対局中なら中断（結果未確定）
    if (m_deps.game) {
        m_deps.game->leaveGame();
    }
    m_inviteActive = false;
    m_pendingConnections.clear();
    m_connectRequests.clear();
    emit peerFailed(failure, detail);
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
    const std::optional<PeerIpc::Notification> notification = PeerIpc::readNotification(name, params);
    if (!notification) return;
    if (const auto* status = std::get_if<PeerIpc::NetworkStatus>(&*notification)) {
        m_connectedPeers = status->connectedPeers;
        m_reachability = status->reachability;
        m_relayReservations = status->relayReservations;
        emit networkStatusChanged();
    } else if (const auto* opened = std::get_if<PeerIpc::ConnectionOpened>(&*notification)) {
        handleConnectionOpened(*opened);
    } else if (const auto* received = std::get_if<PeerIpc::FrameReceived>(&*notification)) {
        if (m_deps.game) m_deps.game->onFrame(received->connection, received->frame);
    } else if (const auto* closed = std::get_if<PeerIpc::ConnectionClosed>(&*notification)) {
        if (m_pendingConnections.remove(closed->connection) > 0) emit connectFailed(closed->reason);
        if (m_deps.game) m_deps.game->onConnectionClosed(closed->connection);
    } else if (const auto* route = std::get_if<PeerIpc::ConnectionRoute>(&*notification)) {
        if (m_deps.game) m_deps.game->onRouteChanged(route->connection, route->viaRelay);
    }
}

// ---------------------------------------------------------------- 接続の振り分け

void P2pLobbyService::handleConnectionOpened(const PeerIpc::ConnectionOpened& opened)
{
    const quint64 connection = opened.connection;
    if (!m_deps.game) {
        closeConnection(connection);
        return;
    }

    // 対局の相手の再接続
    if (opened.gameReconnect) {
        m_deps.game->onConnectionOpened(connection, opened.peerId, opened.viaRelay);
        if (!m_deps.game->ownsConnection(connection)) closeConnection(connection);
        return;
    }

    if (opened.outbound) {
        // 自分が招待で接続した
        const auto it = m_pendingConnections.find(connection);
        if (it == m_pendingConnections.end()) {
            closeConnection(connection);
            return;
        }
        P2pGameCoordinator::NegotiationRequest request = it.value();
        m_pendingConnections.erase(it);
        request.opponentPeerId = opened.peerId;
        request.opponentPeerIdBytes = opened.peerIdBytes;
        request.viaRelay = opened.viaRelay;
        if (!m_deps.game->startNegotiation(request)) closeConnection(connection);
        return;
    }

    // 自分の招待を使った相手からの接続。交渉中・対局中なら BUSY、招待を取り消した後なら期限切れ
    if (m_deps.game->isBusy()) {
        rejectConnection(connection, P2p::ErrorCode::Busy);
        return;
    }
    if (!m_inviteActive) {
        rejectConnection(connection, P2p::ErrorCode::Expired);
        return;
    }
    P2pGameCoordinator::NegotiationRequest request;
    request.connection = connection;
    request.opponentPeerId = opened.peerId;
    request.opponentPeerIdBytes = opened.peerIdBytes;
    request.role = P2p::kRoleHost;
    request.viaRelay = opened.viaRelay;
    request.myDisplayName = m_inviteDisplayName;
    request.mySide = m_inviteSide;
    request.hostTerms = m_inviteTerms;
    if (!m_deps.game->startNegotiation(request)) rejectConnection(connection, P2p::ErrorCode::Busy);
}

void P2pLobbyService::rejectConnection(quint64 connection, P2p::ErrorCode code)
{
    sendFrame(connection, P2p::rejectFrame(code).encode());
    closeConnection(connection);
}

// ---------------------------------------------------------------- 招待

void P2pLobbyService::createInvite(const QString& displayName, const P2p::TimeControlTerms& terms,
                                   P2p::SidePreference side)
{
    if (!m_peer || m_status != Status::Online) return;
    m_inviteDisplayName = displayName;
    m_inviteTerms = terms;
    m_inviteSide = side;
    m_peer->request(PeerIpc::Requests::createInvite());
}

void P2pLobbyService::cancelInvites()
{
    m_inviteActive = false;
    if (m_peer && m_peer->isReady()) m_peer->request(PeerIpc::Requests::cancelInvites());
}

void P2pLobbyService::inspectInvite(const QString& invite)
{
    if (!m_peer || m_status != Status::Online) return;
    m_peer->request(PeerIpc::Requests::inspectInvite(invite));
}

void P2pLobbyService::connectInvite(const QString& invite, bool allowPrivate, const QString& myDisplayName)
{
    if (!m_peer || m_status != Status::Online || (m_deps.game && m_deps.game->isBusy())) return;
    P2pGameCoordinator::NegotiationRequest request;
    request.role = P2p::kRoleApplicant;
    request.myDisplayName = myDisplayName;
    // 招待では先後の希望を「指定なし」にし、条件は招待した側の HELLO の応答で初めて知る
    request.mySide = P2p::SidePreference::None;
    const quint64 id = m_peer->request(PeerIpc::Requests::connectInvite(invite, allowPrivate));
    if (id != 0) m_connectRequests.insert(id, request);
}

// ---------------------------------------------------------------- P2pTransport

bool P2pLobbyService::sendFrame(quint64 connection, const QByteArray& frame)
{
    if (!m_peer || !m_peer->isReady() || connection == 0) return false;
    return m_peer->request(PeerIpc::Requests::sendFrame(connection, frame)) != 0;
}

void P2pLobbyService::closeConnection(quint64 connection)
{
    if (!m_peer || !m_peer->isReady() || connection == 0) return;
    m_peer->request(PeerIpc::Requests::closeConnection(connection));
}

void P2pLobbyService::setGamePeer(const QString& peerId)
{
    if (!m_peer || !m_peer->isReady()) return;
    m_peer->request(PeerIpc::Requests::setGamePeer(peerId));
}

void P2pLobbyService::reconnectGamePeer(const QString& peerId)
{
    if (!m_peer || !m_peer->isReady() || peerId.isEmpty()) return;
    m_peer->request(PeerIpc::Requests::connectPeer(peerId));
}
