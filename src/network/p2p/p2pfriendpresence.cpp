/// @file p2pfriendpresence.cpp
/// @brief 友だち一覧の在席の実装

#include "p2pfriendpresence.h"

#include <QDateTime>
#include <QTimer>

#include "p2pidentitystore.h"
#include "p2pknownpeers.h"
#include "p2plobbyservice.h"
#include "p2pplayerprofile.h"
#include "p2psignature.h"

namespace {

/// 協力ノードへの要求を続けて送らない（協力ノードは、接続元ごとの頻度を絞る）
constexpr int kStepMs = 300;
constexpr int kExpireMs = 15'000;
/// 預けられなかった知らせ（接続先がまだ無い・協力ノードが預からなかった）を作り直すまでの時間
constexpr int kRetryMs = 5'000;

quint64 nowSeconds()
{
    return static_cast<quint64>(QDateTime::currentSecsSinceEpoch());
}

/// 友だちが預けた知らせを探す期間。預ける側は 75 秒ごとに預け直し、知らせは 180 秒残るので、期間が変わった直後は
/// 前の期間にだけあることがある。時計のずれ（1分まで）に備えて、変わる直前は次の期間も見る
QList<quint64> epochsToTry(quint64 now)
{
    const quint64 epoch = P2p::Friend::epochOf(now);
    const quint64 into = now % P2p::Friend::kEpochSeconds;
    QList<quint64> epochs{epoch};
    if (into < static_cast<quint64>(P2pFriendPresence::kDepositTtlSeconds) && epoch > 0) epochs.append(epoch - 1);
    if (into + 60 >= P2p::Friend::kEpochSeconds) epochs.append(epoch + 1);
    return epochs;
}

} // namespace

P2pFriendPresence::P2pFriendPresence(P2pLobbyService* lobby, P2pGameCoordinator* game, QObject* parent)
    : QObject(parent)
    , m_lobby(lobby)
    , m_game(game)
    , m_depositTimer(new QTimer(this))
    , m_watchTimer(new QTimer(this))
    , m_stepTimer(new QTimer(this))
    , m_expireTimer(new QTimer(this))
    , m_retryTimer(new QTimer(this))
{
    m_retryTimer->setSingleShot(true);
    m_retryTimer->setInterval(kRetryMs);
    connect(m_retryTimer, &QTimer::timeout, this, &P2pFriendPresence::startDepositRound);
    m_depositTimer->setInterval(kDepositIntervalSeconds * 1000);
    m_watchTimer->setInterval(kWatchIntervalSeconds * 1000);
    m_stepTimer->setInterval(kStepMs);
    m_expireTimer->setInterval(kExpireMs);
    connect(m_depositTimer, &QTimer::timeout, this, &P2pFriendPresence::startDepositRound);
    connect(m_watchTimer, &QTimer::timeout, this, &P2pFriendPresence::startWatchRound);
    connect(m_stepTimer, &QTimer::timeout, this, &P2pFriendPresence::step);
    connect(m_expireTimer, &QTimer::timeout, this, &P2pFriendPresence::expire);
    if (m_lobby) {
        connect(m_lobby, &P2pLobbyService::statusChanged, this, &P2pFriendPresence::onLobbyChanged);
        connect(m_lobby, &P2pLobbyService::networkStatusChanged, this, &P2pFriendPresence::onLobbyChanged);
        connect(m_lobby, &P2pLobbyService::presenceCreated, this, &P2pFriendPresence::onPresenceCreated);
        connect(m_lobby, &P2pLobbyService::meetStored, this, &P2pFriendPresence::onMeetStored);
        connect(m_lobby, &P2pLobbyService::meetFetched, this, &P2pFriendPresence::onMeetFetched);
        connect(m_lobby, &P2pLobbyService::presenceRequestFailed, this, &P2pFriendPresence::onRequestFailed);
        // 友だちの申し込みを受けるかは、ここで決める（コールバックの注入。connect ではない）
        m_lobby->setFriendHandler([this](quint64 presenceId) { return acceptApplication(presenceId); });
    }
    if (m_game) {
        // 交渉・対局が終わったら、在席を預け直す（その間は預けていない）
        connect(m_game, &P2pGameCoordinator::gameFinished, this, &P2pFriendPresence::startDepositRound);
        connect(m_game, &P2pGameCoordinator::negotiationEnded, this, &P2pFriendPresence::startDepositRound);
    }
}

