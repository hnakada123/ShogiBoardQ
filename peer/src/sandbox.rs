//! 権限の制限（protocol/ipc.md 6章、実装仕様 10.4節）
//!
//! 標準入力を読む前（外部の入力を読む前）に、スレッドを作る前の主スレッドで掛ける
//! （seccomp と Landlock は掛けたスレッドとその後に作るスレッドに効く）。

use serde_json::{json, Value};

/// 掛けた制限
#[derive(Clone, Debug)]
pub struct SandboxReport {
    /// 必須のものをすべて掛けた
    pub applied: bool,
    pub os: &'static str,
    pub mechanisms: Vec<&'static str>,
    pub landlock_abi: i32,
}

impl SandboxReport {
    pub fn to_json(&self) -> Value {
        json!({
            "applied": self.applied,
            "os": self.os,
            "mechanisms": self.mechanisms,
            "landlockAbi": self.landlock_abi,
        })
    }
}

#[cfg(target_os = "linux")]
mod linux {
    use std::collections::BTreeMap;

    use landlock::{Access, AccessFs, LandlockStatus, Ruleset, RulesetAttr, RulesetStatus, Scope, ABI};
    use nix::sys::prctl;
    use nix::sys::resource::{setrlimit, Resource};
    use nix::sys::signal::Signal;
    use nix::unistd::getppid;
    use seccompiler::{
        BpfProgram, SeccompAction, SeccompCmpArgLen, SeccompCmpOp, SeccompCondition, SeccompFilter, SeccompRule,
        TargetArch,
    };

    use super::SandboxReport;

    fn errno_eperm() -> u32 {
        u32::try_from(libc::EPERM).unwrap_or(1)
    }

    fn errno_enosys() -> u32 {
        u32::try_from(libc::ENOSYS).unwrap_or(38)
    }

    fn cond(arg: u8, op: SeccompCmpOp, value: u64) -> Result<SeccompCondition, String> {
        SeccompCondition::new(arg, SeccompCmpArgLen::Qword, op, value).map_err(|e| format!("seccomp condition: {e}"))
    }

    fn arch() -> Result<TargetArch, String> {
        TargetArch::try_from(std::env::consts::ARCH).map_err(|e| format!("seccomp arch: {e}"))
    }

    fn as_u64(v: libc::c_int) -> u64 {
        u64::try_from(v).unwrap_or(0)
    }

    /// 拒否する呼び出しの表（EPERM を返す）
    fn deny_filter() -> Result<BpfProgram, String> {
        let mut rules: BTreeMap<i64, Vec<SeccompRule>> = BTreeMap::new();
        let unconditional: &[libc::c_long] = &[
            libc::SYS_execve,
            libc::SYS_execveat,
            libc::SYS_ptrace,
            libc::SYS_process_vm_readv,
            libc::SYS_process_vm_writev,
            libc::SYS_kill,
            libc::SYS_pidfd_open,
            libc::SYS_pidfd_send_signal,
            libc::SYS_pidfd_getfd,
            libc::SYS_mount,
            libc::SYS_umount2,
            libc::SYS_pivot_root,
            libc::SYS_chroot,
            libc::SYS_unshare,
            libc::SYS_setns,
            libc::SYS_bpf,
            libc::SYS_perf_event_open,
            libc::SYS_keyctl,
            libc::SYS_add_key,
            libc::SYS_request_key,
            libc::SYS_init_module,
            libc::SYS_finit_module,
            libc::SYS_delete_module,
            libc::SYS_kexec_load,
            libc::SYS_reboot,
            libc::SYS_swapon,
            libc::SYS_swapoff,
            libc::SYS_acct,
            libc::SYS_userfaultfd,
            libc::SYS_personality,
            libc::SYS_io_uring_setup,
            #[cfg(target_arch = "x86_64")]
            libc::SYS_fork,
            #[cfg(target_arch = "x86_64")]
            libc::SYS_vfork,
        ];
        for &nr in unconditional {
            rules.insert(nr, vec![]);
        }
        // スレッドでない clone（プロセスの複製）を拒否する
        let clone_thread = as_u64(libc::CLONE_THREAD);
        rules.insert(
            libc::SYS_clone,
            vec![
                SeccompRule::new(vec![cond(0, SeccompCmpOp::MaskedEq(clone_thread), 0)?])
                    .map_err(|e| format!("seccomp rule: {e}"))?,
            ],
        );
        // AF_INET・AF_INET6・AF_NETLINK 以外のソケットを拒否する
        for nr in [libc::SYS_socket, libc::SYS_socketpair] {
            let rule = SeccompRule::new(vec![
                cond(0, SeccompCmpOp::Ne, as_u64(libc::AF_INET))?,
                cond(0, SeccompCmpOp::Ne, as_u64(libc::AF_INET6))?,
                cond(0, SeccompCmpOp::Ne, as_u64(libc::AF_NETLINK))?,
            ])
            .map_err(|e| format!("seccomp rule: {e}"))?;
            rules.insert(nr, vec![rule]);
        }
        let filter = SeccompFilter::new(
            rules,
            SeccompAction::Allow,
            SeccompAction::Errno(errno_eperm()),
            arch()?,
        )
        .map_err(|e| format!("seccomp filter: {e}"))?;
        BpfProgram::try_from(filter).map_err(|e| format!("seccomp compile: {e}"))
    }

