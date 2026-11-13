//! ShogiBoardQ の P2P対局の通信プロセス `shogiboardq-peer`
//!
//! 招待の接続・NAT越え・中継・フレームの送受信だけを受け持ち、署名・鍵・ファイルを持たない
//! （実装仕様 2.1節・10.4節）。対局・UI側とは標準入出力の IPC（protocol/ipc.md）でやり取りする。

mod addr;
mod behaviours;
mod game;
mod ipc;
mod listen;
mod node;
mod sandbox;
mod service;
mod swarm;

fn run_ipc(initial_ppid: i32) -> i32 {
    // 標準入力を読む前（外部の入力を読む前）に、スレッドを作る前の主スレッドで制限を掛ける
    let report = match sandbox::apply(Some(initial_ppid)) {
        Ok(r) => r,
        Err(e) => {
            eprintln!("shogiboardq-peer: sandbox: {e}");
            return 3;
        }
    };
    let Some(runtime) = runtime() else { return 1 };
    runtime.block_on(node::run(report));
    0
}

/// 非同期の実行器（対局・UI側から使うときと協力ノードで同じ）。作れなければ理由を標準エラーに書いて None
fn runtime() -> Option<tokio::runtime::Runtime> {
    match tokio::runtime::Builder::new_multi_thread()
        .worker_threads(2)
        .enable_all()
        .build()
    {
        Ok(r) => Some(r),
        Err(e) => {
            eprintln!("shogiboardq-peer: runtime: {e}");
            None
        }
    }
}

fn self_test(initial_ppid: i32, targets: &[String]) -> i32 {
    match sandbox::apply(Some(initial_ppid)) {
        Ok(report) => {
            println!("{}", sandbox::self_test(&report, targets));
            0
        }
        Err(e) => {
            println!("{}", serde_json::json!({ "applied": false, "error": e }));
            3
        }
    }
}

fn main() {
    // 制限を掛けた後に親が替わっていないかを確かめるため、最初に親を読む
    let initial_ppid = sandbox::initial_parent();
    let args: Vec<String> = std::env::args().skip(1).collect();
    let code = match args.first().map(String::as_str) {
        None => run_ipc(initial_ppid),
        Some("--version") => {
            println!("shogiboardq-peer {}", env!("CARGO_PKG_VERSION"));
            0
        }
        Some("--sandbox-self-test") => self_test(initial_ppid, args.get(1..).unwrap_or_default()),
        Some("--service") => service::run(args.get(1..).unwrap_or_default()),
        Some(other) => {
            eprintln!("shogiboardq-peer: unknown option {other}");
            1
        }
    };
    std::process::exit(code);
}