P2pFriendPresence::~P2pFriendPresence()
{
    if (m_lobby) m_lobby->setFriendHandler({});
    clearState();
}

QByteArray P2pFriendPresence::myPlayerKey() const
{
    const P2pPlayerProfile* profile = m_lobby ? m_lobby->profile() : nullptr;
    const P2p::SigningKey* key = profile && profile->identity() ? profile->identity()->key() : nullptr;
    return key ? key->publicKey() : QByteArray();
}

QByteArray P2pFriendPresence::pairSecretFor(const QByteArray& playerKey)
{
    const auto known = m_pairSecrets.constFind(playerKey);
    if (known != m_pairSecrets.constEnd()) return known.value();
    const P2pPlayerProfile* profile = m_lobby ? m_lobby->profile() : nullptr;
    const P2p::SigningKey* key = profile && profile->identity() ? profile->identity()->key() : nullptr;
    if (!key) return {};
    const QByteArray secret = P2p::Friend::pairSecret(*key, playerKey);
    if (!secret.isEmpty()) m_pairSecrets.insert(playerKey, secret);
    return secret;
}

bool P2pFriendPresence::isAvailable() const
{
    return m_lobby && m_lobby->status() == P2pLobbyService::Status::Online && m_lobby->meetNodes() > 0
           && !myPlayerKey().isEmpty();
}

bool P2pFriendPresence::canDeposit() const
{
    return m_sharing && isAvailable() && !(m_game && m_game->isBusy());
}

bool P2pFriendPresence::canWatch() const
{
    return m_watching && isAvailable();
}

bool P2pFriendPresence::isOnline(const QByteArray& playerKey) const
{
    const auto it = m_online.constFind(playerKey);
    return it != m_online.constEnd() && it->presence.expiresAt > nowSeconds();
}

void P2pFriendPresence::setSharing(bool sharing)
{
    if (m_sharing == sharing) return;
    m_sharing = sharing;
    if (sharing) {
        startDepositRound();
    } else {
        // 作った知らせを無効にする（協力ノードに残った分は、3分で消える。そのトークンでは、もうつなげない）
        m_depositTimer->stop();
        m_depositQueue.clear();
        m_pendingCreates.clear();
        m_created.clear();
        if (m_lobby) m_lobby->cancelPresence();
    }
}

void P2pFriendPresence::setWatching(bool watching)
{
    if (m_watching == watching) return;
    m_watching = watching;
    if (watching) {
        startWatchRound();
    } else {
        m_watchTimer->stop();
        m_watchQueue.clear();
    }
}

void P2pFriendPresence::reloadFriends()
{
    // 外した友だちの秘密と在席を捨て、知らせを作り直す（外した相手に預けた知らせのトークンを無効にする）
    const P2pKnownPeers* known = m_lobby && m_lobby->profile() ? m_lobby->profile()->knownPeers() : nullptr;
    QList<QByteArray> friends;
    if (known) {
        const QList<P2pKnownPeer> peers = known->friends();
        for (const P2pKnownPeer& peer : peers) friends.append(peer.playerKey);
    }
    bool changed = false;
    for (auto it = m_online.begin(); it != m_online.end();) {
        if (friends.contains(it.key())) {
            ++it;
        } else {
            it = m_online.erase(it);
            changed = true;
        }
    }
    for (auto it = m_pairSecrets.begin(); it != m_pairSecrets.end();) {
        if (friends.contains(it.key())) {
            ++it;
        } else {
            it.value().fill('\0');
            it = m_pairSecrets.erase(it);
        }
    }
    if (m_sharing && isAvailable()) {
        m_created.clear();
        m_pendingCreates.clear();
        if (m_lobby) m_lobby->cancelPresence();
        startDepositRound();
    }
    if (m_watching) startWatchRound();
    if (changed) emit presenceChanged();
}

void P2pFriendPresence::onLobbyChanged()
{
    const bool available = isAvailable();
    if (available == m_wasAvailable) return;
    m_wasAvailable = available;
    if (available) {
        m_expireTimer->start();
        startDepositRound();
        startWatchRound();
    } else {
        clearState();
    }
    emit presenceChanged();
}

void P2pFriendPresence::clearState()
{
    m_depositTimer->stop();
    m_watchTimer->stop();
    m_stepTimer->stop();
    m_expireTimer->stop();
    m_retryTimer->stop();
    m_depositQueue.clear();
    m_watchQueue.clear();
    m_pendingCreates.clear();
    m_pendingGets.clear();
    m_pendingPuts.clear();
    m_created.clear();
    m_online.clear();
    // プレイヤー鍵は接続を終えるとメモリから消える。秘密も持ち続けない
    for (QByteArray& secret : m_pairSecrets) secret.fill('\0');
    m_pairSecrets.clear();
}

