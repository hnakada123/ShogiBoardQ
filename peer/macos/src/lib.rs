//! 通信プロセスが自分に権限の制限を掛けるための macOS の関数の呼び出し（protocol/ipc.md 6章）
//!
//! macOS では、通信プロセスが起動の直後に自分でサンドボックス（Seatbelt）を掛ける。掛ける規則（プロファイル）は
//! 通信プロセスの側（peer/src/sandbox.rs）が決め、ここは libSystem の関数を呼ぶだけ（自己試験で、禁止した操作
//! （プロセスの複製・sysctl の読み取り・ほかのプロセスの情報の読み取り）が拒否されるかを試す関数も置く）。
//! 掛ける前に作っておく CoreFoundation の主バンドル（resolve_main_bundle）の呼び出しもここに置く。
//! unsafe を書くのはこのクレートだけで、外へは安全な関数だけを出す。macOS 以外では空。

#![cfg(target_os = "macos")]

use std::ffi::{c_char, c_int, c_uint, c_void, CStr, CString};
use std::ptr::null_mut;

/// ptrace の要求: デバッガなどがこのプロセスに接続するのを断る（sys/ptrace.h）
const PT_DENY_ATTACH: c_int = 31;
/// sysctl の値を読む領域の大きさ（自己試験用。ほかのプロセスの引数（kern.argmax は既定で 1 MiB）が入る大きさ）
const SYSCTL_BUFFER_BYTES: usize = 2 * 1024 * 1024;
/// プロセスの一覧を読む領域の PID の数（自己試験用。読めたかだけを見るので、全部が入らなくてもよい）
const PID_LIST_CAPACITY: usize = 4096;
/// 実行ファイルの場所を読む領域の大きさ（libproc.h の PROC_PIDPATHINFO_MAXSIZE）
const PID_PATH_BYTES: usize = 4 * 1024;

extern "C" {
    /// sandbox.h。flags が 0 なら profile をプロファイルの本文（SBPL）として読む。掛けた制限は外せない
    fn sandbox_init(profile: *const c_char, flags: u64, errorbuf: *mut *mut c_char) -> c_int;
    fn sandbox_free_error(errorbuf: *mut c_char);
    /// このプロセス（pid）にサンドボックスが掛かっていれば 0 以外（operation が NULL のとき）
    fn sandbox_check(pid: c_int, operation: *const c_char, filter_type: c_int, ...) -> c_int;
    fn ptrace(request: c_int, pid: c_int, addr: *mut c_char, data: c_int) -> c_int;
    fn fork() -> c_int;
    fn _exit(status: c_int) -> !;
    fn waitpid(pid: c_int, status: *mut c_int, options: c_int) -> c_int;
    /// sys/sysctl.h。MIB（整数の列）で値を読む。読めなければ -1 で errno
    fn sysctl(
        name: *mut c_int,
        namelen: c_uint,
        oldp: *mut c_void,
        oldlenp: *mut usize,
        newp: *mut c_void,
        newlen: usize,
    ) -> c_int;
    /// sys/sysctl.h。名前で値を読む。読めなければ -1 で errno
    fn sysctlbyname(
        name: *const c_char,
        oldp: *mut c_void,
        oldlenp: *mut usize,
        newp: *mut c_void,
        newlen: usize,
    ) -> c_int;
    /// libproc.h。動いているプロセスの PID の一覧を buffer に書き、PID の数を返す。読めなければ 0 以下で errno
    fn proc_listallpids(buffer: *mut c_void, buffersize: c_int) -> c_int;
    /// libproc.h。プロセスの実行ファイルの場所を buffer に書き、バイト数を返す。読めなければ 0 で errno
    fn proc_pidpath(pid: c_int, buffer: *mut c_void, buffersize: u32) -> c_int;
}

