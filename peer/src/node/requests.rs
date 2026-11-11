//! IPC の要求の処理（protocol/ipc.md 3章）

use std::sync::atomic::Ordering;
use std::time::{Duration, Instant};

use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use libp2p::{Multiaddr, PeerId};
use rand::RngCore;
use sbq_protocol::frame;
use sbq_protocol::hash::{from_hex32, hex};
use sbq_protocol::invite::{Invite, InviteError};
use sbq_protocol::network::Profile;
use sbq_protocol::offer::{self, Availability, ListRequest, OfferBody, SidePreference};
use sbq_protocol::{displayname, invite};
use serde_json::{json, Value};

use super::{unix_now, ConnState, MyOffer, Node, OutstandingInvite, MAX_GAME_CONNECTIONS};
use crate::addr::{self, AddrPolicy};
use crate::ipc::{self, Method, Request};
use crate::offers;
use crate::swarm::parse_peer_id;

/// 同時に持つ招待の上限
const MAX_INVITES: usize = 4;
/// 募集を更新する間隔
const OFFER_REFRESH: Duration = Duration::from_secs(30);
/// 届けるピアがいなかったときに出し直すまでの時間
const OFFER_RETRY: Duration = Duration::from_secs(2);

type ReqResult = Result<Value, (&'static str, String)>;

fn invalid(message: impl Into<String>) -> (&'static str, String) {
    ("invalid_request", message.into())
}

fn invite_error(e: InviteError) -> (&'static str, String) {
    let code = match e {
        InviteError::Invalid => "invite_invalid",
        InviteError::Expired => "invite_expired",
        InviteError::WrongNetwork => "invite_wrong_network",
        InviteError::Own => "invite_own",
    };
    (code, e.to_string())
}

impl Node {
    pub fn on_request(&mut self, request: Request) {
        let result = match request.method {
            Method::Hello | Method::Start => Err(("invalid_state", "already started".to_owned())),
            Method::PublishOffer => self.publish_offer(&request),
            Method::WithdrawOffer => self.empty(&request).map(|()| {
                self.withdraw_offer();
                json!({})
            }),
            Method::ListOffers => self.empty(&request).map(|()| {
                self.request_lists();
                json!({})
            }),
            Method::CreateInvite => self.empty(&request).and_then(|()| self.create_invite()),
            Method::CancelInvites => self.empty(&request).map(|()| {
                self.invites.clear();
                json!({})
            }),
            Method::InspectInvite => self.inspect_invite(&request),
            Method::ConnectInvite => self.connect_invite(&request),
            Method::ConnectOffer => self.connect_offer(&request),
            Method::SetGamePeer => self.set_game_peer(&request),
            Method::ConnectPeer => self.connect_peer(&request),
            Method::SendFrame => self.send_frame(&request),
            Method::CloseConnection => self.close_connection(&request),
            Method::Shutdown => self.empty(&request).map(|()| {
                self.withdraw_offer();
                self.invites.clear();
                self.shutdown_at = Instant::now()
                    .checked_add(Duration::from_millis(300))
                    .or_else(|| Some(Instant::now()));
                json!({})
            }),
        };
        match result {
            Ok(value) => self.out.result(&request.request_id, value),
            Err((code, message)) => self.out.error(&request.request_id, code, &message),
        }
    }

    fn empty(&self, request: &Request) -> Result<(), (&'static str, String)> {
        ipc::params::<ipc::Empty>(request).map(|_| ()).map_err(invalid)
    }

    fn publish_offer(&mut self, request: &Request) -> ReqResult {
        let p: ipc::PublishOfferParams = ipc::params(request).map_err(invalid)?;
        if !displayname::is_valid(&p.display_name) {
            return Err(invalid("invalid displayName"));
        }
        let main_ms = ipc::parse_u64(&p.main_ms).ok_or_else(|| invalid("invalid mainMs"))?;
        let byoyomi_ms = ipc::parse_u64(&p.byoyomi_ms).ok_or_else(|| invalid("invalid byoyomiMs"))?;
        if !offer::is_valid_time_control(main_ms, byoyomi_ms) {
            return Err(invalid("time control out of range"));
        }
        let side = SidePreference::from_name(&p.side_preference).ok_or_else(|| invalid("invalid sidePreference"))?;
        self.withdraw_offer();
        let mut offer_id = [0u8; 32];
        rand::rngs::OsRng.fill_bytes(&mut offer_id);
        self.my_offer = Some(MyOffer {
            offer_id,
            sequence: 0,
            display_name: p.display_name,
            main_ms,
            byoyomi_ms,
            side,
            next_publish: Instant::now(),
            signed: None,
        });
        self.refresh_offer(true);
        Ok(json!({ "offerId": hex(&offer_id) }))
    }

    /// 自分の募集を出し直す（force でなければ 30 秒ごと）
    pub fn refresh_offer(&mut self, force: bool) {
        let Some(my) = self.my_offer.as_mut() else { return };
        let now_instant = Instant::now();
        if !force && now_instant < my.next_publish {
            return;
        }
        my.sequence = my.sequence.saturating_add(1);
        my.next_publish = now_instant.checked_add(OFFER_REFRESH).unwrap_or(now_instant);
        let now = unix_now();
        let body = OfferBody {
            network_id: self.network_id,
            offer_id: my.offer_id,
            sequence: my.sequence,
            peer_id: self.local_peer.to_bytes(),
            display_name: my.display_name.clone(),
            rules_hash: self.rules_hash,
            main_ms: my.main_ms,
            byoyomi_ms: my.byoyomi_ms,
            side_preference: my.side,
            created_at: now,
            expires_at: now.saturating_add(offer::MAX_LIFETIME_SECS),
            availability: Availability::Open,
        };
        let Some(signed) = offers::sign(&self.keypair, body) else {
            return;
        };
        let bytes = signed.encode();
        my.signed = Some(signed);
        // 届けるピアがまだいなければ（だれも受け取っていないので）少し後に出し直す。一覧取得には答えられる
        if let Err(libp2p::gossipsub::PublishError::NoPeersSubscribedToTopic) =
            self.swarm.behaviour_mut().gossipsub.publish(self.topic.clone(), bytes)
        {
            my.next_publish = now_instant.checked_add(OFFER_RETRY).unwrap_or(now_instant);
        }
    }

    /// 自分の募集を取り下げる
    pub fn withdraw_offer(&mut self) {
        let Some(my) = self.my_offer.take() else { return };
        let now = unix_now();
        let body = OfferBody {
            network_id: self.network_id,
            offer_id: my.offer_id,
            sequence: my.sequence.saturating_add(1),
            peer_id: self.local_peer.to_bytes(),
            display_name: my.display_name,
            rules_hash: self.rules_hash,
            main_ms: my.main_ms,
            byoyomi_ms: my.byoyomi_ms,
            side_preference: my.side,
            created_at: now,
            expires_at: now.saturating_add(offer::MAX_LIFETIME_SECS),
            availability: Availability::Withdrawn,
        };
        if let Some(signed) = offers::sign(&self.keypair, body) {
            let _ = self
                .swarm
                .behaviour_mut()
                .gossipsub
                .publish(self.topic.clone(), signed.encode());
        }
    }

    /// つながっているピアのうち数件に一覧取得を送る
    pub fn request_lists(&mut self) {
        let request = ListRequest {
            network_id: self.network_id,
            rules_hash: self.rules_hash,
            cursor: 0,
        }
        .encode();
        let peers: Vec<PeerId> = self.peer_conns.keys().copied().take(4).collect();
        for peer in &peers {
            self.swarm.behaviour_mut().lobby.send_request(peer, request.clone());
        }
        if peers.len() < 3 {
            self.last_provider_query = Some(Instant::now());
            self.swarm.behaviour_mut().kad.get_providers(self.lobby_key.clone());
        }
    }

    /// 招待に入れる自分の到達候補
    fn invite_addrs(&self) -> Vec<Vec<u8>> {
        let mut candidates: Vec<Multiaddr> = Vec::new();
        match self.spec.profile {
            Profile::Local => candidates.extend(self.listen_addrs.iter().cloned()),
            Profile::Internet => {
                candidates.extend(self.swarm.external_addresses().cloned());
                candidates.extend(self.listen_addrs.iter().filter(|a| addr::is_circuit(a)).cloned());
            }
        }
        let mut out: Vec<Vec<u8>> = Vec::new();
        for a in candidates {
            let a = addr::without_peer_id(&a);
            if !self.policy.advertisable(&a) {
                continue;
            }
            let bytes = a.to_vec();
            if bytes.len() > invite::MAX_ADDR_BYTES || out.contains(&bytes) {
                continue;
            }
            out.push(bytes);
            if out.len() >= invite::MAX_ADDRS {
                break;
            }
        }
        out
    }

    fn create_invite(&mut self) -> ReqResult {
        let mut token = [0u8; 32];
        rand::rngs::OsRng.fill_bytes(&mut token);
        let expires_at = unix_now().saturating_add(invite::MAX_LIFETIME_SECS);
        let addrs = self.invite_addrs();
        let address_count = addrs.len();
        let inv = Invite {
            network_id: self.network_id,
            peer_id: self.local_peer.to_bytes(),
            addrs,
            token,
            expires_at,
        };
        let text = inv
            .encode()
            .ok_or(("internal", "could not encode the invite".to_owned()))?;
        while self.invites.len() >= MAX_INVITES {
            self.invites.remove(0);
        }
        self.invites.push(OutstandingInvite { token, expires_at });
        Ok(json!({ "invite": text, "expiresAt": expires_at.to_string(), "addressCount": address_count }))
    }

    /// 招待文字列を読み、今のネットワークで使えるか確かめる
    fn read_invite(&self, text: &str) -> Result<(Invite, PeerId, Vec<Multiaddr>), (&'static str, String)> {
        let inv = Invite::decode(text).map_err(invite_error)?;
        inv.check(unix_now(), &self.network_id, &self.local_peer.to_bytes())
            .map_err(invite_error)?;
        let peer = PeerId::from_bytes(&inv.peer_id).map_err(|_| invite_error(InviteError::Invalid))?;
        let mut addrs = Vec::new();
        for bytes in &inv.addrs {
            let a = Multiaddr::try_from(bytes.clone()).map_err(|_| invite_error(InviteError::Invalid))?;
            addrs.push(addr::without_peer_id(&a));
        }
        Ok((inv, peer, addrs))
    }

    fn inspect_invite(&mut self, request: &Request) -> ReqResult {
        let p: ipc::InviteParams = ipc::params(request).map_err(invalid)?;
        let (inv, peer, addrs) = self.read_invite(&p.invite)?;
        let usable = addrs.iter().filter(|a| self.policy.usable(a)).count();
        let private: Vec<String> = if self.spec.profile == Profile::Internet {
            addrs
                .iter()
                .filter(|a| AddrPolicy::is_private(a))
                .map(ToString::to_string)
                .collect()
        } else {
            Vec::new()
        };
        Ok(json!({
            "peerId": peer.to_base58(),
            "expiresAt": inv.expires_at.to_string(),
            "addressCount": addrs.len(),
            "usableAddressCount": usable,
            "privateAddresses": private,
        }))
    }

    fn check_capacity(&self) -> Result<(), (&'static str, String)> {
        if self.game_connection_count() >= MAX_GAME_CONNECTIONS {
            return Err(("limit", "too many game connections".to_owned()));
        }
        Ok(())
    }

    fn connect_invite(&mut self, request: &Request) -> ReqResult {
        let p: ipc::ConnectInviteParams = ipc::params(request).map_err(invalid)?;
        let (inv, peer, addrs) = self.read_invite(&p.invite)?;
        self.check_capacity()?;
        if p.allow_private && self.spec.profile == Profile::Internet {
            for a in addrs.iter().filter(|a| AddrPolicy::is_private(a)) {
                self.policy.allow_private(a);
            }
        }
        let usable = self.policy.filter_addrs(addrs);
        let conn = self.alloc_conn();
        if usable.is_empty() && !self.swarm.is_connected(&peer) {
            self.conns.insert(
                conn,
                super::Conn {
                    peer,
                    origin: "invite",
                    direction: "outbound",
                    state: ConnState::Connecting,
                },
            );
            let _ = self.internal.send(super::Internal::OutboundFailed {
                conn,
                reason: "unreachable",
            });
        } else {
            self.start_outbound(conn, peer, usable, Some(inv.token), "invite");
        }
        Ok(json!({ "connection": conn.to_string() }))
    }

    fn connect_offer(&mut self, request: &Request) -> ReqResult {
        let p: ipc::OfferIdParams = ipc::params(request).map_err(invalid)?;
        let offer_id = from_hex32(&p.offer_id).ok_or_else(|| invalid("invalid offerId"))?;
        let peer = self
            .offers
            .find_peer(&offer_id)
            .ok_or(("unknown_offer", "unknown offer".to_owned()))?;
        self.check_capacity()?;
        let addrs = self.addr_book.get(&peer).cloned().unwrap_or_default();
        let conn = self.alloc_conn();
        self.start_outbound(conn, peer, addrs, None, "offer");
        Ok(json!({ "connection": conn.to_string() }))
    }

    fn set_game_peer(&mut self, request: &Request) -> ReqResult {
        let p: ipc::PeerIdParams = ipc::params(request).map_err(invalid)?;
        if p.peer_id.is_empty() {
            self.game_peer = None;
        } else {
            let peer = parse_peer_id(&p.peer_id).ok_or_else(|| invalid("invalid peerId"))?;
            self.game_peer = Some(peer);
        }
        Ok(json!({}))
    }

    fn connect_peer(&mut self, request: &Request) -> ReqResult {
        let p: ipc::PeerIdParams = ipc::params(request).map_err(invalid)?;
        let peer = parse_peer_id(&p.peer_id).ok_or_else(|| invalid("invalid peerId"))?;
        if self.game_peer != Some(peer) {
            return Err(("invalid_state", "connectPeer is only for the game peer".to_owned()));
        }
        self.check_capacity()?;
        let addrs = self.addr_book.get(&peer).cloned().unwrap_or_default();
        let conn = self.alloc_conn();
        self.start_outbound(conn, peer, addrs, None, "game");
        Ok(json!({ "connection": conn.to_string() }))
    }

    fn parse_conn(text: &str) -> Result<u64, (&'static str, String)> {
        ipc::parse_u64(text).ok_or_else(|| invalid("invalid connection"))
    }

    fn send_frame(&mut self, request: &Request) -> ReqResult {
        let p: ipc::SendFrameParams = ipc::params(request).map_err(invalid)?;
        let conn = Self::parse_conn(&p.connection)?;
        let bytes = B64
            .decode(p.frame.as_bytes())
            .map_err(|_| invalid("frame is not base64"))?;
        let header = frame::check(&bytes).map_err(|e| ("invalid_frame", e.to_string()))?;
        if header.frame_type == frame::TYPE_INVITE_TOKEN {
            return Err(("invalid_frame", "INVITE_TOKEN cannot be sent".to_owned()));
        }
        let entry = self
            .conns
            .get(&conn)
            .ok_or(("unknown_connection", "unknown connection".to_owned()))?;
        let ConnState::Open { writer, shared, .. } = &entry.state else {
            return Err(("unknown_connection", "connection is not open".to_owned()));
        };
        if header.frame_type == frame::TYPE_START_SIGNATURE {
            shared.sent_start_signature.store(true, Ordering::SeqCst);
        }
        writer.try_send(bytes).map_err(|e| match e {
            tokio::sync::mpsc::error::TrySendError::Full(_) => ("rate_limited", "send queue is full".to_owned()),
            tokio::sync::mpsc::error::TrySendError::Closed(_) => {
                ("unknown_connection", "connection is closing".to_owned())
            }
        })?;
        Ok(json!({}))
    }

    fn close_connection(&mut self, request: &Request) -> ReqResult {
        let p: ipc::ConnectionParams = ipc::params(request).map_err(invalid)?;
        let conn = Self::parse_conn(&p.connection)?;
        if !self.conns.contains_key(&conn) {
            return Err(("unknown_connection", "unknown connection".to_owned()));
        }
        self.close_conn(conn, "closed", false);
        Ok(json!({}))
    }
}
