//! 協力ノードを使う合言葉（protocol/README.md 3.2節・ipc.md 3章）
//!
//! 招待する側: 招待の本体を合言葉から作った鍵で暗号化し、待ち合わせに対応した協力ノードに預ける
//! （start_meet_invite）。招待が使われた・取り消されたら、預けたものを消す（release_meets）。
//! つなぐ側: 合言葉から番号と鍵を作り、協力ノードから引き取って復号し（start_meet_fetch）、招待文字列のときと
//! 同じ手順でつなぐ。合言葉には接続先が入らず、協力ノードには預かったものの中身が分からない。

use std::collections::HashMap;

use libp2p::{identify, Multiaddr, PeerId};
use rand::RngCore;
use sbq_protocol::invite::Invite;
use sbq_protocol::meet::{self as protocol, MeetCode, Request, Status, MAX_TTL_SECS, MEET_PROTOCOL, NONCE_BYTES};

use super::requests::ReadyInvite;
use super::streams::Internal;
use super::{unix_now, Node};
use crate::ipc::{self, ErrorCode, IpcError};
use crate::meet;

/// 引き取った招待（合言葉の秘密で見分ける）
pub struct Fetched {
    secret: u64,
    pub inv: Invite,
    pub peer: PeerId,
    pub addrs: Vec<Multiaddr>,
}

/// 引き取りを頼んだ理由
pub enum Purpose {
    /// inspectInvite（応答を返す）
    Inspect { request_id: String },
    /// connectInvite（応答は返してある。つなぎ始める）
    Connect { conn: u64, allow_private: bool },
}

/// 引き取りの結果
pub enum Outcome {
    /// 復号できた招待の本体
    Found(Vec<u8>),
    /// 協力ノードは答えたが、この合言葉で読めるものが無い（合言葉が違う・期限が過ぎた・もう引き取られた）
    NotFound,
    /// 答えた協力ノードが無い
    NoAnswer,
    /// 自分が作った招待の合言葉（協力ノードから引き取らない。引き取ると、相手が引き取れなくなる）
    Own,
}

/// 引き取りのタスクから主ループへの知らせ
pub struct FetchResult {
    secret: u64,
    outcome: Outcome,
}

/// 協力ノードを使う合言葉の状態
#[derive(Default)]
pub struct Meets {
    /// 待ち合わせに対応した協力ノード（利用者が指定し、つながっているもの）
    pub nodes: Vec<PeerId>,
    /// 引き取った招待（最後の1つ。確かめた（inspectInvite）後の接続に使う）
    fetched: Option<Fetched>,
    /// 引き取っている途中の合言葉（秘密）と、その結果を待っている要求。協力ノードは1回引き取られたら消すので、
    /// 同じ合言葉を続けて確かめられても、引き取りは1回にして、結果を全部の要求に返す
    inflight: HashMap<u64, Vec<Purpose>>,
}

/// 待ち合わせを受け付けるピアか（Identify の protocols）
fn supports_meet(info: &identify::Info) -> bool {
    info.protocols.iter().any(|p| p.as_ref() == MEET_PROTOCOL)
}

/// 待ち合わせに対応した協力ノードにつながっていない
pub(super) fn no_node() -> IpcError {
    IpcError::new(ErrorCode::InviteNoNode, "no node for passcodes is connected")
}

impl Node {
    /// 利用者が指定した協力ノードが名乗った内容から、待ち合わせに対応しているかを覚える
    pub(super) fn note_meet_node(&mut self, peer: PeerId, info: &identify::Info) {
        self.meets.nodes.retain(|p| p != &peer);
        if supports_meet(info) {
            self.meets.nodes.push(peer);
        }
    }

    /// つながらなくなった協力ノードを外す
    pub(super) fn forget_meet_node(&mut self, peer: &PeerId) {
        self.meets.nodes.retain(|p| p != peer);
    }

    /// 協力ノードを使う合言葉を出せるか（待ち合わせに対応した協力ノードにつながっている）
    pub(super) fn can_meet(&self) -> bool {
        !self.meets.nodes.is_empty()
    }

