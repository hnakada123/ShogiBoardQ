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
        sock_filter, BpfProgram, SeccompAction, SeccompCmpArgLen, SeccompCmpOp, SeccompCondition, SeccompFilter,
        SeccompRule, TargetArch,
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

    /// int の引数（下位32ビット）の条件
    fn cond32(arg: u8, op: SeccompCmpOp, value: u64) -> Result<SeccompCondition, String> {
        SeccompCondition::new(arg, SeccompCmpArgLen::Dword, op, value).map_err(|e| format!("seccomp condition: {e}"))
    }

    fn rule(conditions: Vec<SeccompCondition>) -> Result<SeccompRule, String> {
        SeccompRule::new(conditions).map_err(|e| format!("seccomp rule: {e}"))
    }

    fn arch() -> Result<TargetArch, String> {
        TargetArch::try_from(std::env::consts::ARCH).map_err(|e| format!("seccomp arch: {e}"))
    }

    fn as_u64(v: libc::c_int) -> u64 {
        u64::try_from(v).unwrap_or(0)
    }

    /// 拒否する呼び出しの表（EPERM を返す）
    pub(super) fn deny_filter() -> Result<BpfProgram, String> {
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
            // ほかのスレッド・プロセスへのシグナル（スレッドの ID だけで送る。自分のプロセスの中では使わない）
            libc::SYS_tkill,
            // ファイルの属性の変更（Landlock は属性の変更を止めない。鍵のファイルの権限を広げる、などを止める）
            libc::SYS_fchmod,
            libc::SYS_fchmodat,
            libc::SYS_fchmodat2,
            libc::SYS_fchown,
            libc::SYS_fchownat,
            libc::SYS_utimensat,
            libc::SYS_setxattr,
            libc::SYS_lsetxattr,
            libc::SYS_fsetxattr,
            libc::SYS_removexattr,
            libc::SYS_lremovexattr,
            libc::SYS_fremovexattr,
            #[cfg(target_arch = "x86_64")]
            libc::SYS_chmod,
            #[cfg(target_arch = "x86_64")]
            libc::SYS_chown,
            #[cfg(target_arch = "x86_64")]
            libc::SYS_lchown,
            #[cfg(target_arch = "x86_64")]
            libc::SYS_utime,
            #[cfg(target_arch = "x86_64")]
            libc::SYS_utimes,
            #[cfg(target_arch = "x86_64")]
            libc::SYS_futimesat,
        ];
        for &nr in unconditional {
            rules.insert(nr, vec![]);
        }
        // シグナルは自分のプロセスの中だけ（tgkill・rt_tgsigqueueinfo・rt_sigqueueinfo の第1引数はプロセスの ID）。
        // Landlock のシグナルのスコープ（ABI 6 以上）がない古いカーネルでも、親やほかのプロセスへ送らせない
        let own_pid = u64::from(std::process::id());
        for nr in [libc::SYS_tgkill, libc::SYS_rt_tgsigqueueinfo, libc::SYS_rt_sigqueueinfo] {
            rules.insert(nr, vec![rule(vec![cond32(0, SeccompCmpOp::Ne, own_pid)?])?]);
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
        // AF_INET・AF_INET6・AF_NETLINK 以外のソケットを拒否する。AF_INET・AF_INET6 は TCP と UDP だけ（ICMP など
        // を拒否する）、AF_NETLINK はアドレスの一覧を読む NETLINK_ROUTE だけ（ほかのソケットの一覧などを読ませない）
        let other_family = rule(vec![
            cond32(0, SeccompCmpOp::Ne, as_u64(libc::AF_INET))?,
            cond32(0, SeccompCmpOp::Ne, as_u64(libc::AF_INET6))?,
            cond32(0, SeccompCmpOp::Ne, as_u64(libc::AF_NETLINK))?,
        ])?;
        let netlink_other = rule(vec![
            cond32(0, SeccompCmpOp::Eq, as_u64(libc::AF_NETLINK))?,
            cond32(2, SeccompCmpOp::Ne, as_u64(libc::NETLINK_ROUTE))?,
        ])?;
        let mut socket_rules = vec![other_family, netlink_other];
        for family in [libc::AF_INET, libc::AF_INET6] {
            socket_rules.push(rule(vec![
                cond32(0, SeccompCmpOp::Eq, as_u64(family))?,
                cond32(2, SeccompCmpOp::Ne, 0)?,
                cond32(2, SeccompCmpOp::Ne, as_u64(libc::IPPROTO_TCP))?,
                cond32(2, SeccompCmpOp::Ne, as_u64(libc::IPPROTO_UDP))?,
            ])?);
        }
        rules.insert(libc::SYS_socket, socket_rules);
        rules.insert(
            libc::SYS_socketpair,
            vec![rule(vec![
                cond32(0, SeccompCmpOp::Ne, as_u64(libc::AF_INET))?,
                cond32(0, SeccompCmpOp::Ne, as_u64(libc::AF_INET6))?,
                cond32(0, SeccompCmpOp::Ne, as_u64(libc::AF_NETLINK))?,
            ])?],
        );
        let filter = SeccompFilter::new(
            rules,
            SeccompAction::Allow,
            SeccompAction::Errno(errno_eperm()),
            arch()?,
        )
        .map_err(|e| format!("seccomp filter: {e}"))?;
        BpfProgram::try_from(filter).map_err(|e| format!("seccomp compile: {e}"))
    }

    /// x86_64 の上の x32 の呼び出し（番号に 0x40000000 を立てたもの）を止める。seccompiler の表はアーキテクチャの値
    /// （x32 も x86_64 と同じ AUDIT_ARCH_X86_64）と番号で照らすので、x32 の番号で呼ぶと拒否の表に当たらない
    /// （監査 2026-11-13 P8）。通信プロセスは x32 の呼び出しを使わないので、使えばプロセスを終わらせる
    #[cfg(target_arch = "x86_64")]
    pub(super) fn x32_filter() -> BpfProgram {
        const BPF_LD_W_ABS: u16 = 0x20;
        const BPF_JMP_JEQ_K: u16 = 0x15;
        const BPF_JMP_JSET_K: u16 = 0x45;
        const BPF_RET_K: u16 = 0x06;
        const AUDIT_ARCH_X86_64: u32 = 0xc000_003e;
        const X32_SYSCALL_BIT: u32 = 0x4000_0000;
        const SECCOMP_RET_KILL_PROCESS: u32 = 0x8000_0000;
        const SECCOMP_RET_ALLOW: u32 = 0x7fff_0000;
        // seccomp_data の nr は先頭、arch はその次（4バイト目）
        const OFFSET_NR: u32 = 0;
        const OFFSET_ARCH: u32 = 4;
        let insn = |code: u16, jt: u8, jf: u8, k: u32| sock_filter { code, jt, jf, k };
        vec![
            insn(BPF_LD_W_ABS, 0, 0, OFFSET_ARCH),
            // x86_64 でなければ（ほかのアーキテクチャの表が見る）通す
            insn(BPF_JMP_JEQ_K, 0, 3, AUDIT_ARCH_X86_64),
            insn(BPF_LD_W_ABS, 0, 0, OFFSET_NR),
            insn(BPF_JMP_JSET_K, 0, 1, X32_SYSCALL_BIT),
            insn(BPF_RET_K, 0, 0, SECCOMP_RET_KILL_PROCESS),
            insn(BPF_RET_K, 0, 0, SECCOMP_RET_ALLOW),
        ]
    }

    /// clone3 は ENOSYS（libc が clone に戻る。clone3 の引数は構造体なので中身を確かめられない）
    pub(super) fn clone3_filter() -> Result<BpfProgram, String> {
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

        #[cfg(target_arch = "x86_64")]
        seccompiler::apply_filter(&x32_filter()).map_err(|e| format!("seccomp: {e}"))?;
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

#[cfg(all(test, target_os = "linux"))]
mod tests {
    /// 拒否の表が組み立てられる（条件の値・引数の長さが正しい）
    #[test]
    fn filters_build() {
        assert!(super::linux::deny_filter().is_ok_and(|p| !p.is_empty()));
        assert!(super::linux::clone3_filter().is_ok_and(|p| !p.is_empty()));
    }

    /// x32 の番号（0x40000000 を立てたもの）でプロセスを終わらせる（監査 2026-11-13 P8）
    #[cfg(target_arch = "x86_64")]
    #[test]
    fn x32_calls_are_killed() {
        let program = super::linux::x32_filter();
        let jset = program.get(3).map(|i| (i.code, i.k));
        assert_eq!(jset, Some((0x45, 0x4000_0000)));
        let deny = program.get(4).map(|i| (i.code, i.k));
        assert_eq!(deny, Some((0x06, 0x8000_0000)));
        // x86_64 でなければ通す（4つ先の ALLOW へ）
        assert_eq!(program.get(1).map(|i| (i.jt, i.jf)), Some((0, 3)));
        assert_eq!(program.get(5).map(|i| i.k), Some(0x7fff_0000));
    }
}
