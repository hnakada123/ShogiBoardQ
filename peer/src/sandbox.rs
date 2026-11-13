//! 権限の制限（protocol/ipc.md 6章、実装仕様 10.4節）
//!
//! Linux: 標準入力を読む前（外部の入力を読む前）に、スレッドを作る前の主スレッドで掛ける
//! （seccomp と Landlock は掛けたスレッドとその後に作るスレッドに効く）。
//!
//! Windows: 制限は起動する側（対局・UI側）がプロセスを作るときに掛ける（AppContainer・ジョブ・子プロセスの禁止）。
//! 通信プロセスは、標準入力を読む前に、それらが掛かっていることを確かめる。掛かっていなければ動かない。
//!
//! macOS: Linux と同じく、標準入力を読む前に通信プロセスが自分で掛ける（サンドボックス Seatbelt。プロセス全体に効く）。

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

#[cfg(windows)]
mod windows {
    use super::SandboxReport;

    /// 起動する側が掛けた制限を確かめる。必須のものが1つでも掛かっていなければ理由を返す
    pub fn apply(with_parent: bool) -> Result<SandboxReport, String> {
        if !with_parent {
            // 協力ノードは識別鍵のファイルを自分で読むので、起動の時点でファイルを読めなくする AppContainer とは合わない
            return Err("the service mode is not supported on Windows".to_owned());
        }
        let c = sbq_peer_win32::confinement()?;
        let mut mechanisms = Vec::new();
        if !c.app_container {
            return Err("not running in an AppContainer".to_owned());
        }
        mechanisms.push("appcontainer");
        if !c.in_job || !c.kill_on_job_close {
            return Err("not in a job that ends with the parent".to_owned());
        }
        mechanisms.push("job_kill_on_close");
        if c.active_process_limit != Some(1) {
            return Err("the job does not limit the number of processes to one".to_owned());
        }
        mechanisms.push("job_single_process");
        if !c.no_child_process {
            return Err("child processes are not forbidden".to_owned());
        }
        mechanisms.push("no_child_process");
        if !c.die_on_unhandled_exception {
            return Err("the job does not suppress the error reporting dialog".to_owned());
        }
        mechanisms.push("no_error_dialog");
        Ok(SandboxReport {
            applied: true,
            os: "windows",
            mechanisms,
            landlock_abi: 0,
        })
    }
}

#[cfg(target_os = "macos")]
mod macos {
    use nix::errno::Errno;
    use nix::sys::event::{EventFilter, EventFlag, FilterFlag, KEvent, Kqueue};
    use nix::sys::resource::{setrlimit, Resource};
    use nix::sys::signal::{kill, Signal};
    use nix::unistd::{getppid, Pid};

    use super::SandboxReport;

    /// サンドボックスの規則（SBPL）。既定ですべて拒否し、通信プロセスが使うものだけを許す:
    /// TCP と UDP の待受・着信・発信（IPv4・IPv6。Unix ドメインのソケットは許さない）、自分へのシグナル、
    /// sysctl の読み取り（インターフェースのアドレスの一覧・CPU の数など）、アドレスの変化の知らせを受ける configd。
    /// ファイルの読み書き・プログラムの起動・プロセスの複製・ほかのプロセスへのシグナル・ほかの Mach のサービスは
    /// 許していないので拒否される（開いてある標準入出力は使える）。インターフェースの一覧を読むたびに OS が確かめる
    /// 物理アドレス（MAC アドレス）の読み取りは、拒否したうえでログに残さない（一覧のたびに何行も出るので）
    pub(super) const PROFILE: &str = r#"(version 1)
(deny default)
(deny system-info (with no-log) (info-type "net.link.addr"))
(allow signal (target self))
(allow sysctl-read)
(allow mach-lookup (global-name "com.apple.SystemConfiguration.configd"))
(allow network-bind (local tcp "*:*") (local udp "*:*"))
(allow network-inbound (local tcp "*:*") (local udp "*:*"))
(allow network-outbound (remote tcp "*:*") (remote udp "*:*"))
"#;

