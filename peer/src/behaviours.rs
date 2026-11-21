//! libp2p の振る舞いに足す小さな部品（到達性の確認に送る候補の絞り込み・確立前の接続の数の上限・待ち合わせの
//! 要求の接続元）

use std::collections::HashMap;
use std::convert::Infallible;
use std::sync::Arc;
use std::task::{Context, Poll};

use libp2p::autonat::v2::client;
use libp2p::core::transport::PortUse;
use libp2p::core::Endpoint;
use libp2p::swarm::{
    dummy, ConnectionDenied, ConnectionId, FromSwarm, NetworkBehaviour, THandler, THandlerInEvent, THandlerOutEvent,
    ToSwarm,
};
use libp2p::{Multiaddr, PeerId};
use sbq_protocol::network::Profile;

use crate::addr::{self, SourceKey};
use crate::limits::TierLimits;
use crate::meet::MeetService;

/// 到達性の確認（AutoNAT v2）のクライアント。確かめてもらう自分のアドレスの候補を、インターネット上の直接の
/// アドレスだけにする。
///
/// AutoNAT v2 は、相手が観測した自分のアドレスの候補を1件ずつ、確かめる側（協力ノード）へ送る。v1 は待受の
/// アドレス（私設・loopback・link-local を含む）を全部送っていた（監査 2026-11-13 P2）。候補は Identify が
/// 相手の観測から作るので、LAN の相手に観測された私設アドレスなども混ざりうる。それをここで落とす。
pub struct PublicAutonatClient {
    inner: client::Behaviour,
}

impl PublicAutonatClient {
    pub fn new() -> PublicAutonatClient {
        PublicAutonatClient {
            inner: client::Behaviour::default(),
        }
    }
}

impl NetworkBehaviour for PublicAutonatClient {
    type ConnectionHandler = <client::Behaviour as NetworkBehaviour>::ConnectionHandler;
    type ToSwarm = <client::Behaviour as NetworkBehaviour>::ToSwarm;

    fn handle_pending_inbound_connection(
        &mut self,
        connection_id: ConnectionId,
        local_addr: &Multiaddr,
        remote_addr: &Multiaddr,
    ) -> Result<(), ConnectionDenied> {
        self.inner
            .handle_pending_inbound_connection(connection_id, local_addr, remote_addr)
    }

    fn handle_established_inbound_connection(
        &mut self,
        connection_id: ConnectionId,
        peer: PeerId,
        local_addr: &Multiaddr,
        remote_addr: &Multiaddr,
    ) -> Result<THandler<Self>, ConnectionDenied> {
        self.inner
            .handle_established_inbound_connection(connection_id, peer, local_addr, remote_addr)
    }

    fn handle_pending_outbound_connection(
        &mut self,
        connection_id: ConnectionId,
        maybe_peer: Option<PeerId>,
        addresses: &[Multiaddr],
        effective_role: Endpoint,
    ) -> Result<Vec<Multiaddr>, ConnectionDenied> {
        self.inner
            .handle_pending_outbound_connection(connection_id, maybe_peer, addresses, effective_role)
    }

    fn handle_established_outbound_connection(
        &mut self,
        connection_id: ConnectionId,
        peer: PeerId,
        addr: &Multiaddr,
        role_override: Endpoint,
        port_use: PortUse,
    ) -> Result<THandler<Self>, ConnectionDenied> {
        self.inner
            .handle_established_outbound_connection(connection_id, peer, addr, role_override, port_use)
    }

    fn on_swarm_event(&mut self, event: FromSwarm) {
        if let FromSwarm::NewExternalAddrCandidate(candidate) = &event {
            if !addr::is_public_direct(candidate.addr) {
                return;
            }
        }
        self.inner.on_swarm_event(event);
    }

    fn on_connection_handler_event(
        &mut self,
        peer_id: PeerId,
        connection_id: ConnectionId,
        event: THandlerOutEvent<Self>,
    ) {
        self.inner.on_connection_handler_event(peer_id, connection_id, event);
    }

    fn poll(&mut self, cx: &mut Context<'_>) -> Poll<ToSwarm<Self::ToSwarm, THandlerInEvent<Self>>> {
        self.inner.poll(cx)
    }
}

/// 確立前の接続の数の上限を超えた
#[derive(Debug)]
struct TooManyPending;

