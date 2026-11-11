//! 対局・UI側から使うときの通信プロセス（IPC の要求を受けて Swarm を動かす）

use std::collections::{HashMap, HashSet};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use futures::StreamExt;
use libp2p::identity::Keypair;
use libp2p::swarm::ConnectionId;
use libp2p::{Multiaddr, PeerId, Stream, StreamProtocol, Swarm};
use sbq_protocol::frame::{self, FrameHeader, GAME_PROTOCOL};
use sbq_protocol::hash::hex;
use sbq_protocol::network::{self, NetworkSpec, Profile};
use serde_json::{json, Value};
use tokio::sync::mpsc;
use tokio::task::AbortHandle;

use crate::addr::{self, AddrPolicy};
use crate::game::{self, ConnEvent, ConnShared};
use crate::ipc::{self, Method, Out, Request};
use crate::sandbox::SandboxReport;
use crate::swarm::{self, Behaviour, SwarmOptions};

mod events;
mod requests;

/// 対局ストリーム（開始前・対局中を合わせて）の上限
pub const MAX_GAME_CONNECTIONS: usize = 8;
/// 最初のフレームを待つ時間
const FIRST_FRAME_TIMEOUT: Duration = Duration::from_secs(10);
/// 接続を開くまでの時間
const CONNECT_TIMEOUT: Duration = Duration::from_secs(30);
/// 宛先を覚えるピアの上限
const MAX_ADDR_BOOK_PEERS: usize = 64;
/// ローカルのネットワーク（同じ PC・同じ LAN）の待受のポート（TCP と UDP）。起動のたびに変わると、
/// 招待する側の PC のファイアウォールで許可するポートを決められない。使用中なら空いているポートで待ち受ける
pub const LAN_PORT: u16 = 48210;

/// 決まったポートの TCP の待受を、ほかのプロセスが使っていないか（ポートを決めていないアドレスと TCP 以外は true）。
/// libp2p の TCP の待受は、同じポートを複数のプロセスで共有できる設定（SO_REUSEPORT）で開くので、使用中でも開けて
/// しまい、届いた接続が2つのプロセスに振り分けられる。先に、共有しない待受を試して確かめる
fn tcp_port_available(addr: &Multiaddr) -> bool {
    let mut ip: Option<std::net::IpAddr> = None;
    for protocol in addr.iter() {
        match protocol {
            libp2p::multiaddr::Protocol::Ip4(v4) => ip = Some(v4.into()),
            libp2p::multiaddr::Protocol::Ip6(v6) => ip = Some(v6.into()),
            libp2p::multiaddr::Protocol::Tcp(port) if port != 0 => {
                return ip.is_some_and(|ip| std::net::TcpListener::bind((ip, port)).is_ok());
            }
            _ => {}
        }
    }
    true
}

/// 待受のアドレスの候補。外側の1件ごとに、内側を先頭から試して最初に開けたものを使う
pub fn listen_candidates(profile: Profile) -> Vec<Vec<Multiaddr>> {
    let groups: Vec<Vec<String>> = match profile {
        // 同じ PC・同じ LAN: LAN 側でも待ち受ける（受け付ける接続元は loopback と私設アドレスだけ。AddrPolicy）。
        // IPv6 では待ち受けない（家庭の LAN の IPv6 はインターネットから届くアドレスのことが多い）
        Profile::Local => vec![
            vec![format!("/ip4/0.0.0.0/tcp/{LAN_PORT}"), "/ip4/0.0.0.0/tcp/0".to_owned()],
            vec![
                format!("/ip4/0.0.0.0/udp/{LAN_PORT}/quic-v1"),
                "/ip4/0.0.0.0/udp/0/quic-v1".to_owned(),
            ],
        ],
        Profile::Internet => vec![
            vec!["/ip4/0.0.0.0/tcp/0".to_owned()],
            vec!["/ip4/0.0.0.0/udp/0/quic-v1".to_owned()],
            vec!["/ip6/::/tcp/0".to_owned()],
            vec!["/ip6/::/udp/0/quic-v1".to_owned()],
        ],
    };
    groups
        .into_iter()
        .map(|group| group.iter().filter_map(|a| a.parse().ok()).collect())
        .collect()
}

