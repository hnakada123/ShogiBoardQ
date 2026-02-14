# Linux ビルド・リリース手順

ShogiBoardQ を Linux でビルドし、AppImage として GitHub で公開する手順。

---

## 目次

1. [前提条件](#1-前提条件)
2. [開発環境のセットアップ](#2-開発環境のセットアップ)
3. [ビルド](#3-ビルド)
4. [AppImage の作成](#4-appimage-の作成)
5. [GitHub Release での公開](#5-github-release-での公開)
6. [トラブルシューティング](#6-トラブルシューティング)

---

## 1. 前提条件

| 項目 | バージョン |
|---|---|
| Linux | Ubuntu 22.04 / Fedora 38 / Arch Linux 等（x86_64） |
| GCC | 9 以降、または Clang 10 以降（C++17 対応） |
| CMake | 3.16 以上 |
| Qt | 6.x（Widgets, Charts, Network, LinguistTools） |
| FUSE | AppImage 実行に必要 |

> **AppImage のビルド環境について:**
> AppImage はビルド環境の glibc バージョン以降のシステムでのみ動作する。
> より広い互換性を確保するには、古めのディストリビューション（Ubuntu 22.04 等）でビルドすることを推奨する。

---

## 2. 開発環境のセットアップ

### 2.1 ビルドツールのインストール

#### Ubuntu / Debian

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git curl \
  libfuse2 file
```

#### Fedora

```bash
sudo dnf install gcc-c++ cmake ninja-build git curl fuse-libs file
```

#### Arch Linux

```bash
sudo pacman -S base-devel cmake ninja git curl fuse2 file
```

### 2.2 Qt 6 のインストール

#### 方法A: Qt Online Installer（推奨）

[Qt 公式サイト](https://www.qt.io/download-qt-installer)からインストーラをダウンロードし、以下のコンポーネントを選択：

- Qt 6.x > Desktop gcc 64-bit
- Qt 6.x > Qt Charts
- Developer and Designer Tools > CMake
- Developer and Designer Tools > Ninja

インストール後、Qt のパスを環境変数に設定：

```bash
# ~/.bashrc または ~/.zshrc に追加（パスは環境に合わせて変更）
export Qt6_DIR="$HOME/Qt/6.8.3/gcc_64/lib/cmake/Qt6"
export PATH="$HOME/Qt/6.8.3/gcc_64/bin:$PATH"
export LD_LIBRARY_PATH="$HOME/Qt/6.8.3/gcc_64/lib:$LD_LIBRARY_PATH"
```

#### 方法B: ディストリビューションのパッケージ

**Ubuntu / Debian:**

```bash
sudo apt install qt6-base-dev qt6-charts-dev qt6-l10n-tools \
  qt6-tools-dev qt6-tools-dev-tools libgl1-mesa-dev
```

**Fedora:**

```bash
sudo dnf install qt6-qtbase-devel qt6-qtcharts-devel \
  qt6-linguist qt6-qttools-devel mesa-libGL-devel
```

**Arch Linux:**

```bash
sudo pacman -S qt6-base qt6-charts qt6-tools
```

> **注意:** ディストリビューションのパッケージは Qt のバージョンが古い場合がある。
> 最新の Qt 6.x を使うには方法A の Qt Online Installer を推奨。

### 2.3 インストール確認

```bash
cmake --version       # 3.16 以上
g++ --version         # GCC 9 以上
qmake6 --version      # Qt 6.x
```

---

## 3. ビルド

### ビルドスクリプト（推奨）

`scripts/build-linux.sh` を使うと、Release ビルドから AppImage 作成まで一括実行できる：

```bash
# 通常ビルド + AppImage 作成
./scripts/build-linux.sh

# クリーンビルド、AppImage なし
./scripts/build-linux.sh --clean --skip-appimage
```

| オプション | 説明 |
|---|---|
| `--skip-appimage` | AppImage 作成をスキップ（ビルドのみ） |
| `--clean` | build ディレクトリを削除してからビルド |
| `--help` | ヘルプを表示 |

スクリプトは以下の処理を自動実行する：

1. 前提ツールの存在確認（cmake, ninja）
2. CMake Configure + ビルド
3. ビルド成果物の確認（実行ファイル、.qm 翻訳ファイル）
4. linuxdeploy + Qt プラグインのダウンロード（初回のみ）
5. AppImage 作成

以下は個別のコマンドを手動で実行する場合の手順。

### 3.1 Release ビルド

```bash
cd /path/to/ShogiBoardQ

# Configure（Release ビルド、Ninja）
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release

# ビルド
ninja -C build
```

Ninja がない場合：

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -- -j$(nproc)
```

### 3.2 ビルド成果物の確認

```bash
# 実行ファイルが生成されていることを確認
ls -la build/ShogiBoardQ

# 翻訳ファイルの確認
ls build/*.qm

# 依存ライブラリの確認
ldd build/ShogiBoardQ
```

### 3.3 動作確認

```bash
# ビルドしたアプリを起動
./build/ShogiBoardQ
```

> Qt Online Installer で Qt をインストールした場合、`LD_LIBRARY_PATH` に Qt の lib ディレクトリが含まれている必要がある。

---

## 4. AppImage の作成

> **Note:** `scripts/build-linux.sh` を使用した場合、このセクションの手順は自動実行されるため手動での実行は不要。

### AppImage とは

AppImage は Linux 向けのポータブルなアプリケーション配布形式。
インストール不要で、単一ファイルをダウンロードして実行するだけで動作する。

### 4.1 linuxdeploy のダウンロード

[linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) と Qt プラグインをダウンロード：

```bash
# linuxdeploy 本体
curl -fSL -o build/linuxdeploy-x86_64.AppImage \
  https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x build/linuxdeploy-x86_64.AppImage

# Qt プラグイン
curl -fSL -o build/linuxdeploy-plugin-qt-x86_64.AppImage \
  https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x build/linuxdeploy-plugin-qt-x86_64.AppImage
```

### 4.2 .desktop ファイルとアイコンの準備

AppImage の作成には FreeDesktop 準拠の `.desktop` ファイルとアイコンが必要。
これらはリポジトリに同梱されている：

- `resources/platform/shogiboardq.desktop` — デスクトップエントリ
- `resources/icons/shogiboardq.png` — アプリケーションアイコン

### 4.3 AppDir の作成と AppImage 生成

```bash
# AppDir をクリーン
rm -rf build/AppDir

# 実行ファイルと .qm ファイルを AppDir に配置
mkdir -p build/AppDir/usr/bin
cp build/ShogiBoardQ build/AppDir/usr/bin/
cp build/*.qm build/AppDir/usr/bin/

# linuxdeploy で Qt ライブラリをバンドルし、AppImage を生成
export EXTRA_QT_PLUGINS="svg;iconengines"
export OUTPUT="ShogiBoardQ-x86_64.AppImage"

build/linuxdeploy-x86_64.AppImage \
  --appdir build/AppDir \
  --executable build/AppDir/usr/bin/ShogiBoardQ \
  --desktop-file resources/platform/shogiboardq.desktop \
  --icon-file resources/icons/shogiboardq.png \
  --plugin qt \
  --output appimage
```

`linuxdeploy` が自動で行う処理：

1. 依存する共有ライブラリ（Qt, システムライブラリ等）を `AppDir/usr/lib/` にコピー
2. Qt プラグイン（platforms/xcb, imageformats/ 等）をコピー
3. `.desktop` ファイルとアイコンを AppDir に配置
4. AppRun ランチャーを生成
5. `appimagetool` で AppImage ファイルを作成

### 4.4 AppImage の構造

```
ShogiBoardQ-x86_64.AppImage（単一実行ファイル）
  └── (展開時)
      ├── AppRun                          ← エントリーポイント
      ├── shogiboardq.desktop             ← デスクトップエントリ
      ├── shogiboardq.png                 ← アイコン
      └── usr/
          ├── bin/
          │   ├── ShogiBoardQ             ← 実行ファイル
          │   ├── ShogiBoardQ_ja_JP.qm    ← 日本語翻訳
          │   └── ShogiBoardQ_en.qm       ← 英語翻訳
          ├── lib/
          │   ├── libQt6Core.so.6         ← Qt Core
          │   ├── libQt6Gui.so.6          ← Qt GUI
          │   ├── libQt6Widgets.so.6      ← Qt Widgets
          │   ├── libQt6Charts.so.6       ← Qt Charts
          │   ├── libQt6Network.so.6      ← Qt Network
          │   └── ...
          └── plugins/
              ├── platforms/
              │   └── libqxcb.so          ← X11 プラットフォームプラグイン
              ├── imageformats/
              │   ├── libqsvg.so          ← SVG サポート
              │   └── ...
              └── tls/
                  └── libqopensslbackend.so
```

### 4.5 動作テスト

```bash
# 実行権限の確認
chmod +x ShogiBoardQ-x86_64.AppImage

# 起動
./ShogiBoardQ-x86_64.AppImage
```

> **重要**: テストは Qt の lib ディレクトリが `LD_LIBRARY_PATH` に**含まれない**環境で行うこと。
> 含まれていると、AppImage 内のライブラリではなくシステムのライブラリが使われてしまい、
> バンドル漏れを検出できない。

---

## 5. GitHub Release での公開

### 5.1 タグの作成

```bash
git tag -a v0.1.0 -m "v0.1.0 リリース"
git push origin v0.1.0
```

### 5.2 GitHub CLI でリリース作成

[GitHub CLI (gh)](https://cli.github.com/) を使用：

```bash
# インストール
# Ubuntu / Debian
sudo apt install gh
# Fedora
sudo dnf install gh
# Arch Linux
sudo pacman -S github-cli

# ログイン（初回のみ）
gh auth login
```

リリース作成とアセットのアップロード：

```bash
gh release create v0.1.0 \
  --title "ShogiBoardQ v0.1.0" \
  --notes-file RELEASE_NOTES.md \
  ShogiBoardQ-x86_64.AppImage
```

> 他プラットフォームのファイルも同時に公開する場合：
> ```bash
> gh release create v0.1.0 \
>   --title "ShogiBoardQ v0.1.0" \
>   --notes-file RELEASE_NOTES.md \
>   ShogiBoardQ-x86_64.AppImage \
>   ShogiBoardQ.dmg \
>   ShogiBoardQ-windows.zip
> ```

#### リリースノートの自動生成

```bash
gh release create v0.1.0 \
  --title "ShogiBoardQ v0.1.0" \
  --generate-notes \
  ShogiBoardQ-x86_64.AppImage
```

#### 既存リリースにアセットを追加

他のプラットフォームで先にリリースを作成済みの場合：

```bash
gh release upload v0.1.0 ShogiBoardQ-x86_64.AppImage
```

### 5.3 Web UI からリリース作成（代替）

1. GitHub リポジトリ → **Releases** → **Draft a new release**
2. **Choose a tag** → 新しいタグ（例: `v0.1.0`）を入力して作成
3. **Release title** を入力（例: `ShogiBoardQ v0.1.0`）
4. **Description** にリリースノートを記入
5. **Attach binaries** に `ShogiBoardQ-x86_64.AppImage` をドラッグ＆ドロップ
6. **Publish release** をクリック

### 5.4 リリースノートの書き方（テンプレート）

```markdown
## ShogiBoardQ v0.1.0

### ダウンロード

| OS | ファイル |
|---|---|
| Linux (x86_64) | `ShogiBoardQ-x86_64.AppImage` |
| macOS | `ShogiBoardQ.dmg` |
| Windows (64-bit) | `ShogiBoardQ-windows.zip` |

### Linux での起動方法

1. AppImage ファイルをダウンロード
2. 実行権限を付与: `chmod +x ShogiBoardQ-x86_64.AppImage`
3. 実行: `./ShogiBoardQ-x86_64.AppImage`

> FUSE がインストールされていない場合は `--appimage-extract-and-run` オプションで起動できます。

### 変更点

- ...
```

---

## 6. トラブルシューティング

### Qt が見つからない

```
CMake Error: Could not find a package configuration file provided by "Qt6"
```

Qt のインストールパスを明示的に指定する：

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/gcc_64"
```

### Qt Charts が見つからない

ディストリビューションのパッケージで Qt をインストールした場合、Qt Charts が別パッケージになっていることがある：

```bash
# Ubuntu / Debian
sudo apt install qt6-charts-dev

# Fedora
sudo dnf install qt6-qtcharts-devel

# Arch Linux
sudo pacman -S qt6-charts
```

### OpenGL 関連のエラー

```
Could not find EGL/egl.h
```

OpenGL 開発ヘッダーをインストール：

```bash
# Ubuntu / Debian
sudo apt install libgl1-mesa-dev libegl1-mesa-dev

# Fedora
sudo dnf install mesa-libGL-devel mesa-libEGL-devel

# Arch Linux
sudo pacman -S mesa
```

### linuxdeploy が起動しない

```
dlopen(): error loading libfuse.so.2
```

FUSE 2 ライブラリが必要：

```bash
# Ubuntu / Debian
sudo apt install libfuse2

# Fedora
sudo dnf install fuse-libs

# Arch Linux
sudo pacman -S fuse2
```

FUSE をインストールできない環境（Docker コンテナ等）では、`--appimage-extract-and-run` オプションを使用：

```bash
build/linuxdeploy-x86_64.AppImage --appimage-extract-and-run \
  --appdir build/AppDir \
  --executable build/AppDir/usr/bin/ShogiBoardQ \
  --desktop-file resources/platform/shogiboardq.desktop \
  --icon-file resources/icons/shogiboardq.png \
  --plugin qt \
  --output appimage
```

### AppImage が起動しない

```
AppImages require FUSE to run.
```

FUSE をインストールするか、`--appimage-extract-and-run` オプションで起動：

```bash
./ShogiBoardQ-x86_64.AppImage --appimage-extract-and-run
```

または、手動で展開して実行：

```bash
./ShogiBoardQ-x86_64.AppImage --appimage-extract
cd squashfs-root
./AppRun
```

### xcb プラットフォームプラグインのエラー

```
qt.qpa.plugin: Could not load the Qt platform plugin "xcb"
```

X11 関連の依存ライブラリが不足：

```bash
# Ubuntu / Debian
sudo apt install libxcb-xinerama0 libxcb-cursor0

# Fedora
sudo dnf install xcb-util-cursor xcb-util-wm xcb-util-keysyms

# Arch Linux
sudo pacman -S xcb-util-cursor xcb-util-wm xcb-util-keysyms
```

### Wayland 環境で表示が崩れる

Wayland 環境で問題がある場合、XWayland 経由で起動する：

```bash
QT_QPA_PLATFORM=xcb ./ShogiBoardQ-x86_64.AppImage
```

### 翻訳が読み込まれない

`.qm` ファイルが実行ファイルと同じディレクトリに存在するか確認：

```bash
# AppImage を展開して確認
./ShogiBoardQ-x86_64.AppImage --appimage-extract
ls squashfs-root/usr/bin/*.qm
```

ない場合、ビルドスクリプトの `.qm` コピー処理を確認する。

### glibc バージョンエラー

```
version `GLIBC_2.xx' not found
```

AppImage はビルド環境の glibc バージョン以降のシステムでのみ動作する。
古いシステムで実行したい場合は、より古いディストリビューション（Ubuntu 22.04 等）でビルドする。