void P2pFriendPresence::startDepositRound()
{
    if (!canDeposit()) return;
    const P2pKnownPeers* known = m_lobby->profile() ? m_lobby->profile()->knownPeers() : nullptr;
    if (!known) return;
    m_depositQueue.clear();
    const QList<P2pKnownPeer> friends = known->friends();
    for (const P2pKnownPeer& peer : friends) m_depositQueue.append(peer.playerKey);
    m_depositTimer->start();
    if (!m_depositQueue.isEmpty() && !m_stepTimer->isActive()) m_stepTimer->start();
}

void P2pFriendPresence::startWatchRound()
{
    if (!canWatch()) return;
    const P2pKnownPeers* known = m_lobby->profile() ? m_lobby->profile()->knownPeers() : nullptr;
    if (!known) return;
    m_watchQueue.clear();
    const QList<P2pKnownPeer> friends = known->friends();
    for (const P2pKnownPeer& peer : friends) m_watchQueue.append(peer.playerKey);
    m_watchTimer->start();
    if (!m_watchQueue.isEmpty() && !m_stepTimer->isActive()) m_stepTimer->start();
}

void P2pFriendPresence::step()
{
    if (!isAvailable()) {
        m_stepTimer->stop();
        return;
    }
    // 1回に1つだけ送る。預けるのを先にする（知らせを作らせ、応答が届いたら暗号化して預ける）
    if (!m_depositQueue.isEmpty()) {
        const QByteArray playerKey = m_depositQueue.takeFirst();
        if (canDeposit() && !pairSecretFor(playerKey).isEmpty()) {
            const quint64 request = m_lobby->createPresence();
            if (request != 0) m_pendingCreates.insert(request, playerKey);
        }
    } else if (!m_watchQueue.isEmpty()) {
        const QByteArray playerKey = m_watchQueue.takeFirst();
        if (canWatch()) fetch(PendingGet{playerKey, epochsToTry(nowSeconds())});
    }
    if (m_depositQueue.isEmpty() && m_watchQueue.isEmpty()) m_stepTimer->stop();
}

void P2pFriendPresence::fetch(const PendingGet& get)
{
    if (get.epochs.isEmpty()) {
        setOnline(get.playerKey, std::nullopt);
        return;
    }
    const QByteArray secret = pairSecretFor(get.playerKey);
    if (secret.isEmpty()) return;
    // 友だちが預けた知らせの番号（預ける側は友だち）
    const QByteArray slot = P2p::Friend::slot(m_lobby->networkId(), secret, get.playerKey, get.epochs.first());
    const quint64 request = m_lobby->meetGet(slot);
    if (request != 0) m_pendingGets.insert(request, get);
}

void P2pFriendPresence::onPresenceCreated(quint64 requestId, const PeerIpc::CreatedPresence& presence)
{
    const auto it = m_pendingCreates.constFind(requestId);
    if (it == m_pendingCreates.constEnd()) return;
    const QByteArray playerKey = it.value();
    m_pendingCreates.erase(it);
    if (!canDeposit()) return;
    // 接続先の入っていない知らせでは、友だちはつなげない（協力ノードの中継の予約がまだ取れていないなど）。
    // 次の回（75秒後）を待たずに、少し後で作り直す
    if (presence.addressCount <= 0) {
        if (!m_retryTimer->isActive()) m_retryTimer->start();
        return;
    }
    const QByteArray secret = pairSecretFor(playerKey);
    const QByteArray mine = myPlayerKey();
    if (secret.isEmpty() || mine.isEmpty()) return;
    const quint64 epoch = P2p::Friend::epochOf(nowSeconds());
    const QByteArray blob = P2p::Friend::seal(m_lobby->networkId(), secret, mine, epoch,
                                              P2p::Friend::Presence{presence.expiresAt, presence.body});
    if (blob.isEmpty()) return;
    m_created.insert(presence.id, Created{playerKey, presence.expiresAt});
    const quint64 request =
        m_lobby->meetPut(P2p::Friend::slot(m_lobby->networkId(), secret, mine, epoch), blob, kDepositTtlSeconds);
    if (request != 0) m_pendingPuts.append(request);
}

