//! Swarm のイベントの処理（接続・到達性・中継の予約）

use std::time::Instant;

use libp2p::core::ConnectedPoint;
use libp2p::multiaddr::Protocol;
use libp2p::swarm::{DialError, SwarmEvent};
use libp2p::{autonat, dcutr, identify, ping, relay, Multiaddr, PeerId};
use sbq_protocol::network::Profile;

use super::Node;
use crate::addr;
use crate::ipc;
use crate::swarm::{supports_relay_hop, BehaviourEvent};

/// 覚える待受のアドレスの上限
const MAX_LISTEN_ADDRS: usize = 16;

impl Node {
    pub fn on_swarm_event(&mut self, event: SwarmEvent<BehaviourEvent>) {
        match event {
            SwarmEvent::NewListenAddr { address, .. } => {
                if !self.listen_addrs.contains(&address) && self.listen_addrs.len() < MAX_LISTEN_ADDRS {
                    self.listen_addrs.push(address.clone());
                }
                // 協力ノードは、インターネットのグローバルな待受を自分のアドレスにする（中継の予約の応答に載せる）。
                // ふつうのプレイヤーの待受は Identify に載せない（ローカルの宛先は招待で渡す）
                if self.policy.listen_is_external(self.role.is_service(), &address) {
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
                // 中継経由の接続の上で対局ストリームを開いた後にできた直接の接続は、使わないので閉じる（開いた
                // ストリームは動かせない。残すと、使われないまましばらくして閉じる。次につなぎ直すときに試し直す）
                if !relayed && self.conns.has_relayed_stream_to(&peer_id) {
                    self.swarm.close_connection(connection_id);
                    self.on_code_dial_failed(connection_id);
                    return;
                }
                self.links.established(peer_id, connection_id, relayed, Instant::now());
                // 待ち合わせの件数と頻度は、接続元ごとに数える（待ち合わせを受け持つ協力ノード）
                if let (Some(meet), Some(source)) = (&self.meet_service, self.links.source_of(connection_id)) {
                    meet.set_source(peer_id, source.clone());
                }
                self.update_routes(&peer_id);
                self.advance_opening(&peer_id);
                // 合言葉の宛先へ、PeerId を決めずに始めたダイヤルなら、相手の PeerId がここで分かる
                self.on_code_dial_established(connection_id, peer_id);
            }
            SwarmEvent::ConnectionClosed {
                peer_id, connection_id, ..
            } => {
                // 確立した直後に受け付けずに閉じた接続（数に入れていない）は、相手とのほかの接続にも、開く途中の
                // 対局ストリームにも関わらない（監査 2026-11-21 U2）
                let Some(peer_left) = self.links.closed(&peer_id, connection_id) else {
                    return;
                };
                if peer_left {
                    self.forget_meet_node(&peer_id);
                    if let Some(meet) = &self.meet_service {
                        meet.forget_source(&peer_id);
                    }
                }
                self.update_routes(&peer_id);
                if peer_left {
                    // 対局ストリームを開く前に、相手との接続がなくなった
                    self.fail_openings_of(&peer_id);
                } else {
                    self.advance_opening(&peer_id);
                }
            }
            SwarmEvent::OutgoingConnectionError {
                peer_id,
                connection_id,
                error,
            } => {
                if let DialError::Transport(errors) = &error {
                    for (address, _) in errors {
                        self.policy.record_failure(address);
                    }
                }
                self.on_code_dial_failed(connection_id);
                // つながっていない相手への接続に失敗したら、対局ストリームを開くのをやめる。中継でつながった相手への
                // 直接の接続（穴あけ）の失敗は、相手がやり直すことがあるので、待つ時間が過ぎるまで待つ
                if let Some(peer) = peer_id {
                    if !self.links.contains(&peer) {
                        self.fail_openings_of(&peer);
                    }
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

    /// 同じアドレス帯から受け付けた接続が多すぎるか（多すぎなければ数に入れる）。ローカル（同じPC・同じLAN）では
    /// LAN 全体が同じアドレス帯に入るので、接続元の IP ごとに数える（1台の機器で LAN の相手を締め出させない）
    fn admit_inbound(&mut self, connection: libp2p::swarm::ConnectionId, addr: &Multiaddr) -> bool {
        let key = if self.spec.profile == Profile::Local {
            addr::ip_key(addr)
        } else {
            addr::prefix_of(addr)
        };
        self.links.admit_inbound(connection, key)
    }

    /// 届いた側の対局ストリームの経路（直接・中継）が変わったら知らせる（つないだ側は、対局ストリームを開いた
    /// 接続の経路を知っているので、接続が増減しても変わらない）。相手との接続が無くなったときは変えない
    fn update_routes(&mut self, peer: &PeerId) {
        if !self.links.contains(peer) {
            return;
        }
        let route = self.links.route_of(peer);
        for id in self.conns.reroute(peer, route) {
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
            // 穴あけをあきらめた（決まった回数だけ失敗した・手続きに失敗した）相手とは、直接の接続を待たない
            BehaviourEvent::Dcutr(dcutr::Event {
                remote_peer_id,
                result: Err(_),
            }) => self.give_up_hole_punching(&remote_peer_id),
            // 協力ノードが中継している回路の数（ログに書く）
            BehaviourEvent::RelayServer(relay::Event::CircuitReqAccepted {
                src_peer_id,
                dst_peer_id,
            }) => {
                if let Some(circuits) = self.circuits.as_mut() {
                    circuits.opened(src_peer_id, dst_peer_id);
                }
            }
            BehaviourEvent::RelayServer(relay::Event::CircuitClosed {
                src_peer_id,
                dst_peer_id,
                ..
            }) => {
                if let Some(circuits) = self.circuits.as_mut() {
                    circuits.closed(src_peer_id, dst_peer_id);
                }
            }
            BehaviourEvent::AutonatClient(autonat::v2::client::Event { result, .. }) => {
                // 確かめた候補のどれかに外から届けば、インターネットから届く。届かなかった候補しかなければ届かない
                self.reachability = match (result, self.reachability) {
                    (Ok(()), _) => ipc::Reachability::Public,
                    (Err(_), ipc::Reachability::Public) => ipc::Reachability::Public,
                    (Err(_), _) => ipc::Reachability::Private,
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
        // 名乗った宛先を覚えるのは、自分からつないだ相手（招待・対局）だけ。つないできただけの相手に、覚える枠を
        // 埋めさせない（監査 2026-11-13 P19）
        if self.addr_book.contains_key(&peer) || self.game_peer == Some(peer) {
            let addrs = self.policy.filter_addrs(info.listen_addrs.iter().cloned());
            self.remember_addrs(peer, &addrs);
        }
        // 中継の予約は、自分の到達性によらず取る。「直接接続を使わない」相手は、中継経由のアドレスでしか
        // つなげないので、外から直接届く側も中継経由のアドレスを招待に入れる
        let player = !self.role.is_service();
        if player && is_node {
            self.note_meet_node(peer, &info);
        }
        if player && is_node && supports_relay_hop(&info) {
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
        let Some(base) = self.bootstrap.get(&peer).cloned() else {
            return;
        };
        let circuit = base.with(Protocol::P2p(peer)).with(Protocol::P2pCircuit);
        if let Ok(listener) = self.swarm.listen_on(circuit) {
            self.reservations.requested(peer, listener);
        }
    }
}
