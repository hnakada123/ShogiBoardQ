//! 協力ノード（--service。protocol/ipc.md 7章）
//!
//! 利用者のアプリが最初に接続する相手になり、到達性の確認（AutoNAT）に応え、設定すれば中継（--relay）を受け持つ。
//! 対局ストリームは受け付けず、指し手も対局者の ID も読まない。接続元の IP はログに書かない。
//! 識別鍵のファイルを読んだ（なければ作った）後に権限の制限を掛ける。

use std::io::Write;
use std::path::Path;

use libp2p::identity::Keypair;
use libp2p::Multiaddr;
use sbq_protocol::network;

use crate::ipc::Out;
use crate::node::{Node, NodeConfig};
use crate::sandbox;

struct ServiceArgs {
    network: String,
    identity: String,
    listen: Vec<Multiaddr>,
    bootstrap: Vec<String>,
    relay: bool,
}

fn parse_args(args: &[String]) -> Result<ServiceArgs, String> {
    let mut network = None;
    let mut identity = None;
    let mut listen = Vec::new();
    let mut bootstrap = Vec::new();
    let mut relay = false;
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
            other => return Err(format!("unknown option {other}")),
        }
    }
    Ok(ServiceArgs {
        network: network.ok_or("--network is required")?,
        identity: identity.ok_or("--identity is required")?,
        listen,
        bootstrap,
        relay,
    })
}

/// 識別鍵のファイルを、所有者だけが読み書きできるか確かめる（ほかの利用者が読める・書ける鍵は使わない）
#[cfg(unix)]
fn check_identity_permissions(path: &Path) -> Result<(), String> {
    use std::os::unix::fs::{MetadataExt, PermissionsExt};
    let meta = std::fs::symlink_metadata(path).map_err(|e| format!("identity: {e}"))?;
    if !meta.file_type().is_file() {
        return Err("identity: not a regular file".to_owned());
    }
    if meta.uid() != nix::unistd::getuid().as_raw() {
        return Err("identity: owned by another user".to_owned());
    }
    if meta.permissions().mode() & 0o077 != 0 {
        return Err("identity: readable or writable by other users (chmod 600)".to_owned());
    }
    Ok(())
}

#[cfg(not(unix))]
fn check_identity_permissions(_path: &Path) -> Result<(), String> {
    Ok(())
}

/// 識別鍵を読む。なければ作り、所有者だけが読み書きできる権限で保存する（一時ファイルに書いてから置き換えるので、
/// 書く途中で止まっても壊れたファイルを残さない）
fn load_identity(path: &Path) -> Result<Keypair, String> {
    match std::fs::read(path) {
        Ok(bytes) => {
            check_identity_permissions(path)?;
            Keypair::from_protobuf_encoding(&bytes).map_err(|e| format!("identity: {e}"))
        }
        Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
            let keypair = Keypair::generate_ed25519();
            let bytes = keypair.to_protobuf_encoding().map_err(|e| format!("identity: {e}"))?;
            let mut temp = path.as_os_str().to_owned();
            temp.push(".tmp");
            let temp = std::path::PathBuf::from(temp);
            let _ = std::fs::remove_file(&temp);
            let mut options = std::fs::OpenOptions::new();
            options.write(true).create_new(true);
            #[cfg(unix)]
            {
                use std::os::unix::fs::OpenOptionsExt;
                options.mode(0o600);
            }
            let mut file = options.open(&temp).map_err(|e| format!("identity: {e}"))?;
            file.write_all(&bytes).map_err(|e| format!("identity: {e}"))?;
            file.sync_all().map_err(|e| format!("identity: {e}"))?;
            drop(file);
            std::fs::rename(&temp, path).map_err(|e| format!("identity: {e}"))?;
            Ok(keypair)
        }
        Err(e) => Err(format!("identity: {e}")),
    }
}

/// 協力ノードがログに書く数
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Counts {
    /// つながっている相手の数
    pub peers: usize,
    /// 中継している回路の数（中継を受け持つとき（--relay）だけ）
    pub relays: Option<usize>,
}