/// UNIX 秒
pub fn unix_now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

/// ビルドした時刻（build.rs）
fn build_unix() -> u64 {
    env!("SBQ_PEER_BUILD_UNIX").parse().unwrap_or(0)
}

/// ビルドの使用期限を過ぎたか
pub fn build_expired(now: u64) -> bool {
    let lifetime = network::BUILD_LIFETIME_DAYS.saturating_mul(86_400);
    now > build_unix().saturating_add(lifetime)
}

/// 主ループへの知らせ（接続を開くタスクから）
pub enum Internal {
    OutboundOpened {
        conn: u64,
        stream: Stream,
    },
    OutboundFailed {
        conn: u64,
        reason: &'static str,
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
        tasks: [AbortHandle; 2],
        route: &'static str,
    },
}

pub struct Conn {
    pub peer: PeerId,
    pub origin: &'static str,
    pub direction: &'static str,
    pub state: ConnState,
}

impl Drop for Conn {
    fn drop(&mut self) {
        if let ConnState::Open { tasks, .. } = &self.state {
            // 送信のタスクは送る列が閉じれば閉じるが、受信のタスクは止める
            if let Some(reader) = tasks.first() {
                reader.abort();
            }
        }
    }
}

/// Swarm を作る条件
pub struct NodeConfig {
    pub spec: NetworkSpec,
    pub keypair: Keypair,
    pub relay_only: bool,
    pub service: bool,
    pub relay_server: bool,
    /// 待受のアドレスの候補（1件ごとに、先頭から試して最初に開けたものを使う）
    pub listen: Vec<Vec<Multiaddr>>,
}

/// 作った招待
pub struct OutstandingInvite {
    pub token: [u8; 32],
    pub expires_at: u64,
}

pub struct Node {
    pub out: Out,
    pub swarm: Swarm<Behaviour>,
    pub local_peer: PeerId,
    pub spec: NetworkSpec,
    pub network_id: [u8; 32],
    pub policy: Arc<AddrPolicy>,
    pub relay_only: bool,
    /// 協力ノード（--service）
    pub service: bool,
    pub control: libp2p_stream::Control,
    pub conn_events: mpsc::UnboundedSender<ConnEvent>,
    pub internal: mpsc::UnboundedSender<Internal>,
    pub conns: HashMap<u64, Conn>,
    pub next_conn: u64,
    pub invites: Vec<OutstandingInvite>,
    pub game_peer: Option<PeerId>,
    pub peer_conns: HashMap<PeerId, HashMap<ConnectionId, bool>>,
    /// 受け付けた接続の接続元のアドレス帯
    pub inbound_prefixes: HashMap<ConnectionId, Vec<u8>>,
    pub addr_book: HashMap<PeerId, Vec<Multiaddr>>,
    /// 利用者が指定した協力ノードと、指定された宛先
    pub bootstrap: HashMap<PeerId, Multiaddr>,
    pub relay_requested: HashSet<PeerId>,
    pub relay_reservations: HashSet<PeerId>,
    pub listen_addrs: Vec<Multiaddr>,
    pub reachability: &'static str,
    pub status_dirty: bool,
    pub last_status: Option<Value>,
    pub last_second: Instant,
    pub last_service_log: Instant,
    pub shutdown_at: Option<Instant>,
    /// 主ループが受け取る、接続のタスクからの知らせ（start で作り、run_loop が取り出す）
    pub pending_streams: Option<(mpsc::UnboundedReceiver<ConnEvent>, mpsc::UnboundedReceiver<Internal>)>,
}

/// 標準入力を読み続けるタスク。終端なら終了し、違反なら終了コード 2 で終わる
async fn stdin_task(tx: mpsc::UnboundedSender<Request>) {
    let mut stdin = tokio::io::stdin();
    let mut generation: Option<String> = None;
    loop {
        let body = match ipc::read_message(&mut stdin).await {
            Ok(b) => b,
            Err(ipc::ReadError::Eof) => std::process::exit(0),
            Err(ipc::ReadError::Violation(reason)) => {
                eprintln!("shogiboardq-peer: IPC violation: {reason}");
                std::process::exit(2);
            }
        };
        let request = match ipc::parse_request(&body, generation.as_deref()) {
            Ok(r) => r,
            Err(reason) => {
                eprintln!("shogiboardq-peer: IPC violation: {reason}");
                std::process::exit(2);
            }
        };
        if generation.is_none() {
            if request.method != Method::Hello {
                eprintln!("shogiboardq-peer: IPC violation: the first request must be hello");
                std::process::exit(2);
            }
            generation = Some(request.generation.clone());
        }
        if tx.send(request).is_err() {
            return;
        }
    }
}

