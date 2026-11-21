//! IPC の要求の処理（protocol/ipc.md 3章）
//!
//! 要求ごとの処理は、どれも `fn(&mut Node, &Request) -> Result<Reply, IpcError>` の形で、メソッドとの対応は
//! handler の表1つ。応答をすぐ返すものは Reply::Now、主ループの外の処理（鍵の導出・協力ノードとのやり取り）を
//! 終えてから返すものは Reply::Later を返す。

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

use super::codes;
use super::conns::{NotOpen, Outbound};
use super::meets::{no_node, Purpose};
use super::presences::PRESENCE_LIFETIME_SECS;
use super::streams::Internal;
use super::{unix_now, Node};
use crate::addr::{self, AddrPolicy};
use crate::ipc::{self, ErrorCode, IpcError, Method, Reply, Request};

/// 要求の処理
type Handler = fn(&mut Node, &Request) -> Result<Reply, IpcError>;

/// メソッドごとの処理（hello・start は、始めた後には受け付けない）
fn handler(method: Method) -> Handler {
    match method {
        Method::Hello | Method::Start => Node::already_started,
        Method::CreateInvite => Node::create_invite,
        Method::CancelInvites => Node::cancel_invites,
        Method::InspectInvite => Node::inspect_invite,
        Method::ConnectInvite => Node::connect_invite,
        Method::SetGamePeer => Node::set_game_peer,
        Method::ConnectPeer => Node::connect_peer,
        Method::SendFrame => Node::send_frame,
        Method::CloseConnection => Node::close_connection,
        Method::CreatePresence => Node::create_presence,
        Method::CancelPresence => Node::cancel_presence,
        Method::ConnectPresence => Node::connect_presence,
        Method::MeetPut => Node::meet_put,
        Method::MeetGet => Node::meet_get,
        Method::Shutdown => Node::shutdown,
    }
}

/// 引数のない要求の引数を確かめる
fn empty(request: &Request) -> Result<(), IpcError> {
    ipc::params::<ipc::Empty>(request).map(|_| ())
}

