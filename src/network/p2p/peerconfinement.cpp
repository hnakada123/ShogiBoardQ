/// @file peerconfinement.cpp
/// @brief 通信プロセスの起動のしかたの実装（Linux・macOS と Windows）

#include "peerconfinement.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>

#ifdef Q_OS_WIN
#include <qt_windows.h>

#include <aclapi.h>
#include <userenv.h>

#include <array>
#include <string>
#include <vector>
#endif

PeerConfinement::~PeerConfinement() = default;

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

namespace {

/// Linux と macOS では通信プロセスが自分で制限を掛けるので、持ち続けるものはない
class SelfConfinement final : public PeerConfinement
{
};

} // namespace

bool PeerConfinement::isSupported()
{
    return true;
}

QStringList PeerConfinement::requiredMechanisms(Reach reach)
{
#if defined(Q_OS_LINUX)
    Q_UNUSED(reach)
    return {QStringLiteral("no_new_privs"), QStringLiteral("landlock_fs"), QStringLiteral("seccomp"),
            QStringLiteral("parent_death_signal"), QStringLiteral("no_core_dump")};
#else
    QStringList names{QStringLiteral("seatbelt"), QStringLiteral("parent_death_watch"), QStringLiteral("no_core_dump"),
                      QStringLiteral("deny_attach")};
    // loopback への発信を止める規則も掛かっていること
    if (reach == Reach::Internet) names << QStringLiteral("seatbelt_no_loopback");
    return names;
#endif
}

std::unique_ptr<PeerConfinement> PeerConfinement::apply(QProcess* process, const QString&, Reach reach, int, QString*)
{
#if defined(Q_OS_MACOS)
    // ローカルのネットワークを使わないときは、loopback への発信をサンドボックスでも止めさせる（protocol/ipc.md 6.3節）
    if (reach == Reach::Internet) process->setArguments(process->arguments() << QStringLiteral("--no-loopback"));
#else
    Q_UNUSED(reach)
#endif
    // 起動した側のプロセス ID を渡す。通信プロセスは、最初に読んだ親がこれと違えば（起動の直後に親が終わり、ほかの
    // プロセスの子になった）、親の終了を見張れないので動かない（監査 2026-11-21 I-15）
    process->setArguments(process->arguments()
                          << QStringLiteral("--parent-pid") << QString::number(QCoreApplication::applicationPid()));
    // 環境変数は渡さない。標準入出力以外の記述子を引き継がない
    process->setProcessEnvironment(QProcessEnvironment());
    QProcess::UnixProcessParameters params;
    params.flags = QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::ResetSignalHandlers;
    process->setUnixProcessParameters(params);
    process->setWorkingDirectory(QDir::rootPath());
    return std::make_unique<SelfConfinement>();
}

#elif defined(Q_OS_WIN)

namespace {

/// AppContainer の名前（SID はこの名前から決まる）。この PC のプレイヤーごとに分ける（1人目は以前と同じ名前。2人目からは
/// 番号を付ける）。同じ AppContainer のプロセスは互いのプロセスとスレッドを開けるので、2人のプレイヤーの通信プロセスを
/// 同じ AppContainer に入れない（監査 2026-11-21 S13）
std::wstring containerName(int player)
{
    std::wstring name = L"io.github.hnakada123.ShogiBoardQ.peer";
    if (player > 1) name += L"-" + std::to_wstring(player);
    return name;
}
const wchar_t kContainerDisplayName[] = L"ShogiBoardQ P2P communication process";

QString winError(const char* what, DWORD code = GetLastError())
{
    return QStringLiteral("%1 failed (%2)").arg(QLatin1String(what)).arg(code);
}

/// 通信プロセスの実行ファイルを、AppContainer の中から読んで実行できるようにする。利用者のフォルダに置いたアプリ
/// （ZIP の展開先・ビルドのフォルダ）のファイルは AppContainer から読めないので、この AppContainer にだけ許す。
/// すでに読めるなら（Program Files など）何もしない。許せなければ起動に失敗し、P2P対局は使えない
bool allowExecution(const QString& executable, PSID containerSid, QString* error)
{
    const std::wstring path = QDir::toNativeSeparators(executable).toStdWString();
    PACL oldAcl = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    DWORD code = GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                       &oldAcl, nullptr, &descriptor);
    if (code != ERROR_SUCCESS) {
        *error = winError("GetNamedSecurityInfo", code);
        return false;
    }
    // この AppContainer か、すべてのアプリケーション パッケージに読み取りと実行を許す項目があれば足さない
    std::array<BYTE, SECURITY_MAX_SID_SIZE> allPackages{};
    DWORD sidSize = static_cast<DWORD>(allPackages.size());
    const bool haveAllPackages =
        CreateWellKnownSid(WinBuiltinAnyPackageSid, nullptr, allPackages.data(), &sidSize) != 0;
    bool allowed = false;
    const DWORD needed = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
    for (DWORD i = 0; oldAcl && i < oldAcl->AceCount && !allowed; ++i) {
        void* ace = nullptr;
        if (!GetAce(oldAcl, i, &ace)) break;
        const auto* header = static_cast<const ACE_HEADER*>(ace);
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE || (header->AceFlags & INHERIT_ONLY_ACE)) continue;
        auto* allow = static_cast<ACCESS_ALLOWED_ACE*>(ace);
        PSID sid = &allow->SidStart;
        if ((allow->Mask & needed) != needed) continue;
        allowed = EqualSid(sid, containerSid) || (haveAllPackages && EqualSid(sid, allPackages.data()));
    }
    bool ok = true;
    if (!allowed) {
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = GENERIC_READ | GENERIC_EXECUTE;
        access.grfAccessMode = GRANT_ACCESS;
        access.grfInheritance = NO_INHERITANCE;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
        access.Trustee.ptstrName = static_cast<LPWSTR>(containerSid);
        PACL newAcl = nullptr;
        code = SetEntriesInAclW(1, &access, oldAcl, &newAcl);
        if (code == ERROR_SUCCESS) {
            // 継承した項目はそのままにする（保護しない）
            code = SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                         nullptr, nullptr, newAcl, nullptr);
        }
        if (newAcl) LocalFree(newAcl);
        if (code != ERROR_SUCCESS) {
            *error = winError("granting the AppContainer access to the executable", code);
            ok = false;
        }
    }
    LocalFree(descriptor);
    return ok;
}

