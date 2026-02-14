# Windows ビルド・リリース手順

ShogiBoardQ を Windows でビルドし、ZIP ファイルとして GitHub で公開する手順。

---

## 目次

1. [前提条件](#1-前提条件)
2. [開発環境のセットアップ](#2-開発環境のセットアップ)
3. [ビルド](#3-ビルド)
4. [配布パッケージの作成](#4-配布パッケージの作成)
5. [GitHub Release での公開](#5-github-release-での公開)
6. [コード署名（オプション）](#6-コード署名オプション)
7. [トラブルシューティング](#7-トラブルシューティング)

---

## 1. 前提条件

| 項目 | バージョン |
|---|---|
| Windows | 10 / 11（64-bit） |
| Visual Studio | 2019 または 2022（C++ デスクトップ開発ワークロード） |
| CMake | 3.16 以上 |
| Qt | 6.x（Widgets, Charts, Network, LinguistTools） |
| C++ | C++17 対応コンパイラ（MSVC 2019 以降） |

---

## 2. 開発環境のセットアップ

### 2.1 Visual Studio のインストール

[Visual Studio](https://visualstudio.microsoft.com/) をインストールし、以下のワークロードを選択：

- **C++ によるデスクトップ開発**

これにより MSVC コンパイラ、CMake、Ninja が含まれる。

> **Community Edition** は個人開発・OSS 開発に無料で利用可能。

### 2.2 Qt 6 のインストール

#### 方法A: Qt Online Installer（推奨）

[Qt 公式サイト](https://www.qt.io/download-qt-installer)からインストーラをダウンロードし、以下のコンポーネントを選択：

- Qt 6.x > MSVC 2019/2022 64-bit
- Qt 6.x > Qt Charts
- Developer and Designer Tools > CMake
- Developer and Designer Tools > Ninja

インストール後、Qt のパスを環境変数に設定：

```powershell
# システム環境変数に追加（Qt のインストールパスは環境に合わせて変更）
# 例: C:\Qt\6.8.3\msvc2022_64
[Environment]::SetEnvironmentVariable("Qt6_DIR", "C:\Qt\6.8.3\msvc2022_64\lib\cmake\Qt6", "User")
[Environment]::SetEnvironmentVariable("PATH", "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH", "User")
```

#### 方法B: aqtinstall（CI/コマンドライン向け）

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtcharts
```

### 2.3 Ninja のインストール（推奨）

Visual Studio に含まれる Ninja を使うか、別途インストール：

```powershell
winget install Ninja-build.Ninja
```

### 2.4 インストール確認

**Developer PowerShell for VS 2022**（スタートメニューから起動）で確認：

```powershell
cmake --version       # 3.16 以上
cl                    # MSVC コンパイラ
ninja --version       # Ninja
windeployqt --version # Qt デプロイツール
```

---

## 3. ビルド

### ビルドスクリプト（推奨）

`scripts/build-windows.ps1` を使うと、Release ビルドから ZIP 作成まで一括実行できる。

> **重要**: Developer PowerShell for VS 2022 から実行すること（MSVC の環境変数が必要）。

```powershell
# 通常ビルド + ZIP 作成
.\scripts\build-windows.ps1

# クリーンビルド、ZIP なし
.\scripts\build-windows.ps1 -Clean -SkipZip
```

| オプション | 説明 |
|---|---|
| `-Clean` | build / deploy ディレクトリを削除してからビルド |
| `-SkipZip` | ZIP 作成をスキップ（deploy フォルダのみ生成） |
| `-Help` | ヘルプを表示 |

スクリプトは以下の処理を自動実行する：

1. 前提ツールの存在確認（cmake, ninja, cl, windeployqt）
2. CMake Configure + ビルド
3. ビルド成果物の確認（.exe、.qm 翻訳ファイル）
4. deploy ディレクトリ作成 + windeployqt による DLL デプロイ + 検証
5. ZIP ファイル作成

> **実行ポリシーエラーが出る場合:**
> ```powershell
> Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
> ```

以下は個別のコマンドを手動で実行する場合の手順。

### 3.1 Release ビルド

Developer PowerShell for VS 2022 で実行：

```powershell
cd C:\path\to\ShogiBoardQ

# Configure（Release ビルド、Ninja）
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release

# ビルド
ninja -C build
```

Ninja がない場合（Visual Studio ジェネレータを使用）：

```powershell
# Configure
cmake -B build -S .

# Release ビルド
cmake --build build --config Release
```

### 3.2 ビルド成果物の確認

```powershell
# exe が生成されていることを確認
# Ninja の場合:
dir build\ShogiBoardQ.exe

# Visual Studio ジェネレータの場合:
dir build\Release\ShogiBoardQ.exe

# 翻訳ファイルの確認
dir build\*.qm           # Ninja
dir build\Release\*.qm   # VS ジェネレータ
```

### 3.3 動作確認

```powershell
# ビルドしたアプリを起動（Qt DLL が PATH に含まれている環境で）
.\build\ShogiBoardQ.exe
```

---

## 4. 配布パッケージの作成

> **Note:** `scripts/build-windows.ps1` を使用した場合、このセクションの手順は自動実行される。

### 4.1 deploy ディレクトリの作成

exe と翻訳ファイルを配布用ディレクトリにコピー：

```powershell
mkdir deploy
copy build\ShogiBoardQ.exe deploy\
copy build\*.qm deploy\
```

### 4.2 windeployqt で Qt DLL をデプロイ

```powershell
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw deploy\ShogiBoardQ.exe
```

`windeployqt` が自動で行う処理：

1. 依存する Qt DLL（Qt6Core, Qt6Gui, Qt6Widgets, Qt6Charts, Qt6Network 等）をコピー
2. Qt プラグイン（platforms/qwindows.dll, imageformats/ 等）をコピー
3. MSVC ランタイム DLL（vcruntime140.dll 等）をコピー

| オプション | 説明 |
|---|---|
| `--release` | Release ビルド用 DLL をデプロイ |
| `--no-translations` | Qt の翻訳ファイルを除外（アプリ独自の .qm は手動コピー済み） |
| `--no-system-d3d-compiler` | d3dcompiler_47.dll を除外（通常不要） |
| `--no-opengl-sw` | ソフトウェア OpenGL を除外 |

### 4.3 デプロイ後の確認

```powershell
# ディレクトリ構造の確認
tree /F deploy

# 必須 DLL の確認
dir deploy\Qt6Core.dll
dir deploy\Qt6Gui.dll
dir deploy\Qt6Widgets.dll
dir deploy\Qt6Charts.dll
dir deploy\Qt6Network.dll

# platforms プラグインの確認
dir deploy\platforms\qwindows.dll
```

デプロイ後のディレクトリ構造：

```
deploy/
├── ShogiBoardQ.exe                ← 実行ファイル
├── ShogiBoardQ_ja_JP.qm          ← 日本語翻訳
├── ShogiBoardQ_en.qm             ← 英語翻訳
├── Qt6Core.dll                    ← Qt Core
├── Qt6Gui.dll                     ← Qt GUI
├── Qt6Widgets.dll                 ← Qt Widgets
├── Qt6Charts.dll                  ← Qt Charts
├── Qt6Network.dll                 ← Qt Network
├── Qt6OpenGL.dll                  ← Qt OpenGL (Charts 依存)
├── Qt6OpenGLWidgets.dll           ← Qt OpenGL Widgets
├── Qt6Svg.dll                     ← Qt SVG (アイコン用)
├── vc_redist.x64.exe              ← MSVC ランタイム (※)
├── platforms/
│   └── qwindows.dll               ← Windows プラットフォームプラグイン
├── imageformats/
│   ├── qsvg.dll                   ← SVG サポート
│   ├── qico.dll                   ← ICO サポート
│   └── ...
├── styles/
│   └── qmodernwindowsstyle.dll    ← Windows スタイル
├── tls/
│   └── qschannelbackend.dll       ← TLS (CSA ネットワーク通信用)
└── networkinformation/
    └── qnetworklistmanager.dll    ← ネットワーク情報
```

> ※ MSVC ランタイムは `windeployqt` がコピーする場合としない場合がある。
> 配布先のユーザーが MSVC ランタイムを持っていない場合に備え、
> [Visual C++ 再頒布可能パッケージ](https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist)のインストールを案内するか、
> 同梱を検討する。

### 4.4 動作テスト

deploy フォルダから直接起動してテスト：

```powershell
.\deploy\ShogiBoardQ.exe
```

> **重要**: テストは Qt の bin ディレクトリが PATH に**含まれない**環境で行うこと。
> PATH に Qt が含まれていると、deploy フォルダの DLL ではなくシステムの DLL が使われてしまい、
> デプロイ漏れを検出できない。

### 4.5 ZIP ファイルの作成

```powershell
Compress-Archive -Path deploy\* -DestinationPath ShogiBoardQ-windows.zip
```

---

## 5. GitHub Release での公開

### 5.1 タグの作成

```powershell
git tag -a v0.1.0 -m "v0.1.0 リリース"
git push origin v0.1.0
```

### 5.2 GitHub CLI でリリース作成

[GitHub CLI (gh)](https://cli.github.com/) を使用：

```powershell
# インストール
winget install GitHub.cli

# ログイン（初回のみ）
gh auth login
```

リリース作成とアセットのアップロード：

```powershell
gh release create v0.1.0 `
  --title "ShogiBoardQ v0.1.0" `
  --notes-file RELEASE_NOTES.md `
  ShogiBoardQ-windows.zip
```

> macOS の DMG も同時に公開する場合は、アセットを追加：
> ```powershell
> gh release create v0.1.0 `
>   --title "ShogiBoardQ v0.1.0" `
>   --notes-file RELEASE_NOTES.md `
>   ShogiBoardQ-windows.zip `
>   ShogiBoardQ.dmg
> ```

#### リリースノートの自動生成

リリースノートファイルを用意しない場合、GitHub が自動生成する：

```powershell
gh release create v0.1.0 `
  --title "ShogiBoardQ v0.1.0" `
  --generate-notes `
  ShogiBoardQ-windows.zip
```

### 5.3 Web UI からリリース作成（代替）

1. GitHub リポジトリ → **Releases** → **Draft a new release**
2. **Choose a tag** → 新しいタグ（例: `v0.1.0`）を入力して作成
3. **Release title** を入力（例: `ShogiBoardQ v0.1.0`）
4. **Description** にリリースノートを記入
5. **Attach binaries** に `ShogiBoardQ-windows.zip` をドラッグ＆ドロップ
6. **Publish release** をクリック

### 5.4 リリースノートの書き方（テンプレート）

```markdown
## ShogiBoardQ v0.1.0

### ダウンロード

| OS | ファイル |
|---|---|
| Windows (64-bit) | `ShogiBoardQ-windows.zip` |
| macOS | `ShogiBoardQ.dmg` |

### Windows での起動方法

1. ZIP ファイルを展開
2. `ShogiBoardQ.exe` をダブルクリック

> 初回起動時に「Windows によって PC が保護されました」と表示される場合は、
> 「詳細情報」→「実行」をクリックしてください。

### 変更点

- ...
```

---

## 6. コード署名（オプション）

署名なしでも動作するが、初回起動時に SmartScreen の警告が表示される。

### 6.1 コード署名証明書の取得

コード署名には EV（Extended Validation）コード署名証明書が推奨。
個人開発者向けには OV（Organization Validation）証明書も選択肢。

主なプロバイダ:
- [SignPath Foundation](https://signpath.org/)（OSS プロジェクト向け、無料）
- DigiCert
- Sectigo
- GlobalSign

### 6.2 signtool で署名

```powershell
# Windows SDK に含まれる signtool を使用
signtool sign /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 `
  /f "certificate.pfx" /p "password" `
  deploy\ShogiBoardQ.exe
```

### 6.3 署名なしで配布する場合

ユーザーに以下を案内する：

1. ダウンロード後、ZIP を右クリック → プロパティ → 「ブロックの解除」にチェック → OK
2. 展開後、`ShogiBoardQ.exe` を実行
3. SmartScreen の警告が表示された場合、「詳細情報」→「実行」をクリック

---

## 7. トラブルシューティング

### Qt が見つからない

```
CMake Error: Could not find a package configuration file provided by "Qt6"
```

Qt のインストールパスを明示的に指定する：

```powershell
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.8.3\msvc2022_64"
```

### cl.exe が見つからない

Developer PowerShell for VS 2022 を使用していない場合：

```powershell
# 手動で MSVC 環境をセットアップ
# Visual Studio 2022 の場合
& "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
```

または、スタートメニューから **Developer PowerShell for VS 2022** を起動する。

### windeployqt が見つからない

```powershell
# Qt のインストールパスを確認し、PATH に追加
$env:PATH = "C:\Qt\6.8.3\msvc2022_64\bin;$env:PATH"

# 確認
windeployqt --version
```

### アプリが起動しない（DLL エラー）

```
"Qt6Core.dll が見つかりません" 等のエラー
```

1. `windeployqt` を再実行する
2. 必須 DLL がすべて deploy ディレクトリに存在するか確認：
   ```powershell
   dir deploy\Qt6Core.dll
   dir deploy\Qt6Gui.dll
   dir deploy\Qt6Widgets.dll
   dir deploy\Qt6Charts.dll
   dir deploy\Qt6Network.dll
   dir deploy\platforms\qwindows.dll
   ```

3. MSVC ランタイムが不足している場合：
   - [Visual C++ 再頒布可能パッケージ](https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist)をインストール

### Qt Charts の DLL がデプロイされない

windeployqt が Qt Charts を検出できない場合、手動でコピー：

```powershell
copy "C:\Qt\6.8.3\msvc2022_64\bin\Qt6Charts.dll" deploy\
copy "C:\Qt\6.8.3\msvc2022_64\bin\Qt6OpenGL.dll" deploy\
copy "C:\Qt\6.8.3\msvc2022_64\bin\Qt6OpenGLWidgets.dll" deploy\
```

### 翻訳が読み込まれない

`.qm` ファイルが exe と同じディレクトリに存在するか確認：

```powershell
dir deploy\*.qm
```

ない場合、ビルドディレクトリから手動コピー：

```powershell
copy build\ShogiBoardQ_ja_JP.qm deploy\
copy build\ShogiBoardQ_en.qm deploy\
```

### 実行ポリシーエラー（PowerShell スクリプト）

```
このシステムではスクリプトの実行が無効になっているため...
```

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### SmartScreen の警告（署名なし）

配布先ユーザーへの案内：

1. ZIP を右クリック → プロパティ → **ブロックの解除**にチェック → OK
2. ZIP を展開してから実行
3. 「Windows によって PC が保護されました」→ **詳細情報** → **実行**

> ダウンロード数が増えると SmartScreen の信頼度が上がり、警告が表示されにくくなる。
