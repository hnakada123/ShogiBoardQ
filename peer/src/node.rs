//! 対局・UI側から使うときの通信プロセス（IPC の要求を受けて Swarm を動かす）
//!
//! 主ループと Swarm の作成はここに置き、役割ごとに分ける: 要求の処理（requests）、Swarm の出来事（events）、
//! 対局ストリーム（streams）、対局ストリームの接続の番号と段階（conns）、相手との libp2p の接続（peerlinks）、
//! 対局ストリームを開く接続の選び方（outbound）、作った招待（invites）、中継の予約（reservations）、協力ノードが
//! 中継している回路の数（circuits）、同じ LAN の合言葉（codes）、協力ノードを使う合言葉（meets）。待受は
//! crate::listen、協力ノードの待ち合わせは crate::meet

use std::collections::HashMap;
use std::sync::Arc;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use base64::engine::general_purpose::STANDARD as B64;
use base64::Engine;
use futures::StreamExt;
use libp2p::identity::Keypair;
use libp2p::{Multiaddr, PeerId, StreamProtocol, Swarm};
use sbq_protocol::frame::GAME_PROTOCOL;
use sbq_protocol::hash::hex;
use sbq_protocol::meet::MEET_PROTOCOL;
use sbq_protocol::network::{self, NetworkSpec};
use serde_json::json;
use tokio::sync::mpsc;

use crate::addr::{self, AddrPolicy};
use crate::game::ConnEvent;
use crate::ipc::{self, ErrorCode, IpcError, Method, Out, Request};
use crate::listen;
use crate::meet::{self, MeetService};
use crate::sandbox::SandboxReport;
use crate::service;
use crate::swarm::{self, Behaviour, Role, SwarmOptions};

mod circuits;
mod codes;
mod conns;
mod events;
mod invites;
mod meets;
mod outbound;
mod peerlinks;
mod presences;
mod requests;
mod reservations;
mod streams;

pub use streams::Internal;

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
    pub role: Role,
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
    /// プレイヤーか協力ノードか（直接の接続（穴あけ）を試すかも、これで決まる）
    pub role: Role,
    pub control: libp2p_stream::Control,
    pub conn_events: mpsc::UnboundedSender<ConnEvent>,
    pub internal: mpsc::UnboundedSender<Internal>,
    /// 対局ストリームの接続（つなぐ途中・開いた・閉じる途中）
    pub conns: conns::Conns,
    pub invites: invites::Invites,
    /// 在席の知らせ（友だちの申し込みを受けるトークン）
    pub presences: presences::Presences,
    /// 招待を取り消すたびに増やす番号（鍵を作っている間に取り消された招待を足さない）
    pub invite_epoch: u64,
    /// 協力ノードを使う合言葉（待ち合わせに対応した協力ノード・引き取った招待）
    pub meets: meets::Meets,
    /// 待ち合わせで預かっているもの（待ち合わせを受け持つ協力ノードだけ）
    pub meet_service: Option<Arc<MeetService>>,
    pub game_peer: Option<PeerId>,
    /// 最初のフレームを待つ届いた対局ストリームの数と、対局中の相手（受け付けのタスクと共有する）
    pub accept_gate: Arc<streams::AcceptGate>,
    /// 相手との libp2p の接続
    pub links: peerlinks::PeerLinks,
    pub addr_book: HashMap<PeerId, Vec<Multiaddr>>,
    /// 利用者が指定した協力ノードと、指定された宛先
    pub bootstrap: HashMap<PeerId, Multiaddr>,
    /// 協力ノードの中継の予約
    pub reservations: reservations::Reservations,
    /// 中継している回路（中継を受け持つ協力ノードだけ。ログに数を書く）
    pub circuits: Option<circuits::Circuits>,
    pub listen_addrs: Vec<Multiaddr>,
    pub reachability: ipc::Reachability,
    pub last_status: Option<ipc::NetworkStatus>,
    pub last_second: Instant,
    /// 協力ノードが最後にログへ書いた数（変わったときだけ書く）
    pub logged_counts: Option<service::Counts>,
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
        Err(e) => out.error(&hello.request_id, &e),
    }

    // start
    let mut node = loop {
        let Some(request) = req_rx.recv().await else { return };
        match request.method {
            Method::Start => match Node::start(&out, &request) {
                Ok(node) => break node,
                Err(e) => out.error(&request.request_id, &e),
            },
            Method::Shutdown => {
                out.result(&request.request_id, json!({}));
                tokio::time::sleep(Duration::from_millis(200)).await;
                std::process::exit(0);
            }
            _ => out.error(
                &request.request_id,
                &IpcError::new(ErrorCode::InvalidState, "start has not been called"),
            ),
        }
    };
    node.run_loop(Some(req_rx)).await;
}

