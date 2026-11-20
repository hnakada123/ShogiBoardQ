//! 対局ストリーム（接続の番号・開く・閉じる・届いたフレームの中継・届いた接続の受け付け。protocol/ipc.md 3章・4章）
//!
//! 中継するのは外枠と頻度を確かめたフレームだけで、対局の規則（着手・状態・署名）は対局・UI側が確かめる。

use std::collections::HashMap;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex, MutexGuard};
use std::time::{Duration, Instant};

use futures::{AsyncReadExt, StreamExt};
use libp2p::swarm::DialError;
use libp2p::{Multiaddr, PeerId, Stream, StreamProtocol};
use sbq_protocol::frame::{self, FrameHeader, GAME_PROTOCOL};
use tokio::sync::{mpsc, oneshot};
use tokio::task::AbortHandle;

use super::outbound::{self, Links, Opening, Step};
use super::{unix_now, Node};
use crate::game::{self, ConnEvent, ConnShared};
use crate::ipc;

/// 対局ストリーム（開始前・対局中を合わせて）の上限
pub const MAX_GAME_CONNECTIONS: usize = 8;
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
    total: usize,
    per_peer: HashMap<PeerId, usize>,
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
        let mine = state.per_peer.get(peer).copied().unwrap_or(0);
        if state.total >= MAX_PENDING_STREAMS || mine >= MAX_PENDING_STREAMS_PER_PEER {
            return false;
        }
        state.total = state.total.saturating_add(1);
        state.per_peer.insert(*peer, mine.saturating_add(1));
        true
    }

    fn leave(&self, peer: &PeerId) {
        let mut state = self.lock();
        state.total = state.total.saturating_sub(1);
        let left = state.per_peer.get(peer).copied().unwrap_or(0).saturating_sub(1);
        if left == 0 {
            state.per_peer.remove(peer);
        } else {
            state.per_peer.insert(*peer, left);
        }
    }
}
/// 接続を開くまでの時間
const CONNECT_TIMEOUT: Duration = Duration::from_secs(30);

/// 主ループへの知らせ（接続を開くタスクから）
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
    /// 合言葉での接続を始められなかった（node/codes）
    CodeFailed {
        conn: u64,
        reason: ipc::CloseReason,
    },
    /// 協力ノードを使う合言葉の招待を引き取り終えた（node/meets）
    MeetFetched(super::meets::FetchResult),
}

/// 開いた接続の状態
pub enum ConnState {
    Connecting,
    Open {
        writer: mpsc::Sender<Vec<u8>>,
        shared: Arc<ConnShared>,
        /// 受信のタスク（送信のタスクは送る列が閉じれば終わる）
        reader: AbortHandle,
        route: ipc::Route,
    },
}

pub struct Conn {
    pub peer: PeerId,
    pub origin: ipc::Origin,
    pub direction: ipc::Direction,
    pub state: ConnState,
    /// 対局ストリームを開いた接続の経路（つなぐ側だけが決められる。届いた側は None で、相手との接続から決める）
    pub known_route: Option<ipc::Route>,
}

impl Drop for Conn {
    fn drop(&mut self) {
        if let ConnState::Open { reader, .. } = &self.state {
            // 送信のタスクは送る列が閉じれば閉じるが、受信のタスクは止める
            reader.abort();
        }
    }
}

impl Node {
    pub fn alloc_conn(&mut self) -> u64 {
        let id = self.next_conn;
        self.next_conn = self.next_conn.saturating_add(1);
        id
    }

    /// 対局ストリームの数（合言葉でつなぐ途中のものを含む）
    pub fn game_connection_count(&self) -> usize {
        self.conns
            .len()
            .saturating_add(self.code_connects.len())
            .saturating_add(self.meets.connects.len())
    }