    /// loopback への発信を OS の側でも止める規則（後ろの規則が勝つので、上の許可の後ろに足す）。ローカルのネットワーク
    /// （同じ PC・同じ LAN）を使わないときに足す。同じ PC で動いているほかのサービスへつながせない。
    /// 待受と着信には掛けない（`(local ip "localhost:*")` の拒否は、すべてのアドレスでの待受まで止めてしまう）
    pub(super) const NO_LOOPBACK: &str = r#"(deny network-outbound (remote ip "localhost:*"))
"#;

    /// 親の終了の知らせ（kqueue の NOTE_EXIT）を登録する。親がすでに終わっていれば失敗する
    fn watch_parent(ppid: i32) -> Result<Kqueue, String> {
        let ident = usize::try_from(ppid).map_err(|_| "invalid parent".to_owned())?;
        let queue = Kqueue::new().map_err(|e| format!("kqueue: {e}"))?;
        let change = KEvent::new(
            ident,
            EventFilter::EVFILT_PROC,
            EventFlag::EV_ADD | EventFlag::EV_ONESHOT,
            FilterFlag::NOTE_EXIT,
            0,
            0,
        );
        queue
            .kevent(&[change], &mut [], None)
            .map_err(|e| format!("watching the parent: {e}"))?;
        if getppid().as_raw() != ppid {
            return Err("parent already exited".to_owned());
        }
        Ok(queue)
    }

    /// 親が終わるまで待ち、終わったらこのプロセスを終わらせる（Linux の PR_SET_PDEATHSIG と同じく SIGKILL）
    fn die_with_parent(queue: Kqueue) {
        loop {
            let mut events = [KEvent::new(
                0,
                EventFilter::EVFILT_PROC,
                EventFlag::empty(),
                FilterFlag::empty(),
                0,
                0,
            )];
            match queue.kevent(&[], &mut events, None) {
                Ok(0) | Err(Errno::EINTR) => continue,
                // 親の終了の知らせか、見張れなくなった（どちらでも、親なしでは動き続けない）
                Ok(_) | Err(_) => break,
            }
        }
        let _ = kill(Pid::this(), Signal::SIGKILL);
        std::process::exit(1);
    }