/// 標準エラーへ書く行。つながっている相手の数と、中継を受け持つときは中継している回路の数だけを書く
/// （接続元の IP もピアの ID も書かない）
pub fn log_line(counts: Counts) -> String {
    match counts.relays {
        Some(relays) => format!("shogiboardq-peer: peers={} relays={relays}", counts.peers),
        None => format!("shogiboardq-peer: peers={}", counts.peers),
    }
}

/// 前に書いた数から変わっていれば、書く行を返す（起動した直後の最初の数も書く）。決まった間隔では書かない
/// （変化がその場で分かるように。変わらない間は何も書かない）
pub fn log_line_if_changed(logged: &mut Option<Counts>, counts: Counts) -> Option<String> {
    if *logged == Some(counts) {
        return None;
    }
    *logged = Some(counts);
    Some(log_line(counts))
}

/// 協力ノードとして動かす。戻らない（失敗は終了コードで返す）
pub fn run(args: &[String]) -> i32 {
    // 識別鍵のファイルを読んだ後に自分で制限を掛けられる OS だけで動かす（鍵のファイルを作る前に断る）
    if !cfg!(any(target_os = "linux", target_os = "macos")) {
        eprintln!("shogiboardq-peer: sandbox: the service mode runs only on Linux and macOS");
        return 3;
    }
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
    if let Err(e) = sandbox::apply(None, false) {
        eprintln!("shogiboardq-peer: sandbox: {e}");
        return 3;
    }
    let Some(runtime) = crate::runtime() else { return 1 };
    runtime.block_on(async move {
        // 協力ノードは IPC を使わない（知らせは書かず、要求も来ない）
        let out = Out::discard();
        let config = NodeConfig {
            spec,
            keypair,
            relay_only: false,
            service: true,
            relay_server: args.relay,
            listen: args.listen.into_iter().map(|addr| vec![addr]).collect(),
        };
        let mut node = match Node::create(&out, config) {
            Ok(n) => n,
            Err(e) => {
                eprintln!("shogiboardq-peer: {e}");
                return 1;
            }
        };
        node.connect_nodes(&args.bootstrap);
        eprintln!("shogiboardq-peer: service started as {}", node.local_peer);
        node.run_loop(None).await;
        0
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn counts(peers: usize, relays: Option<usize>) -> Counts {
        Counts { peers, relays }
    }

    #[test]
    fn log_line_shows_relays_only_when_relaying_is_enabled() {
        assert_eq!(log_line(counts(2, Some(1))), "shogiboardq-peer: peers=2 relays=1");
        assert_eq!(log_line(counts(0, Some(0))), "shogiboardq-peer: peers=0 relays=0");
        // --relay を付けていない協力ノードは、中継の数を書かない
        assert_eq!(log_line(counts(2, None)), "shogiboardq-peer: peers=2");
    }

    #[test]
    fn log_line_is_written_only_when_the_counts_change() {
        let mut logged = None;
        // 起動した直後の数は書く
        assert_eq!(
            log_line_if_changed(&mut logged, counts(0, Some(0))).as_deref(),
            Some("shogiboardq-peer: peers=0 relays=0")
        );
        // 変わらない間は書かない
        assert_eq!(log_line_if_changed(&mut logged, counts(0, Some(0))), None);
        assert_eq!(
            log_line_if_changed(&mut logged, counts(2, Some(0))).as_deref(),
            Some("shogiboardq-peer: peers=2 relays=0")
        );
        // 中継の数だけが変わっても書く
        assert_eq!(
            log_line_if_changed(&mut logged, counts(2, Some(1))).as_deref(),
            Some("shogiboardq-peer: peers=2 relays=1")
        );
        assert_eq!(log_line_if_changed(&mut logged, counts(2, Some(1))), None);
        // 元の数に戻ったときも書く
        assert_eq!(
            log_line_if_changed(&mut logged, counts(0, Some(0))).as_deref(),
            Some("shogiboardq-peer: peers=0 relays=0")
        );
    }
}