class WindowsConfinement final : public PeerConfinement
{
public:
    ~WindowsConfinement() override
    {
        if (m_attributes) DeleteProcThreadAttributeList(m_attributes);
        // ジョブを閉じると、まだ動いている通信プロセスは終わる（JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE）
        if (m_job) CloseHandle(m_job);
        if (m_containerSid) FreeSid(m_containerSid);
        // AppContainer のプロファイル（利用者のレジストリと %LOCALAPPDATA%\Packages の下のフォルダ。AppContainer の
        // プロセスが書き込める）を消す。乗っ取られた通信プロセスが、次の起動まで残るものを置けないようにする（監査
        // 2026-11-21 S13）。消せなければ（まだ使われている）、次に起動するときに消し直す
        if (!m_containerName.empty()) DeleteAppContainerProfile(m_containerName.c_str());
    }

    bool prepare(const QString& executable, Reach reach, int player, QString* error)
    {
        // プロファイルを登録していない AppContainer ではプロセスを作れない（「ファイルが見つかりません」で失敗する）。
        // 前の起動が残したプロファイル（異常終了で消せなかったもの）は消してから、起動ごとに登録し直す。まだ使われて
        // いて消せなければ、名前から SID を求める
        m_containerName = containerName(player);
        DeleteAppContainerProfile(m_containerName.c_str());
        HRESULT result = CreateAppContainerProfile(m_containerName.c_str(), kContainerDisplayName, kContainerDisplayName,
                                                   nullptr, 0, &m_containerSid);
        if (result == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
            result = DeriveAppContainerSidFromAppContainerName(m_containerName.c_str(), &m_containerSid);
        }
        if (FAILED(result) || !m_containerSid) {
            *error = winError("creating the AppContainer profile", static_cast<DWORD>(result));
            return false;
        }
        // 能力: インターネットへの送受信と待受。同じ LAN は、ローカルのネットワークを使うときだけ。
        // loopback は能力では開かない（AppContainer からは OS が止める）
        if (!addCapability(WinCapabilityInternetClientSid) || !addCapability(WinCapabilityInternetClientServerSid)
            || (reach == Reach::LocalNetwork && !addCapability(WinCapabilityPrivateNetworkClientServerSid))) {
            *error = winError("CreateWellKnownSid");
            return false;
        }
        m_security.AppContainerSid = m_containerSid;
        m_security.Capabilities = m_capabilities.data();
        m_security.CapabilityCount = m_capabilityCount;

        if (!allowExecution(executable, m_containerSid, error)) return false;
        if (!createJob(error)) return false;

        // 緩和ポリシー: ウィンドウとフォントの系統のシステムコール・拡張の読み込み・実行時のコード生成・
        // ネットワーク上と低い整合性の場所からの DLL の読み込みを使えなくする。OS の DLL は、アプリのフォルダより先に
        // System32 から読む（監査 2026-11-21 I-14）
        m_mitigation = PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_STRICT_HANDLE_CHECKS_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_FONT_DISABLE_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_REMOTE_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_ON
            | PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON;
        m_childPolicy = PROCESS_CREATION_CHILD_PROCESS_RESTRICTED;

        // 属性: AppContainer・ジョブ・緩和ポリシー・子プロセスの禁止・引き継ぐハンドル（最後のものは起動の直前に入れる）
        constexpr DWORD kAttributeCount = 5;
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, kAttributeCount, 0, &size);
        m_attributeBuffer.resize(size);
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(m_attributeBuffer.data());
        if (!InitializeProcThreadAttributeList(list, kAttributeCount, 0, &size)) {
            *error = winError("InitializeProcThreadAttributeList");
            return false;
        }
        m_attributes = list;
        if (!UpdateProcThreadAttribute(m_attributes, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &m_security,
                                       sizeof(m_security), nullptr, nullptr)
            || !UpdateProcThreadAttribute(m_attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &m_job, sizeof(m_job),
                                          nullptr, nullptr)
            || !UpdateProcThreadAttribute(m_attributes, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &m_mitigation,
                                          sizeof(m_mitigation), nullptr, nullptr)
            || !UpdateProcThreadAttribute(m_attributes, 0, PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY, &m_childPolicy,
                                          sizeof(m_childPolicy), nullptr, nullptr)) {
            *error = winError("UpdateProcThreadAttribute");
            return false;
        }