/// IPC で動かす（標準入力を読む前に権限の制限を掛け終えていること）
pub async fn run(report: SandboxReport) {
    let (out_tx, out_rx) = mpsc::unbounded_channel();
    tokio::spawn(ipc::writer_task(tokio::io::stdout(), out_rx));
    let out = Out::new(out_tx);
    let (req_tx, mut req_rx) = mpsc::unbounded_channel();
    tokio::spawn(stdin_task(req_tx));

    // hello
    let Some(hello) = req_rx.recv().await else { return };
    out.set_generation(&hello.generation);
    match ipc::params::<ipc::HelloParams>(&hello) {
        Ok(_) => out.result(
            &hello.request_id,
            json!({
                "peerVersion": env!("CARGO_PKG_VERSION"),
                "ipcVersion": ipc::IPC_VERSION,
                "sandbox": report.to_json(),
            }),
        ),
        Err(e) => out.error(&hello.request_id, "invalid_request", &e),
    }

    // start
    let mut node = loop {
        let Some(request) = req_rx.recv().await else { return };
        match request.method {
            Method::Start => match Node::start(&out, &request) {
                Ok(node) => break node,
                Err((code, message)) => out.error(&request.request_id, code, &message),
            },
            Method::Shutdown => {
                out.result(&request.request_id, json!({}));
                tokio::time::sleep(Duration::from_millis(200)).await;
                std::process::exit(0);
            }
            _ => out.error(&request.request_id, "invalid_state", "start has not been called"),
        }
    };
    node.run_loop(req_rx).await;
}

impl Node {
    fn start(out: &Out, request: &Request) -> Result<Node, (&'static str, String)> {
        let p: ipc::StartParams = ipc::params(request).map_err(|e| ("invalid_request", e))?;
        if !network::is_valid_name(&p.network_name) {
            return Err(("invalid_request", "invalid networkName".into()));
        }
        let spec = network::find(&p.network_name).ok_or(("network_not_allowed", "network not allowed".into()))?;
        if spec.expires_with_build && build_expired(unix_now()) {
            return Err(("build_expired", "this build has expired".into()));
        }
        if p.bootstrap.len() > 16 {
            return Err(("invalid_request", "too many addresses".into()));
        }
        let config = NodeConfig {
            spec,
            keypair: Keypair::generate_ed25519(),
            relay_only: p.relay_only,
            service: false,
            relay_server: false,
            listen: listen_candidates(spec.profile),
        };
        let mut node = Node::create(out, config).map_err(|e| ("internal", e))?;
        node.connect_nodes(&p.bootstrap);
        out.result(
            &request.request_id,
            json!({
                "peerId": node.local_peer.to_base58(),
                "peerIdBytes": B64.encode(node.local_peer.to_bytes()),
                "networkId": hex(&node.network_id),
                "profile": spec.profile.name(),
            }),
        );
        Ok(node)
    }

