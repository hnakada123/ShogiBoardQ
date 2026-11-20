//! IPC の要求の処理（protocol/ipc.md 3章）

use std::time::{Duration, Instant};

use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use libp2p::{Multiaddr, PeerId};
use rand::RngCore;
use sbq_protocol::code::Code;
use sbq_protocol::frame;
use sbq_protocol::invite::{self, Invite, InviteError};
use sbq_protocol::meet::{MeetCode, Request as MeetRequest};
use sbq_protocol::network::Profile;
use serde_json::{json, Value};

use super::meets::Purpose;
use super::presences::PRESENCE_LIFETIME_SECS;
use super::streams::Internal;
use super::{unix_now, ConnState, Node, MAX_GAME_CONNECTIONS};
use crate::addr::{self, AddrPolicy};
use crate::ipc::{self, Method, Request};

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

/// 招待の要求（inspectInvite・connectInvite）の文字列を読んだもの
enum Target {
    /// 招待文字列
    Invite {
        inv: Invite,
        peer: PeerId,
        addrs: Vec<Multiaddr>,
    },
    /// 同じ LAN の合言葉（addrs は、合言葉の宛先から自分の待受を除いたもの）
    Code { code: Code, addrs: Vec<Multiaddr> },
    /// 協力ノードを使う合言葉（招待は、協力ノードから引き取る）
    Meet { code: MeetCode },
}

/// 合言葉を付け終えた招待（createInvite の応答を、合言葉の鍵を作ってから返す）
pub struct ReadyInvite {
    request_id: String,
    /// 鍵を作り始めたときの番号（その後に招待が取り消されていれば、足さない）
    epoch: u64,
    invite: String,
    token: [u8; 32],
    pub(super) expires_at: u64,
    address_count: usize,
    /// 合言葉（付けられなければ空。招待文字列だけの招待にする）
    pub(super) code: String,
    /// 同じ LAN の合言葉の鍵（その合言葉を付けたときだけ）
    code_key: Option<[u8; 32]>,
    /// 協力ノードに預けた番号（協力ノードを使う合言葉を付けたときだけ）
    pub(super) meet_slot: Option<[u8; 32]>,
}

/// 要求の引数の PeerId を表記から読む（Ed25519 の identity multihash に限る）
fn parse_peer_id(text: &str) -> Option<PeerId> {
    let peer: PeerId = text.parse().ok()?;
    if sbq_protocol::peerid::is_ed25519(&peer.to_bytes()) {
        Some(peer)
    } else {
        None
    }
}

