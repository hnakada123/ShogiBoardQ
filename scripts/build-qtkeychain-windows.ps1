#
# Windows 用の QtKeychain を、アプリと同じ Qt でソースからビルドする。
#
# P2P対局のプレイヤー鍵を Windows の資格情報マネージャーに置くのに使う（無ければ、プレイヤー鍵の保管で
# 「OSの鍵の保管庫」を選べない）。Qt Online Installer の Qt には QtKeychain が無いので、公式のソースを
# SHA-256 で確かめて、静的ライブラリとしてビルドする（アプリに取り込まれるので、同梱する DLL は増えない）。
# 詳細: docs/dev/windows-build-and-release.md 3.4節
#
# Usage:
#   .\scripts\build-qtkeychain-windows.ps1 [OPTIONS]
#
# Options:
#   -QtPrefix DIR    ビルドに使う Qt の場所（例: C:\Qt\6.12.0\msvc2022_64）。
#                    省くと CMake が見つける Qt（CMAKE_PREFIX_PATH・PATH）
#   -BuildType TYPE  Release（既定）か Debug。アプリと同じにする（MSVC は、Debug と Release で C++ の
#                    ランタイムが違い、混ぜるとリンクできない）
#   -WorkDir DIR     ソースの展開とビルドに使うフォルダ（既定: build-qtkeychain）。
#                    インストール先は <DIR>\install
#   -Help            このヘルプを表示
#
# 実行前に "Developer PowerShell for VS" などで cl.exe・cmake を PATH に入れておく（アプリのビルドと同じ）。
# 終わると、アプリの構成に渡すオプション（-DQt6Keychain_DIR=...）を標準出力の最後の行に書く。

param(
    [string]$QtPrefix = "",
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release",
    [string]$WorkDir = "",
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# 版を上げるときは、https://github.com/frankosterfeld/qtkeychain/releases の版と、その
# ソース（archive/refs/tags/<版>.tar.gz）の SHA-256 に替える（scripts/build-qtkeychain-macos.sh と同じ版にする）。
# ライセンスの文書（resources/licenses/NOTICE*.md の QtKeychain の節）の著作権表示も確かめる
$QTKEYCHAIN_VERSION = "0.18.0"
$QTKEYCHAIN_SHA256 = "837af59f2b950760a73eff81ccae817f7d3051183b447438dc610bb984a41691"
$QTKEYCHAIN_URL = "https://github.com/frankosterfeld/qtkeychain/archive/refs/tags/$QTKEYCHAIN_VERSION.tar.gz"

# 進み具合は画面へ書く（標準出力は最後の1行だけ）
function Write-Info($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }
function Stop-WithError($msg) {
    Write-Host "==> ERROR: $msg" -ForegroundColor Red
    exit 1
}

if ($Help) {
    Get-Content -Encoding UTF8 $MyInvocation.MyCommand.Path | Select-Object -Skip 1 -First 21 |
        ForEach-Object { $_ -replace '^# ?', '' } | Write-Host
    exit 0
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Stop-WithError "cmake が見つかりません。"
}
if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    Stop-WithError "cl.exe が見つかりません。Developer PowerShell for VS から実行してください。"
}
# Windows 10 以降に付属の tar（bsdtar）。PATH の先にある別の tar（Git の tar はドライブ名の付いたパスを読めない）を使わない
$tarExe = Join-Path $env:SystemRoot "System32\tar.exe"
if (-not (Test-Path $tarExe)) {
    Stop-WithError "tar.exe が見つかりません: $tarExe"
}
$useNinja = [bool](Get-Command ninja -ErrorAction SilentlyContinue)

$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $WorkDir) {
    $WorkDir = Join-Path $projectRoot "build-qtkeychain"
}
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
$workDirPath = (Resolve-Path $WorkDir).Path
if ($QtPrefix) {
    if (-not (Test-Path (Join-Path $QtPrefix "lib\cmake\Qt6"))) {
        Stop-WithError "Qt が見つかりません: $QtPrefix\lib\cmake\Qt6"
    }
    $QtPrefix = (Resolve-Path $QtPrefix).Path
}