    pub fn apply(initial_ppid: Option<i32>, no_loopback: bool) -> Result<SandboxReport, String> {
        let mut mechanisms = Vec::new();

        let parent_watch = match initial_ppid {
            Some(ppid) => Some(watch_parent(ppid)?),
            None => None,
        };

        setrlimit(Resource::RLIMIT_CORE, 0, 0).map_err(|e| format!("RLIMIT_CORE: {e}"))?;
        mechanisms.push("no_core_dump");
        sbq_peer_macos::deny_attach().map_err(|e| format!("PT_DENY_ATTACH: {e}"))?;
        mechanisms.push("deny_attach");

        let profile = if no_loopback {
            [PROFILE, NO_LOOPBACK].concat()
        } else {
            PROFILE.to_owned()
        };
        sbq_peer_macos::apply_profile(&profile).map_err(|e| format!("sandbox_init: {e}"))?;
        if !sbq_peer_macos::is_sandboxed() {
            return Err("the sandbox is not in effect".to_owned());
        }
        mechanisms.push("seatbelt");
        if no_loopback {
            mechanisms.push("seatbelt_no_loopback");
        }

        if let Some(queue) = parent_watch {
            std::thread::Builder::new()
                .name("parent-watch".to_owned())
                .spawn(move || die_with_parent(queue))
                .map_err(|e| format!("parent watch: {e}"))?;
            mechanisms.push("parent_death_watch");
        }

        Ok(SandboxReport {
            applied: true,
            os: "macos",
            mechanisms,
            landlock_abi: 0,
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

#[cfg(any(target_os = "linux", target_os = "macos"))]
fn nix_parent() -> i32 {
    nix::unistd::getppid().as_raw()
}

#[cfg(all(unix, not(any(target_os = "linux", target_os = "macos"))))]
fn nix_parent() -> i32 {
    0
}

/// 制限を掛ける（Linux と macOS。Windows では、起動する側が掛けた制限を確かめる）。掛けられなければ理由。initial_ppid を渡すと、
/// 親の終了で終わるようにする（協力ノードは渡さない）。no_loopback は、loopback への発信を OS の側でも止める
/// （`--no-loopback`。macOS だけ。OS の側で止められない Linux と、起動する側が決める Windows では受け付けない）
pub fn apply(initial_ppid: Option<i32>, no_loopback: bool) -> Result<SandboxReport, String> {
    #[cfg(target_os = "macos")]
    {
        macos::apply(initial_ppid, no_loopback)
    }
    #[cfg(not(target_os = "macos"))]
    {
        if no_loopback {
            return Err("--no-loopback is not supported on this operating system".to_owned());
        }
        apply_without_options(initial_ppid)
    }
}

#[cfg(not(target_os = "macos"))]
fn apply_without_options(initial_ppid: Option<i32>) -> Result<SandboxReport, String> {
    #[cfg(target_os = "linux")]
    {
        linux::apply(initial_ppid)
    }
    #[cfg(windows)]
    {
        windows::apply(initial_ppid.is_some())
    }
    #[cfg(not(any(target_os = "linux", windows)))]
    {
        let _ = initial_ppid;
        Err("the sandbox is not implemented for this operating system".to_owned())
    }
}

/// 制限を掛けた後に、禁止した操作を試す（--sandbox-self-test）。targets は試す相手（Windows は
/// `file=<パス>`・`pipe=<名前>`・`pid=<プロセス>`・`port=<loopback のポート>`。渡されなかったものは試さない。
/// macOS は `file=<パス>`・`pipe=<Unix ドメインのソケットのパス>`・`connect=<アドレス:ポート>`。file は渡されなければ
/// 決まった相手で試し、pipe と connect は渡されたときだけ試す。Linux は使わない）
pub fn self_test(report: &SandboxReport, targets: &[String]) -> Value {
    #[cfg(target_os = "linux")]
    {
        let _ = targets;
        let results = linux_probe::run();
        json!({
            "applied": report.applied,
            "mechanisms": report.mechanisms,
            "landlockAbi": report.landlock_abi,
            "results": results,
        })
    }
    #[cfg(windows)]
    {
        json!({
            "applied": report.applied,
            "mechanisms": report.mechanisms,
            "results": windows_probe::run(targets),
        })
    }
    #[cfg(target_os = "macos")]
    {
        json!({
            "applied": report.applied,
            "mechanisms": report.mechanisms,
            "results": macos_probe::run(targets),
        })
    }
    #[cfg(not(any(target_os = "linux", windows, target_os = "macos")))]
    {
        let _ = targets;
        json!({ "applied": report.applied, "results": {} })
    }
}

#[cfg(windows)]
mod windows_probe {
    use std::net::{SocketAddr, TcpStream};
    use std::time::Duration;

    use serde_json::{Map, Value};

    fn verdict(ok: bool) -> Value {
        Value::from(if ok { "allowed" } else { "denied" })
    }

    fn target<'a>(targets: &'a [String], name: &str) -> Option<&'a str> {
        targets.iter().find_map(|t| t.strip_prefix(name)?.strip_prefix('='))
    }

    /// WSAEACCES（フィルターが接続を止めた）
    const WSAEACCES: i32 = 10013;

    pub fn run(targets: &[String]) -> Value {
        let mut results = Map::new();

        if let Some(path) = target(targets, "file") {
            results.insert("read_file".into(), verdict(std::fs::File::open(path).is_ok()));
            let created = format!("{path}.created");
            let write = std::fs::File::create(&created);
            if write.is_ok() {
                let _ = std::fs::remove_file(&created);
            }
            results.insert("write_file".into(), verdict(write.is_ok()));
        }

        // 子プロセス（ジョブの上限と緩和ポリシーの両方で止める）
        let root = std::env::var("SystemRoot").unwrap_or_else(|_| r"C:\Windows".to_owned());
        let exec = std::process::Command::new(format!(r"{root}\System32\cmd.exe"))
            .args(["/c", "exit"])
            .status();
        results.insert("exec".into(), verdict(exec.is_ok()));

        // 名前付きパイプ（自動化 API など、ローカルのサービスの入口）
        if let Some(name) = target(targets, "pipe") {
            let pipe = std::fs::OpenOptions::new()
                .read(true)
                .write(true)
                .open(format!(r"\\.\pipe\{name}"));
            results.insert("connect_named_pipe".into(), verdict(pipe.is_ok()));
        }

        // ほかのプロセス（起動した側）を開く
        if let Some(pid) = target(targets, "pid").and_then(|t| t.parse::<u32>().ok()) {
            results.insert("open_process".into(), verdict(sbq_peer_win32::can_open_process(pid)));
        }

        // loopback で待ち受けているポート。つながるか、相手に拒まれた（OS の側では止めていない）なら allowed
        if let Some(port) = target(targets, "port").and_then(|t| t.parse::<u16>().ok()) {
            let addr = SocketAddr::from(([127, 0, 0, 1], port));
            let tcp = match TcpStream::connect_timeout(&addr, Duration::from_millis(1500)) {
                Ok(_) => true,
                Err(e) => e.kind() == std::io::ErrorKind::ConnectionRefused && e.raw_os_error() != Some(WSAEACCES),
            };
            results.insert("connect_loopback_tcp".into(), verdict(tcp));
        }

        Value::Object(results)
    }
}

#[cfg(target_os = "macos")]
mod macos_probe {
    use std::net::{SocketAddr, TcpStream};
    use std::time::Duration;

    use nix::errno::Errno;
    use serde_json::{Map, Value};

    fn verdict(ok: bool) -> Value {
        Value::from(if ok { "allowed" } else { "denied" })
    }

    fn target<'a>(targets: &'a [String], name: &str) -> Option<&'a str> {
        targets.iter().find_map(|t| t.strip_prefix(name)?.strip_prefix('='))
    }

