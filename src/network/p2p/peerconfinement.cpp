/// @file peerconfinement.cpp
/// @brief 通信プロセスの起動のしかたの実装（Linux・macOS と Windows）

#include "peerconfinement.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRandomGenerator>

#ifdef Q_OS_WIN
#include <qt_windows.h>

#include <aclapi.h>
#include <userenv.h>

#include <array>
#include <string>
#include <utility>
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

std::unique_ptr<PeerConfinement> PeerConfinement::apply(QProcess* process, const QString&, Reach reach, QString*)
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

/// AppContainer の名前（SID はこの名前から決まる）。この PC のどのプレイヤーの通信プロセスも同じ AppContainer に入れる。
/// Windows は、別々の AppContainer どうしの同じ PC の中での接続（loopback と、自分の LAN のアドレスへの接続）を止めるので、
/// 分けると、同じ PC の2人（2つ目のアプリ）がローカルのネットワークで対局できない。同じ AppContainer のほかの通信プロセスが
/// 互いのプロセスとスレッドを開けないことは、利用者と SYSTEM だけを許す DACL で守る（監査 2026-11-21 S13。
/// WindowsConfinement::prepareSecurity）
const wchar_t kContainerName[] = L"io.github.hnakada123.ShogiBoardQ.peer";
const wchar_t kContainerDisplayName[] = L"ShogiBoardQ P2P communication process";

/// AppContainer のプロファイルの登録と削除を、同じ PC（ログオンのセッション）で動いているアプリの間で1つずつ行うための
/// ミューテックスと、プロファイルを使っている通信プロセスの数を数えるセマフォ。プロファイルは1つの名前を共有するので、
/// ほかのアプリの通信プロセスが終わって消したり、起動の前に消し直したりすると、登録してからプロセスを作るまでの間に
/// プロファイルが無くなり、プロセスを作れない（同じ PC の2人が同時に「接続」したとき）。数が 0 になったときだけ消す。
/// 名前付きのオブジェクトの既定の DACL は利用者と SYSTEM だけを許すので、AppContainer の通信プロセスからは開けない
const wchar_t kProfileLockName[] = L"Local\\io.github.hnakada123.ShogiBoardQ.peer.profile-lock";
const wchar_t kProfileUsersName[] = L"Local\\io.github.hnakada123.ShogiBoardQ.peer.profile-users";
/// プロファイルの登録・削除は短い。ほかのアプリが持ったまま止まっていても、アプリを止めない
constexpr DWORD kProfileLockTimeoutMs = 10000;

/// SID を入れる領域。SID の中の DWORD のために DWORD の境界にそろえる（BYTE の配列のままだと、メンバの並びによっては
/// 境界からずれ、CreateProcess が ERROR_NOACCESS で失敗する）
struct alignas(DWORD) SidBuffer : std::array<BYTE, SECURITY_MAX_SID_SIZE> {
};

QString winError(const char* what, DWORD code = GetLastError())
{
    return QStringLiteral("%1 failed (%2)").arg(QLatin1String(what)).arg(code);
}

/// kProfileLockName のミューテックスを、作ってから壊すまで持つ（持てなければ locked() が false）
class ProfileLock
{
public:
    ProfileLock()
        : m_mutex(CreateMutexW(nullptr, FALSE, kProfileLockName))
    {
        if (!m_mutex) return;
        // 持ったまま終わったアプリのもの（WAIT_ABANDONED）も持てたとみなす。守る中身はプロファイルと数だけで、
        // 数の食い違いは、すべてのアプリが終わるとセマフォごと消える
        const DWORD result = WaitForSingleObject(m_mutex, kProfileLockTimeoutMs);
        m_locked = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    }
    ~ProfileLock()
    {
        if (m_locked) ReleaseMutex(m_mutex);
        if (m_mutex) CloseHandle(m_mutex);
    }
    bool locked() const { return m_locked; }

private:
    Q_DISABLE_COPY_MOVE(ProfileLock)
    HANDLE m_mutex;
    bool m_locked = false;
};

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
    SidBuffer allPackages{};
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

class WindowsConfinement;