// CoreFoundation（通信プロセスは、アドレスの変化を受ける if-watch の SystemConfiguration を通して、もうリンクしている）
#[link(name = "CoreFoundation", kind = "framework")]
extern "C" {
    /// CFBundle.h。このプロセスの主バンドル。初めて呼んだときに、実行ファイルのあるディレクトリを読んで作り、以後は
    /// 作ったものを返す（作れなければ NULL）。返る参照は CoreFoundation が持つ（解放しない）
    fn CFBundleGetMainBundle() -> *const c_void;
    /// CFBundle.h。バンドルの識別子（Info.plist を読んで作る。無ければ NULL）。返る参照はバンドルが持つ
    fn CFBundleGetIdentifier(bundle: *const c_void) -> *const c_void;
}

/// 直前の呼び出しの errno
fn last_errno() -> i32 {
    std::io::Error::last_os_error().raw_os_error().unwrap_or(0)
}

fn own_pid() -> Result<c_int, String> {
    c_int::try_from(std::process::id()).map_err(|_| "the process id does not fit".to_owned())
}

/// プロファイル（SBPL の本文）でこのプロセスにサンドボックスを掛ける。掛けられなければ理由
pub fn apply_profile(profile: &str) -> Result<(), String> {
    let text = CString::new(profile).map_err(|_| "the profile contains a NUL byte".to_owned())?;
    let mut error: *mut c_char = null_mut();
    // SAFETY: text は NUL で終わる有効な文字列で、呼び出しの間生きている。error は有効な書き込み先
    let result = unsafe { sandbox_init(text.as_ptr(), 0, &mut error) };
    if result == 0 {
        return Ok(());
    }
    let message = if error.is_null() {
        "sandbox_init failed".to_owned()
    } else {
        // SAFETY: 失敗のとき、sandbox_init は NUL で終わる文字列を error に入れる。写した後に解放する
        let text = unsafe { CStr::from_ptr(error) }.to_string_lossy().into_owned();
        // SAFETY: error は sandbox_init が確保したもので、ここで1回だけ解放する
        unsafe { sandbox_free_error(error) };
        text
    };
    Err(message)
}

/// このプロセスの主バンドル（CoreFoundation）を作っておく。作れれば true。
/// アドレスの変化を受ける SystemConfiguration（SCDynamicStoreCreateWithOptions）は、自分の識別子を主バンドルから
/// 読むので、サンドボックスを掛けた後に初めて主バンドルを作ると、実行ファイルのあるディレクトリの読み取りが
/// 拒否され、起動のたびにログ（送信元 Sandbox）に残る。掛ける前に作っておけば、後は作ったものを使い、ファイルを
/// 読まない。掛けた後に呼ぶと、作ってあるかを確かめられる（作っていなければ読めずに false）
pub fn resolve_main_bundle() -> bool {
    // SAFETY: 引数を取らない。返る参照は CoreFoundation が持ち、解放しない
    let bundle = unsafe { CFBundleGetMainBundle() };
    if bundle.is_null() {
        return false;
    }
    // SAFETY: bundle は CFBundleGetMainBundle が返した、NULL でない、プロセスの終わりまで有効なバンドル。
    // 識別子は SystemConfiguration が読む Info.plist（実行ファイルに埋め込まれたものを含む）を、ここで読んでおくために
    // 求めるだけで、使わない
    unsafe { CFBundleGetIdentifier(bundle) };
    true
}

/// このプロセスにサンドボックスが掛かっている
pub fn is_sandboxed() -> bool {
    let Ok(pid) = own_pid() else { return false };
    // SAFETY: operation が NULL のときは、可変の引数を読まずに、掛かっているかだけを返す
    unsafe { sandbox_check(pid, std::ptr::null(), 0) != 0 }
}

/// デバッガなどがこのプロセスに接続するのを断る（メモリを読ませない）。接続しようとした側が失敗する
pub fn deny_attach() -> Result<(), String> {
    // SAFETY: PT_DENY_ATTACH は pid・addr・data を使わない
    if unsafe { ptrace(PT_DENY_ATTACH, 0, null_mut(), 0) } == 0 {
        Ok(())
    } else {
        Err(std::io::Error::last_os_error().to_string())
    }
}

