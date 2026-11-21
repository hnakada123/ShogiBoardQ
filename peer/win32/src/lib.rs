//! 通信プロセスが自分に掛かっている制限を確かめるための Win32 API の呼び出し（protocol/ipc.md 6章）
//!
//! Windows では、権限の制限は起動する側（対局・UI側）がプロセスを作るときに掛ける（AppContainer・ジョブ・
//! 子プロセスの禁止・緩和ポリシー・能力）。通信プロセスは、標準入力を読む前に、それらが掛かっていることをここで
//! 確かめ、掛かっているものを hello の mechanisms で起動した側へ知らせる。
//! unsafe を書くのはこのクレートだけで、外へは安全な関数だけを出す。Windows 以外では空。
//! エラーの文には OS の文（利用者の言語に訳される）を使わず、番号だけを書く（対局・UI側は、診断の行を印字できる
//! ASCII に絞るので、訳された文は欠ける）。

#![cfg(windows)]

use std::ffi::c_void;
use std::mem::{size_of, zeroed};
use std::ptr::null_mut;

use windows_sys::core::BOOL;
use windows_sys::Win32::Foundation::{CloseHandle, HANDLE};
use windows_sys::Win32::Security::{
    CheckTokenCapability, CreateWellKnownSid, GetTokenInformation, TokenIsAppContainer,
    WinCapabilityPrivateNetworkClientServerSid, SECURITY_MAX_SID_SIZE, TOKEN_QUERY,
};
use windows_sys::Win32::System::JobObjects::{
    IsProcessInJob, JobObjectExtendedLimitInformation, QueryInformationJobObject, JOBOBJECT_EXTENDED_LIMIT_INFORMATION,
    JOB_OBJECT_LIMIT_ACTIVE_PROCESS, JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE,
};
use windows_sys::Win32::System::Threading::{
    GetCurrentProcess, GetProcessMitigationPolicy, OpenProcess, OpenProcessToken, ProcessChildProcessPolicy,
    ProcessDynamicCodePolicy, ProcessExtensionPointDisablePolicy, ProcessFontDisablePolicy, ProcessImageLoadPolicy,
    ProcessSystemCallDisablePolicy, PROCESS_MITIGATION_POLICY, PROCESS_QUERY_LIMITED_INFORMATION,
};

/// 緩和ポリシーの値（PROCESS_MITIGATION_*_POLICY。どれも 32 ビットのフラグ1つ）
#[repr(C)]
struct MitigationFlags {
    flags: u32,
}

/// PROCESS_MITIGATION_CHILD_PROCESS_POLICY の NoChildProcessCreation
const NO_CHILD_PROCESS_CREATION: u32 = 1;
/// PROCESS_MITIGATION_DYNAMIC_CODE_POLICY の ProhibitDynamicCode
const PROHIBIT_DYNAMIC_CODE: u32 = 1;
/// PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY の DisallowWin32kSystemCalls
const DISALLOW_WIN32K_SYSTEM_CALLS: u32 = 1;
/// PROCESS_MITIGATION_IMAGE_LOAD_POLICY の NoRemoteImages・NoLowMandatoryLabelImages・PreferSystem32Images
const NO_REMOTE_IMAGES: u32 = 1;
const NO_LOW_MANDATORY_LABEL_IMAGES: u32 = 0b10;
const PREFER_SYSTEM32_IMAGES: u32 = 0b100;
/// PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY の DisableExtensionPoints
const DISABLE_EXTENSION_POINTS: u32 = 1;
/// PROCESS_MITIGATION_FONT_DISABLE_POLICY の DisableNonSystemFonts
const DISABLE_NON_SYSTEM_FONTS: u32 = 1;
/// SID を書く領域の大きさ（32 ビットの語の数。SID の中の 32 ビットの値がそろう並びにする）
const SID_WORDS: usize = SECURITY_MAX_SID_SIZE.div_ceil(4) as usize;

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
    /// トークンに、同じ LAN との送受信の能力（privateNetworkClientServer）がある（確かめられなければ true とみなす。
    /// 起動した側が「無い」ことを求めるときに、確かめられないまま通さない）
    pub private_network: bool,
    /// 実行時のコード生成の禁止（緩和ポリシー。読めなければ false。以下も同じ）
    pub no_dynamic_code: bool,
    /// win32k のシステムコールの禁止
    pub no_win32k: bool,
    /// ネットワーク上と低い整合性の場所からの DLL の読み込みの禁止（両方）
    pub no_untrusted_images: bool,
    /// OS の DLL を、アプリのフォルダより先に System32 から読む
    pub prefer_system32_images: bool,
    /// 拡張ポイント（古い形の DLL の差し込み）の禁止
    pub no_extension_points: bool,
    /// システムのフォント以外の読み込みの禁止
    pub no_non_system_fonts: bool,
}