    /// clone3 は ENOSYS（libc が clone に戻る。clone3 の引数は構造体なので中身を確かめられない）
    fn clone3_filter() -> Result<BpfProgram, String> {
        let mut rules: BTreeMap<i64, Vec<SeccompRule>> = BTreeMap::new();
        rules.insert(libc::SYS_clone3, vec![]);
        let filter = SeccompFilter::new(
            rules,
            SeccompAction::Allow,
            SeccompAction::Errno(errno_enosys()),
            arch()?,
        )
        .map_err(|e| format!("seccomp filter: {e}"))?;
        BpfProgram::try_from(filter).map_err(|e| format!("seccomp compile: {e}"))
    }

    pub fn apply(initial_ppid: Option<i32>) -> Result<SandboxReport, String> {
        let mut mechanisms = Vec::new();

        if let Some(ppid) = initial_ppid {
            prctl::set_pdeathsig(Signal::SIGKILL).map_err(|e| format!("PR_SET_PDEATHSIG: {e}"))?;
            if getppid().as_raw() != ppid {
                return Err("parent already exited".to_owned());
            }
            mechanisms.push("parent_death_signal");
        }

        setrlimit(Resource::RLIMIT_CORE, 0, 0).map_err(|e| format!("RLIMIT_CORE: {e}"))?;
        prctl::set_dumpable(false).map_err(|e| format!("PR_SET_DUMPABLE: {e}"))?;
        mechanisms.push("no_core_dump");

        prctl::set_no_new_privs().map_err(|e| format!("PR_SET_NO_NEW_PRIVS: {e}"))?;
        mechanisms.push("no_new_privs");

        // ファイルの操作をすべて拒否する（規則を1つも足さない）。知っている最新の ABI の権利を求め、
        // カーネルが知らないものは落とす（BestEffort）。ABI 6 以上ならスコープ（抽象 Unix ソケット・シグナル）も掛ける
        let target = ABI::V9;
        let status = Ruleset::default()
            .handle_access(AccessFs::from_all(target))
            .map_err(|e| format!("landlock: {e}"))?
            .scope(Scope::from_all(target))
            .map_err(|e| format!("landlock: {e}"))?
            .create()
            .map_err(|e| format!("landlock: {e}"))?
            .restrict_self()
            .map_err(|e| format!("landlock: {e}"))?;
        let abi = match status.landlock {
            LandlockStatus::Available { effective_abi, .. } => effective_abi,
            _ => ABI::Unsupported,
        };
        if status.ruleset == RulesetStatus::NotEnforced || abi == ABI::Unsupported {
            return Err("landlock is not available".to_owned());
        }
        mechanisms.push("landlock_fs");
        if abi >= ABI::V6 {
            mechanisms.push("landlock_scope");
        }

        seccompiler::apply_filter(&clone3_filter()?).map_err(|e| format!("seccomp: {e}"))?;
        seccompiler::apply_filter(&deny_filter()?).map_err(|e| format!("seccomp: {e}"))?;
        mechanisms.push("seccomp");

        Ok(SandboxReport {
            applied: true,
            os: "linux",
            mechanisms,
            landlock_abi: abi as i32,
        })
    }
}

