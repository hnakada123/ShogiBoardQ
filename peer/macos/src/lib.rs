//! 通信プロセスが自分に権限の制限を掛けるための macOS の関数の呼び出し（protocol/ipc.md 6章）
//!
//! macOS では、通信プロセスが起動の直後に自分でサンドボックス（Seatbelt）を掛ける。掛ける規則（プロファイル）は
//! 通信プロセスの側（peer/src/sandbox.rs）が決め、ここは libSystem の関数を呼ぶだけ。
//! unsafe を書くのはこのクレートだけで、外へは安全な関数だけを出す。macOS 以外では空。

#![cfg(target_os = "macos")]

use std::ffi::{c_char, c_int, CStr, CString};
use std::ptr::null_mut;

/// ptrace の要求: デバッガなどがこのプロセスに接続するのを断る（sys/ptrace.h）
const PT_DENY_ATTACH: c_int = 31;

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