/// sysctl の値を名前で読めるか（自己試験用。値そのものは返さない）。読めれば Ok(読んだバイト数)、読めなければ
/// Err(errno。サンドボックスが拒否したときは EPERM)
pub fn sysctl_by_name(name: &str) -> Result<usize, i32> {
    let name = CString::new(name).map_err(|_| 0)?;
    let mut buffer = vec![0u8; SYSCTL_BUFFER_BYTES];
    let mut length = buffer.len();
    // SAFETY: name は NUL で終わる有効な文字列。oldp と oldlenp は buffer とその長さで、呼び出しの間生きている。
    // 書き込みはしない（newp は NULL、newlen は 0）
    let result = unsafe {
        sysctlbyname(
            name.as_ptr(),
            buffer.as_mut_ptr().cast::<c_void>(),
            &mut length,
            null_mut(),
            0,
        )
    };
    if result == 0 {
        Ok(length)
    } else {
        Err(last_errno())
    }
}

/// sysctl の値を MIB（整数の列。例えば kern.procargs2 は [CTL_KERN, KERN_PROCARGS2, pid]）で読めるか（自己試験用）。
/// 読めれば Ok(読んだバイト数)、読めなければ Err(errno。サンドボックスが拒否したときは EPERM)
pub fn sysctl_mib(mib: &[i32]) -> Result<usize, i32> {
    let mut name: Vec<c_int> = mib.to_vec();
    let count = c_uint::try_from(name.len()).map_err(|_| 0)?;
    let mut buffer = vec![0u8; SYSCTL_BUFFER_BYTES];
    let mut length = buffer.len();
    // SAFETY: name は count 個の c_int で、呼び出しの間生きている（sysctl は読むだけ）。oldp と oldlenp は buffer と
    // その長さ。書き込みはしない（newp は NULL、newlen は 0）
    let result = unsafe {
        sysctl(
            name.as_mut_ptr(),
            count,
            buffer.as_mut_ptr().cast::<c_void>(),
            &mut length,
            null_mut(),
            0,
        )
    };
    if result == 0 {
        Ok(length)
    } else {
        Err(last_errno())
    }
}

/// プロセスを複製できる（自己試験用。複製できたら子はすぐ終わる）
pub fn can_fork() -> bool {
    // SAFETY: 子は何も触らずに _exit だけを呼ぶ（async-signal-safe な関数だけ）
    let child = unsafe { fork() };
    if child == 0 {
        // SAFETY: 子のプロセスを、後始末を走らせずに終える
        unsafe { _exit(0) };
    }
    if child < 0 {
        return false;
    }
    // SAFETY: child は今作った子。状態は要らないので NULL を渡す
    unsafe { waitpid(child, null_mut(), 0) };
    true
}

/// 動いているプロセスの一覧を libproc（proc_listallpids）で読めるか（自己試験用。一覧そのものは返さない）。読めれば
/// Ok(PID の数)、読めなければ Err(errno。サンドボックスが拒否したときは EPERM)
pub fn list_all_pids() -> Result<usize, i32> {
    let mut buffer = vec![c_int::default(); PID_LIST_CAPACITY];
    let bytes = c_int::try_from(std::mem::size_of_val(buffer.as_slice())).map_err(|_| 0)?;
    // SAFETY: buffer は bytes バイトの書き込める領域で、呼び出しの間生きている
    let count = unsafe { proc_listallpids(buffer.as_mut_ptr().cast::<c_void>(), bytes) };
    match usize::try_from(count) {
        Ok(count) if count > 0 => Ok(count),
        _ => Err(last_errno()),
    }
}

/// プロセス（pid）の実行ファイルの場所を libproc（proc_pidpath）で読めるか（自己試験用。場所そのものは返さない）。
/// 読めれば Ok(バイト数)、読めなければ Err(errno。サンドボックスが拒否したときは EPERM)
pub fn pid_path(pid: i32) -> Result<usize, i32> {
    let mut buffer = vec![0u8; PID_PATH_BYTES];
    let size = u32::try_from(buffer.len()).map_err(|_| 0)?;
    // SAFETY: buffer は size バイトの書き込める領域で、呼び出しの間生きている
    let length = unsafe { proc_pidpath(pid, buffer.as_mut_ptr().cast::<c_void>(), size) };
    match usize::try_from(length) {
        Ok(length) if length > 0 => Ok(length),
        _ => Err(last_errno()),
    }
}