        // 環境変数は SystemRoot（Winsock が要る）と LOCALAPPDATA だけ。LOCALAPPDATA は、無いと AppContainer の
        // プロセスを作れない（OS が AppContainer のフォルダに置き換えて渡す）。作業フォルダは AppContainer から
        // 開けるシステムのフォルダ
        std::array<wchar_t, MAX_PATH> buffer{};
        UINT length = GetSystemWindowsDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
        if (length == 0 || length >= buffer.size()) {
            *error = winError("GetSystemWindowsDirectory");
            return false;
        }
        const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
        if (localAppData.isEmpty() || localAppData.contains(QLatin1Char('='))) {
            *error = QStringLiteral("LOCALAPPDATA is not set");
            return false;
        }
        m_environment = L"LOCALAPPDATA=" + localAppData.toStdWString();
        m_environment.push_back(L'\0');
        m_environment += L"SystemRoot=" + std::wstring(buffer.data(), length);
        m_environment.push_back(L'\0');
        m_environment.push_back(L'\0');
        length = GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
        if (length == 0 || length >= buffer.size()) {
            *error = winError("GetSystemDirectory");
            return false;
        }
        m_directory.assign(buffer.data(), length);
        return true;
    }

    /// CreateProcess の引数を差し替える（QProcess が起動の直前に呼ぶ）。差し替えられなければ、起動そのものを失敗させる
    void modify(QProcess::CreateProcessArguments* args)
    {
        if (!fill(args)) {
            // 制限なしで起動させない（実行ファイルもコマンドラインも無い CreateProcess は失敗する）
            args->applicationName = nullptr;
            args->arguments = nullptr;
        }
    }

