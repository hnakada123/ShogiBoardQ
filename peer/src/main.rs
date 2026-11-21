//! ShogiBoardQ の P2P対局の通信プロセス `shogiboardq-peer`
//!
//! 招待の接続・NAT越え・中継・フレームの送受信だけを受け持ち、署名・鍵・ファイルを持たない
//! （実装仕様 2.1節・10.4節）。対局・UI側とは標準入出力の IPC（protocol/ipc.md）でやり取りする。

mod addr;
mod behaviours;
mod game;
mod ipc;
mod limits;
mod listen;
mod meet;
mod meetstore;
mod node;
mod sandbox;
mod service;
mod swarm;
mod wire;

const NO_LOOPBACK: &str = "--no-loopback";
const PARENT_PID: &str = "--parent-pid";

/// 起動した側の PID（`--parent-pid <pid>`）を引数から取り除いて読む（引数のどこに置いてもよい）。無ければ None。
/// 値が無い・10進の正の整数でない・2回ある、Linux と macOS 以外の OS では理由を返す
fn take_parent_pid(args: &mut Vec<String>) -> Result<Option<i32>, String> {
    let Some(at) = args.iter().position(|a| a == PARENT_PID) else {
        return Ok(None);
    };
    if !cfg!(any(target_os = "linux", target_os = "macos")) {
        return Err(format!("{PARENT_PID} is not supported on this operating system"));
    }
    args.remove(at);
    if at >= args.len() {
        return Err(format!("{PARENT_PID} needs a value"));
    }
    let value = args.remove(at);
    if args.iter().any(|a| a == PARENT_PID) {
        return Err(format!("{PARENT_PID} is given twice"));
    }
    let valid = !value.is_empty() && value.bytes().all(|b| b.is_ascii_digit()) && !value.starts_with('0');
    match value.parse::<i32>() {
        Ok(pid) if valid && pid > 0 => Ok(Some(pid)),
        _ => Err(format!("{PARENT_PID} is not a process id")),
    }
}

/// OS のエラーの文。Windows では OS の文が利用者の言語に訳され、対局・UI側が診断の行を印字できる ASCII に絞ると
/// 欠けるので、番号だけにする（監査 2026-11-21 I-16）
fn os_error_text(e: &std::io::Error) -> String {
    if cfg!(windows) {
        match e.raw_os_error() {
            Some(code) => format!("os error {code}"),
            None => format!("{:?}", e.kind()),
        }
    } else {
        e.to_string()
    }
}

fn run_ipc(initial_ppid: i32, no_loopback: bool) -> i32 {
    // 標準入力を読む前（外部の入力を読む前）に、スレッドを作る前の主スレッドで制限を掛ける
    let report = match sandbox::apply(Some(initial_ppid), no_loopback) {
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
            eprintln!("shogiboardq-peer: runtime: {}", os_error_text(&e));
            None
        }
    }
}

fn self_test(initial_ppid: i32, no_loopback: bool, targets: &[String]) -> i32 {
    match sandbox::apply(Some(initial_ppid), no_loopback) {
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
    let mut args: Vec<String> = std::env::args().skip(1).collect();
    // 起動した側の PID（Linux と macOS）。読んだ親と違えば、ほかのことをする前に終わる（起動した側がもう終わっていて、
    // 親が別のプロセス（PID 1 など）に替わっていると、その親の終了を見張っても意味がない。監査 2026-11-21 I-15）
    match take_parent_pid(&mut args) {
        Ok(Some(expected)) if expected != initial_ppid => {
            eprintln!("shogiboardq-peer: sandbox: the parent process has changed");
            std::process::exit(3);
        }
        Ok(_) => {}
        Err(e) => {
            eprintln!("shogiboardq-peer: sandbox: {e}");
            std::process::exit(3);
        }
    }
    // loopback への発信を OS の側でも止める（macOS で、起動する側がローカルのネットワークを使わないときに付ける）
    let no_loopback = args.iter().any(|a| a == NO_LOOPBACK);
    args.retain(|a| a != NO_LOOPBACK);
    let code = match args.first().map(String::as_str) {
        None => run_ipc(initial_ppid, no_loopback),
        Some("--version") => {
            println!("shogiboardq-peer {}", env!("CARGO_PKG_VERSION"));
            0
        }
        Some("--sandbox-self-test") => self_test(initial_ppid, no_loopback, args.get(1..).unwrap_or_default()),
        Some("--service") => service::run(args.get(1..).unwrap_or_default()),
        Some(other) => {
            eprintln!("shogiboardq-peer: unknown option {other}");
            1
        }
    };
    std::process::exit(code);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn args(list: &[&str]) -> Vec<String> {
        list.iter().map(|s| (*s).to_owned()).collect()
    }

    /// `--parent-pid <pid>` は引数のどこに置いてもよく、取り除いて読む。値の形が違えば断る
    #[cfg(any(target_os = "linux", target_os = "macos"))]
    #[test]
    fn parent_pid_is_taken_from_anywhere() {
        let mut a = args(&["--sandbox-self-test", "--parent-pid", "1234", "file=x"]);
        assert_eq!(take_parent_pid(&mut a), Ok(Some(1234)));
        assert_eq!(a, args(&["--sandbox-self-test", "file=x"]));
        let mut none = args(&["--no-loopback"]);
        assert_eq!(take_parent_pid(&mut none), Ok(None));
        assert_eq!(none, args(&["--no-loopback"]));
        let mut last = args(&["--parent-pid", "7"]);
        assert_eq!(take_parent_pid(&mut last), Ok(Some(7)));
        assert!(last.is_empty());
        for bad in [
            &["--parent-pid"][..],
            &["--parent-pid", ""],
            &["--parent-pid", "0"],
            &["--parent-pid", "-5"],
            &["--parent-pid", "+5"],
            &["--parent-pid", "012"],
            &["--parent-pid", "x1"],
            &["--parent-pid", "99999999999"],
            &["--parent-pid", "5", "--parent-pid", "5"],
        ] {
            assert!(take_parent_pid(&mut args(bad)).is_err(), "{bad:?}");
        }
    }

    /// Linux と macOS 以外では受け付けない（起動する側は付けない）
    #[cfg(not(any(target_os = "linux", target_os = "macos")))]
    #[test]
    fn parent_pid_is_refused_elsewhere() {
        assert!(take_parent_pid(&mut args(&["--parent-pid", "5"])).is_err());
    }
}