impl Node {
    fn start(out: &Out, request: &Request) -> Result<Node, IpcError> {
        let p: ipc::StartParams = ipc::params(request)?;
        if !network::is_valid_name(&p.network_name) {
            return Err(IpcError::invalid_request("invalid networkName"));
        }
        let spec = network::find(&p.network_name)
            .ok_or_else(|| IpcError::new(ErrorCode::NetworkNotAllowed, "network not allowed"))?;
        if spec.expires_with_build && build_expired(unix_now()) {
            return Err(IpcError::new(ErrorCode::BuildExpired, "this build has expired"));
        }
        if p.bootstrap.len() > ipc::MAX_BOOTSTRAP {
            return Err(IpcError::invalid_request("too many addresses"));
        }
        let config = NodeConfig {
            spec,
            keypair: Keypair::generate_ed25519(),
            role: Role::Player {
                relay_only: p.relay_only,
            },
            listen: listen::candidates(spec.profile),
        };
        let mut node = Node::create(out, config).map_err(|e| IpcError::new(ErrorCode::Internal, e))?;
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
        let role = config.role;
        let policy = Arc::new(AddrPolicy::new(spec.profile, role.relay_only()));
        let keypair = config.keypair;
        let local_peer = keypair.public().to_peer_id();
        // 待ち合わせ（協力ノードの --meet）は Swarm より先に作る（受け付けた接続の接続元を、振る舞いが覚える）
        let meet_service = role.meets().then(|| Arc::new(MeetService::default()));
        let mut swarm = swarm::build(SwarmOptions {
            keypair,
            policy: policy.clone(),
            role,
            profile: spec.profile,
            meet: meet_service.clone(),
        })?;

        // 協力ノードの --listen は運用する人が指定したアドレスなので、そのまま試し、開けなかったものを標準エラーに
        // 書く（一部だけ開けないと、そのアドレスでつなぐ利用者のアプリが黙ってつながらない）。書くのは自分の待受の
        // アドレスだけで、接続元の IP ではない
        let source = if role.is_service() {
            listen::Source::Specified
        } else {
            listen::Source::Candidates
        };
        let opened = listen::open(&mut swarm, config.listen, source);
        if role.is_service() {
            for addr in &opened.failed {
                eprintln!("shogiboardq-peer: could not listen on {addr}");
            }
        }
        if opened.listening == 0 {
            return Err("could not listen".into());
        }

        let (conn_tx, conn_rx) = mpsc::unbounded_channel();
        let (internal_tx, internal_rx) = mpsc::unbounded_channel();
        let mut control = swarm.behaviour().game.new_control();
        let accept_gate = Arc::new(streams::AcceptGate::default());
        if !role.is_service() {
            // 協力ノードは対局ストリームを受け付けない
            let incoming = control
                .accept(StreamProtocol::new(GAME_PROTOCOL))
                .map_err(|e| e.to_string())?;
            tokio::spawn(streams::accept_task(incoming, internal_tx.clone(), accept_gate.clone()));
        }
        if let Some(service) = &meet_service {
            let incoming = control
                .accept(StreamProtocol::new(MEET_PROTOCOL))
                .map_err(|e| e.to_string())?;
            tokio::spawn(meet::accept_task(incoming, service.clone()));
        }

        Ok(Node {
            out: out.clone(),
            swarm,
            local_peer,
            spec,
            network_id,
            policy,
            role,
            control,
            conn_events: conn_tx,
            internal: internal_tx,
            conns: conns::Conns::default(),
            invites: invites::Invites::default(),
            presences: presences::Presences::default(),
            invite_epoch: 0,
            meets: meets::Meets::default(),
            meet_service,
            game_peer: None,
            accept_gate,
            links: peerlinks::PeerLinks::default(),
            addr_book: HashMap::new(),
            bootstrap: HashMap::new(),
            reservations: reservations::Reservations::default(),
            circuits: role.relays().then(circuits::Circuits::default),
            listen_addrs: Vec::new(),
            reachability: ipc::Reachability::Unknown,
            last_status: None,
            last_second: Instant::now(),
            logged_counts: None,
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
            // 閉じた接続を知らせる（つなぎ始められなかった接続は、番号を返す応答の後になる）
            self.report_closed();
        }
    }

    fn on_tick(&mut self) {
        if let Some(at) = self.shutdown_at {
            if Instant::now() >= at {
                std::process::exit(0);
            }
        }
        // 期限を過ぎた接続（合言葉の宛先へのダイヤル・開く接続を待つ間）を閉じ、直接の接続を待つ時間が過ぎた
        // 対局ストリームを開く
        self.conns.expire(Instant::now());
        self.advance_all_openings();
        if self.last_second.elapsed() < Duration::from_secs(1) {
            return;
        }
        self.last_second = Instant::now();
        self.invites.expire(unix_now());
        self.presences.expire(unix_now());
        self.release_meets();
        // 断られた・失われた中継の予約を頼み直す（協力ノードとつながっている間）
        for peer in self.reservations.due(Instant::now()) {
            if self.links.contains(&peer) {
                self.request_reservation(peer);
            }
        }
        self.emit_status();
        if self.role.is_service() {
            // 接続元の IP もピアの ID も書かない（数だけ）。数が変わったときに書く（ここは1秒に1回なので、
            // 続けて変わっても1秒に1行まで）
            let counts = service::Counts {
                peers: self.links.peer_count(),
                relays: self.circuits.as_ref().map(circuits::Circuits::count),
                // 期限の過ぎたものを消してから数える
                meets: self.meet_service.as_ref().map(|m| m.expire(Instant::now())),
            };
            if let Some(line) = service::log_line_if_changed(&mut self.logged_counts, counts) {
                eprintln!("{line}");
            }
        }
    }

    pub fn emit_status(&mut self) {
        let connected = self.links.peer_count();
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
            meet_nodes: self.meets.nodes.len(),
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