    /// 招待に合言葉を付ける: 鍵を作り、招待の本体（body）を暗号化して協力ノードに預けてから、createInvite の
    /// 応答を返す（Internal::InviteReady）。預けられなければ、合言葉なしの招待にする
    pub(super) fn start_meet_invite(&mut self, mut ready: ReadyInvite, body: Vec<u8>) {
        let code = MeetCode::new(rand::rngs::OsRng.next_u64());
        let mut nonce = [0u8; NONCE_BYTES];
        rand::rngs::OsRng.fill_bytes(&mut nonce);
        let ttl_secs = ready.expires_at.saturating_sub(unix_now()).clamp(1, MAX_TTL_SECS);
        let nodes = self.meets.nodes.clone();
        let mut control = self.control.clone();
        let internal = self.internal.clone();
        let network_id = self.network_id;
        tokio::spawn(async move {
            // 鍵の導出は時間とメモリを使うので、主ループの外で行う
            let keys = tokio::task::spawn_blocking(move || code.derive(&network_id))
                .await
                .ok()
                .flatten();
            let sealed = keys.and_then(|keys| {
                let blob = protocol::seal(&keys.key, &network_id, &nonce, &body)?;
                Some((keys.slot, blob, code.encode()?))
            });
            if let Some((slot, blob, text)) = sealed {
                let request = Request::Put {
                    slot,
                    blob,
                    ttl_secs,
                    one_time: true,
                };
                // 対応した協力ノードのすべてに預ける（相手は、どれか1つにつながっていれば引き取れる）
                let mut stored = false;
                meet::ask_nodes(&mut control, &nodes, &request, |response| {
                    stored |= response.status == Status::Ok;
                    false
                })
                .await;
                if stored {
                    ready.code = text;
                    ready.meet_slot = Some(slot);
                }
            }
            let _ = internal.send(Internal::InviteReady(ready));
        });
    }

    /// 番号で預けたものを、協力ノードから消す
    pub(super) fn delete_meets(&mut self, slots: Vec<[u8; 32]>) {
        if slots.is_empty() || self.meets.nodes.is_empty() {
            return;
        }
        let nodes = self.meets.nodes.clone();
        let mut control = self.control.clone();
        tokio::spawn(async move {
            for slot in slots {
                meet::ask_nodes(&mut control, &nodes, &Request::Delete { slot }, |_| false).await;
            }
        });
    }

    /// 使われた・取り消された招待の預けたものを、協力ノードから消す（残しても期限で消える）
    pub(super) fn release_meets(&mut self) {
        let slots = self.invites.take_released();
        self.delete_meets(slots);
    }

    /// この合言葉で引き取ってある招待（期限内のもの）
    pub(super) fn cached_meet(&self, code: &MeetCode) -> Option<&Fetched> {
        self.meets
            .fetched
            .as_ref()
            .filter(|f| f.secret == code.secret && f.inv.expires_at > unix_now())
    }

    /// この合言葉で引き取ってある招待を取り出す（接続に使う。1つの招待でつなげるのは1回）
    pub(super) fn take_cached_meet(&mut self, code: &MeetCode) -> Option<Fetched> {
        self.cached_meet(code)?;
        self.meets.fetched.take()
    }

    /// 合言葉の招待を協力ノードから引き取り始める（結果は Internal::MeetFetched で届く）
    pub(super) fn start_meet_fetch(&mut self, code: MeetCode, purpose: Purpose) -> Result<(), IpcError> {
        if !self.can_meet() {
            return Err(no_node());
        }
        if let Some(waiting) = self.meets.inflight.get_mut(&code.secret) {
            waiting.push(purpose);
            return Ok(());
        }
        self.meets.inflight.insert(code.secret, vec![purpose]);
        let nodes = self.meets.nodes.clone();
        let own_slots = self.invites.meet_slots();
        let mut control = self.control.clone();
        let internal = self.internal.clone();
        let network_id = self.network_id;
        tokio::spawn(async move {
            let keys = tokio::task::spawn_blocking(move || code.derive(&network_id))
                .await
                .ok()
                .flatten();
            let outcome = match keys {
                None => Outcome::NoAnswer,
                Some(keys) if own_slots.contains(&keys.slot) => Outcome::Own,
                Some(keys) => {
                    let mut found = None;
                    let mut answered = false;
                    let request = Request::Get { slot: keys.slot };
                    meet::ask_nodes(&mut control, &nodes, &request, |response| {
                        match response.status {
                            // 鍵を知らない協力ノードや第三者が作ったものは、復号の検査で捨てる
                            Status::Ok => {
                                answered = true;
                                found = protocol::open(&keys.key, &network_id, &response.blob);
                            }
                            Status::None => answered = true,
                            Status::Limit | Status::Refused | Status::Invalid => {}
                        }
                        found.is_some()
                    })
                    .await;
                    match (found, answered) {
                        (Some(body), _) => Outcome::Found(body),
                        (None, true) => Outcome::NotFound,
                        (None, false) => Outcome::NoAnswer,
                    }
                }
            };
            let _ = internal.send(Internal::MeetFetched(FetchResult {
                secret: code.secret,
                outcome,
            }));
        });
        Ok(())
    }