impl std::fmt::Display for TooManyPending {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("too many pending connections from this source")
    }
}

impl std::error::Error for TooManyPending {}

/// 届いた確立前の接続を、接続元ごと（2段。addr::SourceKey。IPv6 は /64 で数える）に数えて絞る。全体の上限
/// （connection_limits）だけでは、1つの接続元が何も送らない接続で確立前の枠を使い切り、ほかの相手からの接続を
/// 受け付けられなくなる（監査 2026-11-13 P5。IPv6 を /128 で数えると、1台の /64 で埋まった。監査 2026-11-21 S2）
pub struct PendingPerSource {
    limits: TierLimits,
    profile: Profile,
    pending: HashMap<ConnectionId, SourceKey>,
}

/// 数えておく確立前の接続の上限（全体の上限より十分大きい。知らせが届かずに残ったものを溜め続けない）
const MAX_TRACKED: usize = 1024;

impl PendingPerSource {
    pub fn new(limits: TierLimits, profile: Profile) -> PendingPerSource {
        PendingPerSource {
            limits,
            profile,
            pending: HashMap::new(),
        }
    }

    fn admit(&mut self, connection_id: ConnectionId, key: SourceKey) -> bool {
        if !self.limits.admits(self.pending.values(), &key) || self.pending.len() >= MAX_TRACKED {
            return false;
        }
        self.pending.insert(connection_id, key);
        true
    }
}

impl NetworkBehaviour for PendingPerSource {
    type ConnectionHandler = dummy::ConnectionHandler;
    type ToSwarm = Infallible;

    fn handle_pending_inbound_connection(
        &mut self,
        connection_id: ConnectionId,
        _local_addr: &Multiaddr,
        remote_addr: &Multiaddr,
    ) -> Result<(), ConnectionDenied> {
        // 中継経由の接続は接続元の IP を持たない（中継の側で数を絞る）
        let Some(key) = SourceKey::of_addr(remote_addr, self.profile) else {
            return Ok(());
        };
        if self.admit(connection_id, key) {
            Ok(())
        } else {
            Err(ConnectionDenied::new(TooManyPending))
        }
    }

    fn handle_established_inbound_connection(
        &mut self,
        connection_id: ConnectionId,
        _peer: PeerId,
        _local_addr: &Multiaddr,
        _remote_addr: &Multiaddr,
    ) -> Result<THandler<Self>, ConnectionDenied> {
        self.pending.remove(&connection_id);
        Ok(dummy::ConnectionHandler)
    }

    fn handle_established_outbound_connection(
        &mut self,
        _connection_id: ConnectionId,
        _peer: PeerId,
        _addr: &Multiaddr,
        _role_override: Endpoint,
        _port_use: PortUse,
    ) -> Result<THandler<Self>, ConnectionDenied> {
        Ok(dummy::ConnectionHandler)
    }

    fn on_swarm_event(&mut self, event: FromSwarm) {
        match event {
            FromSwarm::ListenFailure(failure) => {
                self.pending.remove(&failure.connection_id);
            }
            FromSwarm::ConnectionClosed(closed) => {
                self.pending.remove(&closed.connection_id);
            }
            FromSwarm::ConnectionEstablished(established) => {
                self.pending.remove(&established.connection_id);
            }
            _ => {}
        }
    }

    fn on_connection_handler_event(
        &mut self,
        _peer_id: PeerId,
        _connection_id: ConnectionId,
        event: THandlerOutEvent<Self>,
    ) {
        match event {}
    }

    fn poll(&mut self, _cx: &mut Context<'_>) -> Poll<ToSwarm<Self::ToSwarm, THandlerInEvent<Self>>> {
        Poll::Pending
    }
}

/// 待ち合わせを受け持つ協力ノードで、受け付けた接続の接続元を、その接続のストリームが届く前に覚える
/// （MeetService::add_source）。接続の処理（ストリームを受け取る部品）は、すべての振る舞いが接続を受け付けた後に
/// 作られるので、待ち合わせの要求は、接続元を覚えた後にしか届かない。主ループで覚えると、先に届いた要求の接続元が
/// 分からず、帯ごとの上限を逃れられた（監査 2026-11-21 S5）
pub struct MeetSources {
    service: Arc<MeetService>,
    profile: Profile,
}

