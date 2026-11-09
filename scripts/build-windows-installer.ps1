#
# Windows インストーラの作成スクリプト for ShogiBoardQ
#
# scripts/build-windows.ps1 が作った配布用ディレクトリ（または配布 ZIP を展開したフォルダ）を、
# packaging/windows/shogiboardq.iss で ShogiBoardQ-windows-setup.exe に包む。
# 版番号は配布用ディレクトリの shogiboardq-cli.exe から読むので、中身と食い違わない。
# 詳細: docs/dev/windows-build-and-release.md
#
# Usage:
#   .\scripts\build-windows-installer.ps1 [-DeployDir deploy] [-OutputDir build\installer]
#
# Prerequisites:
#   - Inno Setup 6（winget install --id JRSoftware.InnoSetup）

param(
    [string]$DeployDir = "deploy",
    [string]$OutputDir = "build\installer"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Find-InnoCompiler {
    $onPath = Get-Command iscc -ErrorAction SilentlyContinue
    if ($onPath) {
        return $onPath.Source
    }
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    return $null
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $projectRoot

$iscc = Find-InnoCompiler
if (-not $iscc) {
    throw "Inno Setup 6 が見つかりません。winget install --id JRSoftware.InnoSetup で入れてください。"
}

$deployPath = (Resolve-Path $DeployDir).Path
foreach ($required in @("ShogiBoardQ.exe", "shogiboardq-cli.exe", "LICENSE", "Hayanagi\hayanagi.exe", "Hayanagi\book\hayanagi_book.db", "data\tsumeshogi")) {
    if (-not (Test-Path (Join-Path $deployPath $required))) {
        throw "配布用ディレクトリに $required がありません: $deployPath"
    }
}

$versionJson = & (Join-Path $deployPath "shogiboardq-cli.exe") version
if ($LASTEXITCODE -ne 0) {
    throw "shogiboardq-cli.exe version に失敗しました。"
}
$version = ($versionJson | ConvertFrom-Json).version
if (-not $version) {
    throw "版番号を読めませんでした: $versionJson"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$outputPath = (Resolve-Path $OutputDir).Path

Write-Host "==> ShogiBoardQ $version のインストーラを作成中..."
& $iscc /Q "/DAppVersion=$version" "/DDeployDir=$deployPath" "/DOutputDir=$outputPath" "packaging\windows\shogiboardq.iss"
if ($LASTEXITCODE -ne 0) {
    throw "インストーラの作成に失敗しました。"
}

$installer = Get-Item (Join-Path $outputPath "ShogiBoardQ-windows-setup.exe")
Write-Host ("==> {0} ({1:N1} MiB)" -f $installer.FullName, ($installer.Length / 1MB))
Write-Host ("==> SHA-256: {0}" -f (Get-FileHash $installer.FullName -Algorithm SHA256).Hash.ToLowerInvariant())
