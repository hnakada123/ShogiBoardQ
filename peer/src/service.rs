//! 協力ノード（--service。protocol/ipc.md 7章）
//!
//! ブートストラップ・DHT の server・中継を設定で有効にする。募集の転送と一覧取得には常に応え、
//! 対局ストリームは受け付けない。識別鍵のファイルを読んだ（なければ作った）後に権限の制限を掛ける。

use std::io::Write;
use std::path::Path;

use libp2p::identity::Keypair;
use libp2p::Multiaddr;
use sbq_protocol::network;
use tokio::sync::mpsc;

use crate::ipc::Out;
use crate::node::{Node, NodeConfig};
use crate::sandbox;

struct ServiceArgs {
    network: String,
    identity: String,
    listen: Vec<Multiaddr>,
    bootstrap: Vec<String>,
    relay: bool,
    dht_server: bool,
}

fn parse_args(args: &[String]) -> Result<ServiceArgs, String> {
    let mut network = None;
    let mut identity = None;
    let mut listen = Vec::new();
    let mut bootstrap = Vec::new();
    let mut relay = false;
    let mut dht_server = false;
    let mut it = args.iter();
    while let Some(arg) = it.next() {
        match arg.as_str() {
            "--network" => network = Some(it.next().ok_or("--network needs a value")?.clone()),
            "--identity" => identity = Some(it.next().ok_or("--identity needs a value")?.clone()),
            "--listen" => {
                let text = it.next().ok_or("--listen needs a value")?;
                listen.push(text.parse::<Multiaddr>().map_err(|e| format!("--listen {text}: {e}"))?);
            }
            "--bootstrap" => bootstrap.push(it.next().ok_or("--bootstrap needs a value")?.clone()),
            "--relay" => relay = true,
            "--dht-server" => dht_server = true,
            other => return Err(format!("unknown option {other}")),
        }
    }
    Ok(ServiceArgs {
        network: network.ok_or("--network is required")?,
        identity: identity.ok_or("--identity is required")?,
        listen,
        bootstrap,
        relay,
        dht_server,
    })
}

/// 識別鍵を読む。なければ作り、所有者だけが読み書きできる権限で保存する
fn load_identity(path: &Path) -> Result<Keypair, String> {
    match std::fs::read(path) {
        Ok(bytes) => Keypair::from_protobuf_encoding(&bytes).map_err(|e| format!("identity: {e}")),
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            let keypair = Keypair::generate_ed25519();
            let bytes = keypair.to_protobuf_encoding().map_err(|e| format!("identity: {e}"))?;
            let mut options = std::fs::OpenOptions::new();
            options.write(true).create_new(true);
            #[cfg(unix)]
            {
                use std::os::unix::fs::OpenOptionsExt;
                options.mode(0o600);
            }
            let mut file = options.open(path).map_err(|e| format!("identity: {e}"))?;
            file.write_all(&bytes).map_err(|e| format!("identity: {e}"))?;
            file.sync_all().map_err(|e| format!("identity: {e}"))?;
            Ok(keypair)
        }
        Err(e) => Err(format!("identity: {e}")),
    }
}

/// 協力ノードとして動かす。戻らない（失敗は終了コードで返す）
pub fn run(args: &[String]) -> i32 {
    let args = match parse_args(args) {
        Ok(a) => a,
        Err(e) => {
            eprintln!("shogiboardq-peer: {e}");
            return 1;
        }
    };
    let Some(spec) = network::find(&args.network) else {
        eprintln!("shogiboardq-peer: network not allowed: {}", args.network);
        return 1;
    };
    if args.listen.is_empty() {
        eprintln!("shogiboardq-peer: --listen is required");
        return 1;
    }
    let keypair = match load_identity(Path::new(&args.identity)) {
        Ok(k) => k,
        Err(e) => {
            eprintln!("shogiboardq-peer: {e}");
            return 1;
        }
    };
    // 協力ノードは親の終了に合わせない（サービスとして動く）
    if let Err(e) = sandbox::apply(None) {
        eprintln!("shogiboardq-peer: sandbox: {e}");
        return 3;
    }
    let runtime = match tokio::runtime::Builder::new_multi_thread()
        .worker_threads(2)
        .enable_all()
        .build()
    {
        Ok(r) => r,
        Err(e) => {
            eprintln!("shogiboardq-peer: runtime: {e}");
            return 1;
        }
    };
    runtime.block_on(async move {
        // 協力ノードは IPC を使わない。知らせは捨てる
        let (out_tx, mut out_rx) = mpsc::unbounded_channel();
        tokio::spawn(async move { while out_rx.recv().await.is_some() {} });
        let out = Out::new(out_tx);
        let config = NodeConfig {
            spec,
            rules_hash: network::rules_hash_v1(),
            keypair,
            relay_only: false,
            service: true,
            relay_server: args.relay,
            dht_server: args.dht_server,
            listen: args.listen,
        };
        let mut node = match Node::create(&out, config) {
            Ok(n) => n,
            Err(e) => {
                eprintln!("shogiboardq-peer: {e}");
                return 1;
            }
        };
        node.connect_initial_peers(&args.bootstrap, &[]);
        eprintln!("shogiboardq-peer: service started as {}", node.local_peer);
        let (_keep, requests) = mpsc::unbounded_channel();
        node.run_loop(requests).await;
        0
    })
}