    pub fn run(targets: &[String]) -> Value {
        let mut results = Map::new();

        let read = match target(targets, "file") {
            Some(path) => std::fs::File::open(path),
            None => std::fs::File::open("/etc/hosts"),
        };
        results.insert("read_file".into(), verdict(read.is_ok()));

        let created = match target(targets, "file") {
            Some(path) => format!("{path}.created"),
            None => format!("/tmp/sbq-peer-selftest-{}", std::process::id()),
        };
        let write = std::fs::File::create(&created);
        if write.is_ok() {
            let _ = std::fs::remove_file(&created);
        }
        results.insert("write_file".into(), verdict(write.is_ok()));

        let exec = std::process::Command::new("/usr/bin/true").status();
        results.insert("exec".into(), verdict(exec.is_ok()));

        results.insert("fork".into(), verdict(sbq_peer_macos::can_fork()));

        // Unix ドメインのソケット（自動化 API など、ローカルのサービスの入口）
        if let Some(path) = target(targets, "pipe") {
            let unix = std::os::unix::net::UnixStream::connect(path);
            results.insert("connect_unix_socket".into(), verdict(unix.is_ok()));
        }

        // 親へシグナル 0（届くかだけを確かめる）。サンドボックスが止めれば EPERM
        let signal = nix::sys::signal::kill(nix::unistd::getppid(), None);
        results.insert("signal_parent".into(), verdict(signal != Err(Errno::EPERM)));

        // loopback でない待受（すべてのアドレス）。どちらの起動のしかたでも許している
        let listen = std::net::TcpListener::bind(SocketAddr::from(([0, 0, 0, 0], 0)));
        results.insert("listen_tcp".into(), verdict(listen.is_ok()));

        // loopback でない宛先への接続（渡されたときだけ）。相手に拒まれた・届かないなら、OS の側では止めていない
        if let Some(addr) = target(targets, "connect").and_then(|t| t.parse::<SocketAddr>().ok()) {
            let remote = match TcpStream::connect_timeout(&addr, Duration::from_millis(3000)) {
                Ok(_) => true,
                Err(e) => e.kind() != std::io::ErrorKind::PermissionDenied,
            };
            results.insert("connect_tcp".into(), verdict(remote));
        }

        // loopback の閉じたポート（IPv4 と IPv6）。接続を拒まれたなら、OS の側では止めていない（allowed）。
        // `--no-loopback` を付けて起動されたときだけ denied になる
        let closed = [
            SocketAddr::from(([127, 0, 0, 1], 9)),
            SocketAddr::from((std::net::Ipv6Addr::LOCALHOST, 9)),
        ];
        let tcp = closed.iter().any(
            |addr| match TcpStream::connect_timeout(addr, Duration::from_millis(500)) {
                Ok(_) => true,
                Err(e) => e.kind() != std::io::ErrorKind::PermissionDenied,
            },
        );
        results.insert("connect_loopback_tcp".into(), verdict(tcp));

        Value::Object(results)
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