impl MeetSources {
    pub fn new(service: Arc<MeetService>, profile: Profile) -> MeetSources {
        MeetSources { service, profile }
    }
}

impl NetworkBehaviour for MeetSources {
    type ConnectionHandler = dummy::ConnectionHandler;
    type ToSwarm = Infallible;

    fn handle_established_inbound_connection(
        &mut self,
        connection_id: ConnectionId,
        peer: PeerId,
        _local_addr: &Multiaddr,
        remote_addr: &Multiaddr,
    ) -> Result<THandler<Self>, ConnectionDenied> {
        // 中継経由の接続は接続元の IP を持たない（その接続からの要求は、接続元の分からない要求として断る）
        if let Some(key) = SourceKey::of_addr(remote_addr, self.profile) {
            self.service.add_source(peer, connection_id, key);
        }
        Ok(dummy::ConnectionHandler)
    }

    fn handle_established_outbound_connection(
        &mut self,
        _connection_id: ConnectionId,
        _peer: PeerId,
        _addr: &Multiaddr,
        _role_override: Endpoint,
        _port_use: PortUse,
    ) -> Result<THandler<Self>, ConnectionDenied> {
        Ok(dummy::ConnectionHandler)
    }

    fn on_swarm_event(&mut self, event: FromSwarm) {
        match event {
            FromSwarm::ConnectionClosed(closed) => {
                self.service.remove_source(&closed.peer_id, closed.connection_id);
            }
            // 後ろの振る舞いが、確立した接続を断った
            FromSwarm::ListenFailure(failure) => {
                if let Some(peer) = failure.peer_id {
                    self.service.remove_source(&peer, failure.connection_id);
                }
            }
            _ => {}
        }
    }

    fn on_connection_handler_event(
        &mut self,
        _peer_id: PeerId,
        _connection_id: ConnectionId,
        event: THandlerOutEvent<Self>,
    ) {
        match event {}
    }

    fn poll(&mut self, _cx: &mut Context<'_>) -> Poll<ToSwarm<Self::ToSwarm, THandlerInEvent<Self>>> {
        Poll::Pending
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn key(ip: [u8; 4]) -> SourceKey {
        SourceKey::of_ip(ip.into(), Profile::Internet)
    }

    /// 同じ接続元からの確立前の接続は上限まで。同じアドレス帯の別の接続元は、帯の上限まで
    #[test]
    fn pending_connections_are_limited_per_source() {
        let limits = TierLimits {
            per_source: 2,
            per_prefix: 3,
        };
        let mut limit = PendingPerSource::new(limits, Profile::Internet);
        let a = key([203, 0, 114, 2]);
        let b = key([203, 0, 114, 3]);
        let c = key([198, 51, 101, 3]);
        let id = ConnectionId::new_unchecked;
        assert!(limit.admit(id(1), a.clone()));
        assert!(limit.admit(id(2), a.clone()));
        assert!(!limit.admit(id(3), a.clone()));
        assert!(limit.admit(id(4), b.clone()));
        // 同じ /24 は3つまで
        assert!(!limit.admit(id(5), b));
        assert!(limit.admit(id(6), c));
        // 確立した（または失敗した）ものは数えない
        limit.pending.remove(&id(1));
        assert!(limit.admit(id(3), a));
    }

    /// IPv6 は /64 で1つの接続元として数える（/64 の中でアドレスを替えても、同じ枠）
    #[test]
    fn pending_ipv6_connections_are_counted_per_64() {
        let limits = TierLimits {
            per_source: 2,
            per_prefix: 8,
        };
        let mut limit = PendingPerSource::new(limits, Profile::Internet);
        let v6 = |s: &str| {
            SourceKey::of_ip(
                s.parse::<std::net::IpAddr>()
                    .unwrap_or(std::net::Ipv6Addr::UNSPECIFIED.into()),
                Profile::Internet,
            )
        };
        let id = ConnectionId::new_unchecked;
        assert!(limit.admit(id(1), v6("2400:4050:1:2::1")));
        assert!(limit.admit(id(2), v6("2400:4050:1:2::2")));
        assert!(!limit.admit(id(3), v6("2400:4050:1:2:ffff::3")));
        assert!(limit.admit(id(4), v6("2400:4050:1:3::1")));
    }
}
