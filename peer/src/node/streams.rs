//! 対局ストリーム（接続の番号・開く・閉じる・届いたフレームの中継・届いた接続の受け付け。protocol/ipc.md 3章・4章）
//!
//! 中継するのは外枠と頻度を確かめたフレームだけで、対局の規則（着手・状態・署名）は対局・UI側が確かめる。

use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::Duration;

use futures::{AsyncReadExt, StreamExt};
use libp2p::{Multiaddr, PeerId, Stream, StreamProtocol};
use sbq_protocol::frame::{self, FrameHeader, GAME_PROTOCOL};
use tokio::sync::mpsc;
use tokio::task::AbortHandle;

use super::{unix_now, Node};
use crate::game::{self, ConnEvent, ConnShared};
use crate::ipc;

/// 対局ストリーム（開始前・対局中を合わせて）の上限
pub const MAX_GAME_CONNECTIONS: usize = 8;
/// 最初のフレームを待つ時間
const FIRST_FRAME_TIMEOUT: Duration = Duration::from_secs(10);
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

    pub fn game_connection_count(&self) -> usize {
        self.conns.len()
    }

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
        if !connected {
            let opts = libp2p::swarm::dial_opts::DialOpts::peer_id(peer)
                .addresses(addrs.clone())
                .extend_addresses_through_behaviour()
                .build();
            let _ = self.swarm.dial(opts);
        }
        self.conns.insert(
            conn,
            Conn {
                peer,
                origin,
                direction: ipc::Direction::Outbound,
                state: ConnState::Connecting,
            },
        );
        let mut control = self.control.clone();
        let internal = self.internal.clone();
        tokio::spawn(async move {
            let result = tokio::time::timeout(
                CONNECT_TIMEOUT,
                control.open_stream(peer, StreamProtocol::new(GAME_PROTOCOL)),
            )
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
    }

    /// 開いたストリームの読み書きを始め、connectionOpened を知らせる
    pub fn open_conn(&mut self, conn: u64, stream: Stream) {
        let Some(entry) = self.conns.get(&conn) else { return };
        let peer = entry.peer;
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
        let route = self.route_of(&peer);
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
        if self.conns.remove(&conn).is_some() && notify {
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
            if !self.invites.redeem(&token, unix_now()) {
                return;
            }
            (ipc::Origin::Invite, false)
        } else {
            // 有効な招待の合言葉を持たない相手のストリームは受け付けない
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
    pending: Arc<AtomicUsize>,
) {
    while let Some((peer, mut stream)) = incoming.next().await {
        if pending.load(Ordering::SeqCst) >= MAX_GAME_CONNECTIONS {
            continue;
        }
        pending.fetch_add(1, Ordering::SeqCst);
        let internal = internal.clone();
        let pending = pending.clone();
        tokio::spawn(async move {
            let first = tokio::time::timeout(FIRST_FRAME_TIMEOUT, game::read_frame(&mut stream)).await;
            pending.fetch_sub(1, Ordering::SeqCst);
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
