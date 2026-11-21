//! 対局ストリーム（開く・閉じる・届いたフレームの中継・届いた接続の受け付け。protocol/ipc.md 3章・4章）
//!
//! 接続の番号と段階は node/conns が持ち、ここは Swarm・タスク・知らせとの間をつなぐ。中継するのは外枠と頻度を
//! 確かめたフレームだけで、対局の規則（着手・状態・署名）は対局・UI側が確かめる。

use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex, MutexGuard};
use std::time::{Duration, Instant};

use futures::{AsyncReadExt, StreamExt};
use libp2p::swarm::DialError;
use libp2p::{Multiaddr, PeerId, Stream, StreamProtocol};
use sbq_protocol::frame::{self, FrameHeader, GAME_PROTOCOL};
use tokio::sync::mpsc;

use super::conns::{ConnIo, Opened, Outbound, Release};
use super::outbound::{self, Step};
use super::{unix_now, Node};
use crate::game::{self, ConnEvent, ConnShared};
use crate::ipc;
use crate::limits::PeerSlots;

/// 最初のフレームを待つ時間（つなぐ側は開いた直後に送る）
const FIRST_FRAME_TIMEOUT: Duration = Duration::from_secs(5);
/// 最初のフレームを待つ届いたストリームの上限（全体）。接続は同じアドレス帯から8本までなので、1つのアドレス帯で
/// 使い切れない数にする
const MAX_PENDING_STREAMS: usize = 16;
/// 最初のフレームを待つ届いたストリームの上限（相手ごと）
const MAX_PENDING_STREAMS_PER_PEER: usize = 1;

/// 届いた対局ストリームのうち、最初のフレームを待つものの数と、対局中の相手（受け付けのタスクと主ループで共有する）。
///
/// 招待のトークンと相手の確認は最初のフレームの後なので、最初のフレームを送らない相手に待ちの枠を使い切られると、
/// 招待を持つ相手や対局相手のストリームを捨てることになる（監査 2026-11-13 P6）。待ちの枠は相手ごとに数え、対局中は
/// 相手以外のストリームを読まずに閉じる
#[derive(Default)]
pub struct AcceptGate {
    state: Mutex<GateState>,
}

#[derive(Default)]
struct GateState {
    waiting: PeerSlots,
    game_peer: Option<PeerId>,
}

impl AcceptGate {
    fn lock(&self) -> MutexGuard<'_, GateState> {
        match self.state.lock() {
            Ok(g) => g,
            Err(poisoned) => poisoned.into_inner(),
        }
    }

    /// 対局中の相手（None で対局していない）
    pub fn set_game_peer(&self, peer: Option<PeerId>) {
        self.lock().game_peer = peer;
    }

    /// 最初のフレームを待ってよいか（よければ数に入れる）
    fn enter(&self, peer: &PeerId) -> bool {
        let mut state = self.lock();
        if state.game_peer.is_some_and(|game| &game != peer) {
            return false;
        }
        state
            .waiting
            .enter(peer, MAX_PENDING_STREAMS, MAX_PENDING_STREAMS_PER_PEER)
    }

    fn leave(&self, peer: &PeerId) {
        self.lock().waiting.leave(peer);
    }
}

/// 主ループへの知らせ（接続を開くタスク・鍵を作るタスク・協力ノードとやり取りするタスクから）
pub enum Internal {
    OutboundOpened {
        conn: u64,
        stream: Stream,
    },
    OutboundFailed {
        conn: u64,
        reason: ipc::CloseReason,
    },
    Inbound {
        peer: PeerId,
        stream: Stream,
        header: FrameHeader,
        first: Vec<u8>,
    },
    /// 招待に付ける合言葉の鍵を作り終えた（createInvite の応答を返す。node/requests）
    InviteReady(super::requests::ReadyInvite),
    /// 合言葉でつなぐ側の鍵を作り終えた（作れなければ None。node/codes）
    CodeKey {
        conn: u64,
        key: Option<[u8; 32]>,
    },
    /// 協力ノードを使う合言葉の招待を引き取り終えた（node/meets）
    MeetFetched(super::meets::FetchResult),
    /// meetPut を協力ノードに送り終えた（stored は、預かった協力ノードの数）
    MeetPutDone {
        request_id: String,
        stored: usize,
    },
    /// meetGet で協力ノードに尋ね終えた（blobs は、協力ノードが返したもの）
    MeetGot {
        request_id: String,
        blobs: Vec<Vec<u8>>,
    },
}