/// 通信プロセスが作られた直後（止めたまま）に、残りの制限を掛けて動かし始める（QProcess::started を受ける）
class SuspendedStart final : public QObject
{
public:
    explicit SuspendedStart(WindowsConfinement* confinement)
        : m_confinement(confinement)
    {
    }
    void onStarted();

private:
    WindowsConfinement* m_confinement;
};

class WindowsConfinement final : public PeerConfinement
{
public:
    ~WindowsConfinement() override
    {
        if (m_attributes) DeleteProcThreadAttributeList(m_attributes);
        // ジョブを閉じると、まだ動いている通信プロセスは終わる（JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE）
        if (m_job) CloseHandle(m_job);
        if (m_containerSid) FreeSid(m_containerSid);
        releaseProfile();
    }

    bool prepare(const QString& executable, Reach reach, QString* error)
    {
        if (!acquireProfile(error)) return false;
        // 能力: インターネットへの送受信と待受。同じ LAN は、ローカルのネットワークを使うときだけ。
        // loopback は能力では開かない（AppContainer からは OS が止める）
        if (!addCapability(WinCapabilityInternetClientSid) || !addCapability(WinCapabilityInternetClientServerSid)
            || (reach == Reach::LocalNetwork && !addCapability(WinCapabilityPrivateNetworkClientServerSid))) {
            *error = winError("CreateWellKnownSid");
            return false;
        }
        // この起動だけの能力（prepareSecurity の DACL で、この通信プロセスだけを許すために使う）
        if (!addInstanceCapability()) {
            *error = winError("creating the instance capability");
            return false;
        }
        m_security.AppContainerSid = m_containerSid;
        m_security.Capabilities = m_capabilities.data();
        m_security.CapabilityCount = m_capabilityCount;

        if (!allowExecution(executable, m_containerSid, error)) return false;
        if (!createJob(error)) return false;
        if (!prepareSecurity(error)) return false;

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

    /// 止めたまま作った通信プロセスの、この後に作るもの（スレッドなど）の DACL を利用者と SYSTEM だけにしてから動かす。
    /// できなければ動かさずに終わらせる（hello が届かないので、起動した側は失敗として扱う）
    void resume()
    {
        const PROCESS_INFORMATION* info = std::exchange(m_processInformation, nullptr);
        if (!info || !info->hProcess || !info->hThread) return;
        HANDLE token = nullptr;
        bool ok = OpenProcessToken(info->hProcess, TOKEN_ADJUST_DEFAULT, &token) != 0;
        if (ok) {
            TOKEN_DEFAULT_DACL dacl{};
            dacl.DefaultDacl = reinterpret_cast<PACL>(m_acl.data());
            ok = SetTokenInformation(token, TokenDefaultDacl, &dacl, sizeof(dacl)) != 0;
            CloseHandle(token);
        }
        if (!ok || ResumeThread(info->hThread) == static_cast<DWORD>(-1)) TerminateProcess(info->hProcess, 1);
    }

private:
    /// AppContainer のプロファイルを登録し、使っている通信プロセスの数に自分を足す。プロファイルを登録していない
    /// AppContainer ではプロセスを作れない（「ファイルが見つかりません」で失敗する）。ほかに使っている通信プロセスが
    /// なければ、前の起動が残したプロファイル（異常終了で消せなかったもの）を消してから登録し直す。使っているものが
    /// あれば（同じ PC のほかのプレイヤーの通信プロセスが動いているか、起動の途中）、消さずに名前から SID を求める
    bool acquireProfile(QString* error)
    {
        const ProfileLock lock;
        if (!lock.locked()) {
            *error = winError("locking the AppContainer profile");
            return false;
        }
        m_profileUsers = CreateSemaphoreW(nullptr, 0, LONG_MAX, kProfileUsersName);
        LONG previous = 0;
        if (!m_profileUsers || !ReleaseSemaphore(m_profileUsers, 1, &previous)) {
            *error = winError("counting the AppContainer profile users");
            return false;
        }
        m_profileCounted = true;
        if (previous == 0) DeleteAppContainerProfile(kContainerName);
        HRESULT result = CreateAppContainerProfile(kContainerName, kContainerDisplayName, kContainerDisplayName,
                                                   nullptr, 0, &m_containerSid);
        if (result == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
            result = DeriveAppContainerSidFromAppContainerName(kContainerName, &m_containerSid);
        }
        if (FAILED(result) || !m_containerSid) {
            *error = winError("creating the AppContainer profile", static_cast<DWORD>(result));
            return false;
        }
        return true;
    }

    /// 使っている通信プロセスの数から自分を引き、最後の1つなら AppContainer のプロファイル（利用者のレジストリと
    /// %LOCALAPPDATA%\Packages の下のフォルダ。AppContainer のプロセスが書き込める）を消す。乗っ取られた通信プロセスが、
    /// 次の起動まで残るものを置けないようにする（監査 2026-11-21 S13）。消せなければ（まだ使われている・ほかのアプリが
    /// ロックを持ったまま）、次に最初の通信プロセスを起動するときに消し直す
    void releaseProfile()
    {
        if (!m_profileUsers) return;
        if (m_profileCounted) {
            const ProfileLock lock;
            WaitForSingleObject(m_profileUsers, 0);
            if (lock.locked()) {
                // 数を読む関数はないので、もう1つ引けるか（ほかに使っているものがあるか）を試して戻す
                if (WaitForSingleObject(m_profileUsers, 0) == WAIT_OBJECT_0) {
                    ReleaseSemaphore(m_profileUsers, 1, nullptr);
                } else {
                    DeleteAppContainerProfile(kContainerName);
                }
            }
        }
        CloseHandle(m_profileUsers);
    }

    /// 通信プロセスのプロセスとスレッドに付ける DACL（利用者・SYSTEM と、この起動だけの能力）。AppContainer のトークンの
    /// 既定の DACL には AppContainer の SID が入るので、そのままでは同じ AppContainer のほかの通信プロセス（同じ PC の
    /// ほかのプレイヤー）が、このプロセスとスレッドを開ける（監査 2026-11-21 S13）。AppContainer のプロセスが開けるのは、
    /// DACL が自分の AppContainer の SID・「すべてのアプリケーション パッケージ」・自分の持つ能力のどれかを許すものだけ
    /// なので、AppContainer の SID の代わりに、この通信プロセスだけが持つ能力を許す（通信プロセスは自分のプロセスを開ける
    /// （Windows の IP のインターフェースの見張りが使う）が、ほかの通信プロセスは開けない）。起動した側（利用者）は開ける
    bool prepareSecurity(QString* error)
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            *error = winError("OpenProcessToken");
            return false;
        }
        DWORD size = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        m_tokenUser.resize(size);
        const bool haveUser = size > 0 && GetTokenInformation(token, TokenUser, m_tokenUser.data(), size, &size);
        CloseHandle(token);
        if (!haveUser) {
            *error = winError("GetTokenInformation");
            return false;
        }
        const PSID user = reinterpret_cast<const TOKEN_USER*>(m_tokenUser.data())->User.Sid;
        DWORD systemSize = static_cast<DWORD>(m_systemSid.size());
        if (!CreateWellKnownSid(WinLocalSystemSid, nullptr, m_systemSid.data(), &systemSize)) {
            *error = winError("CreateWellKnownSid");
            return false;
        }
        const PSID instance = m_instanceSid.data();
        const DWORD aclSize = sizeof(ACL) + 3 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD))
            + GetLengthSid(user) + GetLengthSid(m_systemSid.data()) + GetLengthSid(instance);
        // ACL の先頭は DWORD の境界にそろえる
        m_acl.assign((aclSize + sizeof(DWORD) - 1) / sizeof(DWORD), 0);
        auto* acl = reinterpret_cast<PACL>(m_acl.data());
        if (!InitializeAcl(acl, static_cast<DWORD>(m_acl.size() * sizeof(DWORD)), ACL_REVISION)
            || !AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, user)
            || !AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, m_systemSid.data())
            || !AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, instance)
            || !InitializeSecurityDescriptor(&m_descriptor, SECURITY_DESCRIPTOR_REVISION)
            || !SetSecurityDescriptorDacl(&m_descriptor, TRUE, acl, FALSE)) {
            *error = winError("building the process DACL");
            return false;
        }
        for (SECURITY_ATTRIBUTES* attributes : {&m_processSecurity, &m_threadSecurity}) {
            attributes->nLength = sizeof(SECURITY_ATTRIBUTES);
            attributes->lpSecurityDescriptor = &m_descriptor;
            attributes->bInheritHandle = FALSE;
        }
        return true;
    }

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

    /// この起動だけの能力の SID（S-1-15-3-1024- に続く6つの乱数。名前から作る能力と同じ形）を作って渡す
    bool addInstanceCapability()
    {
        if (m_capabilityCount >= m_capabilities.size()) return false;
        SID_IDENTIFIER_AUTHORITY authority = SECURITY_APP_PACKAGE_AUTHORITY;
        constexpr BYTE kSubAuthorities = 8;
        PSID sid = m_instanceSid.data();
        if (GetSidLengthRequired(kSubAuthorities) > m_instanceSid.size() || !InitializeSid(sid, &authority, kSubAuthorities)) {
            return false;
        }
        *GetSidSubAuthority(sid, 0) = SECURITY_CAPABILITY_BASE_RID;
        *GetSidSubAuthority(sid, 1) = SECURITY_CAPABILITY_APP_RID;
        for (DWORD i = 2; i < kSubAuthorities; ++i) *GetSidSubAuthority(sid, i) = QRandomGenerator::system()->generate();
        if (!IsValidSid(sid)) return false;
        m_capabilities[m_capabilityCount].Sid = sid;
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
        // 止めたまま作り、スレッドの DACL を決めてから動かす（resume。QProcess::started のとき）
        args->flags |= EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | DETACHED_PROCESS | CREATE_SUSPENDED;
        args->inheritHandles = TRUE;
        args->environment = m_environment.data();
        args->currentDirectory = m_directory.c_str();
        args->processAttributes = &m_processSecurity;
        args->threadAttributes = &m_threadSecurity;
        // QProcess が持つプロセスとスレッドのハンドル（プロセスが終わるまで QProcess が持ち続ける）
        m_processInformation = args->processInformation;
        return true;
    }

    PSID m_containerSid = nullptr;
    /// kProfileUsersName のセマフォ（数は、プロファイルを使っている通信プロセスの数）
    HANDLE m_profileUsers = nullptr;
    /// セマフォの数に自分を足した（releaseProfile で引く）
    bool m_profileCounted = false;
    std::array<SidBuffer, 3> m_capabilitySids{};
    SidBuffer m_instanceSid{};
    std::array<SID_AND_ATTRIBUTES, 4> m_capabilities{};
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
    std::vector<BYTE> m_tokenUser;
    SidBuffer m_systemSid{};
    std::vector<DWORD> m_acl;
    SECURITY_DESCRIPTOR m_descriptor{};
    SECURITY_ATTRIBUTES m_processSecurity{};
    SECURITY_ATTRIBUTES m_threadSecurity{};
    PROCESS_INFORMATION* m_processInformation = nullptr;
    SuspendedStart m_started{this};

public:
    /// QProcess::started を受けて、止めたまま作った通信プロセスを動かす
    SuspendedStart* startReceiver() { return &m_started; }
};

void SuspendedStart::onStarted()
{
    m_confinement->resume();
}

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
                                                        QString* error)
{
    auto confinement = std::make_unique<WindowsConfinement>();
    if (!confinement->prepare(executable, reach, error)) return nullptr;
    WindowsConfinement* raw = confinement.get();
    // 呼ばれるのは process->start() の中だけ。返したものは、呼び出し側がプロセスの終わりまで持つ
    process->setCreateProcessArgumentsModifier(
        [raw](QProcess::CreateProcessArguments* args) { raw->modify(args); });
    // started は start() の中で（CreateProcess の直後に）出る。止めたまま作った通信プロセスを、ここで動かす
    QObject::connect(process, &QProcess::started, raw->startReceiver(), &SuspendedStart::onStarted);
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

std::unique_ptr<PeerConfinement> PeerConfinement::apply(QProcess*, const QString&, Reach, QString* error)
{
    *error = QStringLiteral("the sandbox is not implemented for this operating system");
    return nullptr;
}

#endif
