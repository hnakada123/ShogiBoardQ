//! Swarm のイベントの処理（接続・到達性・中継の予約）

use std::time::Instant;

use libp2p::core::ConnectedPoint;
use libp2p::multiaddr::Protocol;
use libp2p::swarm::{DialError, SwarmEvent};
use libp2p::{autonat, identify, ping, relay, Multiaddr, PeerId};

use super::{ConnState, Node};
use crate::addr;
use crate::ipc;
use crate::swarm::{supports_relay_hop, BehaviourEvent};

/// 同じアドレス帯から受け付ける接続の上限
const MAX_INBOUND_PER_PREFIX: usize = 8;
/// 覚える待受のアドレスの上限
const MAX_LISTEN_ADDRS: usize = 16;

impl Node {
    pub fn on_swarm_event(&mut self, event: SwarmEvent<BehaviourEvent>) {
        match event {
            SwarmEvent::NewListenAddr { address, .. } => {
                if !self.listen_addrs.contains(&address) && self.listen_addrs.len() < MAX_LISTEN_ADDRS {
                    self.listen_addrs.push(address.clone());
                }
                // ローカル（同じPC・同じLAN）では、待受のアドレスのうち loopback と私設アドレスを、そのまま自分の
                // アドレスにする（Identify に載せ、再接続の宛先にする）。協力ノードは、インターネットのグローバルな
                // 待受を自分のアドレスにする（中継の予約の応答に載せる）。「直接接続を使わない」のときは載せない
                if self.policy.listen_is_external(self.service, &address) {
                    self.swarm.add_external_address(address);
                }
            }
            SwarmEvent::ListenerClosed { listener_id, .. } => {
                // 中継の待受が閉じた（予約を断られた・協力ノードとの接続が切れた）。少し待って頼み直す
                self.reservations.closed(listener_id, Instant::now());
            }
            SwarmEvent::ExpiredListenAddr { address, .. } => {
                self.listen_addrs.retain(|a| a != &address);
            }
            SwarmEvent::ConnectionEstablished {
                peer_id,
                connection_id,
                endpoint,
                ..
            } => {
                // 中継経由かは、つないだ宛先（自分から）か、届いた待受のアドレス（相手から）で決まる。
                // 中継経由で届いた接続の相手のアドレスは /p2p/<相手> だけで、中継の印が入らない
                let relayed = endpoint.is_relayed();
                if let ConnectedPoint::Listener { send_back_addr, .. } = &endpoint {
                    if !relayed && !self.admit_inbound(connection_id, send_back_addr) {
                        self.swarm.close_connection(connection_id);
                        return;
                    }
                }
                self.peer_conns
                    .entry(peer_id)
                    .or_default()
                    .insert(connection_id, relayed);
                self.update_routes(&peer_id);
            }
            SwarmEvent::ConnectionClosed {
                peer_id, connection_id, ..
            } => {
                self.inbound_prefixes.remove(&connection_id);
                if let Some(conns) = self.peer_conns.get_mut(&peer_id) {
                    conns.remove(&connection_id);
                    if conns.is_empty() {
                        self.peer_conns.remove(&peer_id);
                    }
                }
                self.update_routes(&peer_id);
            }
            SwarmEvent::OutgoingConnectionError {
                error: DialError::Transport(errors),
                ..
            } => {
                for (address, _) in errors {
                    self.policy.record_failure(&address);
                }
            }
            SwarmEvent::ExternalAddrConfirmed { address } => {
                // 「直接接続を使わない」のときは、中継経由のアドレスだけを自分のアドレスにする（advertisable が見る）
                if !self.policy.advertisable(&address) {
                    self.swarm.remove_external_address(&address);
                }
            }
            SwarmEvent::Behaviour(event) => self.on_behaviour_event(event),
            _ => {}
        }
    }

    /// 同じアドレス帯から受け付けた接続が多すぎるか（多すぎなければ数に入れる）
    fn admit_inbound(&mut self, connection: libp2p::swarm::ConnectionId, addr: &Multiaddr) -> bool {
        let Some(prefix) = addr::prefix_of(addr) else {
            return true;
        };
        let count = self.inbound_prefixes.values().filter(|p| **p == prefix).count();
        if count >= MAX_INBOUND_PER_PREFIX {
            return false;
        }
        self.inbound_prefixes.insert(connection, prefix);
        true
    }

    /// 対局ストリームの経路（直接・中継）が変わったら知らせる
    fn update_routes(&mut self, peer: &PeerId) {
        let route = self.route_of(peer);
        let mut changed = Vec::new();
        for (id, conn) in self.conns.iter_mut() {
            if &conn.peer != peer {
                continue;
            }
            if let ConnState::Open { route: current, .. } = &mut conn.state {
                if *current != route && self.peer_conns.contains_key(peer) {
                    *current = route;
                    changed.push(*id);
                }
            }
        }
        for id in changed {
            self.out
                .notify(ipc::Notification::ConnectionRoute { connection: id, route });
        }
    }

    fn on_behaviour_event(&mut self, event: BehaviourEvent) {
        match event {
            BehaviourEvent::Identify(identify::Event::Received { peer_id, info, .. }) => {
                self.on_identify(peer_id, info)
            }
            BehaviourEvent::Ping(ping::Event {
                connection,
                result: Err(_),
                ..
            }) => {
                // 応答がない接続は閉じる（対局ストリームは閉じた知らせで再接続へ移る）
                self.swarm.close_connection(connection);
            }
            BehaviourEvent::RelayClient(relay::client::Event::ReservationReqAccepted { relay_peer_id, .. }) => {
                self.reservations.accepted(relay_peer_id);
            }
            BehaviourEvent::Autonat(autonat::Event::StatusChanged { new, .. }) => {
                self.reachability = match new {
                    autonat::NatStatus::Public(_) => ipc::Reachability::Public,
                    autonat::NatStatus::Private => ipc::Reachability::Private,
                    autonat::NatStatus::Unknown => ipc::Reachability::Unknown,
                };
            }
            _ => {}
        }
    }

    fn on_identify(&mut self, peer: PeerId, info: identify::Info) {
        let node_addr = self.bootstrap.get(&peer).cloned();
        let is_node = node_addr.is_some();
        if is_node {
            // 利用者が指定した協力ノードが名乗った別の宛先（別の経路・ポート）も、そのノードの宛先として覚える
            for a in &info.listen_addrs {
                self.policy.allow_node(peer, a);
            }
        }
        let addrs = self.policy.filter_addrs(info.listen_addrs.iter().cloned());
        self.remember_addrs(peer, &addrs);
        // 中継の予約は、自分の到達性によらず取る。「直接接続を使わない」相手は、中継経由のアドレスでしか
        // つなげないので、外から直接届く側も中継経由のアドレスを招待に入れる
        if !self.service && is_node && supports_relay_hop(&info) {
            self.reservations.add_hop_node(peer);
            self.request_reservation(peer);
        }
    }

    /// 協力ノードに中継の予約を頼む（中継の待受を始める）
    pub(super) fn request_reservation(&mut self, peer: PeerId) {
        if !self.reservations.may_request(&peer, Instant::now()) {
            return;
        }
        // 中継のアドレスは、利用者が指定した宛先から作る（同じ協力ノードを指定した相手が、そのまま使える）
        let Some(base) = self.bootstrap.get(&peer).cloned() else { return };
        let circuit = base.with(Protocol::P2p(peer)).with(Protocol::P2pCircuit);
        if let Ok(listener) = self.swarm.listen_on(circuit) {
            self.reservations.requested(peer, listener);
        }
    }
}