/// 開いたストリームの読み書きを始める（受信のタスクと送信のタスク）
fn spawn_io(conn: u64, stream: Stream, events: &mpsc::UnboundedSender<ConnEvent>) -> ConnIo {
    let (reader, writer) = stream.split();
    let shared = Arc::new(ConnShared::default());
    let (frame_tx, frame_rx) = mpsc::channel(64);
    let reader_task = tokio::spawn(game::reader_task(conn, reader, shared.clone(), events.clone()));
    // 送信のタスクは、送る列（writer）が閉じれば（接続を消せば）終わる
    tokio::spawn(game::writer_task(conn, writer, frame_rx, events.clone()));
    ConnIo::new(frame_tx, shared, reader_task.abort_handle())
}

/// 対局ストリームを開き、招待のトークンを送る（結果は Internal で届く）
async fn open_task(
    mut control: libp2p_stream::Control,
    internal: mpsc::UnboundedSender<Internal>,
    conn: u64,
    release: Release,
) {
    let deadline = tokio::time::Instant::from_std(release.deadline);
    let opened = tokio::time::timeout_at(
        deadline,
        control.open_stream(release.peer, StreamProtocol::new(GAME_PROTOCOL)),
    )
    .await;
    let event = match opened {
        Ok(Ok(mut stream)) => {
            let token_ok = match release.token {
                Some(t) => game::write_frame(&mut stream, &frame::invite_token_frame(&t))
                    .await
                    .is_ok(),
                None => true,
            };
            if token_ok {
                Internal::OutboundOpened { conn, stream }
            } else {
                Internal::OutboundFailed {
                    conn,
                    reason: ipc::CloseReason::Unreachable,
                }
            }
        }
        Ok(Err(_)) => Internal::OutboundFailed {
            conn,
            reason: ipc::CloseReason::Unreachable,
        },
        Err(_) => Internal::OutboundFailed {
            conn,
            reason: ipc::CloseReason::Timeout,
        },
    };
    let _ = internal.send(event);
}

impl Node {
    /// 相手へ対局ストリームを開く準備をする: 宛先を覚え、つながっていなければ接続を始める。接続を始められなければ
    /// （宛先がない・上限）、開くのを待たずに unreachable。相手へ接続している途中（DialPeerConditionFalse）なら、
    /// その結果を待つ
    pub(super) fn begin_outbound(
        &mut self,
        peer: PeerId,
        addrs: Vec<Multiaddr>,
        token: Option<[u8; 32]>,
        origin: ipc::Origin,
    ) -> Result<Outbound, ipc::CloseReason> {
        let connected = self.swarm.is_connected(&peer);
        for a in &addrs {
            self.swarm.add_peer_address(peer, a.clone());
        }
        // つなぐのに使った宛先を覚える（対局中に切れたときの再接続に使う）
        self.remember_addrs(peer, &addrs);
        if !connected {
            let opts = libp2p::swarm::dial_opts::DialOpts::peer_id(peer)
                .addresses(addrs)
                .extend_addresses_through_behaviour()
                .build();
            if let Err(e) = self.swarm.dial(opts) {
                if !matches!(e, DialError::DialPeerConditionFalse(_)) {
                    return Err(ipc::CloseReason::Unreachable);
                }
            }
        }
        Ok(Outbound { peer, origin, token })
    }

    /// 新しい番号で、相手へ対局ストリームを開き始める。始められなかったら、応答の後で connectionClosed を知らせる
    pub(super) fn connect_new(&mut self, start: Result<Outbound, ipc::CloseReason>) -> u64 {
        let peer = start.as_ref().ok().map(|o| o.peer);
        let conn = self.conns.connect(start, Instant::now());
        if let Some(peer) = peer {
            self.advance_opening(&peer);
        }
        conn
    }

    /// 引き取り・合言葉の宛先へのダイヤルを終えた接続で、相手へ対局ストリームを開き始める
    pub(super) fn proceed(&mut self, conn: u64, start: Result<Outbound, ipc::CloseReason>) {
        let peer = start.as_ref().ok().map(|o| o.peer);
        if self.conns.proceed(conn, start, Instant::now()) {
            if let Some(peer) = peer {
                self.advance_opening(&peer);
            }
        }
    }

