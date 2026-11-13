//! libp2p の振る舞いに足す小さな部品（到達性の確認に送る候補の絞り込み・確立前の接続の数の上限）

use std::collections::HashMap;
use std::convert::Infallible;
use std::net::IpAddr;
use std::task::{Context, Poll};

use libp2p::autonat::v2::client;
use libp2p::core::transport::PortUse;
use libp2p::core::Endpoint;
use libp2p::swarm::{
    dummy, ConnectionDenied, ConnectionId, FromSwarm, NetworkBehaviour, THandler, THandlerInEvent, THandlerOutEvent,
    ToSwarm,
};
use libp2p::{Multiaddr, PeerId};

use crate::addr;

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

/// 届いた確立前の接続を、接続元の IP ごとに数えて絞る。全体の上限（connection_limits）だけでは、1つの接続元が
/// 何も送らない接続で確立前の枠を使い切り、ほかの相手からの接続を受け付けられなくなる（監査 2026-11-13 P5）
pub struct PendingPerSource {
    max_per_ip: usize,
    pending: HashMap<ConnectionId, IpAddr>,
}

/// 数えておく確立前の接続の上限（全体の上限より十分大きい。知らせが届かずに残ったものを溜め続けない）
const MAX_TRACKED: usize = 256;

impl PendingPerSource {
    pub fn new(max_per_ip: usize) -> PendingPerSource {
        PendingPerSource {
            max_per_ip,
            pending: HashMap::new(),
        }
    }

    fn admit(&mut self, connection_id: ConnectionId, ip: IpAddr) -> bool {
        let count = self.pending.values().filter(|p| **p == ip).count();
        if count >= self.max_per_ip || self.pending.len() >= MAX_TRACKED {
            return false;
        }
        self.pending.insert(connection_id, ip);
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
        let Some(ip) = addr::first_ip(remote_addr) else {
            return Ok(());
        };
        if self.admit(connection_id, ip) {
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

#[cfg(test)]
mod tests {
    use super::*;

    /// 同じ接続元からの確立前の接続は上限まで。ほかの接続元は別に数える
    #[test]
    fn pending_connections_are_limited_per_source() {
        let mut limit = PendingPerSource::new(2);
        let a: IpAddr = [192, 168, 1, 2].into();
        let b: IpAddr = [192, 168, 1, 3].into();
        let c1 = ConnectionId::new_unchecked(1);
        let c2 = ConnectionId::new_unchecked(2);
        let c3 = ConnectionId::new_unchecked(3);
        let c4 = ConnectionId::new_unchecked(4);
        assert!(limit.admit(c1, a));
        assert!(limit.admit(c2, a));
        assert!(!limit.admit(c3, a));
        assert!(limit.admit(c4, b));
        // 確立した（または失敗した）ものは数えない
        limit.pending.remove(&c1);
        assert!(limit.admit(c3, a));
    }
}
