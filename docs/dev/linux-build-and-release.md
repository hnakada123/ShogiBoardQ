# Linux ビルド・リリース手順

ShogiBoardQ を Linux でビルドし、AppImage・問題集・Hayanagi を含む `ShogiBoardQ-linux.zip` を GitHub で公開する手順。
3・5・7・9・11・13手詰の問題集（各1,000題、計6,000題）と Hayanagi の USI エンジンは、ZIP 展開後すぐ選択できる外部ファイルとしても配置する。
**リリース添付は Linux 用 ZIP 1個のみとする。** Qt 関連ファイルや AppImage を別添付しない。

配布前に [Qt 文書とリリース添付の方針](qt-licensing.md) に従って
AppImage 内のライセンス文書を準備してください。Qt ソースは文書抽出のための作業用ファイルです。
配布スクリプトには Python 3 も必要です。

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
| Qt | 6.x（Widgets, Charts, Network, Multimedia, Sql, LinguistTools） |
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
  libfuse2 file python3 imagemagick
```

#### Fedora

```bash
sudo dnf install gcc-c++ cmake ninja-build git curl fuse-libs file python3 ImageMagick
```

#### Arch Linux

```bash
sudo pacman -S base-devel cmake ninja git curl fuse2 file python imagemagick
```

### 2.2 Qt 6 のインストール

#### 方法A: Qt Online Installer（推奨）

[Qt 公式サイト](https://www.qt.io/download-qt-installer)からインストーラをダウンロードし、以下のコンポーネントを選択：

- Qt 6.x > Desktop gcc 64-bit
- Qt 6.x > Qt Charts
- Qt 6.x > Qt Multimedia
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
  qt6-tools-dev qt6-tools-dev-tools qt6-multimedia-dev libqt6sql6-sqlite libgl1-mesa-dev
```

**Fedora:**

```bash
sudo dnf install qt6-qtbase-devel qt6-qtcharts-devel \
  qt6-linguist qt6-qttools-devel qt6-qtmultimedia-devel mesa-libGL-devel
```

**Arch Linux:**

```bash
sudo pacman -S qt6-base qt6-charts qt6-tools qt6-multimedia
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

`scripts/build-linux.sh` を使うと、Release ビルドから AppImage / ZIP 作成まで一括実行できる：

```bash
# 通常ビルド + AppImage / ZIP 作成
./scripts/build-linux.sh

# クリーンビルド、AppImage なし
./scripts/build-linux.sh --clean --skip-appimage
```

| オプション | 説明 |
|---|---|
| `--skip-appimage` | AppImage / ZIP 作成をスキップ（ビルドのみ） |
| `--clean` | build ディレクトリを削除してからビルド |
| `--help` | ヘルプを表示 |

スクリプトは以下の処理を自動実行する：

1. 前提ツールの存在確認（cmake, python3、ninja は推奨）
2. CMake Configure + Release ビルド（ShogiBoardQ と Hayanagi）
3. ビルド成果物、翻訳ファイル、3〜13手詰の6ファイルの確認
4. linuxdeploy + Qt プラグイン + appimagetool のダウンロード（初回のみ）
5. Hayanagi・問題集・説明書・Qt ライセンスを含む AppImage 作成
6. AppImage と、外から参照できる問題集・Hayanagi を含む ZIP 作成

出力はリポジトリ直下の `ShogiBoardQ-linux-x86_64.AppImage` と
`ShogiBoardQ-linux.zip`。ZIP 用の作業ディレクトリは `build/ShogiBoardQ-linux/`。
Qt の対応ソースと文書は事前に [qt-licensing.md](qt-licensing.md) に従って準備する。
既定の文書ディレクトリは `build/qt-licenses`。別の場所を使う場合は次のように指定する。

```bash
SHOGIBOARDQ_QT_LICENSE_DIR=/path/to/matching-qt-licenses ./scripts/build-linux.sh
```

ビルド時と文書の Qt バージョンが一致しない場合は配布ファイルを生成しない。
OS パッケージ版 Qt を使う場合は、同じ上流バージョンのソースに加え、
そのパッケージのビルド手順と適用パッチの取得元も `QT-SOURCE.json` に記録する。
これらを Release の別添付ファイルにはしない。

以下は個別のコマンドを手動で実行する場合の手順。

### 3.1 Release ビルド

```bash
cd /path/to/ShogiBoardQ