private:
    bool addCapability(WELL_KNOWN_SID_TYPE type)
    {
        if (m_capabilityCount >= m_capabilities.size()) return false;
        auto& bytes = m_capabilitySids[m_capabilityCount];
        DWORD size = static_cast<DWORD>(bytes.size());
        if (!CreateWellKnownSid(type, nullptr, bytes.data(), &size)) return false;
        m_capabilities[m_capabilityCount].Sid = bytes.data();
        m_capabilities[m_capabilityCount].Attributes = SE_GROUP_ENABLED;
        ++m_capabilityCount;
        return true;
    }

    /// 通信プロセスだけを入れるジョブ。起動した側が終わる（ハンドルが閉じる）と通信プロセスも終わり、プロセスは1つだけ
    bool createJob(QString* error)
    {
        m_job = CreateJobObjectW(nullptr, nullptr);
        if (!m_job) {
            *error = winError("CreateJobObject");
            return false;
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_ACTIVE_PROCESS
            | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
        limits.BasicLimitInformation.ActiveProcessLimit = 1;
        JOBOBJECT_BASIC_UI_RESTRICTIONS ui{};
        ui.UIRestrictionsClass = JOB_OBJECT_UILIMIT_DESKTOP | JOB_OBJECT_UILIMIT_DISPLAYSETTINGS
            | JOB_OBJECT_UILIMIT_EXITWINDOWS | JOB_OBJECT_UILIMIT_GLOBALATOMS | JOB_OBJECT_UILIMIT_HANDLES
            | JOB_OBJECT_UILIMIT_READCLIPBOARD | JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS
            | JOB_OBJECT_UILIMIT_WRITECLIPBOARD;
        if (!SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))
            || !SetInformationJobObject(m_job, JobObjectBasicUIRestrictions, &ui, sizeof(ui))) {
            *error = winError("SetInformationJobObject");
            return false;
        }
        return true;
    }

    bool fill(QProcess::CreateProcessArguments* args)
    {
        if (!m_attributes || !args->startupInfo || m_handlesSet) return false;
        // 引き継ぐのは標準入出力のパイプだけ（ほかの継承できるハンドルを渡さない）
        DWORD count = 0;
        for (HANDLE handle :
             {args->startupInfo->hStdInput, args->startupInfo->hStdOutput, args->startupInfo->hStdError}) {
            if (!handle || handle == INVALID_HANDLE_VALUE) return false;
            bool seen = false;
            for (DWORD i = 0; i < count; ++i) seen = seen || m_handles[i] == handle;
            if (!seen) m_handles[count++] = handle;
        }
        if (!UpdateProcThreadAttribute(m_attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, m_handles.data(),
                                       count * sizeof(HANDLE), nullptr, nullptr)) {
            return false;
        }
        m_handlesSet = true;
        m_startup.StartupInfo = *args->startupInfo;
        m_startup.StartupInfo.cb = sizeof(m_startup);
        m_startup.lpAttributeList = m_attributes;
        args->startupInfo = &m_startup.StartupInfo;
        // コンソールを持たせない。コンソールを作ると OS がそのためのプロセス（conhost）を起動しようとし、
        // 子プロセスの禁止に当たって通信プロセスが初期化に失敗する
        args->flags &= ~static_cast<DWORD>(CREATE_NO_WINDOW | CREATE_NEW_CONSOLE);
        args->flags |= EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | DETACHED_PROCESS;
        args->inheritHandles = TRUE;
        args->environment = m_environment.data();
        args->currentDirectory = m_directory.c_str();
        return true;
    }

    std::wstring m_containerName;
    PSID m_containerSid = nullptr;
    std::array<std::array<BYTE, SECURITY_MAX_SID_SIZE>, 3> m_capabilitySids{};
    std::array<SID_AND_ATTRIBUTES, 3> m_capabilities{};
    DWORD m_capabilityCount = 0;
    SECURITY_CAPABILITIES m_security{};
    HANDLE m_job = nullptr;
    DWORD64 m_mitigation = 0;
    DWORD m_childPolicy = 0;
    std::array<HANDLE, 3> m_handles{};
    bool m_handlesSet = false;
    std::vector<BYTE> m_attributeBuffer;
    LPPROC_THREAD_ATTRIBUTE_LIST m_attributes = nullptr;
    STARTUPINFOEXW m_startup{};
    std::wstring m_environment;
    std::wstring m_directory;
};

} // namespace

bool PeerConfinement::isSupported()
{
    return true;
}

QStringList PeerConfinement::requiredMechanisms(Reach reach)
{
    // 起動のときに掛けた緩和ポリシー（WindowsConfinement::prepare）も、掛かっていることを通信プロセスに確かめさせる
    QStringList names{QStringLiteral("appcontainer"),          QStringLiteral("job_kill_on_close"),
                      QStringLiteral("job_single_process"),    QStringLiteral("no_child_process"),
                      QStringLiteral("no_error_dialog"),       QStringLiteral("no_dynamic_code"),
                      QStringLiteral("no_win32k"),             QStringLiteral("no_untrusted_images"),
                      QStringLiteral("prefer_system32_images"), QStringLiteral("no_extension_points"),
                      QStringLiteral("no_non_system_fonts")};
    // 同じ LAN へ出る能力は、ローカルのネットワークを使うときだけ渡す（渡していないこと）
    if (reach == Reach::Internet) names << QStringLiteral("no_private_network");
    return names;
}

std::unique_ptr<PeerConfinement> PeerConfinement::apply(QProcess* process, const QString& executable, Reach reach,
                                                        int player, QString* error)
{
    auto confinement = std::make_unique<WindowsConfinement>();
    if (!confinement->prepare(executable, reach, player, error)) return nullptr;
    WindowsConfinement* raw = confinement.get();
    // 呼ばれるのは process->start() の中だけ。返したものは、呼び出し側がプロセスの終わりまで持つ
    process->setCreateProcessArgumentsModifier(
        [raw](QProcess::CreateProcessArguments* args) { raw->modify(args); });
    return confinement;
}

#else

bool PeerConfinement::isSupported()
{
    return false;
}

QStringList PeerConfinement::requiredMechanisms(Reach)
{
    return {};
}

std::unique_ptr<PeerConfinement> PeerConfinement::apply(QProcess*, const QString&, Reach, int, QString* error)
{
    *error = QStringLiteral("the sandbox is not implemented for this operating system");
    return nullptr;
}

#endif
