//! Swarm のイベントの処理（接続・発見・募集・一覧取得）

use std::net::IpAddr;
use std::time::{Duration, Instant};

use libp2p::core::ConnectedPoint;
use libp2p::kad::store::RecordStore;
use libp2p::multiaddr::Protocol;
use libp2p::swarm::dial_opts::DialOpts;
use libp2p::swarm::{DialError, SwarmEvent};
use libp2p::{autonat, gossipsub, identify, kad, ping, relay, request_response, Multiaddr, PeerId};
use sbq_protocol::network::Profile;
use sbq_protocol::offer::{ListRequest, ListResponse, SignedOffer, MAX_LIST_ITEMS};
use serde_json::json;

use super::{unix_now, ConnState, Node, MAX_AUTO_DIALS};
use crate::addr;
use crate::offers::{self, Verdict};
use crate::swarm::{supports_relay_hop, BehaviourEvent};

/// 同じアドレス帯から受け付ける接続の上限
const MAX_INBOUND_PER_PREFIX: usize = 8;
/// 同じピアからの一覧取得の最短の間隔
const LIST_REQUEST_INTERVAL: Duration = Duration::from_secs(10);
/// 中継の予約の数
const MAX_RELAY_RESERVATIONS: usize = 2;

/// 接続元のアドレス帯（IPv4 は /24、IPv6 は /48）
fn prefix_of(addr: &Multiaddr) -> Option<Vec<u8>> {
    match addr::first_ip(addr)? {
        IpAddr::V4(v4) => v4.octets().get(..3).map(<[u8]>::to_vec),
        IpAddr::V6(v6) => v6.octets().get(..6).map(<[u8]>::to_vec),
    }
}

impl Node {
    pub fn on_swarm_event(&mut self, event: SwarmEvent<BehaviourEvent>) {
        match event {
            SwarmEvent::NewListenAddr { address, .. } => {
                if !self.listen_addrs.contains(&address) && self.listen_addrs.len() < 16 {
                    self.listen_addrs.push(address.clone());
                }
                // ローカル検証では待受のアドレスをそのまま自分のアドレスにする（DHT の provider 情報・Identify に載せる）
                if self.spec.profile == Profile::Local && !addr::is_circuit(&address) {
                    self.swarm.add_external_address(address);
                }
                self.status_dirty = true;
            }
            SwarmEvent::ExpiredListenAddr { address, .. } => {
                self.listen_addrs.retain(|a| a != &address);
                self.status_dirty = true;
            }
            SwarmEvent::ConnectionEstablished {
                peer_id,
                connection_id,
                endpoint,
                ..
            } => {
                let remote = endpoint.get_remote_address().clone();
                let relayed = addr::is_circuit(&remote);
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
                if endpoint.is_dialer() && !relayed {
                    let base = addr::without_peer_id(&remote);
                    if self.policy.usable(&base) && self.known.len() < 64 {
                        self.known.insert(peer_id, base);
                        self.known_dirty = true;
                    }
                }
                if self.bootstrap.contains(&peer_id) && !self.provided_after_bootstrap && !self.service {
                    // 起動の時点では DHT に届かなかった provider 情報を、協力ノードにつながった後に出し直す
                    self.provided_after_bootstrap = true;
                    let _ = self.swarm.behaviour_mut().kad.start_providing(self.lobby_key.clone());
                    self.last_provider_query = None;
                }
                self.update_routes(&peer_id);
                self.status_dirty = true;
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
                self.status_dirty = true;
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
                let keep = self.policy.advertisable(&address) && (!self.relay_only || addr::is_circuit(&address));
                if !keep {
                    self.swarm.remove_external_address(&address);
                }
                self.status_dirty = true;
            }
            SwarmEvent::Behaviour(event) => self.on_behaviour_event(event),
            _ => {}
        }
    }

