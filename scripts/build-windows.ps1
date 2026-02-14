#
# Windows ビルドスクリプト for ShogiBoardQ
#
# Release ビルド → windeployqt → ZIP 作成を一括実行する。
# 詳細: docs/dev/windows-build-and-release.md
#
# Usage:
#   .\scripts\build-windows.ps1 [OPTIONS]
#
# Options:
#   -Clean       build ディレクトリを削除してからビルド
#   -SkipZip     ZIP 作成をスキップ
#   -Help        このヘルプを表示
#
# Prerequisites:
#   - Visual Studio 2019/2022（C++ デスクトップ開発）
#   - Qt 6.x (Widgets, Charts, Network, LinguistTools)
#   - CMake 3.16+
#   - Ninja（推奨）
#
# 実行前に以下のいずれかで環境を準備:
#   - "Developer PowerShell for VS 2022" を使用
#   - または Qt の環境変数を設定済みの PowerShell を使用

param(
    [switch]$Clean,
    [switch]$SkipZip,
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ──────────────────────────────────────────────
# 定数
# ──────────────────────────────────────────────

$APP_NAME = "ShogiBoardQ"
$BUILD_DIR = "build"
$DEPLOY_DIR = "deploy"
$ZIP_NAME = "${APP_NAME}-windows.zip"

# ──────────────────────────────────────────────
# ヘルパー関数
# ──────────────────────────────────────────────

function Write-Info($msg)  { Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Warn($msg)  { Write-Host "==> WARNING: $msg" -ForegroundColor Yellow }
function Write-Err($msg)   { Write-Host "==> ERROR: $msg" -ForegroundColor Red }

function Stop-WithError($msg) {
    Write-Err $msg
    exit 1
}

function Show-Usage {
    @"
Usage: .\scripts\build-windows.ps1 [OPTIONS]

Windows 用の Release ビルド〜ZIP 作成を一括実行するスクリプト。

Options:
  -Clean       build ディレクトリを削除してからビルド
  -SkipZip     ZIP 作成をスキップ（deploy フォルダのみ生成）
  -Help        このヘルプを表示

Examples:
  # 通常ビルド + ZIP 作成
  .\scripts\build-windows.ps1

  # クリーンビルド、ZIP なし
  .\scripts\build-windows.ps1 -Clean -SkipZip
"@
}

if ($Help) {
    Show-Usage
    exit 0
}

# ──────────────────────────────────────────────
# Step 1: 前提チェック
# ──────────────────────────────────────────────

Write-Info "前提ツールを確認中..."

# CMake
$cmakeExe = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeExe) {
    Stop-WithError "cmake が見つかりません。Visual Studio または CMake をインストールしてください。"
}
Write-Info "cmake: $(cmake --version | Select-Object -First 1)"

# Ninja（オプションだが推奨）
$ninjaExe = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninjaExe) {
    $generator = "Ninja"
    Write-Info "ninja: $(ninja --version)"
} else {
    Write-Warn "ninja が見つかりません。Visual Studio ジェネレータを使用します。"
    Write-Warn "Ninja の使用を推奨します: winget install Ninja-build.Ninja"
    $generator = $null
}

# MSVC コンパイラ
$clExe = Get-Command cl -ErrorAction SilentlyContinue
if (-not $clExe) {
    Write-Warn "cl.exe が見つかりません。"
    Write-Warn "Developer PowerShell for VS 2022 から実行するか、"
    Write-Warn "vcvarsall.bat を事前に実行してください。"
    if ($generator -eq "Ninja") {
        Stop-WithError "Ninja ビルドには cl.exe が PATH に必要です。"
    }
}

# windeployqt
$windeployqtExe = Get-Command windeployqt -ErrorAction SilentlyContinue
if (-not $windeployqtExe) {
    # Qt の bin ディレクトリを探す
    $qtBinCandidates = @(
        "$env:Qt6_DIR/../../bin",
        "$env:QTDIR/bin",
        "$env:QTDIR6/bin"
    )
    foreach ($candidate in $qtBinCandidates) {
        if (Test-Path "$candidate/windeployqt.exe") {
            $env:PATH = "$candidate;$env:PATH"
            $windeployqtExe = Get-Command windeployqt -ErrorAction SilentlyContinue
            break
        }
    }
    if (-not $windeployqtExe) {
        Stop-WithError "windeployqt が見つかりません。Qt の bin ディレクトリを PATH に追加してください。"
    }
}
Write-Info "windeployqt: $($windeployqtExe.Source)"

# ──────────────────────────────────────────────
# Step 2: プロジェクトルートへ移動
# ──────────────────────────────────────────────

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$PROJECT_ROOT = Split-Path -Parent $SCRIPT_DIR
Set-Location $PROJECT_ROOT
Write-Info "プロジェクトルート: $PROJECT_ROOT"

# ──────────────────────────────────────────────
# Step 3: クリーン（オプション）
# ──────────────────────────────────────────────

if ($Clean) {
    Write-Info "build ディレクトリを削除中..."
    if (Test-Path $BUILD_DIR) {
        Remove-Item -Recurse -Force $BUILD_DIR
    }
    if (Test-Path $DEPLOY_DIR) {
        Remove-Item -Recurse -Force $DEPLOY_DIR
    }
}

# ──────────────────────────────────────────────
# Step 4: CMake Configure
# ──────────────────────────────────────────────

Write-Info "CMake Configure (Release)..."

