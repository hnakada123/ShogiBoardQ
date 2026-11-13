//! 通信プロセスが自分に掛かっている制限を確かめるための Win32 API の呼び出し（protocol/ipc.md 6章）
//!
//! Windows では、権限の制限は起動する側（対局・UI側）がプロセスを作るときに掛ける（AppContainer・ジョブ・
//! 子プロセスの禁止）。通信プロセスは、標準入力を読む前に、それらが掛かっていることをここで確かめる。
//! unsafe を書くのはこのクレートだけで、外へは安全な関数だけを出す。Windows 以外では空。

#![cfg(windows)]

use std::ffi::c_void;
use std::mem::{size_of, zeroed};
use std::ptr::null_mut;

use windows_sys::core::BOOL;
use windows_sys::Win32::Foundation::{CloseHandle, HANDLE};
use windows_sys::Win32::Security::{GetTokenInformation, TokenIsAppContainer, TOKEN_QUERY};
use windows_sys::Win32::System::JobObjects::{
    IsProcessInJob, JobObjectExtendedLimitInformation, QueryInformationJobObject, JOBOBJECT_EXTENDED_LIMIT_INFORMATION,
    JOB_OBJECT_LIMIT_ACTIVE_PROCESS, JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE,
};
use windows_sys::Win32::System::Threading::{
    GetCurrentProcess, GetProcessMitigationPolicy, OpenProcess, OpenProcessToken, ProcessChildProcessPolicy,
    PROCESS_QUERY_LIMITED_INFORMATION,
};

/// PROCESS_MITIGATION_CHILD_PROCESS_POLICY（32ビットのフラグ1つ。最下位のビットが NoChildProcessCreation）
#[repr(C)]
struct ChildProcessPolicy {
    flags: u32,
}

/// このプロセスに掛かっている制限
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Confinement {
    /// トークンが AppContainer のもの
    pub app_container: bool,
    /// ジョブに入っている
    pub in_job: bool,
    /// ジョブのハンドルが閉じられたら（起動した側が終わったら）終わる
    pub kill_on_job_close: bool,
    /// 処理されない例外で、エラーの報告の画面を出さずに終わる
    pub die_on_unhandled_exception: bool,
    /// ジョブの中で動けるプロセスの数の上限（上限がなければ None）
    pub active_process_limit: Option<u32>,
    /// 子プロセスを作れない（プロセスの緩和ポリシー）
    pub no_child_process: bool,
}

fn last_error(what: &str) -> String {
    format!("{what}: {}", std::io::Error::last_os_error())
}

fn size_u32<T>() -> u32 {
    u32::try_from(size_of::<T>()).unwrap_or(0)
}

fn token_is_app_container() -> Result<bool, String> {
    let mut token: HANDLE = null_mut();
    // SAFETY: GetCurrentProcess は閉じなくてよい疑似ハンドルを返す。token は有効な書き込み先
    if unsafe { OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &mut token) } == 0 {
        return Err(last_error("OpenProcessToken"));
    }
    let mut value: u32 = 0;
    let mut returned: u32 = 0;
    // SAFETY: token は上で開いた。書き込み先は u32 1つで、その大きさを渡している
    let ok = unsafe {
        GetTokenInformation(
            token,
            TokenIsAppContainer,
            (&raw mut value).cast::<c_void>(),
            size_u32::<u32>(),
            &mut returned,
        )
    };
    let result = if ok == 0 {
        Err(last_error("GetTokenInformation"))
    } else {
        Ok(value != 0)
    };
    // SAFETY: token は上で開いたハンドルで、ここで1回だけ閉じる
    unsafe { CloseHandle(token) };
    result
}

fn in_job() -> Result<bool, String> {
    let mut result: BOOL = 0;
    // SAFETY: 疑似ハンドルと、有効な書き込み先。ジョブに null を渡すと、どれかのジョブに入っているかを返す
    if unsafe { IsProcessInJob(GetCurrentProcess(), null_mut(), &mut result) } == 0 {
        return Err(last_error("IsProcessInJob"));
    }
    Ok(result != 0)
}

/// 自分が入っているジョブ（入れ子なら最も内側）の制限
fn job_limits() -> Result<(u32, u32), String> {
    // SAFETY: 整数とポインタだけの構造体で、すべて 0 のビット列は有効な値
    let mut info: JOBOBJECT_EXTENDED_LIMIT_INFORMATION = unsafe { zeroed() };
    // SAFETY: ジョブに null を渡すと、呼び出したプロセスのジョブを読む。書き込み先とその大きさは info のもの
    let ok = unsafe {
        QueryInformationJobObject(
            null_mut(),
            JobObjectExtendedLimitInformation,
            (&raw mut info).cast::<c_void>(),
            size_u32::<JOBOBJECT_EXTENDED_LIMIT_INFORMATION>(),
            null_mut(),
        )
    };
    if ok == 0 {
        return Err(last_error("QueryInformationJobObject"));
    }
    Ok((
        info.BasicLimitInformation.LimitFlags,
        info.BasicLimitInformation.ActiveProcessLimit,
    ))
}

fn child_process_forbidden() -> Result<bool, String> {
    let mut policy = ChildProcessPolicy { flags: 0 };
    // SAFETY: 疑似ハンドル。書き込み先とその大きさは policy のもの
    let ok = unsafe {
        GetProcessMitigationPolicy(
            GetCurrentProcess(),
            ProcessChildProcessPolicy,
            (&raw mut policy).cast::<c_void>(),
            size_of::<ChildProcessPolicy>(),
        )
    };
    if ok == 0 {
        return Err(last_error("GetProcessMitigationPolicy"));
    }
    Ok(policy.flags & 1 != 0)
}

/// このプロセスに掛かっている制限を読む。読めなければ理由
pub fn confinement() -> Result<Confinement, String> {
    let mut c = Confinement {
        app_container: token_is_app_container()?,
        in_job: in_job()?,
        no_child_process: child_process_forbidden()?,
        ..Confinement::default()
    };
    if c.in_job {
        let (flags, active) = job_limits()?;
        c.kill_on_job_close = flags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE != 0;
        c.die_on_unhandled_exception = flags & JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION != 0;
        c.active_process_limit = (flags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS != 0).then_some(active);
    }
    Ok(c)
}

/// ほかのプロセスを、最も弱い権利（終了コードなどの照会）で開けるか（--sandbox-self-test）
pub fn can_open_process(pid: u32) -> bool {
    // SAFETY: 引数は整数だけ。戻り値が null でなければ、ここで閉じる
    let handle = unsafe { OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, 0, pid) };
    if handle.is_null() {
        return false;
    }
    // SAFETY: handle は上で開いたハンドルで、ここで1回だけ閉じる
    unsafe { CloseHandle(handle) };
    true
}