/// 親の PID（起動の直後に読む。制限を掛けた後に親が替わっていないかを確かめる）
pub fn initial_parent() -> i32 {
    #[cfg(unix)]
    {
        nix_parent()
    }
    #[cfg(not(unix))]
    {
        0
    }
}

#[cfg(target_os = "linux")]
fn nix_parent() -> i32 {
    nix::unistd::getppid().as_raw()
}

#[cfg(all(unix, not(target_os = "linux")))]
fn nix_parent() -> i32 {
    0
}

/// 制限を掛ける。掛けられなければ理由。initial_ppid を渡すと、親の終了で終わるようにする
/// （協力ノードは渡さない）
pub fn apply(initial_ppid: Option<i32>) -> Result<SandboxReport, String> {
    #[cfg(target_os = "linux")]
    {
        linux::apply(initial_ppid)
    }
    #[cfg(not(target_os = "linux"))]
    {
        let _ = initial_ppid;
        Err("the sandbox is not implemented for this operating system".to_owned())
    }
}

/// 制限を掛けた後に、禁止した操作を試す（--sandbox-self-test）
pub fn self_test(report: &SandboxReport) -> Value {
    #[cfg(target_os = "linux")]
    {
        let results = linux_probe::run();
        json!({
            "applied": report.applied,
            "mechanisms": report.mechanisms,
            "landlockAbi": report.landlock_abi,
            "results": results,
        })
    }
    #[cfg(not(target_os = "linux"))]
    {
        json!({ "applied": report.applied, "results": {} })
    }
}

#[cfg(target_os = "linux")]
mod linux_probe {
    use std::io::ErrorKind;
    use std::net::{SocketAddr, TcpStream};
    use std::os::linux::net::SocketAddrExt;
    use std::time::Duration;

    use serde_json::{Map, Value};

    fn verdict(ok: bool) -> Value {
        Value::from(if ok { "allowed" } else { "denied" })
    }

    pub fn run() -> Value {
        let mut results = Map::new();

        let read = std::fs::File::open("/etc/hostname").or_else(|_| std::fs::File::open("/etc/passwd"));
        results.insert("read_file".into(), verdict(read.is_ok()));

        let path = format!("/tmp/sbq-peer-selftest-{}", std::process::id());
        let write = std::fs::File::create(&path);
        if write.is_ok() {
            let _ = std::fs::remove_file(&path);
        }
        results.insert("write_file".into(), verdict(write.is_ok()));

        // 自分のプロセスを置き換える。許されてしまえば /bin/true になって何も書かずに終わる（試験は失敗として扱う）
        let exec_ok = match std::ffi::CString::new("/bin/true") {
            Ok(path) => nix::unistd::execv(&path, &[&path]).is_ok(),
            Err(_) => false,
        };
        results.insert("exec".into(), verdict(exec_ok));

        // スレッドでない clone（daemon は fork する。許されると親は終わり、子がこの先を書く）
        let fork_ok = nix::unistd::daemon(true, true).is_ok();
        results.insert("fork".into(), verdict(fork_ok));

        let unix = std::os::unix::net::UnixDatagram::unbound();
        results.insert("unix_socket".into(), verdict(unix.is_ok()));

        let abstract_ok = std::os::unix::net::SocketAddr::from_abstract_name(b"sbq-peer-selftest")
            .and_then(|addr| std::os::unix::net::UnixStream::connect_addr(&addr))
            .map(|_| true)
            .unwrap_or_else(|e| e.kind() != ErrorKind::PermissionDenied && e.raw_os_error() != Some(libc::EPERM));
        results.insert("unix_connect_abstract".into(), verdict(abstract_ok));

        let signal = nix::sys::signal::kill(nix::unistd::getppid(), None);
        results.insert("signal_parent".into(), verdict(signal.is_ok()));

        // loopback の閉じたポート。接続を拒まれたなら、OS の側では止めていない（allowed）
        let target = SocketAddr::from(([127, 0, 0, 1], 9));
        let tcp = match TcpStream::connect_timeout(&target, Duration::from_millis(500)) {
            Ok(_) => true,
            Err(e) => e.raw_os_error() != Some(libc::EPERM) && e.raw_os_error() != Some(libc::EACCES),
        };
        results.insert("connect_loopback_tcp".into(), verdict(tcp));

        Value::Object(results)
    }
}