$cmakeArgs = @(
    "-B", $BUILD_DIR,
    "-S", ".",
    "-DCMAKE_BUILD_TYPE=Release"
)

if ($generator) {
    $cmakeArgs += @("-G", $generator)
}

cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    Stop-WithError "CMake Configure に失敗しました。"
}

# ──────────────────────────────────────────────
# Step 5: ビルド
# ──────────────────────────────────────────────

Write-Info "ビルド中..."

if ($generator -eq "Ninja") {
    ninja -C $BUILD_DIR
} else {
    cmake --build $BUILD_DIR --config Release
}

if ($LASTEXITCODE -ne 0) {
    Stop-WithError "ビルドに失敗しました。"
}

# ──────────────────────────────────────────────
# Step 6: ビルド成果物の確認
# ──────────────────────────────────────────────

Write-Info "ビルド成果物を確認中..."

# exe の検索（Ninja: build/ 直下、VS: build/Release/）
$exePath = $null
$searchPaths = @(
    "$BUILD_DIR/${APP_NAME}.exe",
    "$BUILD_DIR/Release/${APP_NAME}.exe"
)
foreach ($path in $searchPaths) {
    if (Test-Path $path) {
        $exePath = $path
        break
    }
}

if (-not $exePath) {
    Stop-WithError "${APP_NAME}.exe が見つかりません。ビルドに失敗した可能性があります。"
}

Write-Info "実行ファイル: $exePath"

# 翻訳ファイルの確認
$exeDir = Split-Path -Parent $exePath
$qmFiles = Get-ChildItem -Path $exeDir -Filter "*.qm" -ErrorAction SilentlyContinue
if ($qmFiles) {
    Write-Info "翻訳ファイル: $($qmFiles.Count) 個の .qm ファイルを検出"
} else {
    Write-Warn ".qm 翻訳ファイルが見つかりません。"
}

# ──────────────────────────────────────────────
# Step 7: deploy ディレクトリ作成
# ──────────────────────────────────────────────

Write-Info "deploy ディレクトリを作成中..."

if (Test-Path $DEPLOY_DIR) {
    Remove-Item -Recurse -Force $DEPLOY_DIR
}
New-Item -ItemType Directory -Path $DEPLOY_DIR | Out-Null

# exe をコピー
Copy-Item $exePath $DEPLOY_DIR

# .qm ファイルをコピー
if ($qmFiles) {
    foreach ($qm in $qmFiles) {
        Copy-Item $qm.FullName $DEPLOY_DIR
    }
}

# ──────────────────────────────────────────────
# Step 8: windeployqt
# ──────────────────────────────────────────────

Write-Info "windeployqt で Qt DLL をデプロイ中..."

$deployExePath = Join-Path $DEPLOY_DIR "${APP_NAME}.exe"
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $deployExePath

if ($LASTEXITCODE -ne 0) {
    Stop-WithError "windeployqt に失敗しました。"
}

# ──────────────────────────────────────────────
# Step 9: デプロイ後の検証
# ──────────────────────────────────────────────

Write-Info "デプロイを検証中..."

$deployedFiles = Get-ChildItem -Path $DEPLOY_DIR -Recurse -File
$dllCount = ($deployedFiles | Where-Object { $_.Extension -eq ".dll" }).Count
$exeCount = ($deployedFiles | Where-Object { $_.Extension -eq ".exe" }).Count
$qmCount  = ($deployedFiles | Where-Object { $_.Extension -eq ".qm" }).Count

Write-Info "ファイル数: exe=$exeCount, dll=$dllCount, qm=$qmCount"

# 必須 DLL の確認
$requiredDlls = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Charts.dll", "Qt6Network.dll")
$missingDlls = @()
foreach ($dll in $requiredDlls) {
    if (-not (Test-Path (Join-Path $DEPLOY_DIR $dll))) {
        $missingDlls += $dll
    }
}

if ($missingDlls.Count -gt 0) {
    Write-Warn "以下の DLL が見つかりません: $($missingDlls -join ', ')"
    Write-Warn "windeployqt の出力を確認してください。"
} else {
    Write-Info "必須 Qt DLL: すべて存在を確認"
}

# platforms プラグインの確認
if (-not (Test-Path (Join-Path $DEPLOY_DIR "platforms"))) {
    Write-Warn "platforms プラグインディレクトリが見つかりません。"
} else {
    Write-Info "platforms プラグイン: OK"
}

# ──────────────────────────────────────────────
# Step 10: ZIP 作成
# ──────────────────────────────────────────────

if ($SkipZip) {
    Write-Info "ZIP 作成をスキップしました。"
    Write-Info "出力: $DEPLOY_DIR"
    exit 0
}

Write-Info "ZIP を作成中..."

if (Test-Path $ZIP_NAME) {
    Write-Info "既存の ${ZIP_NAME} を削除中..."
    Remove-Item -Force $ZIP_NAME
}

Compress-Archive -Path "$DEPLOY_DIR\*" -DestinationPath $ZIP_NAME

if (-not (Test-Path $ZIP_NAME)) {
    Stop-WithError "ZIP ファイルの作成に失敗しました。"
}

$zipSize = (Get-Item $ZIP_NAME).Length / 1MB
Write-Info ("ZIP ファイルサイズ: {0:N1} MB" -f $zipSize)

# ──────────────────────────────────────────────
# 完了
# ──────────────────────────────────────────────

Write-Info "ビルド完了!"
Write-Info "デプロイ先:   $DEPLOY_DIR"
Write-Info "ZIP ファイル: $ZIP_NAME"