void P2pFriendPresence::onMeetStored(quint64 requestId, int stored)
{
    if (!m_pendingPuts.removeOne(requestId)) return;
    // どの協力ノードも預からなかった（応答がない・上限）。次の回（75秒後）を待たずに、少し後で預け直す
    if (stored <= 0 && canDeposit() && !m_retryTimer->isActive()) m_retryTimer->start();
}

void P2pFriendPresence::onMeetFetched(quint64 requestId, const QList<QByteArray>& blobs)
{
    const auto it = m_pendingGets.constFind(requestId);
    if (it == m_pendingGets.constEnd()) return;
    PendingGet get = it.value();
    m_pendingGets.erase(it);
    if (get.epochs.isEmpty()) return;
    const QByteArray secret = pairSecretFor(get.playerKey);
    const quint64 now = nowSeconds();
    // 復号（認証の検査）に通り、期限内のものだけを読む。鍵を知らない協力ノードが作ったものは、ここで捨てる
    for (const QByteArray& blob : blobs) {
        const std::optional<P2p::Friend::Presence> presence =
            P2p::Friend::open(m_lobby->networkId(), secret, get.playerKey, get.epochs.first(), blob);
        if (presence && presence->expiresAt > now) {
            setOnline(get.playerKey, presence);
            return;
        }
    }
    get.epochs.removeFirst();
    fetch(get);
}

void P2pFriendPresence::onRequestFailed(quint64 requestId)
{
    m_pendingCreates.remove(requestId);
    m_pendingGets.remove(requestId);
    if (m_pendingPuts.removeOne(requestId) && canDeposit() && !m_retryTimer->isActive()) m_retryTimer->start();
}

void P2pFriendPresence::setOnline(const QByteArray& playerKey, const std::optional<P2p::Friend::Presence>& presence)
{
    const bool was = isOnline(playerKey);
    if (presence) m_online.insert(playerKey, Online{*presence});
    else m_online.remove(playerKey);
    if (was != isOnline(playerKey)) emit presenceChanged();
}

void P2pFriendPresence::expire()
{
    const quint64 now = nowSeconds();
    bool changed = false;
    for (auto it = m_online.begin(); it != m_online.end();) {
        if (it->presence.expiresAt > now) {
            ++it;
        } else {
            it = m_online.erase(it);
            changed = true;
        }
    }
    for (auto it = m_created.begin(); it != m_created.end();) {
        if (it->expiresAt > now) ++it;
        else it = m_created.erase(it);
    }
    if (changed) emit presenceChanged();
}

P2pFriendPresence::ApplyResult P2pFriendPresence::apply(const QByteArray& playerKey,
                                                        const P2pGameCoordinator::Conditions& mine)
{
    if (!isAvailable()) return ApplyResult::Unavailable;
    if (m_game && m_game->isBusy()) return ApplyResult::Busy;
    const auto it = m_online.constFind(playerKey);
    if (it == m_online.constEnd() || it->presence.expiresAt <= nowSeconds()) return ApplyResult::NotOnline;
    P2pGameCoordinator::NegotiationRequest request;
    request.mine = mine.asApplicant();
    // 相手は、この友だちのプレイヤーID を名乗らなければならない
    request.friendPlayerKey = playerKey;
    return m_lobby->connectPresence(it->presence.body, request) ? ApplyResult::Started : ApplyResult::Unavailable;
}

std::optional<P2pGameCoordinator::NegotiationRequest> P2pFriendPresence::acceptApplication(quint64 presenceId)
{
    const auto it = m_created.constFind(presenceId);
    if (!m_sharing || it == m_created.constEnd() || it->expiresAt <= nowSeconds()) return std::nullopt;
    const QByteArray playerKey = it->playerKey;
    // 知らせを預けた後で、友だちから外した・ブロックした相手の申し込みは受けない
    const P2pKnownPeers* known = m_lobby && m_lobby->profile() ? m_lobby->profile()->knownPeers() : nullptr;
    const P2pKnownPeer* peer = known ? known->find(playerKey) : nullptr;
    if (!peer || !peer->isFriend || peer->blocked) return std::nullopt;
    const std::optional<P2pGameCoordinator::Conditions> mine = m_conditions ? m_conditions() : std::nullopt;
    if (!mine) return std::nullopt;
    P2pGameCoordinator::NegotiationRequest request;
    request.mine = *mine;
    request.friendPlayerKey = playerKey;
    return request;
}