$archive = Join-Path $workDirPath "qtkeychain-$QTKEYCHAIN_VERSION.tar.gz"
$sourceDir = Join-Path $workDirPath "qtkeychain-$QTKEYCHAIN_VERSION"
$buildDir = Join-Path $workDirPath "build"
$installDir = Join-Path $workDirPath "install"
$configDir = Join-Path $installDir "lib\cmake\Qt6Keychain"
$configFile = Join-Path $configDir "Qt6KeychainConfig.cmake"
$stampFile = Join-Path $installDir ".sbq-stamp"
# 同じ条件でビルド済みなら作り直さない（コンパイラの版が変わったら作り直す）
$compiler = if ($env:VCToolsVersion) { $env:VCToolsVersion } else { "default" }
$qtStamp = if ($QtPrefix) { $QtPrefix } else { "default" }
$stamp = "qtkeychain=$QTKEYCHAIN_VERSION sha256=$QTKEYCHAIN_SHA256 qt=$qtStamp type=$BuildType msvc=$compiler"

function Test-Checksum {
    if (-not (Test-Path $archive)) { return $false }
    return (Get-FileHash -Algorithm SHA256 -Path $archive).Hash -eq $QTKEYCHAIN_SHA256
}

$upToDate = (Test-Path $stampFile) -and (Test-Path $configFile) -and
    ((Get-Content -Raw -Encoding UTF8 $stampFile).Trim() -eq $stamp)
if ($upToDate) {
    Write-Info "QtKeychain $QTKEYCHAIN_VERSION はビルド済みです: $installDir"
} else {
    if (-not (Test-Checksum)) {
        Write-Info "QtKeychain $QTKEYCHAIN_VERSION のソースを取得中..."
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        # 進み具合の表示は、Windows PowerShell では取得をとても遅くするので出さない
        $ProgressPreference = "SilentlyContinue"
        Invoke-WebRequest -UseBasicParsing -Uri $QTKEYCHAIN_URL -OutFile $archive
        if (-not (Test-Checksum)) {
            Stop-WithError "QtKeychain のソースの SHA-256 が合いません: $archive"
        }
    }

    foreach ($dir in @($sourceDir, $buildDir, $installDir)) {
        if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    }
    & $tarExe -xzf $archive -C $workDirPath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path (Join-Path $sourceDir "CMakeLists.txt"))) {
        Stop-WithError "QtKeychain のソースを展開できません: $sourceDir"
    }

    Write-Info "QtKeychain $QTKEYCHAIN_VERSION をビルド中（静的ライブラリ、$BuildType）..."
    $cmakeArgs = @(
        "-B", $buildDir,
        "-S", $sourceDir,
        "-DCMAKE_BUILD_TYPE=$BuildType",
        "-DCMAKE_INSTALL_PREFIX=$installDir",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DBUILD_TRANSLATIONS=OFF",
        "-DBUILD_TEST_APPLICATION=OFF",
        "-DBUILD_TESTING=OFF"
    )
    if ($useNinja) {
        $cmakeArgs += @("-G", "Ninja")
    }
    if ($QtPrefix) {
        $cmakeArgs += "-DCMAKE_PREFIX_PATH=$QtPrefix"
    }
    cmake @cmakeArgs | Out-Host
    if ($LASTEXITCODE -ne 0) { Stop-WithError "QtKeychain の構成に失敗しました。" }
    cmake --build $buildDir --config $BuildType | Out-Host
    if ($LASTEXITCODE -ne 0) { Stop-WithError "QtKeychain のビルドに失敗しました。" }
    cmake --install $buildDir --config $BuildType | Out-Host
    if ($LASTEXITCODE -ne 0) { Stop-WithError "QtKeychain のインストールに失敗しました。" }
    if (-not (Test-Path $configFile)) {
        Stop-WithError "QtKeychain の CMake の設定ファイルがありません: $configDir"
    }
    [IO.File]::WriteAllText($stampFile, "$stamp`n")
    Write-Info "QtKeychain ${QTKEYCHAIN_VERSION}: $installDir"
}

Write-Info "アプリの構成に次のオプションを付けてください:"
# CMake に渡すので区切りは / にする
Write-Output ("-DQt6Keychain_DIR=" + ($configDir -replace '\\', '/'))