impl Node {
    pub fn on_request(&mut self, request: Request) {
        let result = match request.method {
            Method::Hello | Method::Start => Err(("invalid_state", "already started".to_owned())),
            Method::CreateInvite => match self.empty(&request) {
                // 応答は、合言葉の鍵を作ってから返す（on_invite_ready）
                Ok(()) => match self.create_invite(&request.request_id) {
                    Ok(()) => return,
                    Err(e) => Err(e),
                },
                Err(e) => Err(e),
            },
            Method::CancelInvites => self.empty(&request).map(|()| {
                self.cancel_invites();
                json!({})
            }),
            Method::InspectInvite => match self.inspect_invite(&request) {
                Ok(Some(value)) => Ok(value),
                // 応答は、協力ノードから招待を引き取ってから返す（on_meet_fetched）
                Ok(None) => return,
                Err(e) => Err(e),
            },
            Method::ConnectInvite => self.connect_invite(&request),
            Method::SetGamePeer => self.set_game_peer(&request),
            Method::ConnectPeer => self.connect_peer(&request),
            Method::SendFrame => self.send_frame(&request),
            Method::CloseConnection => self.close_connection(&request),
            Method::CreatePresence => self.empty(&request).and_then(|()| self.create_presence()),
            Method::CancelPresence => self.empty(&request).map(|()| {
                self.presences.clear();
                json!({})
            }),
            Method::ConnectPresence => self.connect_presence(&request),
            // 応答は、協力ノードとのやり取りを終えてから返す（Internal::MeetPutDone・MeetGot）
            Method::MeetPut => match self.meet_put(&request) {
                Ok(()) => return,
                Err(e) => Err(e),
            },
            Method::MeetGet => match self.meet_get(&request) {
                Ok(()) => return,
                Err(e) => Err(e),
            },
            Method::Shutdown => self.empty(&request).map(|()| {
                self.presences.clear();
                self.cancel_invites();
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
        self.policy.invite_addrs(candidates)
    }

    /// すべての招待を無効にする（鍵を作っている途中の招待も足さない）
    fn cancel_invites(&mut self) {
        self.invites.clear();
        self.invite_epoch = self.invite_epoch.wrapping_add(1);
        self.release_meets();
    }

    /// 招待を作る。合言葉も付けるときは（同じ LAN の相手と直接つなぐとき・待ち合わせに対応した協力ノードに
    /// つながっているとき）、応答は鍵を作ってから返す
    fn create_invite(&mut self, request_id: &str) -> Result<(), (&'static str, String)> {
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
        let mut ready = ReadyInvite {
            request_id: request_id.to_owned(),
            epoch: self.invite_epoch,
            invite: text,
            token,
            expires_at,
            address_count,
            code: String::new(),
            code_key: None,
            meet_slot: None,
        };
        let Some(code) = self.code_for_invite() else {
            // 同じ LAN の合言葉を付けられないときは、協力ノードを使う合言葉を付ける。接続先の入っていない招待は
            // 相手がつなげないので、付けない
            match inv.to_body() {
                Some(body) if address_count > 0 && self.can_meet() => self.start_meet_invite(ready, body),
                _ => self.on_invite_ready(ready),
            }
            return Ok(());
        };
        // 鍵の導出は時間とメモリを使うので、主ループの外で行う
        let internal = self.internal.clone();
        let network_id = self.network_id;
        tokio::task::spawn_blocking(move || {
            if let Some((text, key)) = code.encode().zip(code.derive_key(&network_id)) {
                ready.code = text;
                ready.code_key = Some(key);
            }
            let _ = internal.send(Internal::InviteReady(ready));
        });
        Ok(())
    }

    /// 作った招待を覚えて、createInvite の応答を返す
    pub(super) fn on_invite_ready(&mut self, ready: ReadyInvite) {
        if ready.epoch != self.invite_epoch {
            // 取り消された招待を協力ノードに預けていたら、消す
            self.delete_meets(ready.meet_slot.into_iter().collect());
            self.out
                .error(&ready.request_id, "invalid_state", "invites were cancelled");
            return;
        }
        self.invites
            .add(ready.token, ready.code_key, ready.meet_slot, ready.expires_at);
        // 上限で押し出された招待を協力ノードに預けていたら、消す
        self.release_meets();
        self.out.result(
            &ready.request_id,
            json!({
                "invite": ready.invite,
                "code": ready.code,
                "expiresAt": ready.expires_at.to_string(),
                "addressCount": ready.address_count,
            }),
        );
    }

    /// 読んだ招待が今のネットワークで使えるか確かめ、相手の PeerId と宛先を取り出す
    fn check_invite(&self, inv: Invite) -> Result<(Invite, PeerId, Vec<Multiaddr>), (&'static str, String)> {
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

    /// 招待文字列を読み、今のネットワークで使えるか確かめる
    fn read_invite(&self, text: &str) -> Result<Target, (&'static str, String)> {
        let inv = Invite::decode(text).map_err(invite_error)?;
        let (inv, peer, addrs) = self.check_invite(inv)?;
        Ok(Target::Invite { inv, peer, addrs })
    }

    /// 協力ノードから引き取って復号した招待の本体を読み、今のネットワークで使えるか確かめる
    pub(super) fn read_invite_body(
        &self,
        body: &[u8],
    ) -> Result<(Invite, PeerId, Vec<Multiaddr>), (&'static str, String)> {
        let inv = Invite::from_body(body).map_err(invite_error)?;
        self.check_invite(inv)
    }

    /// 同じ LAN の合言葉を読み、今のネットワークで使えるか確かめる（相手の LAN の中の宛先なので、ローカルだけ）
    fn read_code(&self, text: &str) -> Result<Target, (&'static str, String)> {
        let code = Code::decode(text).ok_or_else(|| invite_error(InviteError::Invalid))?;
        if self.spec.profile != Profile::Local {
            return Err(invite_error(InviteError::WrongNetwork));
        }
        let addrs = self.code_addrs(&code);
        if addrs.is_empty() {
            return Err(invite_error(InviteError::Own));
        }
        Ok(Target::Code { code, addrs })
    }

    /// 招待の要求の文字列（招待文字列か合言葉）を読む。合言葉の種類は形で決まる（協力ノードを使う合言葉は 10 文字で、
    /// 検査の文字の作り方も違う。同じ LAN の合言葉として読めるものと重ならない）
    fn read_target(&self, text: &str) -> Result<Target, (&'static str, String)> {
        if text.trim_start().starts_with(invite::PREFIX) {
            self.read_invite(text)
        } else if let Some(code) = MeetCode::decode(text) {
            Ok(Target::Meet { code })
        } else {
            self.read_code(text)
        }
    }

    /// inspectInvite の応答。協力ノードから招待を引き取ってから返すときは None
    fn inspect_invite(&mut self, request: &Request) -> Result<Option<Value>, (&'static str, String)> {
        let p: ipc::InviteParams = ipc::params(request).map_err(invalid)?;
        let (kind, peer_id, expires_at, addrs) = match self.read_target(&p.invite)? {
            Target::Invite { inv, peer, addrs } => ("invite", peer.to_base58(), inv.expires_at, addrs),
            // 同じ LAN の合言葉には、相手の PeerId も期限も入っていない
            Target::Code { addrs, .. } => ("code", String::new(), 0, addrs),
            Target::Meet { code } => match self.cached_meet(&code) {
                Some(f) => ("meetCode", f.peer.to_base58(), f.inv.expires_at, f.addrs.clone()),
                None => {
                    let purpose = Purpose::Inspect {
                        request_id: request.request_id.clone(),
                    };
                    self.start_meet_fetch(code, purpose)?;
                    return Ok(None);
                }
            },
        };
        Ok(Some(self.inspect_result(kind, &peer_id, expires_at, &addrs)))
    }

    /// inspectInvite の応答の中身
    pub(super) fn inspect_result(&self, kind: &str, peer_id: &str, expires_at: u64, addrs: &[Multiaddr]) -> Value {
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
        json!({
            "kind": kind,
            "peerId": peer_id,
            "expiresAt": expires_at.to_string(),
            "addressCount": addrs.len(),
            "usableAddressCount": usable,
            "privateAddresses": private,
        })
    }

    fn check_capacity(&self) -> Result<(), (&'static str, String)> {
        if self.game_connection_count() >= MAX_GAME_CONNECTIONS {
            return Err(("limit", "too many game connections".to_owned()));
        }
        Ok(())
    }

    fn connect_invite(&mut self, request: &Request) -> ReqResult {
        let p: ipc::ConnectInviteParams = ipc::params(request).map_err(invalid)?;
        let (inv, peer, addrs) = match self.read_target(&p.invite)? {
            Target::Invite { inv, peer, addrs } => (inv, peer, addrs),
            Target::Code { code, addrs } => {
                self.check_capacity()?;
                let usable = self.policy.filter_addrs(addrs);
                let conn = self.alloc_conn();
                self.start_code_connect(conn, code, usable);
                return Ok(json!({ "connection": conn.to_string() }));
            }
            Target::Meet { code } => {
                self.check_capacity()?;
                match self.take_cached_meet(&code) {
                    // 確かめた（inspectInvite）ときに引き取った招待
                    Some(f) => (f.inv, f.peer, f.addrs),
                    None => {
                        // 協力ノードから引き取ってからつなぐ（結果は connectionOpened・connectionClosed で知らせる）
                        if !self.can_meet() {
                            return Err(("invite_no_node", "no node for passcodes is connected".to_owned()));
                        }
                        let conn = self.alloc_conn();
                        let purpose = Purpose::Connect {
                            conn,
                            allow_private: p.allow_private,
                        };
                        self.start_meet_fetch(code, purpose)?;
                        self.meets.connects.insert(conn);
                        return Ok(json!({ "connection": conn.to_string() }));
                    }
                }
            }
        };
        self.check_capacity()?;
        let conn = self.alloc_conn();
        self.connect_to_invite(conn, inv.token, peer, addrs, p.allow_private);
        Ok(json!({ "connection": conn.to_string() }))
    }

    /// 招待の相手へつなぎ始める（招待文字列と、協力ノードから引き取った招待で共通）
    pub(super) fn connect_to_invite(
        &mut self,
        conn: u64,
        token: [u8; 32],
        peer: PeerId,
        addrs: Vec<Multiaddr>,
        allow_private: bool,
    ) {
        if allow_private && self.spec.profile == Profile::Internet {
            for a in addrs.iter().filter(|a| AddrPolicy::is_private(a)) {
                self.policy.allow_private(a);
            }
        }
        let had_addrs = !addrs.is_empty();
        let usable = self.policy.filter_addrs(addrs);
        if usable.is_empty() && !self.swarm.is_connected(&peer) {
            // 「直接接続を使わない」のときは、自分の協力ノードの中継を通る宛先が招待に無ければつながない
            let reason = if self.policy.relay_only() && had_addrs {
                ipc::CloseReason::NoRelayRoute
            } else {
                ipc::CloseReason::Unreachable
            };
            self.conns.insert(
                conn,
                super::Conn {
                    peer,
                    origin: ipc::Origin::Invite,
                    direction: ipc::Direction::Outbound,
                    state: ConnState::Connecting,
                    known_route: None,
                    presence: 0,
                },
            );
            let _ = self.internal.send(super::Internal::OutboundFailed { conn, reason });
        } else {
            self.start_outbound(conn, peer, usable, Some(token), ipc::Origin::Invite);
        }
    }

    /// 在席の知らせを作る（自分の到達候補と、期限まで何度でも使えるトークン。招待の本体と同じ形）。対局・UI側が、
    /// 友だちごとの鍵で暗号化して協力ノードに預ける（中身は読まない）
    fn create_presence(&mut self) -> ReqResult {
        let mut token = [0u8; 32];
        rand::rngs::OsRng.fill_bytes(&mut token);
        let expires_at = unix_now().saturating_add(PRESENCE_LIFETIME_SECS);
        let addrs = self.invite_addrs();
        let address_count = addrs.len();
        let inv = Invite {
            network_id: self.network_id,
            peer_id: self.local_peer.to_bytes(),
            addrs,
            token,
            expires_at,
        };
        let body = inv
            .to_body()
            .ok_or(("internal", "could not encode the presence".to_owned()))?;
        let id = self.presences.add(token, expires_at);
        Ok(json!({
            "presence": B64.encode(body),
            "id": id.to_string(),
            "expiresAt": expires_at.to_string(),
            "addressCount": address_count,
        }))
    }

    /// 友だちの在席の知らせ（対局・UI側が協力ノードから引き取って復号したもの）の相手へ、申し込みの接続を始める
    fn connect_presence(&mut self, request: &Request) -> ReqResult {
        let p: ipc::ConnectPresenceParams = ipc::params(request).map_err(invalid)?;
        if p.presence.len() > invite::MAX_STRING_BYTES.saturating_mul(2) {
            return Err(invalid("presence is too long"));
        }
        let body = B64
            .decode(p.presence.as_bytes())
            .map_err(|_| invalid("presence is not base64"))?;
        let (inv, peer, addrs) = self.read_invite_body(&body)?;
        self.check_capacity()?;
        let conn = self.alloc_conn();
        self.connect_to_invite(conn, inv.token, peer, addrs, p.allow_private);
        Ok(json!({ "connection": conn.to_string() }))
    }

    /// 要求の引数の番号（16進 64 文字）を読む
    fn parse_slot(text: &str) -> Result<[u8; 32], (&'static str, String)> {
        sbq_protocol::hash::from_hex(text)
            .and_then(|bytes| <[u8; 32]>::try_from(bytes).ok())
            .ok_or_else(|| invalid("invalid slot"))
    }

    /// 対局・UI側が作った番号と預けるもの（中身は読まない）を、待ち合わせに対応した協力ノードに預ける
    fn meet_put(&mut self, request: &Request) -> Result<(), (&'static str, String)> {
        let p: ipc::MeetPutParams = ipc::params(request).map_err(invalid)?;
        let slot = Self::parse_slot(&p.slot)?;
        let blob = B64
            .decode(p.blob.as_bytes())
            .map_err(|_| invalid("blob is not base64"))?;
        let put = MeetRequest::Put {
            slot,
            blob,
            ttl_secs: p.ttl_seconds,
            one_time: false,
        };
        // 大きさと秒数の範囲は、協力ノードが読むときと同じ規則で確かめる
        if MeetRequest::decode(&put.encode()).is_none() {
            return Err(invalid("blob or ttlSeconds out of range"));
        }
        self.start_meet_put(request.request_id.clone(), put);
        Ok(())
    }

    /// 番号で預けられているものを、待ち合わせに対応した協力ノードから引き取る（中身は読まずに返す）
    fn meet_get(&mut self, request: &Request) -> Result<(), (&'static str, String)> {
        let p: ipc::MeetGetParams = ipc::params(request).map_err(invalid)?;
        let slot = Self::parse_slot(&p.slot)?;
        self.start_meet_get(request.request_id.clone(), slot);
        Ok(())
    }

    fn set_game_peer(&mut self, request: &Request) -> ReqResult {
        let p: ipc::PeerIdParams = ipc::params(request).map_err(invalid)?;
        if p.peer_id.is_empty() {
            self.game_peer = None;
        } else {
            let peer = parse_peer_id(&p.peer_id).ok_or_else(|| invalid("invalid peerId"))?;
            self.game_peer = Some(peer);
        }
        // 対局中は、相手以外の届いた対局ストリームを読まずに閉じる
        self.accept_gate.set_game_peer(self.game_peer);
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
        self.start_outbound(conn, peer, addrs, None, ipc::Origin::Game);
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
        shared.observe_sent(&header);
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
        if !self.conns.contains_key(&conn)
            && !self.code_connects.contains_key(&conn)
            && !self.meets.connects.contains(&conn)
        {
            return Err(("unknown_connection", "unknown connection".to_owned()));
        }
        self.close_conn(conn, ipc::CloseReason::Closed, false);
        Ok(json!({}))
    }
}