# 初回のみ: Hayanagi サブモジュールを取得
git submodule update --init --recursive

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
ls -la build/ShogiBoardQ build/Hayanagi/hayanagi

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

# AppImage 生成ツール
curl -fSL -o build/appimagetool-x86_64.AppImage \
  https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x build/appimagetool-x86_64.AppImage
```

### 4.2 .desktop ファイルとアイコンの準備

AppImage の作成には FreeDesktop 準拠の `.desktop` ファイルとアイコンが必要。
これらはリポジトリに同梱されている：

- `resources/platform/shogiboardq.desktop` — デスクトップエントリ
- `resources/icons/linux/shogiboardq.png` — アプリケーションアイコン

### 4.3 AppDir の作成と AppImage 生成

配布処理は `scripts/build-linux.sh` に集約している。手動で Release ビルドした場合も、
準備済み Qt 文書を指定して同スクリプトを実行する（本体の再ビルドは差分のみ）。

```bash
./scripts/build-linux.sh

# FUSE を利用できないビルド環境
APPIMAGE_EXTRACT_AND_RUN=1 ./scripts/build-linux.sh
```

スクリプトは `build/AppDir` を作り直し、次のファイルを配置する。

| 元ファイル | AppDir 内の配置先 |
|---|---|
| `build/ShogiBoardQ`、`build/*.qm` | `usr/bin/` |
| `build/Hayanagi/hayanagi` | `usr/bin/hayanagi` |
| `data/tsumeshogi/tsume_{3,5,7,9,11,13}ply_*.txt` | `usr/share/ShogiBoardQ/data/tsumeshogi/` |
| 問題集の `README.md`、`validation_*.json` | 同上 |
| `Hayanagi/README.md` | `usr/share/ShogiBoardQ/Hayanagi/` |
| 本手順書、Qt 配布手順、詰将棋対局・生成の説明書 | `usr/share/ShogiBoardQ/docs/dev/` |
| 準備済み Qt 文書・ビルド情報 | `usr/share/licenses/ShogiBoardQ/` |

問題集は内容を変更せずコピーする。アプリ内蔵の監査記録とハッシュが一致するため、
同梱問題集の読み込み時に検証済みの手数と手順を再利用できる。

`linuxdeploy` に ShogiBoardQ と Hayanagi の両実行ファイルを渡して依存ライブラリを収集し、
Qt プラグインを配置する。SQLite ドライバー（`sqldrivers/libqsqlite.so`）も必須。
フィルタ済みプラグイン、システムの `strip`、同梱 Qt に検索先を固定する `AppRun` を使い、
最後に `qt_licenses.py stage` で文書を検証して `appimagetool` でパッケージ化する。

### 4.4 AppImage の構造

```
ShogiBoardQ-linux-x86_64.AppImage（単一実行ファイル）
  └── (展開時)
      ├── AppRun                          ← エントリーポイント
      ├── shogiboardq.desktop             ← デスクトップエントリ
      ├── shogiboardq.png                 ← アイコン
      └── usr/
          ├── bin/
          │   ├── ShogiBoardQ             ← 実行ファイル
          │   ├── hayanagi                ← Hayanagi USI エンジン
          │   ├── ShogiBoardQ_ja_JP.qm    ← 日本語翻訳
          │   └── ShogiBoardQ_en.qm       ← 英語翻訳
          ├── lib/
          │   ├── libQt6Core.so.6         ← Qt Core
          │   ├── libQt6Gui.so.6          ← Qt GUI
          │   ├── libQt6Widgets.so.6      ← Qt Widgets
          │   ├── libQt6Charts.so.6       ← Qt Charts
          │   ├── libQt6Network.so.6      ← Qt Network
          │   ├── libQt6Multimedia.so.6   ← Qt Multimedia（駒音）
          │   └── ...
          ├── share/
          │   ├── ShogiBoardQ/
          │   │   ├── data/tsumeshogi/
          │   │   │   ├── tsume_3ply_1000_20260926.txt
          │   │   │   ├── tsume_5ply_1000_20260926.txt
          │   │   │   ├── tsume_7ply_1000_20260926.txt
          │   │   │   ├── tsume_9ply_1000_20260926.txt
          │   │   │   ├── tsume_11ply_1000_20260926.txt
          │   │   │   ├── tsume_13ply_1000_20260926.txt
          │   │   │   ├── README.md
          │   │   │   └── validation_20260926.json
          │   │   ├── Hayanagi/README.md
          │   │   └── docs/dev/            ← 本手順書など
          │   └── licenses/ShogiBoardQ/    ← Qt 文書・対応ソース情報
          └── plugins/
              ├── platforms/
              │   └── libqxcb.so          ← X11 プラットフォームプラグイン
              ├── imageformats/
              │   ├── libqsvg.so          ← SVG サポート
              │   └── ...
              ├── sqldrivers/
              │   └── libqsqlite.so       ← 解答履歴・解析キャッシュ
              └── tls/
                  └── libqopensslbackend.so
```

### 4.5 ZIP の構造と同梱ファイルの利用

AppImage 内の問題集と通常対局用 Hayanagi は、現行アプリでは自動的にファイル選択できない。
ZIP を展開してすぐ使えるよう、問題集とエンジンを AppImage の外にも配置する。
詰将棋対局用の内蔵 Hayanagi はアプリ本体に組み込まれており、エンジン登録は不要。

ZIP の外部ファイルには `docs/`、問題集の検証記録 `validation_*.json`、`licenses/` を含めない。
Qt 文書は AppImage 内に収録し、「バージョン情報」から参照する。

```text
ShogiBoardQ-linux.zip
└── ShogiBoardQ-linux/
    ├── ShogiBoardQ-linux-x86_64.AppImage
    ├── README.md
    ├── LICENSE
    ├── Hayanagi/
    │   ├── hayanagi
    │   └── README.md
    └── data/tsumeshogi/
        ├── tsume_{3,5,7,9,11,13}ply_1000_20260926.txt（6ファイル）
        └── README.md
```

```bash
unzip ShogiBoardQ-linux.zip
cd ShogiBoardQ-linux
chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi
./ShogiBoardQ-linux-x86_64.AppImage
```

- 詰将棋対局の「局面集を開く…」で `data/tsumeshogi/tsume_*ply_1000_20260926.txt` を選ぶ。
- 通常対局用のエンジン登録では `Hayanagi/hayanagi` を選ぶ。
- 詳しい開発・操作手順書はリポジトリの `docs/dev/` を参照する。

AppImage 内にも同じ問題集とエンジンが入っている。そちらを取り出す必要がある場合は
`./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract` を実行し、問題集は
`squashfs-root/usr/share/ShogiBoardQ/data/tsumeshogi/`、エンジンは
`squashfs-root/usr/bin/hayanagi` を参照する。

### 4.6 動作テスト

```bash
# 実行権限の確認
chmod +x ShogiBoardQ-linux-x86_64.AppImage

# 起動
./ShogiBoardQ-linux-x86_64.AppImage

# 展開して同梱物を確認
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract
ls squashfs-root/usr/share/ShogiBoardQ/data/tsumeshogi/
printf 'usi\nisready\nquit\n' | squashfs-root/usr/bin/hayanagi

# ZIP に AppImage・問題集・Hayanagi があり、破損がないことを確認
python3 -m zipfile -l ShogiBoardQ-linux.zip
python3 -m zipfile -t ShogiBoardQ-linux.zip
```

> **重要**: テストは Qt の lib ディレクトリが `LD_LIBRARY_PATH` に**含まれない**環境で行うこと。
> 含まれていると、AppImage 内のライブラリではなくシステムのライブラリが使われてしまい、
> バンドル漏れを検出できない。

---

## 5. GitHub Release での公開

**Linux の公開対象は `ShogiBoardQ-linux.zip` のみ。**
ZIP には AppImage・問題集・Hayanagi と利用説明を収録する。Qt ソース、パッチ集、`QT-SOURCE.json`、`SOURCE_CODE.md`、
`BUILD-INFO.txt`、ソースアーカイブ、チェックサム、SBOM は別添付しない。
Qt 文書は AppImage 内に収録し、ZIP 展開後の `licenses/` は作成しない。

アップロード時はファイル名を明示し、作業フォルダ全体や `assets/*` を指定しない。
公開後は GitHub Release の添付一覧を確認する。GitHub が自動表示する Source code の
ダウンロードリンクは手動添付ファイルとは別扱いとなる。

以下はタグ `v0.1.0` の例。Linux のファイル名は版番号にかかわらず `ShogiBoardQ-linux.zip` とする。
CI は各 OS の製品ファイルだけを明示してアップロードし、SHA256 と SBOM は
Actions の `release-verification` artifact に保存する。

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
  ShogiBoardQ-linux.zip
```

> 他プラットフォームのファイルも同時に公開する場合：
> ```bash
> gh release create v0.1.0 \
>   --title "ShogiBoardQ v0.1.0" \
>   --notes-file RELEASE_NOTES.md \
>   ShogiBoardQ-linux.zip \
>   ShogiBoardQ.dmg \
>   ShogiBoardQ-windows.zip
> ```

#### リリースノートの自動生成

```bash
gh release create v0.1.0 \
  --title "ShogiBoardQ v0.1.0" \
  --generate-notes \
  ShogiBoardQ-linux.zip
```

#### 既存リリースにアセットを追加

他のプラットフォームで先にリリースを作成済みの場合：

```bash
gh release upload v0.1.0 ShogiBoardQ-linux.zip
```

### 5.3 Web UI からリリース作成（代替）

1. GitHub リポジトリ → **Releases** → **Draft a new release**
2. **Choose a tag** → 新しいタグ（例: `v0.1.0`）を入力して作成
3. **Release title** を入力（例: `ShogiBoardQ v0.1.0`）
4. **Description** にリリースノートを記入
5. **Attach binaries** に `ShogiBoardQ-linux.zip` だけをドラッグ＆ドロップ
6. **Publish release** をクリック

### 5.4 リリースノートの書き方（テンプレート）

```markdown
## ShogiBoardQ v0.1.0

### ダウンロード

| OS | ファイル |
|---|---|
| Linux (x86_64) | `ShogiBoardQ-linux.zip`（AppImage・詰将棋問題集・Hayanagi） |
| macOS | `ShogiBoardQ.dmg` |
| Windows (64-bit) | `ShogiBoardQ-windows.zip` |

### Linux での起動方法

1. ZIP ファイルをダウンロードして展開
2. 展開先の `ShogiBoardQ-linux` ディレクトリへ移動
3. 実行権限を付与: `chmod +x ShogiBoardQ-linux-x86_64.AppImage Hayanagi/hayanagi`
4. 実行: `./ShogiBoardQ-linux-x86_64.AppImage`

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

FUSE をインストールできない環境（Docker コンテナ等）では、配布ツールを
展開して実行する環境変数を指定する：

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./scripts/build-linux.sh
```

### AppImage が起動しない

```
AppImages require FUSE to run.
```

FUSE をインストールするか、`--appimage-extract-and-run` オプションで起動：

```bash
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract-and-run
```

または、手動で展開して実行：

```bash
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract
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
QT_QPA_PLATFORM=xcb ./ShogiBoardQ-linux-x86_64.AppImage
```

### 翻訳が読み込まれない

`.qm` ファイルが実行ファイルと同じディレクトリに存在するか確認：

```bash
# AppImage を展開して確認
./ShogiBoardQ-linux-x86_64.AppImage --appimage-extract
ls squashfs-root/usr/bin/*.qm
```

ない場合、ビルドスクリプトの `.qm` コピー処理を確認する。

### glibc バージョンエラー

```
version `GLIBC_2.xx' not found
```

AppImage はビルド環境の glibc バージョン以降のシステムでのみ動作する。
古いシステムで実行したい場合は、より古いディストリビューション（Ubuntu 22.04 等）でビルドする。