    /// 同じアドレス帯から受け付けた接続が多すぎるか（多すぎなければ数に入れる）
    fn admit_inbound(&mut self, connection: libp2p::swarm::ConnectionId, addr: &Multiaddr) -> bool {
        let Some(prefix) = prefix_of(addr) else { return true };
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
            self.out.notify(
                "connectionRoute",
                json!({ "connection": id.to_string(), "route": route }),
            );
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
                self.relay_reservations.insert(relay_peer_id);
                self.status_dirty = true;
            }
            BehaviourEvent::Autonat(autonat::Event::StatusChanged { new, .. }) => {
                self.reachability = match new {
                    autonat::NatStatus::Public(_) => "public",
                    autonat::NatStatus::Private => "private",
                    autonat::NatStatus::Unknown => "unknown",
                };
                self.status_dirty = true;
            }
            BehaviourEvent::Kad(event) => self.on_kad(event),
            BehaviourEvent::Gossipsub(gossipsub::Event::Message {
                propagation_source,
                message_id,
                message,
            }) => {
                self.on_gossip(propagation_source, message_id, message);
            }
            BehaviourEvent::Lobby(request_response::Event::Message { peer, message, .. }) => match message {
                request_response::Message::Request { request, channel, .. } => {
                    let response = self.answer_list(peer, &request);
                    let _ = self.swarm.behaviour_mut().lobby.send_response(channel, response);
                }
                request_response::Message::Response { response, .. } => self.on_list_response(&response),
            },
            _ => {}
        }
    }

    fn on_identify(&mut self, peer: PeerId, info: identify::Info) {
        let addrs = self.policy.filter_addrs(info.listen_addrs.iter().cloned());
        for a in &addrs {
            self.swarm.behaviour_mut().kad.add_address(&peer, a.clone());
        }
        if !addrs.is_empty() {
            self.addr_book.insert(peer, addrs.clone());
        }
        let want_relay = self.reachability != "public" || self.relay_only;
        let reserved = self.relay_requested.len();
        if want_relay
            && self.bootstrap.contains(&peer)
            && supports_relay_hop(&info)
            && reserved < MAX_RELAY_RESERVATIONS
            && !self.relay_requested.contains(&peer)
        {
            if let Some(base) = addrs.first() {
                let circuit = base.clone().with(Protocol::P2p(peer)).with(Protocol::P2pCircuit);
                if self.swarm.listen_on(circuit).is_ok() {
                    self.relay_requested.insert(peer);
                }
            }
        }
    }

    fn on_kad(&mut self, event: kad::Event) {
        match event {
            kad::Event::InboundRequest {
                request: kad::InboundRequest::AddProvider { record: Some(record) },
            } => {
                // 受け付けるのはロビーキーの provider 情報だけ（任意の値のレコードを保存・転送しない）
                if record.key == self.lobby_key {
                    let _ = self.swarm.behaviour_mut().kad.store_mut().add_provider(record);
                }
            }
            kad::Event::OutboundQueryProgressed {
                result: kad::QueryResult::GetProviders(Ok(kad::GetProvidersOk::FoundProviders { providers, .. })),
                ..
            } => {
                for peer in providers {
                    if peer == self.local_peer || self.swarm.is_connected(&peer) {
                        continue;
                    }
                    if self.auto_dials.len() >= MAX_AUTO_DIALS && !self.auto_dials.contains(&peer) {
                        break;
                    }
                    // 問い合わせで知った宛先（Kademlia が持つ）で接続する。宛先の規則は Transport が掛ける
                    let opts = DialOpts::peer_id(peer).build();
                    if self.swarm.dial(opts).is_ok() {
                        self.auto_dials.insert(peer);
                    }
                }
            }
            _ => {}
        }
    }

    /// GossipSub の募集を検査し、通ったものだけを転送する
    fn on_gossip(&mut self, source: PeerId, id: gossipsub::MessageId, message: gossipsub::Message) {
        let acceptance = match self.check_gossip_offer(&message) {
            Ok(()) => gossipsub::MessageAcceptance::Accept,
            Err(Verdict::Reject) => gossipsub::MessageAcceptance::Reject,
            Err(Verdict::Ignore) | Err(Verdict::Skewed) => gossipsub::MessageAcceptance::Ignore,
        };
        let rejected = matches!(acceptance, gossipsub::MessageAcceptance::Reject);
        let _ = self
            .swarm
            .behaviour_mut()
            .gossipsub
            .report_message_validation_result(&id, &source, acceptance);
        // 不正な募集を繰り返して点が graylist を下回ったピアは切断する（実装仕様 3.2節）
        if rejected && crate::swarm::below_graylist(&self.swarm.behaviour().gossipsub, &source) {
            let _ = self.swarm.disconnect_peer_id(source);
        }
    }

    fn check_gossip_offer(&mut self, message: &gossipsub::Message) -> Result<(), Verdict> {
        if message.topic != self.topic.hash() {
            return Err(Verdict::Reject);
        }
        let signed = SignedOffer::decode(&message.data).map_err(|_| Verdict::Reject)?;
        let peer = self.verify_offer(&signed)?;
        // 発信者（メッセージの署名者）と募集の peerId が一致すること
        if message.source != Some(peer) {
            return Err(Verdict::Reject);
        }
        if peer == self.local_peer {
            return Err(Verdict::Ignore);
        }
        match self.offers.insert(signed, peer)? {
            true => Ok(()),
            false => Err(Verdict::Ignore),
        }
    }

    fn verify_offer(&mut self, signed: &SignedOffer) -> Result<PeerId, Verdict> {
        match offers::verify(signed, &self.network_id, &self.rules_hash, unix_now()) {
            Err(Verdict::Skewed) => {
                if let Ok(peer) = PeerId::from_bytes(&signed.body.peer_id) {
                    if self.skew_reports.len() < 64 {
                        self.skew_reports.insert(peer, Instant::now());
                    }
                }
                Err(Verdict::Skewed)
            }
            other => other,
        }
    }

    fn answer_list(&mut self, peer: PeerId, request: &[u8]) -> Vec<u8> {
        let empty = ListResponse {
            offers: Vec::new(),
            next_cursor: 0,
        };
        let Some(req) = ListRequest::decode(request) else {
            return empty.encode();
        };
        if req.network_id != self.network_id || req.rules_hash != self.rules_hash {
            return empty.encode();
        }
        if self
            .list_rate
            .get(&peer)
            .is_some_and(|t| t.elapsed() < LIST_REQUEST_INTERVAL)
        {
            return empty.encode();
        }
        self.list_rate.insert(peer, Instant::now());
        let own = self.my_offer.as_ref().and_then(|m| m.signed.as_ref());
        let (offers, next_cursor) = self.offers.page(req.cursor, MAX_LIST_ITEMS, own);
        ListResponse { offers, next_cursor }.encode()
    }

    fn on_list_response(&mut self, response: &[u8]) {
        let Some(resp) = ListResponse::decode(response) else {
            return;
        };
        for signed in resp.offers {
            // 応答した相手を募集の発信者とはみなさない（署名だけで確かめる）
            if let Ok(peer) = self.verify_offer(&signed) {
                if peer != self.local_peer {
                    let _ = self.offers.insert(signed, peer);
                }
            }
        }
    }
}
