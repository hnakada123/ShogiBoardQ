/// @file p2plobbyservice.cpp
/// @brief P2P対局の通信プロセスの窓口の実装

#include "p2plobbyservice.h"

#include <QPointer>

#include <variant>

#include "logcategories.h"
#include "p2pbuildexpiry.h"
#include "p2pidentitystore.h"
#include "p2pplayerprofile.h"
#include "peeripc.h"

namespace {
/// 接続の状態の知らせを出す間隔の下限
constexpr int kNetworkStatusIntervalMs = 250;
} // namespace

P2pLobbyService::P2pLobbyService(const Deps& deps, P2pPlayerProfile* profile, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
    , m_profile(profile)
    , m_networkStatusTimer(new QTimer(this))
{
    m_networkStatusTimer->setSingleShot(true);
    m_networkStatusTimer->setInterval(kNetworkStatusIntervalMs);
    connect(m_networkStatusTimer, &QTimer::timeout, this, &P2pLobbyService::networkStatusChanged);
    if (m_deps.activity) {
        connect(m_deps.activity, &AppActivityState::revoked, this, &P2pLobbyService::onActivityRevoked);
        // ほかの動作（対局・検討など）が始まる・終わると、接続できるか（canBegin）が変わる
        connect(m_deps.activity, &AppActivityState::changed, this, &P2pLobbyService::statusChanged);
    }
    if (m_deps.game) {
        connect(m_deps.game, &P2pGameCoordinator::resultAgreed, this, &P2pLobbyService::onGameReleased);
        connect(m_deps.game, &P2pGameCoordinator::gameFinished, this, &P2pLobbyService::onGameReleased);
        connect(m_deps.game, &P2pGameCoordinator::inviteRefused, this, &P2pLobbyService::onInviteRefused);
    }
}

P2pLobbyService::~P2pLobbyService() = default;

// ---------------------------------------------------------------- 開始と終了

P2pLobbyService::StartFailure P2pLobbyService::begin(const QString& networkName, const QStringList& bootstrap,
                                                     bool relayOnly)
{
    if (m_automationDisabled) return StartFailure::AutomationEnabled;
    if (m_status == Status::Starting || m_status == Status::Online) return StartFailure::None;
    const std::optional<P2p::NetworkInfo> network = P2p::findNetwork(networkName);
    if (!network) return StartFailure::NetworkNotAllowed;
    // 期限を過ぎたビルドは、インターネットのネットワークへ参加しない（ローカルのネットワークは使える）
    if (!network->local && P2pBuildExpiry::isExpired()) return StartFailure::BuildExpired;
    // 保存先を使えるアプリは同時に1つ（実装仕様 9章）
    if (!m_profile || !m_profile->open()) return StartFailure::Locked;
    if (!m_profile->identity()->isUnlocked()) return StartFailure::NoIdentity;
    // ほかの動作（ローカル対局・CSA 通信対局・検討など）を取り上げない（待機中だけ始める）
    if (!canBegin()) return StartFailure::Busy;

    m_networkName = networkName;
    m_bootstrap = bootstrap.mid(0, 16);
    m_relayOnly = relayOnly;
    m_localProfile = network->local;
    m_status = Status::Starting;
    m_connectedPeers = 0;
    m_reachability = PeerIpc::Reachability::Unknown;
    m_relayReservations = 0;
    m_meetNodes = 0;
    holdConnectionActivity();

    // 止めている途中の前の通信プロセスは、知らせを受けずに、止まったら消す（古い知らせを新しい接続に混ぜない。
    // 前のプロセスの stopped で新しいプロセスを消さない）
    if (m_peer) {
        PeerProcess* previous = m_peer;
        m_peer = nullptr;
        previous->disconnect(this);
        if (previous->isRunning()) {
            connect(previous, &PeerProcess::stopped, previous, &QObject::deleteLater);
            previous->stop();
        } else {
            previous->deleteLater();
        }
    }
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
    // 同じ LAN へ出られるのは、ローカルのネットワークだけ（Windows では OS が止める。実装仕様 10.4節）
    m_peer->start(network->local ? PeerConfinement::Reach::LocalNetwork : PeerConfinement::Reach::Internet,
                  m_profile ? m_profile->playerNumber() : 1);
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

void P2pLobbyService::waitForExit(int msecs)
{
    const QPointer<PeerProcess> peer = m_peer;
    if (peer) peer->waitForExit(msecs);
}

bool P2pLobbyService::canBegin() const
{
    return !m_deps.activity || m_deps.activity->is(AppActivityState::Activity::Idle);
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

void P2pLobbyService::onPeerReplied(quint64 requestId, const PeerIpc::Reply& reply)
{
    std::visit([this, requestId](const auto& value) { handleReply(requestId, value); }, reply);
}

void P2pLobbyService::handleReply(quint64, const PeerIpc::Hello&)
{
    // hello は PeerProcess が受ける（ここには届かない）
}

void P2pLobbyService::handleReply(quint64, const PeerIpc::StartResult& started)
{
    m_peerIdBytes = started.peerIdBytes;
    m_networkId = started.networkId;
    // 通信プロセスが返した networkId を、こちらで計算した値と比べる
    if (m_networkId != P2p::networkId(m_networkName.toLatin1())) {
        // 食い違う通信プロセスは使わない（ネットワークに参加したまま残さない）
        m_peer->stop();
        onPeerFailed(PeerProcess::Failure::HelloMismatch, QStringLiteral("networkId mismatch"));
        return;
    }
    m_status = Status::Online;
    emit statusChanged();
}

void P2pLobbyService::handleReply(quint64, const PeerIpc::CreatedInvite& created)
{
    m_inviteActive = true;
    emit inviteCreated(created.invite, created.code, created.expiresAt, created.addressCount);
}

void P2pLobbyService::handleReply(quint64, const PeerIpc::InspectedInvite& inspected)
{
    emit inviteInspected(inspected);
}

void P2pLobbyService::handleReply(quint64 requestId, const PeerIpc::ConnectionStarted& connecting)
{
    // 招待・友だちへの申し込みで接続を始めた（接続し直しの connectPeer は振り分けない）。開いたら交渉を始める
    const auto it = m_connectRequests.find(requestId);
    if (it == m_connectRequests.end()) return;
    PendingConnect pending = it.value();
    m_connectRequests.erase(it);
    pending.request.connection = connecting.connection;
    m_pendingConnections.insert(connecting.connection, pending);
}

void P2pLobbyService::handleReply(quint64, const PeerIpc::Done&)
{
}

void P2pLobbyService::handleReply(quint64 requestId, const PeerIpc::CreatedPresence& presence)
{
    emit presenceCreated(requestId, presence);
}

void P2pLobbyService::handleReply(quint64 requestId, const PeerIpc::MeetStored& stored)
{
    emit meetStored(requestId, stored.stored);
}

void P2pLobbyService::handleReply(quint64 requestId, const PeerIpc::MeetFetched& fetched)
{
    emit meetFetched(requestId, fetched.blobs);
}

void P2pLobbyService::onPeerRequestFailed(quint64 requestId, PeerIpc::Method method, const QString& errorCode)
{
    qCInfo(lcGame) << "P2P request failed:" << PeerIpc::methodName(method) << errorCode;
    const std::optional<PendingConnect> pending =
        m_connectRequests.contains(requestId) ? std::optional<PendingConnect>(m_connectRequests.take(requestId)) : std::nullopt;
    const std::optional<PeerIpc::InviteRejection> rejection = PeerIpc::inviteRejection(method, errorCode);
    if (pending && rejection) {
        failAttempt(pending->attempt, ConnectFailure::Stage::Rejected, *rejection, PeerIpc::CloseReason::Closed);
        return;
    }
    if (method == PeerIpc::Method::InspectInvite && rejection) {
        emit inspectRejected(*rejection);
        return;
    }
    switch (method) {
    case PeerIpc::Method::CreatePresence:
    case PeerIpc::Method::MeetPut:
    case PeerIpc::Method::MeetGet:
        emit presenceRequestFailed(requestId);
        return;
    default:
        return;
    }
}

void P2pLobbyService::onPeerFailed(PeerProcess::Failure failure, const QString& detail)
{
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

void P2pLobbyService::onPeerNotified(const PeerIpc::Notification& notification)
{
    std::visit([this](const auto& value) { handleNotification(value); }, notification);
}

void P2pLobbyService::handleNotification(const PeerIpc::NetworkStatus& status)
{
    const bool changed = m_connectedPeers != status.connectedPeers || m_reachability != status.reachability
        || m_relayReservations != status.relayReservations || m_meetNodes != status.meetNodes;
    m_connectedPeers = status.connectedPeers;
    m_reachability = status.reachability;
    m_relayReservations = status.relayReservations;
    m_meetNodes = status.meetNodes;
    // 変わったときだけ、少しまとめてから知らせる（通信プロセスが知らせを続けて送っても、画面の負荷にしない）
    if (changed && !m_networkStatusTimer->isActive()) m_networkStatusTimer->start();
}

void P2pLobbyService::handleNotification(const PeerIpc::ConnectionRoute& route)
{
    if (m_deps.game) m_deps.game->onRouteChanged(route.connection, route.viaRelay);
}

void P2pLobbyService::handleNotification(const PeerIpc::FrameReceived& received)
{
    if (m_deps.game) m_deps.game->onFrame(received.connection, received.frame);
}

void P2pLobbyService::handleNotification(const PeerIpc::ConnectionClosed& closed)
{
    if (m_pendingConnections.contains(closed.connection)) {
        const PendingConnect pending = m_pendingConnections.take(closed.connection);
        // 合言葉の宛先で応えた相手が、合言葉を知っていることを確かめられなかった（確認の値が返らない・合わない）。
        // 開いた後に相手が何も返さずに閉じたときと同じく、断られたものとして知らせる（監査 2026-11-21 S3）
        const ConnectFailure::Stage stage = closed.reason == PeerIpc::CloseReason::Refused
                                                ? ConnectFailure::Stage::Refused
                                                : ConnectFailure::Stage::NotOpened;
        failAttempt(pending.attempt, stage, PeerIpc::InviteRejection::Unreadable, closed.reason);
    }
    if (m_deps.game) m_deps.game->onConnectionClosed(closed.connection);
}

// ---------------------------------------------------------------- 接続の振り分け

void P2pLobbyService::handleNotification(const PeerIpc::ConnectionOpened& opened)
{
    const quint64 connection = opened.connection;
    if (!m_deps.game) {
        closeConnection(connection);
        return;
    }

    // 対局の相手の再接続
    if (opened.origin == PeerIpc::Origin::Game) {
        m_deps.game->onConnectionOpened(connection, opened.peerId, opened.viaRelay);
        if (!m_deps.game->ownsConnection(connection)) closeConnection(connection);
        return;
    }

    if (opened.outbound) {
        // 自分が招待・友だちへの申し込みで接続した
        const auto it = m_pendingConnections.find(connection);
        if (it == m_pendingConnections.end()) {
            closeConnection(connection);
            return;
        }
        PendingConnect pending = it.value();
        m_pendingConnections.erase(it);
        pending.request.opponentPeerId = opened.peerId;
        pending.request.opponentPeerIdBytes = opened.peerIdBytes;
        pending.request.viaRelay = opened.viaRelay;
        if (!m_deps.game->startNegotiation(pending.request)) {
            closeConnection(connection);
            return;
        }
        m_negotiatingAttempt = pending.attempt;
        return;
    }

    // 自分の招待を使った相手・在席の知らせを引き取った友だちからの接続。交渉中・対局中なら BUSY
    if (m_deps.game->isBusy()) {
        rejectConnection(connection, P2p::ErrorCode::Busy);
        return;
    }
    if (opened.origin == PeerIpc::Origin::Friend) {
        // 友だちの申し込み。だれの知らせか・自分の条件は、友だち一覧の部品が決める（受けないなら BUSY）
        std::optional<P2pGameCoordinator::NegotiationRequest> request =
            m_friendHandler ? m_friendHandler(opened.presence) : std::nullopt;
        if (!request || request->friendPlayerKey.isEmpty()) {
            rejectConnection(connection, P2p::ErrorCode::Busy);
            return;
        }
        request->connection = connection;
        request->opponentPeerId = opened.peerId;
        request->opponentPeerIdBytes = opened.peerIdBytes;
        request->role = P2p::kRoleHost;
        request->viaRelay = opened.viaRelay;
        if (!m_deps.game->startNegotiation(*request)) rejectConnection(connection, P2p::ErrorCode::Busy);
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
    request.mine = m_inviteConditions;
    if (!m_deps.game->startNegotiation(request)) rejectConnection(connection, P2p::ErrorCode::Busy);
}

void P2pLobbyService::rejectConnection(quint64 connection, P2p::ErrorCode code)
{
    sendFrame(connection, P2p::rejectFrame(code).encode());
    closeConnection(connection);
}

// ---------------------------------------------------------------- 招待

void P2pLobbyService::createInvite(const P2pGameCoordinator::Conditions& mine)
{
    if (!m_peer || m_status != Status::Online) return;
    m_inviteConditions = mine;
    m_peer->request(PeerIpc::Requests::createInvite());
}

void P2pLobbyService::inspectInvite(const QString& invite)
{
    if (!m_peer || m_status != Status::Online) return;
    m_peer->request(PeerIpc::Requests::inspectInvite(invite));
}

void P2pLobbyService::connectInvite(const QString& invite, ConnectAttempt attempt, bool allowPrivate,
                                    const P2pGameCoordinator::Conditions& mine)
{
    PendingConnect pending;
    pending.attempt = attempt;
    pending.request.mine = mine;
    startConnect(PeerIpc::Requests::connectInvite(invite, allowPrivate), pending);
}

// ---------------------------------------------------------------- 友だち一覧

quint64 P2pLobbyService::createPresence()
{
    if (!m_peer || m_status != Status::Online) return 0;
    return m_peer->request(PeerIpc::Requests::createPresence());
}

void P2pLobbyService::cancelPresence()
{
    if (m_peer && m_peer->isReady() && m_status == Status::Online) m_peer->request(PeerIpc::Requests::cancelPresence());
}

quint64 P2pLobbyService::meetPut(const QByteArray& slot, const QByteArray& blob, int ttlSeconds)
{
    if (!m_peer || m_status != Status::Online) return 0;
    return m_peer->request(PeerIpc::Requests::meetPut(slot, blob, ttlSeconds));
}

quint64 P2pLobbyService::meetGet(const QByteArray& slot)
{
    if (!m_peer || m_status != Status::Online) return 0;
    return m_peer->request(PeerIpc::Requests::meetGet(slot));
}

bool P2pLobbyService::connectPresence(const QByteArray& presence, const P2pGameCoordinator::NegotiationRequest& request)
{
    if (request.friendPlayerKey.isEmpty()) return false;
    PendingConnect pending;
    pending.attempt = ConnectAttempt::Friend;
    pending.request = request;
    return startConnect(PeerIpc::Requests::connectPresence(presence, false), pending);
}

bool P2pLobbyService::startConnect(const PeerIpc::Request& ipcRequest, const PendingConnect& pending)
{
    if (!m_peer || m_status != Status::Online || (m_deps.game && m_deps.game->isBusy())) return false;
    PendingConnect applicant = pending;
    applicant.request.role = P2p::kRoleApplicant;
    // 先後の希望は「指定なし」にし、条件は相手（招待した側・友だち）の HELLO の応答で初めて知る
    applicant.request.mine = applicant.request.mine.asApplicant();
    const quint64 id = m_peer->request(ipcRequest);
    if (id == 0) return false;
    m_connectRequests.insert(id, applicant);
    return true;
}

void P2pLobbyService::failAttempt(ConnectAttempt attempt, ConnectFailure::Stage stage, PeerIpc::InviteRejection rejection,
                                  PeerIpc::CloseReason reason)
{
    ConnectFailure failure;
    failure.attempt = attempt;
    failure.stage = stage;
    failure.rejection = rejection;
    failure.reason = reason;
    emit connectAttemptFailed(failure);
}

void P2pLobbyService::onInviteRefused()
{
    if (!m_negotiatingAttempt) return;
    const ConnectAttempt attempt = *m_negotiatingAttempt;
    m_negotiatingAttempt.reset();
    failAttempt(attempt, ConnectFailure::Stage::Refused, PeerIpc::InviteRejection::Unreadable, PeerIpc::CloseReason::Closed);
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