/// 直前の Win32 API の呼び出しの失敗を、番号だけの文にする（OS の文は訳されるので使わない）
fn last_error(what: &str) -> String {
    let code = std::io::Error::last_os_error().raw_os_error().unwrap_or(0);
    format!("{what} failed (os error {code})")
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

/// このプロセスの緩和ポリシーの値（PROCESS_MITIGATION_*_POLICY のフラグ）
fn mitigation_flags(policy: PROCESS_MITIGATION_POLICY) -> Result<u32, String> {
    let mut value = MitigationFlags { flags: 0 };
    // SAFETY: 疑似ハンドル。書き込み先とその大きさは value のもの（どのポリシーの構造体も 32 ビットのフラグ1つ）
    let ok = unsafe {
        GetProcessMitigationPolicy(
            GetCurrentProcess(),
            policy,
            (&raw mut value).cast::<c_void>(),
            size_of::<MitigationFlags>(),
        )
    };
    if ok == 0 {
        return Err(last_error("GetProcessMitigationPolicy"));
    }
    Ok(value.flags)
}

/// 緩和ポリシーの、決めたビットがすべて立っているか（読めなければ false）
fn mitigation_on(policy: PROCESS_MITIGATION_POLICY, bits: u32) -> bool {
    mitigation_flags(policy).is_ok_and(|flags| flags & bits == bits)
}

/// トークンに、同じ LAN との送受信の能力（privateNetworkClientServer）があるか
fn has_private_network_capability() -> Result<bool, String> {
    let mut sid = [0u32; SID_WORDS];
    let mut size = SECURITY_MAX_SID_SIZE;
    // SAFETY: 書き込み先は sid（SECURITY_MAX_SID_SIZE バイト以上）で、その大きさを size で渡している。ドメインは使わない
    let created = unsafe {
        CreateWellKnownSid(
            WinCapabilityPrivateNetworkClientServerSid,
            null_mut(),
            sid.as_mut_ptr().cast::<c_void>(),
            &mut size,
        )
    };
    if created == 0 {
        return Err(last_error("CreateWellKnownSid"));
    }
    let mut has: BOOL = 0;
    // SAFETY: トークンに null を渡すと、呼び出したスレッド（なりすましていなければプロセス）のトークンを調べる。
    // sid は上で作った有効な SID で、呼び出しの間生きている。has は有効な書き込み先
    let ok = unsafe { CheckTokenCapability(null_mut(), sid.as_mut_ptr().cast::<c_void>(), &mut has) };
    if ok == 0 {
        return Err(last_error("CheckTokenCapability"));
    }
    Ok(has != 0)
}

/// このプロセスに掛かっている制限を読む。読めなければ理由
pub fn confinement() -> Result<Confinement, String> {
    let child = mitigation_flags(ProcessChildProcessPolicy)?;
    let mut c = Confinement {
        app_container: token_is_app_container()?,
        in_job: in_job()?,
        no_child_process: child & NO_CHILD_PROCESS_CREATION != 0,
        // 確かめられなければ「ある」とみなす（「無い」と知らせない）
        private_network: has_private_network_capability().unwrap_or(true),
        no_dynamic_code: mitigation_on(ProcessDynamicCodePolicy, PROHIBIT_DYNAMIC_CODE),
        no_win32k: mitigation_on(ProcessSystemCallDisablePolicy, DISALLOW_WIN32K_SYSTEM_CALLS),
        no_untrusted_images: mitigation_on(ProcessImageLoadPolicy, NO_REMOTE_IMAGES | NO_LOW_MANDATORY_LABEL_IMAGES),
        prefer_system32_images: mitigation_on(ProcessImageLoadPolicy, PREFER_SYSTEM32_IMAGES),
        no_extension_points: mitigation_on(ProcessExtensionPointDisablePolicy, DISABLE_EXTENSION_POINTS),
        no_non_system_fonts: mitigation_on(ProcessFontDisablePolicy, DISABLE_NON_SYSTEM_FONTS),
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