    /// 相手への対局ストリームを開けるようになったか確かめ、開く（相手との接続が変わったとき・穴あけの結果が
    /// 分かったとき・時間が過ぎたときに呼ぶ）
    pub(super) fn advance_opening(&mut self, peer: &PeerId) {
        for (conn, no_hole_punch) in self.conns.waiting_for(peer) {
            let links = self
                .links
                .summary(peer, self.role.uses_direct() && !no_hole_punch, Instant::now());
            match outbound::next_step(links) {
                Step::Wait => {}
                Step::CloseRelayed => {
                    // 閉じ終わると ConnectionClosed が届き、もう一度ここへ来る
                    for id in self.links.relayed_ids(peer) {
                        self.swarm.close_connection(id);
                    }
                }
                Step::OpenDirect => self.release_opening(conn, ipc::Route::Direct),
                Step::OpenRelayed => self.release_opening(conn, ipc::Route::Relay),
            }
        }
    }

    pub(super) fn advance_all_openings(&mut self) {
        for peer in self.conns.waiting_peers() {
            self.advance_opening(&peer);
        }
    }

    /// 相手との穴あけをあきらめた（直接の接続を待たない）
    pub(super) fn give_up_hole_punching(&mut self, peer: &PeerId) {
        self.conns.give_up_hole_punching(peer);
        self.advance_opening(peer);
    }

    /// 相手への接続に失敗した・相手との接続がなくなった: 開くのを待っている対局ストリームをやめる
    pub(super) fn fail_openings_of(&mut self, peer: &PeerId) {
        self.conns.fail_waiting(peer, ipc::CloseReason::Unreachable);
    }

    /// 開く接続が決まった。ストリームを開くタスクを始める
    fn release_opening(&mut self, conn: u64, route: ipc::Route) {
        if let Some(release) = self.conns.release(conn, route) {
            tokio::spawn(open_task(self.control.clone(), self.internal.clone(), conn, release));
        }
    }

    fn notify_opened(&self, opened: &Opened) {
        self.out.notify(ipc::Notification::ConnectionOpened {
            connection: opened.conn,
            peer_id: &opened.peer.to_base58(),
            peer_id_bytes: &opened.peer.to_bytes(),
            route: opened.route,
            direction: opened.direction,
            origin: opened.origin,
            presence: opened.presence,
        });
    }

    /// 開いたストリームの読み書きを始め、connectionOpened を知らせる（つないだ側）
    fn open_outbound(&mut self, conn: u64, stream: Stream) {
        if !self.conns.is_opening(conn) {
            // 開いている間に閉じられた接続のストリームは、捨てる（捨てると閉じる）
            return;
        }
        let io = spawn_io(conn, stream, &self.conn_events);
        if let Some(opened) = self.conns.open(conn, io) {
            self.notify_opened(&opened);
        }
    }

    /// 閉じた接続の connectionClosed を知らせる（主ループが、出来事を1つ処理するたびに呼ぶ。つなぎ始められなかった
    /// 接続の知らせは、番号を返す応答の後になる）
    pub(super) fn report_closed(&mut self) {
        for closed in self.conns.take_closed() {
            self.out.notify(ipc::Notification::ConnectionClosed {
                connection: closed.conn,
                reason: closed.reason,
            });
        }
    }

    pub(super) fn on_conn_event(&mut self, event: ConnEvent) {
        match event {
            ConnEvent::Frame { conn, frame, shared } => {
                let len = frame.len();
                if self.conns.is_open(conn) {
                    self.out.notify_with_release(
                        ipc::Notification::FrameReceived {
                            connection: conn,
                            frame: &frame,
                        },
                        Some(ipc::Release {
                            counter: shared.pending.clone(),
                            bytes: len,
                        }),
                    );
                } else {
                    shared.pending.fetch_sub(len, Ordering::SeqCst);
                }
            }
            ConnEvent::Closed { conn, reason } => self.conns.fail(conn, reason),
        }
    }

    pub(super) fn on_internal(&mut self, event: Internal) {
        match event {
            Internal::OutboundOpened { conn, stream } => self.open_outbound(conn, stream),
            Internal::OutboundFailed { conn, reason } => self.conns.fail(conn, reason),
            Internal::Inbound {
                peer,
                stream,
                header,
                first,
            } => self.on_inbound(peer, stream, header, first),
            Internal::InviteReady(ready) => self.on_invite_ready(ready),
            Internal::CodeKey { conn, key } => self.on_code_key(conn, key),
            Internal::MeetFetched(result) => self.on_meet_fetched(result),
            Internal::MeetPutDone { request_id, stored } => {
                self.out.result(&request_id, serde_json::json!({ "stored": stored }));
            }
            Internal::MeetGot { request_id, blobs } => self.on_meet_got(&request_id, &blobs),
        }
    }