    /// Swarm を作り、待受を始める（対局・UI側から使うときと協力ノードで共通）
    pub fn create(out: &Out, config: NodeConfig) -> Result<Node, String> {
        let spec = config.spec;
        let network_id = network::network_id(spec.name);
        let policy = Arc::new(AddrPolicy::new(spec.profile, config.relay_only));
        let keypair = config.keypair;
        let local_peer = keypair.public().to_peer_id();
        let internet = spec.profile == Profile::Internet;
        let mut swarm = swarm::build(SwarmOptions {
            keypair,
            policy: policy.clone(),
            service: config.service,
            relay_server: config.relay_server,
            relay_only: config.relay_only,
            // 「直接接続を使わない」のときは、到達性の確認（AutoNAT）をしない。確認の依頼には自分の
            // アドレスの候補が入り、中継経由でつながった相手にも届きうる
            autonat: internet && !config.relay_only,
        })?;

        let mut listening = 0usize;
        for candidates in config.listen {
            // 候補の先頭（決まったポート）が使用中なら、次の候補（空いているポート）で待ち受ける
            if candidates
                .into_iter()
                .any(|addr| tcp_port_available(&addr) && swarm.listen_on(addr).is_ok())
            {
                listening = listening.saturating_add(1);
            }
        }
        if listening == 0 {
            return Err("could not listen".into());
        }

        let (conn_tx, conn_rx) = mpsc::unbounded_channel();
        let (internal_tx, internal_rx) = mpsc::unbounded_channel();
        let mut control = swarm.behaviour().game.new_control();
        if !config.service {
            // 協力ノードは対局ストリームを受け付けない
            let incoming = control
                .accept(StreamProtocol::new(GAME_PROTOCOL))
                .map_err(|e| e.to_string())?;
            tokio::spawn(accept_task(
                incoming,
                internal_tx.clone(),
                Arc::new(AtomicUsize::new(0)),
            ));
        }

        Ok(Node {
            out: out.clone(),
            swarm,
            local_peer,
            spec,
            network_id,
            policy,
            relay_only: config.relay_only,
            service: config.service,
            control,
            conn_events: conn_tx,
            internal: internal_tx,
            conns: HashMap::new(),
            next_conn: 1,
            invites: Vec::new(),
            game_peer: None,
            peer_conns: HashMap::new(),
            inbound_prefixes: HashMap::new(),
            addr_book: HashMap::new(),
            bootstrap: HashMap::new(),
            relay_requested: HashSet::new(),
            relay_reservations: HashSet::new(),
            listen_addrs: Vec::new(),
            reachability: "unknown",
            status_dirty: true,
            last_status: None,
            last_second: Instant::now(),
            last_service_log: Instant::now(),
            shutdown_at: None,
            pending_streams: Some((conn_rx, internal_rx)),
        })
    }

    /// 利用者が指定した協力ノードへ接続する（自分から接続を始める相手は、協力ノードと、招待・対局の相手だけ）
    pub fn connect_nodes(&mut self, bootstrap: &[String]) {
        for text in bootstrap {
            let Ok(full) = text.parse::<Multiaddr>() else { continue };
            let Some(peer) = addr::peer_id_of(&full) else { continue };
            if peer == self.local_peer {
                continue;
            }
            let base = addr::without_peer_id(&full);
            self.policy.allow_node(peer, &base);
            if !self.policy.usable(&base) {
                continue;
            }
            self.swarm.add_peer_address(peer, base.clone());
            self.bootstrap.insert(peer, base.clone());
            let opts = libp2p::swarm::dial_opts::DialOpts::peer_id(peer)
                .addresses(vec![base])
                .build();
            let _ = self.swarm.dial(opts);
        }
    }

    /// ピアの宛先を覚える（重複を除き、上限まで）
    pub fn remember_addrs(&mut self, peer: PeerId, addrs: &[Multiaddr]) {
        if addrs.is_empty() || (!self.addr_book.contains_key(&peer) && self.addr_book.len() >= MAX_ADDR_BOOK_PEERS) {
            return;
        }
        let known = self.addr_book.entry(peer).or_default();
        for a in addrs {
            if known.len() >= addr::MAX_ADDRS_PER_PEER {
                break;
            }
            if !known.contains(a) {
                known.push(a.clone());
            }
        }
    }

    pub fn alloc_conn(&mut self) -> u64 {
        let id = self.next_conn;
        self.next_conn = self.next_conn.saturating_add(1);
        id
    }

    pub fn game_connection_count(&self) -> usize {
        self.conns.len()
    }

