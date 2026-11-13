//! 対局・UI側から使うときの通信プロセス（IPC の要求を受けて Swarm を動かす）
//!
//! 主ループと Swarm の作成はここに置き、役割ごとに分ける: 要求の処理（requests）、Swarm の出来事（events）、
//! 対局ストリーム（streams）、作った招待（invites）、中継の予約（reservations）。待受は crate::listen

use std::collections::HashMap;
use std::sync::Arc;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use futures::StreamExt;
use libp2p::identity::Keypair;
use libp2p::swarm::ConnectionId;
use libp2p::{Multiaddr, PeerId, StreamProtocol, Swarm};
use sbq_protocol::frame::GAME_PROTOCOL;
use sbq_protocol::hash::hex;
use sbq_protocol::network::{self, NetworkSpec, Profile};
use serde_json::json;
use tokio::sync::mpsc;

use crate::addr::{self, AddrPolicy};
use crate::game::ConnEvent;
use crate::ipc::{self, Method, Out, Request};
use crate::listen;
use crate::sandbox::SandboxReport;
use crate::swarm::{self, Behaviour, SwarmOptions};

mod events;
mod invites;
mod requests;
mod reservations;
mod streams;

pub use streams::{Conn, ConnState, Internal, MAX_GAME_CONNECTIONS};

/// 宛先を覚えるピアの上限
const MAX_ADDR_BOOK_PEERS: usize = 64;

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

pub struct Node {
    pub out: Out,
    pub swarm: Swarm<Behaviour>,
    pub local_peer: PeerId,
    pub spec: NetworkSpec,
    pub network_id: [u8; 32],
    /// 宛先の規則（「直接接続を使わない」も持つ）
    pub policy: Arc<AddrPolicy>,
    /// 協力ノード（--service）
    pub service: bool,
    pub control: libp2p_stream::Control,
    pub conn_events: mpsc::UnboundedSender<ConnEvent>,
    pub internal: mpsc::UnboundedSender<Internal>,
    pub conns: HashMap<u64, Conn>,
    pub next_conn: u64,
    pub invites: invites::Invites,
    pub game_peer: Option<PeerId>,
    /// 最初のフレームを待つ届いた対局ストリームの数と、対局中の相手（受け付けのタスクと共有する）
    pub accept_gate: Arc<streams::AcceptGate>,
    pub peer_conns: HashMap<PeerId, HashMap<ConnectionId, bool>>,
    /// 受け付けた接続の接続元のアドレス帯
    pub inbound_prefixes: HashMap<ConnectionId, Vec<u8>>,
    pub addr_book: HashMap<PeerId, Vec<Multiaddr>>,
    /// 利用者が指定した協力ノードと、指定された宛先
    pub bootstrap: HashMap<PeerId, Multiaddr>,
    /// 協力ノードの中継の予約
    pub reservations: reservations::Reservations,
    pub listen_addrs: Vec<Multiaddr>,
    pub reachability: ipc::Reachability,
    pub last_status: Option<ipc::NetworkStatus>,
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
    node.run_loop(Some(req_rx)).await;
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
        if p.bootstrap.len() > ipc::MAX_BOOTSTRAP {
            return Err(("invalid_request", "too many addresses".into()));
        }
        let config = NodeConfig {
            spec,
            keypair: Keypair::generate_ed25519(),
            relay_only: p.relay_only,
            service: false,
            relay_server: false,
            listen: listen::candidates(spec.profile),
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
            // アドレスの候補が入る。確認に応えるのは協力ノードだけ（AutoNAT v2 のクライアントは、応える側の
            // ピアにだけ頼むので、確認の依頼は協力ノードにだけ届く）
            autonat_client: internet && !config.relay_only && !config.service,
            autonat_server: internet && config.service,
        })?;

        if listen::open(&mut swarm, config.listen) == 0 {
            return Err("could not listen".into());
        }

        let (conn_tx, conn_rx) = mpsc::unbounded_channel();
        let (internal_tx, internal_rx) = mpsc::unbounded_channel();
        let mut control = swarm.behaviour().game.new_control();
        let accept_gate = Arc::new(streams::AcceptGate::default());
        if !config.service {
            // 協力ノードは対局ストリームを受け付けない
            let incoming = control
                .accept(StreamProtocol::new(GAME_PROTOCOL))
                .map_err(|e| e.to_string())?;
            tokio::spawn(streams::accept_task(incoming, internal_tx.clone(), accept_gate.clone()));
        }

        Ok(Node {
            out: out.clone(),
            swarm,
            local_peer,
            spec,
            network_id,
            policy,
            service: config.service,
            control,
            conn_events: conn_tx,
            internal: internal_tx,
            conns: HashMap::new(),
            next_conn: 1,
            invites: invites::Invites::default(),
            game_peer: None,
            accept_gate,
            peer_conns: HashMap::new(),
            inbound_prefixes: HashMap::new(),
            addr_book: HashMap::new(),
            bootstrap: HashMap::new(),
            reservations: reservations::Reservations::default(),
            listen_addrs: Vec::new(),
            reachability: ipc::Reachability::Unknown,
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
            // 協力ノードへは、新しいポートで接続する。起動の直後は TCP の待受がまだ自分のアドレスを登録していないので、
            // 待受のポートを使い回せず一時的なポートで出ていく。ポートを使い回すつもりで出ると、Identify は協力ノードが
            // 観測したアドレス（一時的なポート）をそのまま到達性の確認の候補にし、確認は必ず失敗する。新しいポートで
            // 出たと分かっていれば、Identify は観測したアドレスを待受のポートに直して候補にする
            let opts = libp2p::swarm::dial_opts::DialOpts::peer_id(peer)
                .addresses(vec![base])
                .allocate_new_port()
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

    /// 主ループ。requests は対局・UI側からの要求（協力ノードは IPC を持たないので None）。列が閉じたら終わる
    pub async fn run_loop(&mut self, mut requests: Option<mpsc::UnboundedReceiver<Request>>) {
        let Some((mut conn_events, mut internal)) = self.pending_streams.take() else {
            return;
        };
        let mut tick = tokio::time::interval(Duration::from_millis(250));
        loop {
            tokio::select! {
                event = self.swarm.select_next_some() => self.on_swarm_event(event),
                request = next_request(&mut requests) => match request {
                    Some(r) => self.on_request(r),
                    None => std::process::exit(0),
                },
                Some(event) = conn_events.recv() => self.on_conn_event(event),
                Some(event) = internal.recv() => self.on_internal(event),
                _ = tick.tick() => self.on_tick(),
            }
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
        self.invites.expire(unix_now());
        // 断られた・失われた中継の予約を頼み直す（協力ノードとつながっている間）
        for peer in self.reservations.due(Instant::now()) {
            if self.peer_conns.contains_key(&peer) {
                self.request_reservation(peer);
            }
        }
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
            ipc::NetworkState::Online
        } else if !self.bootstrap.is_empty() {
            ipc::NetworkState::Connecting
        } else {
            ipc::NetworkState::Offline
        };
        let status = ipc::NetworkStatus {
            state,
            connected_peers: connected,
            reachability: self.reachability,
            relay_reservations: self.reservations.accepted_count(),
        };
        if self.last_status != Some(status) {
            self.out.notify(ipc::Notification::NetworkStatus(status));
            self.last_status = Some(status);
        }
    }
}

/// 次の要求（要求の列がなければ来ない）
async fn next_request(requests: &mut Option<mpsc::UnboundedReceiver<Request>>) -> Option<Request> {
    match requests {
        Some(rx) => rx.recv().await,
        None => std::future::pending().await,
    }
}