    /// 届いた対局ストリームを受け付けるか決める（protocol/README.md 2.1節・ipc.md 3章）
    fn on_inbound(&mut self, peer: PeerId, stream: Stream, header: FrameHeader, first: Vec<u8>) {
        if !self.conns.has_room() {
            return;
        }
        let mut presence = 0;
        let (origin, forward_first) = if let Some(game_peer) = self.game_peer {
            if peer != game_peer || header.frame_type == frame::TYPE_INVITE_TOKEN {
                return;
            }
            (ipc::Origin::Game, true)
        } else if header.frame_type == frame::TYPE_INVITE_TOKEN {
            let Some(token) = frame::invite_token(&header) else {
                return;
            };
            let now = unix_now();
            if let Some(id) = self.presences.find(&token, now) {
                // 在席の知らせのトークン（友だちの申し込み）。使い終えない
                presence = id;
                (ipc::Origin::Friend, false)
            } else if self
                .invites
                .redeem(&token, &self.local_peer.to_bytes(), &peer.to_bytes(), now)
            {
                // 招待文字列のトークンか、合言葉の鍵とこの相手の PeerId から作ったトークン。
                // 使われた招待を協力ノードに預けていたら、消す
                self.release_meets();
                (ipc::Origin::Invite, false)
            } else {
                return;
            }
        } else {
            // 有効な招待のトークンを持たない相手のストリームは受け付けない
            return;
        };
        let route = self.links.route_of(&peer);
        let events = &self.conn_events;
        let opened = self
            .conns
            .accept(peer, origin, presence, route, |conn| spawn_io(conn, stream, events));
        self.notify_opened(&opened);
        if forward_first {
            let Ok(io) = self.conns.io(opened.conn) else {
                return;
            };
            let shared = io.shared.clone();
            shared.observe_received(&header);
            // 2つ目からのフレーム（game::reader_task）と同じく、書き出すまで受信キューに数える
            let len = first.len();
            shared.pending.fetch_add(len, Ordering::SeqCst);
            self.out.notify_with_release(
                ipc::Notification::FrameReceived {
                    connection: opened.conn,
                    frame: &first,
                },
                Some(ipc::Release {
                    counter: shared.pending.clone(),
                    bytes: len,
                }),
            );
        }
    }
}

/// 届いた対局ストリームの最初のフレームを読み、主ループへ渡す
pub(super) async fn accept_task(
    mut incoming: libp2p_stream::IncomingStreams,
    internal: mpsc::UnboundedSender<Internal>,
    gate: Arc<AcceptGate>,
) {
    while let Some((peer, mut stream)) = incoming.next().await {
        // 待ちの枠がない・対局中の相手でないストリームは、読まずに閉じる（捨てると閉じる）
        if !gate.enter(&peer) {
            continue;
        }
        let internal = internal.clone();
        let gate = gate.clone();
        tokio::spawn(async move {
            let first = tokio::time::timeout(FIRST_FRAME_TIMEOUT, game::read_frame(&mut stream)).await;
            gate.leave(&peer);
            if let Ok(Ok((bytes, header))) = first {
                let _ = internal.send(Internal::Inbound {
                    peer,
                    stream,
                    header,
                    first: bytes,
                });
            }
        });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// 待ちの枠は相手ごとに1件。対局中は相手以外を受け付けない（監査 2026-11-13 P6）
    #[test]
    fn accept_gate_limits_per_peer_and_keeps_the_game_peer() {
        let gate = AcceptGate::default();
        let attacker = PeerId::random();
        let friend = PeerId::random();
        assert!(gate.enter(&attacker));
        assert!(!gate.enter(&attacker));
        assert!(gate.enter(&friend));
        gate.leave(&attacker);
        assert!(gate.enter(&attacker));
        // 全体の上限
        let others: Vec<PeerId> = (0..MAX_PENDING_STREAMS).map(|_| PeerId::random()).collect();
        let admitted = others.iter().filter(|p| gate.enter(p)).count();
        assert_eq!(admitted, MAX_PENDING_STREAMS.saturating_sub(2));
        // 対局中は相手だけ
        let fresh = AcceptGate::default();
        fresh.set_game_peer(Some(friend));
        assert!(!fresh.enter(&attacker));
        assert!(fresh.enter(&friend));
        fresh.set_game_peer(None);
        assert!(fresh.enter(&attacker));
    }
}