/// 接続の番号
fn connection(conn: u64) -> Reply {
    Reply::Now(json!({ "connection": conn.to_string() }))
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

/// 要求の引数の番号（16進 64 文字）を読む
fn parse_slot(text: &str) -> Result<[u8; 32], IpcError> {
    sbq_protocol::hash::from_hex(text)
        .and_then(|bytes| <[u8; 32]>::try_from(bytes).ok())
        .ok_or_else(|| IpcError::invalid_request("invalid slot"))
}

fn parse_conn(text: &str) -> Result<u64, IpcError> {
    ipc::parse_u64(text).ok_or_else(|| IpcError::invalid_request("invalid connection"))
}

impl Node {
    pub fn on_request(&mut self, request: Request) {
        match handler(request.method)(self, &request) {
            Ok(Reply::Now(value)) => self.out.result(&request.request_id, value),
            Ok(Reply::Later) => {}
            Err(e) => self.out.error(&request.request_id, &e),
        }
    }

    fn already_started(&mut self, _request: &Request) -> Result<Reply, IpcError> {
        Err(IpcError::new(ErrorCode::InvalidState, "already started"))
    }

    /// 招待に入れる自分の到達候補。ローカル（同じ PC・同じ LAN）では、合言葉と同じ選び方の待受（codes::lan_invite_addrs。
    /// 回線のつながっていないインターフェースと、私設アドレスがあるときの loopback を入れない）
    fn invite_addrs(&self) -> Vec<Vec<u8>> {
        let mut candidates: Vec<Multiaddr> = Vec::new();
        match self.spec.profile {
            Profile::Local => candidates.extend(codes::lan_invite_addrs(&self.listen_addrs, &codes::idle_addrs())),
            Profile::Internet => {
                candidates.extend(self.swarm.external_addresses().cloned());
                candidates.extend(self.listen_addrs.iter().filter(|a| addr::is_circuit(a)).cloned());
            }
        }
        self.policy.invite_addrs(candidates)
    }

    /// すべての招待を無効にする（鍵を作っている途中の招待も足さない）
    fn invalidate_invites(&mut self) {
        self.invites.clear();
        self.invite_epoch = self.invite_epoch.wrapping_add(1);
        self.release_meets();
    }

    fn cancel_invites(&mut self, request: &Request) -> Result<Reply, IpcError> {
        empty(request)?;
        self.invalidate_invites();
        Ok(Reply::Now(json!({})))
    }

    /// 招待を作る。合言葉も付けるときは（同じ LAN の相手と直接つなぐとき・待ち合わせに対応した協力ノードに
    /// つながっているとき）、応答は鍵を作ってから返す（on_invite_ready）
    fn create_invite(&mut self, request: &Request) -> Result<Reply, IpcError> {
        empty(request)?;
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
            .ok_or_else(|| IpcError::new(ErrorCode::Internal, "could not encode the invite"))?;
        let mut ready = ReadyInvite {
            request_id: request.request_id.clone(),
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
            return match inv.to_body() {
                Some(body) if address_count > 0 && self.can_meet() => {
                    self.start_meet_invite(ready, body);
                    Ok(Reply::Later)
                }
                _ => self.finish_invite(ready).map(Reply::Now),
            };
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
        Ok(Reply::Later)
    }

    /// 作った招待を覚えて、createInvite の応答の中身を返す
    fn finish_invite(&mut self, ready: ReadyInvite) -> Result<Value, IpcError> {
        if ready.epoch != self.invite_epoch {
            // 取り消された招待を協力ノードに預けていたら、消す
            self.delete_meets(ready.meet_slot.into_iter().collect());
            return Err(IpcError::new(ErrorCode::InvalidState, "invites were cancelled"));
        }
        self.invites
            .add(ready.token, ready.code_key, ready.meet_slot, ready.expires_at);
        // 上限で押し出された招待を協力ノードに預けていたら、消す
        self.release_meets();
        Ok(json!({
            "invite": ready.invite,
            "code": ready.code,
            "expiresAt": ready.expires_at.to_string(),
            "addressCount": ready.address_count,
        }))
    }

    /// 合言葉を付け終えた招待の createInvite の応答を返す
    pub(super) fn on_invite_ready(&mut self, ready: ReadyInvite) {
        let request_id = ready.request_id.clone();
        match self.finish_invite(ready) {
            Ok(value) => self.out.result(&request_id, value),
            Err(e) => self.out.error(&request_id, &e),
        }
    }

    /// 読んだ招待が今のネットワークで使えるか確かめ、相手の PeerId と宛先を取り出す
    fn check_invite(&self, inv: Invite) -> Result<(Invite, PeerId, Vec<Multiaddr>), IpcError> {
        inv.check(unix_now(), &self.network_id, &self.local_peer.to_bytes())?;
        let peer = PeerId::from_bytes(&inv.peer_id).map_err(|_| IpcError::from(InviteError::Invalid))?;
        let mut addrs = Vec::new();
        for bytes in &inv.addrs {
            let a = Multiaddr::try_from(bytes.clone()).map_err(|_| IpcError::from(InviteError::Invalid))?;
            addrs.push(addr::without_peer_id(&a));
        }
        Ok((inv, peer, addrs))
    }

    /// 招待文字列を読み、今のネットワークで使えるか確かめる
    fn read_invite(&self, text: &str) -> Result<Target, IpcError> {
        let inv = Invite::decode(text)?;
        let (inv, peer, addrs) = self.check_invite(inv)?;
        Ok(Target::Invite { inv, peer, addrs })
    }

    /// 協力ノードから引き取って復号した招待の本体を読み、今のネットワークで使えるか確かめる
    pub(super) fn read_invite_body(&self, body: &[u8]) -> Result<(Invite, PeerId, Vec<Multiaddr>), IpcError> {
        let inv = Invite::from_body(body)?;
        self.check_invite(inv)
    }

    /// 同じ LAN の合言葉を読み、今のネットワークで使えるか確かめる（相手の LAN の中の宛先なので、ローカルだけ）
    fn read_code(&self, text: &str) -> Result<Target, IpcError> {
        let code = Code::decode(text).ok_or(InviteError::Invalid)?;
        if self.spec.profile != Profile::Local {
            return Err(InviteError::WrongNetwork.into());
        }
        let addrs = self.code_addrs(&code);
        if addrs.is_empty() {
            return Err(InviteError::Own.into());
        }
        Ok(Target::Code { code, addrs })
    }

    /// 招待の要求の文字列（招待文字列か合言葉）を読む。合言葉の種類は形で決まる（協力ノードを使う合言葉は 10 文字で、
    /// 検査の文字の作り方も違う。同じ LAN の合言葉として読めるものと重ならない）
    fn read_target(&self, text: &str) -> Result<Target, IpcError> {
        if text.trim_start().starts_with(invite::PREFIX) {
            self.read_invite(text)
        } else if let Some(code) = MeetCode::decode(text) {
            Ok(Target::Meet { code })
        } else {
            self.read_code(text)
        }
    }

    /// inspectInvite。協力ノードを使う合言葉は、協力ノードから招待を引き取ってから応答する（on_meet_fetched）
    fn inspect_invite(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::InviteParams = ipc::params(request)?;
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
                    return Ok(Reply::Later);
                }
            },
        };
        Ok(Reply::Now(self.inspect_result(kind, &peer_id, expires_at, &addrs)))
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

    fn check_capacity(&self) -> Result<(), IpcError> {
        if !self.conns.has_room() {
            return Err(IpcError::new(ErrorCode::Limit, "too many game connections"));
        }
        Ok(())
    }

    /// connectInvite。番号をすぐ返し、結果は connectionOpened・connectionClosed で知らせる
    fn connect_invite(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::ConnectInviteParams = ipc::params(request)?;
        let conn = match self.read_target(&p.invite)? {
            Target::Invite { inv, peer, addrs } => {
                self.check_capacity()?;
                let start = self.invite_outbound(inv.token, peer, addrs, p.allow_private);
                self.connect_new(start)
            }
            Target::Code { code, addrs } => {
                self.check_capacity()?;
                let usable = self.policy.filter_addrs(addrs);
                self.start_code_connect(code, usable)
            }
            Target::Meet { code } => {
                self.check_capacity()?;
                match self.take_cached_meet(&code) {
                    // 確かめた（inspectInvite）ときに引き取った招待
                    Some(f) => {
                        let start = self.invite_outbound(f.inv.token, f.peer, f.addrs, p.allow_private);
                        self.connect_new(start)
                    }
                    // 協力ノードから引き取ってからつなぐ（on_meet_fetched）
                    None => {
                        if !self.can_meet() {
                            return Err(no_node());
                        }
                        let conn = self.conns.fetch();
                        let purpose = Purpose::Connect {
                            conn,
                            allow_private: p.allow_private,
                        };
                        if let Err(e) = self.start_meet_fetch(code, purpose) {
                            self.conns.cancel(conn);
                            return Err(e);
                        }
                        conn
                    }
                }
            }
        };
        Ok(connection(conn))
    }

    /// 招待の相手へつなぐ準備（招待文字列・協力ノードから引き取った招待・在席の知らせで共通）。使える宛先がなく、
    /// つながってもいなければ閉じる理由
    pub(super) fn invite_outbound(
        &mut self,
        token: [u8; 32],
        peer: PeerId,
        addrs: Vec<Multiaddr>,
        allow_private: bool,
    ) -> Result<Outbound, ipc::CloseReason> {
        if allow_private && self.spec.profile == Profile::Internet {
            for a in addrs.iter().filter(|a| AddrPolicy::is_private(a)) {
                self.policy.allow_private(a);
            }
        }
        let had_addrs = !addrs.is_empty();
        let usable = self.policy.filter_addrs(addrs);
        if usable.is_empty() && !self.swarm.is_connected(&peer) {
            // 「直接接続を使わない」のときは、自分の協力ノードの中継を通る宛先が招待に無ければつながない
            return Err(if self.policy.relay_only() && had_addrs {
                ipc::CloseReason::NoRelayRoute
            } else {
                ipc::CloseReason::Unreachable
            });
        }
        self.begin_outbound(peer, usable, Some(token), None, ipc::Origin::Invite)
    }

    /// 在席の知らせを作る（自分の到達候補と、期限まで何度でも使えるトークン。招待の本体と同じ形）。対局・UI側が、
    /// 友だちごとの鍵で暗号化して協力ノードに預ける（中身は読まない）
    fn create_presence(&mut self, request: &Request) -> Result<Reply, IpcError> {
        empty(request)?;
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
            .ok_or_else(|| IpcError::new(ErrorCode::Internal, "could not encode the presence"))?;
        let id = self.presences.add(token, expires_at);
        Ok(Reply::Now(json!({
            "presence": B64.encode(body),
            "id": id.to_string(),
            "expiresAt": expires_at.to_string(),
            "addressCount": address_count,
        })))
    }

    fn cancel_presence(&mut self, request: &Request) -> Result<Reply, IpcError> {
        empty(request)?;
        self.presences.clear();
        Ok(Reply::Now(json!({})))
    }

    /// 友だちの在席の知らせ（対局・UI側が協力ノードから引き取って復号したもの）の相手へ、申し込みの接続を始める
    fn connect_presence(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::ConnectPresenceParams = ipc::params(request)?;
        if p.presence.len() > invite::MAX_STRING_BYTES.saturating_mul(2) {
            return Err(IpcError::invalid_request("presence is too long"));
        }
        let body = B64
            .decode(p.presence.as_bytes())
            .map_err(|_| IpcError::invalid_request("presence is not base64"))?;
        let (inv, peer, addrs) = self.read_invite_body(&body)?;
        self.check_capacity()?;
        let start = self.invite_outbound(inv.token, peer, addrs, p.allow_private);
        Ok(connection(self.connect_new(start)))
    }

    /// 対局・UI側が作った番号と預けるもの（中身は読まない）を、待ち合わせに対応した協力ノードに預ける
    /// （応答は Internal::MeetPutDone）
    fn meet_put(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::MeetPutParams = ipc::params(request)?;
        let slot = parse_slot(&p.slot)?;
        let blob = B64
            .decode(p.blob.as_bytes())
            .map_err(|_| IpcError::invalid_request("blob is not base64"))?;
        let put = MeetRequest::Put {
            slot,
            blob,
            ttl_secs: p.ttl_seconds,
            one_time: false,
        };
        // 大きさと秒数の範囲は、協力ノードが読むときと同じ規則で確かめる
        if MeetRequest::decode(&put.encode()).is_none() {
            return Err(IpcError::invalid_request("blob or ttlSeconds out of range"));
        }
        self.start_meet_put(request.request_id.clone(), put);
        Ok(Reply::Later)
    }

    /// 番号で預けられているものを、待ち合わせに対応した協力ノードから引き取る（中身は読まずに返す。
    /// 応答は Internal::MeetGot）
    fn meet_get(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::MeetGetParams = ipc::params(request)?;
        let slot = parse_slot(&p.slot)?;
        self.start_meet_get(request.request_id.clone(), slot);
        Ok(Reply::Later)
    }

    fn set_game_peer(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::PeerIdParams = ipc::params(request)?;
        if p.peer_id.is_empty() {
            self.game_peer = None;
        } else {
            let peer = parse_peer_id(&p.peer_id).ok_or_else(|| IpcError::invalid_request("invalid peerId"))?;
            self.game_peer = Some(peer);
        }
        // 対局中は、相手以外の届いた対局ストリームを読まずに閉じる
        self.accept_gate.set_game_peer(self.game_peer);
        Ok(Reply::Now(json!({})))
    }

    fn connect_peer(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::PeerIdParams = ipc::params(request)?;
        let peer = parse_peer_id(&p.peer_id).ok_or_else(|| IpcError::invalid_request("invalid peerId"))?;
        if self.game_peer != Some(peer) {
            return Err(IpcError::new(
                ErrorCode::InvalidState,
                "connectPeer is only for the game peer",
            ));
        }
        self.check_capacity()?;
        let addrs = self.addr_book.get(&peer).cloned().unwrap_or_default();
        let start = self.begin_outbound(peer, addrs, None, None, ipc::Origin::Game);
        Ok(connection(self.connect_new(start)))
    }

    fn send_frame(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::SendFrameParams = ipc::params(request)?;
        let conn = parse_conn(&p.connection)?;
        let bytes = B64
            .decode(p.frame.as_bytes())
            .map_err(|_| IpcError::invalid_request("frame is not base64"))?;
        let header = frame::check(&bytes).map_err(|e| IpcError::new(ErrorCode::InvalidFrame, e.to_string()))?;
        if header.frame_type == frame::TYPE_INVITE_TOKEN {
            return Err(IpcError::new(ErrorCode::InvalidFrame, "INVITE_TOKEN cannot be sent"));
        }
        let io = self.conns.io(conn).map_err(|e| match e {
            NotOpen::Unknown => IpcError::new(ErrorCode::UnknownConnection, "unknown connection"),
            NotOpen::Pending => IpcError::new(ErrorCode::UnknownConnection, "connection is not open"),
        })?;
        io.shared.observe_sent(&header);
        io.writer.try_send(bytes).map_err(|e| match e {
            tokio::sync::mpsc::error::TrySendError::Full(_) => {
                IpcError::new(ErrorCode::RateLimited, "send queue is full")
            }
            tokio::sync::mpsc::error::TrySendError::Closed(_) => {
                IpcError::new(ErrorCode::UnknownConnection, "connection is closing")
            }
        })?;
        Ok(Reply::Now(json!({})))
    }

    /// closeConnection（閉じた接続には connectionClosed を出さない）
    fn close_connection(&mut self, request: &Request) -> Result<Reply, IpcError> {
        let p: ipc::ConnectionParams = ipc::params(request)?;
        let conn = parse_conn(&p.connection)?;
        if !self.conns.cancel(conn) {
            return Err(IpcError::new(ErrorCode::UnknownConnection, "unknown connection"));
        }
        Ok(Reply::Now(json!({})))
    }

    /// shutdown（応答を返した後、招待と在席の知らせを無効にして終わる）
    fn shutdown(&mut self, request: &Request) -> Result<Reply, IpcError> {
        empty(request)?;
        self.presences.clear();
        self.invalidate_invites();
        self.shutdown_at = Instant::now()
            .checked_add(Duration::from_millis(300))
            .or_else(|| Some(Instant::now()));
        Ok(Reply::Now(json!({})))
    }
}