    /// 対局・UI側の meetPut: 対応した協力ノードのすべてに預け、預かった協力ノードの数を応答で返す
    pub(super) fn start_meet_put(&mut self, request_id: String, put: Request) {
        let nodes = self.meets.nodes.clone();
        let mut control = self.control.clone();
        let internal = self.internal.clone();
        tokio::spawn(async move {
            let mut stored: usize = 0;
            meet::ask_nodes(&mut control, &nodes, &put, |response| {
                if response.status == Status::Ok {
                    stored = stored.saturating_add(1);
                }
                false
            })
            .await;
            let _ = internal.send(Internal::MeetPutDone { request_id, stored });
        });
    }

    /// 対局・UI側の meetGet: 対応した協力ノードのすべてに尋ね、返ってきたものを応答で返す（復号は対局・UI側が行い、
    /// 鍵を知らない協力ノードが作ったものは、そこで捨てられる）
    pub(super) fn start_meet_get(&mut self, request_id: String, slot: [u8; 32]) {
        let nodes = self.meets.nodes.clone();
        let mut control = self.control.clone();
        let internal = self.internal.clone();
        tokio::spawn(async move {
            let mut blobs: Vec<Vec<u8>> = Vec::new();
            meet::ask_nodes(&mut control, &nodes, &Request::Get { slot }, |response| {
                if response.status == Status::Ok && !response.blob.is_empty() {
                    blobs.push(response.blob);
                }
                false
            })
            .await;
            let _ = internal.send(Internal::MeetGot { request_id, blobs });
        });
    }

    /// meetGet の応答を返す
    pub(super) fn on_meet_got(&mut self, request_id: &str, blobs: &[Vec<u8>]) {
        use base64::engine::general_purpose::STANDARD as B64;
        use base64::Engine;
        let encoded: Vec<String> = blobs.iter().map(|b| B64.encode(b)).collect();
        self.out.result(request_id, serde_json::json!({ "blobs": encoded }));
    }

    /// 引き取りの結果が届いた。待っていた要求のすべてに結果を返す
    pub(super) fn on_meet_fetched(&mut self, result: FetchResult) {
        let waiting = self.meets.inflight.remove(&result.secret).unwrap_or_default();
        let fetched = match result.outcome {
            Outcome::Found(body) => self.read_invite_body(&body).map(|(inv, peer, addrs)| Fetched {
                secret: result.secret,
                inv,
                peer,
                addrs,
            }),
            Outcome::NotFound => Err(IpcError::new(ErrorCode::InviteNotFound, "no invite for this passcode")),
            Outcome::NoAnswer => Err(no_node()),
            Outcome::Own => Err(IpcError::new(ErrorCode::InviteOwn, "own invite")),
        };
        let inspected = fetched
            .as_ref()
            .map(|f| self.inspect_result("meetCode", &f.peer.to_base58(), f.inv.expires_at, &f.addrs))
            .map_err(Clone::clone);
        // つなぎ始めるのは、待っていた接続の最初の1つだけ（1つの招待でつなげるのは1回）。ほかは unreachable で閉じる
        let mut connect: Option<(u64, bool)> = None;
        for purpose in waiting {
            match purpose {
                Purpose::Inspect { request_id } => match &inspected {
                    Ok(value) => self.out.result(&request_id, value.clone()),
                    Err(e) => self.out.error(&request_id, e),
                },
                Purpose::Connect { conn, allow_private } => {
                    if !self.conns.is_fetching(conn) {
                        // 引き取りを待つ間に、接続が取りやめられた（closeConnection）
                        continue;
                    }
                    if connect.is_none() && fetched.is_ok() {
                        connect = Some((conn, allow_private));
                    } else {
                        self.conns.fail(conn, ipc::CloseReason::Unreachable);
                    }
                }
            }
        }
        match (connect, fetched) {
            (Some((conn, allow_private)), Ok(f)) => {
                let start = self.invite_outbound(f.inv.token, f.peer, f.addrs, allow_private);
                self.proceed(conn, start);
            }
            // 協力ノードは、1回引き取られたら消す。続く connectInvite で使えるように持っておく
            (None, Ok(f)) => self.meets.fetched = Some(f),
            _ => {}
        }
    }
}