    pub fn route_of(&self, peer: &PeerId) -> &'static str {
        match self.peer_conns.get(peer) {
            Some(conns) if conns.values().any(|relayed| !relayed) => "direct",
            Some(conns) if !conns.is_empty() => "relay",
            _ => "direct",
        }
    }

    /// 接続を開き始める（結果は Internal で届く）
    pub fn start_outbound(
        &mut self,
        conn: u64,
        peer: PeerId,
        addrs: Vec<Multiaddr>,
        token: Option<[u8; 32]>,
        origin: &'static str,
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
                direction: "outbound",
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
                            reason: "unreachable",
                        }
                    }
                }
                Ok(Err(_)) => Internal::OutboundFailed {
                    conn,
                    reason: "unreachable",
                },
                Err(_) => Internal::OutboundFailed {
                    conn,
                    reason: "timeout",
                },
            };
            let _ = internal.send(event);
        });
    }

    /// 開いたストリームの読み書きを始め、connectionOpened を知らせる
    pub fn open_conn(&mut self, conn: u64, stream: Stream) {
        let Some(entry) = self.conns.get(&conn) else { return };
        let peer = entry.peer;
        let (reader, writer) = game::split(stream);
        let shared = Arc::new(ConnShared::default());
        let (frame_tx, frame_rx) = mpsc::channel(64);
        let reader_task = tokio::spawn(game::reader_task(
            conn,
            reader,
            shared.clone(),
            self.conn_events.clone(),
        ));
        let writer_task = tokio::spawn(game::writer_task(conn, writer, frame_rx, self.conn_events.clone()));
        let route = self.route_of(&peer);
        if let Some(entry) = self.conns.get_mut(&conn) {
            entry.state = ConnState::Open {
                writer: frame_tx,
                shared,
                tasks: [reader_task.abort_handle(), writer_task.abort_handle()],
                route,
            };
            self.out.notify(
                "connectionOpened",
                json!({
                    "connection": conn.to_string(),
                    "peerId": peer.to_base58(),
                    "peerIdBytes": B64.encode(peer.to_bytes()),
                    "route": route,
                    "direction": entry.direction,
                    "origin": entry.origin,
                }),
            );
        }
    }

    /// 接続を閉じて connectionClosed を知らせる
    pub fn close_conn(&mut self, conn: u64, reason: &str, notify: bool) {
        if self.conns.remove(&conn).is_some() && notify {
            self.out.notify(
                "connectionClosed",
                json!({ "connection": conn.to_string(), "reason": reason }),
            );
        }
    }

    pub async fn run_loop(&mut self, mut requests: mpsc::UnboundedReceiver<Request>) {
        let Some((mut conn_events, mut internal)) = self.pending_streams.take() else {
            return;
        };
        let mut tick = tokio::time::interval(Duration::from_millis(250));
        loop {
            tokio::select! {
                event = self.swarm.select_next_some() => self.on_swarm_event(event),
                request = requests.recv() => match request {
                    Some(r) => self.on_request(r),
                    None => std::process::exit(0),
                },
                Some(event) = conn_events.recv() => self.on_conn_event(event),
                Some(event) = internal.recv() => self.on_internal(event),
                _ = tick.tick() => self.on_tick(),
            }
        }
    }

    fn on_conn_event(&mut self, event: ConnEvent) {
        match event {
            ConnEvent::Frame { conn, frame, shared } => {
                let len = frame.len();
                if self.conns.contains_key(&conn) {
                    self.out.notify_with_release(
                        "frameReceived",
                        json!({ "connection": conn.to_string(), "frame": B64.encode(&frame) }),
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

    fn on_internal(&mut self, event: Internal) {
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
            ("game", true)
        } else if header.frame_type == frame::TYPE_INVITE_TOKEN {
            let Some(token) = frame::invite_token(&header) else {
                return;
            };
            let now = unix_now();
            let mut matched: Option<usize> = None;
            for (i, invite) in self.invites.iter().enumerate() {
                // すべての招待と、処理時間が一致した長さで変わらない比較で照合する
                if constant_time_eq(&invite.token, &token) && invite.expires_at > now {
                    matched = Some(i);
                }
            }
            let Some(i) = matched else { return };
            if i < self.invites.len() {
                self.invites.remove(i);
            }
            ("invite", false)
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
                direction: "inbound",
                state: ConnState::Connecting,
            },
        );
        self.open_conn(conn, stream);
        if forward_first {
            let shared = match self.conns.get(&conn).map(|c| &c.state) {
                Some(ConnState::Open { shared, .. }) => shared.clone(),
                _ => return,
            };
            if header.frame_type == frame::TYPE_START_SIGNATURE {
                shared.received_start_signature.store(true, Ordering::SeqCst);
            }
            self.out.notify(
                "frameReceived",
                json!({ "connection": conn.to_string(), "frame": B64.encode(&first) }),
            );
        }
    }

    fn on_tick(&mut self) {
        if let Some(at) = self.shutdown_at {
            if Instant::now() >= at {
                std::process::exit(0);
            }
        }
        if self.last_second.elapsed() < Duration::from_secs(1) {
            return;
        }
        self.last_second = Instant::now();
        let now = unix_now();
        self.invites.retain(|i| i.expires_at > now);
        self.emit_status();
        if self.service && self.last_service_log.elapsed() >= Duration::from_secs(60) {
            self.last_service_log = Instant::now();
            // 接続元の IP は書かない（数だけ）
            eprintln!("shogiboardq-peer: peers={}", self.peer_conns.len());
        }
    }

    pub fn emit_status(&mut self) {
        let connected = self.peer_conns.len();
        let state = if connected > 0 || (self.bootstrap.is_empty() && !self.listen_addrs.is_empty()) {
            "online"
        } else if !self.bootstrap.is_empty() {
            "connecting"
        } else {
            "offline"
        };
        let status = json!({
            "state": state,
            "connectedPeers": connected,
            "reachability": self.reachability,
            "relayReservations": self.relay_reservations.len(),
        });
        if self.last_status.as_ref() != Some(&status) {
            self.out.notify("networkStatus", status.clone());
            self.last_status = Some(status);
        }
        self.status_dirty = false;
    }
}

/// 長さが一致した値どうしで、処理時間が中身によらない比較
pub fn constant_time_eq(a: &[u8; 32], b: &[u8; 32]) -> bool {
    let mut diff = 0u8;
    for (x, y) in a.iter().zip(b.iter()) {
        diff |= x ^ y;
    }
    diff == 0
}

/// 届いた対局ストリームの最初のフレームを読み、主ループへ渡す
async fn accept_task(
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

#[cfg(test)]
#[allow(clippy::unwrap_used)]
mod tests {
    use super::*;

    /// ローカルのネットワークは、TCP と UDP のそれぞれで、決まったポートを先に試し、使用中なら空いているポートを使う。
    /// IPv6 では待ち受けない
    #[test]
    fn local_listens_on_the_fixed_port_first() {
        let groups = listen_candidates(Profile::Local);
        let text: Vec<Vec<String>> = groups
            .iter()
            .map(|g| g.iter().map(ToString::to_string).collect())
            .collect();
        assert_eq!(
            text,
            vec![
                vec!["/ip4/0.0.0.0/tcp/48210".to_owned(), "/ip4/0.0.0.0/tcp/0".to_owned()],
                vec![
                    "/ip4/0.0.0.0/udp/48210/quic-v1".to_owned(),
                    "/ip4/0.0.0.0/udp/0/quic-v1".to_owned()
                ],
            ]
        );
        // インターネットの profile は、空いているポートで IPv4 と IPv6 の両方
        let internet = listen_candidates(Profile::Internet);
        assert_eq!(internet.len(), 4);
        assert!(internet.iter().all(|g| g.len() == 1));
    }

    /// ほかのプロセスが待ち受けている TCP のポートは、使えないと判定する（同じポートを共有しない）
    #[test]
    fn busy_tcp_port_is_not_shared() {
        let holder = std::net::TcpListener::bind(("127.0.0.1", 0)).unwrap();
        let port = holder.local_addr().unwrap().port();
        let busy: Multiaddr = format!("/ip4/127.0.0.1/tcp/{port}").parse().unwrap();
        assert!(!tcp_port_available(&busy));
        drop(holder);
        assert!(tcp_port_available(&busy));
        // ポートを決めていないアドレスと、TCP でないアドレスは確かめない
        assert!(tcp_port_available(&"/ip4/0.0.0.0/tcp/0".parse().unwrap()));
        assert!(tcp_port_available(
            &format!("/ip4/127.0.0.1/udp/{port}/quic-v1").parse().unwrap()
        ));
    }
}