    /// 相手との接続から見た経路（1つでも直接の接続があれば直接）。届いた対局ストリームは、どの接続の上にあるか
    /// 分からないので、これで決める
    pub fn route_of(&self, peer: &PeerId) -> ipc::Route {
        match self.peer_conns.get(peer) {
            Some(conns) if conns.values().any(|relayed| !relayed) => ipc::Route::Direct,
            Some(conns) if !conns.is_empty() => ipc::Route::Relay,
            _ => ipc::Route::Direct,
        }
    }

    /// 接続を開き始める（結果は Internal で届く）
    pub fn start_outbound(
        &mut self,
        conn: u64,
        peer: PeerId,
        addrs: Vec<Multiaddr>,
        token: Option<[u8; 32]>,
        origin: ipc::Origin,
    ) {
        let connected = self.swarm.is_connected(&peer);
        for a in &addrs {
            self.swarm.add_peer_address(peer, a.clone());
        }
        // つなぐのに使った宛先を覚える（対局中に切れたときの再接続に使う）
        self.remember_addrs(peer, &addrs);
        // 接続を始められなければ（宛先がない・上限）、開くのを待たずにやめる。相手へ接続している途中
        // （DialPeerConditionFalse）なら、その結果を待つ
        let dial_failed = !connected && {
            let opts = libp2p::swarm::dial_opts::DialOpts::peer_id(peer)
                .addresses(addrs.clone())
                .extend_addresses_through_behaviour()
                .build();
            matches!(self.swarm.dial(opts), Err(e) if !matches!(e, DialError::DialPeerConditionFalse(_)))
        };
        self.conns.insert(
            conn,
            Conn {
                peer,
                origin,
                direction: ipc::Direction::Outbound,
                state: ConnState::Connecting,
                known_route: None,
            },
        );
        // どの接続の上で開くかは主ループが決める（outbound）。決まるまで、開くタスクは待つ
        let (go, go_rx) = oneshot::channel();
        self.opening.insert(
            conn,
            Opening {
                peer,
                go,
                no_hole_punch: false,
            },
        );
        let mut control = self.control.clone();
        let internal = self.internal.clone();
        tokio::spawn(async move {
            let result = tokio::time::timeout(CONNECT_TIMEOUT, async {
                match go_rx.await {
                    Ok(true) => control
                        .open_stream(peer, StreamProtocol::new(GAME_PROTOCOL))
                        .await
                        .map_err(|_| ()),
                    _ => Err(()),
                }
            })
            .await;
            let event = match result {
                Ok(Ok(mut stream)) => {
                    let token_ok = match token {
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
        });
        if dial_failed {
            self.fail_opening(conn);
        } else {
            self.advance_opening(&peer);
        }
    }

    /// 相手との接続の様子（対局ストリームを開く前に見る）
    fn links_of(&self, peer: &PeerId, no_hole_punch: bool) -> Links {
        let now = Instant::now();
        let mut links = Links {
            direct: 0,
            relayed: 0,
            relayed_for: Duration::ZERO,
            hole_punching: self.hole_punching && !no_hole_punch,
        };
        for (id, relayed) in self.peer_conns.get(peer).into_iter().flatten() {
            if *relayed {
                links.relayed = links.relayed.saturating_add(1);
                // 時刻を覚えていない接続は、待ち終えたものとして扱う
                let age = self
                    .conn_since
                    .get(id)
                    .map_or(outbound::HOLE_PUNCH_WAIT, |since| now.saturating_duration_since(*since));
                links.relayed_for = links.relayed_for.max(age);
            } else {
                links.direct = links.direct.saturating_add(1);
            }
        }
        links
    }

    /// 相手への対局ストリームを開けるようになったか確かめ、開く（相手との接続が変わったとき・穴あけの結果が
    /// 分かったとき・時間が過ぎたときに呼ぶ）
    pub(super) fn advance_opening(&mut self, peer: &PeerId) {
        let waiting: Vec<(u64, bool)> = self
            .opening
            .iter()
            .filter(|(_, o)| &o.peer == peer)
            .map(|(conn, o)| (*conn, o.no_hole_punch))
            .collect();
        for (conn, no_hole_punch) in waiting {
            match outbound::next_step(self.links_of(peer, no_hole_punch)) {
                Step::Wait => {}
                Step::CloseRelayed => {
                    // 閉じ終わると ConnectionClosed が届き、もう一度ここへ来る
                    let relayed: Vec<_> = self
                        .peer_conns
                        .get(peer)
                        .into_iter()
                        .flatten()
                        .filter(|(_, relayed)| **relayed)
                        .map(|(id, _)| *id)
                        .collect();
                    for id in relayed {
                        self.swarm.close_connection(id);
                    }
                }
                Step::OpenDirect => self.release_opening(conn, ipc::Route::Direct),
                Step::OpenRelayed => self.release_opening(conn, ipc::Route::Relay),
            }
        }
    }

    pub(super) fn advance_all_openings(&mut self) {
        if self.opening.is_empty() {
            return;
        }
        let mut peers: Vec<PeerId> = self.opening.values().map(|o| o.peer).collect();
        peers.sort_unstable();
        peers.dedup();
        for peer in peers {
            self.advance_opening(&peer);
        }
    }

    /// 相手との穴あけをあきらめた（直接の接続を待たない）
    pub(super) fn give_up_hole_punching(&mut self, peer: &PeerId) {
        for opening in self.opening.values_mut().filter(|o| &o.peer == peer) {
            opening.no_hole_punch = true;
        }
        self.advance_opening(peer);
    }

    /// 相手への接続に失敗した・相手との接続がなくなった: 開くのを待っている対局ストリームをやめる
    pub(super) fn fail_openings_of(&mut self, peer: &PeerId) {
        let waiting: Vec<u64> = self
            .opening
            .iter()
            .filter(|(_, o)| &o.peer == peer)
            .map(|(conn, _)| *conn)
            .collect();
        for conn in waiting {
            self.fail_opening(conn);
        }
    }

    fn release_opening(&mut self, conn: u64, route: ipc::Route) {
        let Some(opening) = self.opening.remove(&conn) else {
            return;
        };
        if let Some(entry) = self.conns.get_mut(&conn) {
            entry.known_route = Some(route);
        }
        let _ = opening.go.send(true);
    }

    /// 開くタスクに、やめるよう知らせる（タスクが OutboundFailed を返し、connectionClosed を知らせる）
    fn fail_opening(&mut self, conn: u64) {
        if let Some(opening) = self.opening.remove(&conn) {
            let _ = opening.go.send(false);
        }
    }

    /// 中継経由の接続の上で対局ストリームを開いた（開いている途中の）相手か。その後にできた直接の接続は使えない
    pub(super) fn has_relayed_stream_to(&self, peer: &PeerId) -> bool {
        !self.opening.values().any(|o| &o.peer == peer)
            && self
                .conns
                .values()
                .any(|c| &c.peer == peer && c.known_route == Some(ipc::Route::Relay))
    }

    /// 開いたストリームの読み書きを始め、connectionOpened を知らせる
    pub fn open_conn(&mut self, conn: u64, stream: Stream) {
        let Some(entry) = self.conns.get(&conn) else { return };
        let peer = entry.peer;
        let known_route = entry.known_route;
        let (reader, writer) = stream.split();
        let shared = Arc::new(ConnShared::default());
        let (frame_tx, frame_rx) = mpsc::channel(64);
        let reader_task = tokio::spawn(game::reader_task(
            conn,
            reader,
            shared.clone(),
            self.conn_events.clone(),
        ));
        // 送信のタスクは、送る列（writer）が閉じれば（接続を消せば）終わる
        tokio::spawn(game::writer_task(conn, writer, frame_rx, self.conn_events.clone()));
        let route = known_route.unwrap_or_else(|| self.route_of(&peer));
        if let Some(entry) = self.conns.get_mut(&conn) {
            entry.state = ConnState::Open {
                writer: frame_tx,
                shared,
                reader: reader_task.abort_handle(),
                route,
            };
            self.out.notify(ipc::Notification::ConnectionOpened {
                connection: conn,
                peer_id: &peer.to_base58(),
                peer_id_bytes: &peer.to_bytes(),
                route,
                direction: entry.direction,
                origin: entry.origin,
            });
        }
    }

    /// 接続を閉じて connectionClosed を知らせる
    pub fn close_conn(&mut self, conn: u64, reason: ipc::CloseReason, notify: bool) {
        // 開くのを待っていたら、やめる（知らせる側を捨てると、開くタスクが終わる）
        self.opening.remove(&conn);
        let known =
            self.drop_code_connect(conn) | self.meets.connects.remove(&conn) | self.conns.remove(&conn).is_some();
        if known && notify {
            self.out.notify(ipc::Notification::ConnectionClosed {
                connection: conn,
                reason,
            });
        }
    }

    pub(super) fn on_conn_event(&mut self, event: ConnEvent) {
        match event {
            ConnEvent::Frame { conn, frame, shared } => {
                let len = frame.len();
                if self.conns.contains_key(&conn) {
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
            ConnEvent::Closed { conn, reason } => self.close_conn(conn, reason, true),
        }
    }

    pub(super) fn on_internal(&mut self, event: Internal) {
        match event {
            Internal::OutboundOpened { conn, stream } => {
                if matches!(self.conns.get(&conn).map(|c| &c.state), Some(ConnState::Connecting)) {
                    self.open_conn(conn, stream);
                }
            }
            Internal::OutboundFailed { conn, reason } => self.close_conn(conn, reason, true),
            Internal::Inbound {
                peer,
                stream,
                header,
                first,
            } => self.on_inbound(peer, stream, header, first),
            Internal::InviteReady(ready) => self.on_invite_ready(ready),
            Internal::CodeKey { conn, key } => self.on_code_key(conn, key),
            Internal::CodeFailed { conn, reason } => self.fail_code_connect(conn, reason),
            Internal::MeetFetched(result) => self.on_meet_fetched(result),
        }
    }

    /// 届いた対局ストリームを受け付けるか決める（protocol/README.md 2.1節・ipc.md 3章）
    fn on_inbound(&mut self, peer: PeerId, stream: Stream, header: FrameHeader, first: Vec<u8>) {
        if self.game_connection_count() >= MAX_GAME_CONNECTIONS {
            return;
        }
        let (origin, forward_first) = if let Some(game_peer) = self.game_peer {
            if peer != game_peer || header.frame_type == frame::TYPE_INVITE_TOKEN {
                return;
            }
            (ipc::Origin::Game, true)
        } else if header.frame_type == frame::TYPE_INVITE_TOKEN {
            let Some(token) = frame::invite_token(&header) else {
                return;
            };
            // 招待文字列のトークンか、合言葉の鍵とこの相手の PeerId から作ったトークン
            if !self
                .invites
                .redeem(&token, &self.local_peer.to_bytes(), &peer.to_bytes(), unix_now())
            {
                return;
            }
            // 使われた招待を協力ノードに預けていたら、消す
            self.release_meets();
            (ipc::Origin::Invite, false)
        } else {
            // 有効な招待のトークンを持たない相手のストリームは受け付けない
            return;
        };
        let conn = self.alloc_conn();
        self.conns.insert(
            conn,
            Conn {
                peer,
                origin,
                direction: ipc::Direction::Inbound,
                state: ConnState::Connecting,
                known_route: None,
            },
        );
        self.open_conn(conn, stream);
        if forward_first {
            let shared = match self.conns.get(&conn).map(|c| &c.state) {
                Some(ConnState::Open { shared, .. }) => shared.clone(),
                _ => return,
            };
            shared.observe_received(&header);
            // 2つ目からのフレーム（game::reader_task）と同じく、書き出すまで受信キューに数える
            let len = first.len();
            shared.pending.fetch_add(len, Ordering::SeqCst);
            self.out.notify_with_release(
                ipc::Notification::FrameReceived {
                    connection: conn,
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
