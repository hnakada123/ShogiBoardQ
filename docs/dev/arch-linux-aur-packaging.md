# Arch Linux パッケージング（AUR 公開）手順

ShogiBoardQ を AUR (Arch User Repository) に登録し、`yay` や `paru` などの AUR ヘルパーでインストールできるようにする手順。

---

## 目次

1. [概要](#1-概要)
2. [前提条件](#2-前提条件)
3. [CMakeLists.txt の install ターゲット整備](#3-cmakeliststxt-の-install-ターゲット整備)
4. [PKGBUILD の作成](#4-pkgbuild-の作成)
5. [ローカルでのテストビルド](#5-ローカルでのテストビルド)
6. [AUR への登録](#6-aur-への登録)
7. [パッケージの更新](#7-パッケージの更新)
8. [ユーザー向けインストール手順](#8-ユーザー向けインストール手順)
9. [トラブルシューティング](#9-トラブルシューティング)

---

## 1. 概要

### AUR とは

AUR (Arch User Repository) は Arch Linux のコミュニティ主導パッケージリポジトリ。
公式リポジトリに含まれないソフトウェアを PKGBUILD スクリプト経由で配布できる。

### パッケージの種類

| 種類 | パッケージ名例 | ソース |
|---|---|---|
| 安定版（リリースタグ） | `shogiboardq` | GitHub Release のソースアーカイブ |
| Git 最新版 | `shogiboardq-git` | Git リポジトリの最新コミット |

本ガイドでは両方の PKGBUILD を作成する。

---

## 2. 前提条件

### 2.1 開発環境

```bash
# ビルドに必要な基本パッケージ
sudo pacman -S base-devel git

# AUR ヘルパー（テスト用、いずれか1つ）
# yay
sudo pacman -S --needed git base-devel
git clone https://aur.archlinux.org/yay.git
cd yay && makepkg -si

# または paru
sudo pacman -S --needed git base-devel
git clone https://aur.archlinux.org/paru.git
cd paru && makepkg -si
```

### 2.2 AUR アカウント

1. [AUR](https://aur.archlinux.org/) にアクセスし、アカウントを作成
2. SSH 公開鍵を AUR のアカウント設定に登録：

```bash
# SSH 鍵がない場合は作成
ssh-keygen -t ed25519 -C "your-email@example.com"

# 公開鍵を表示してコピー
cat ~/.ssh/id_ed25519.pub
```

3. AUR のプロフィールページ（https://aur.archlinux.org/account/ユーザー名）で **SSH Public Key** に貼り付け

4. `~/.ssh/config` に AUR の設定を追加：

```
Host aur.archlinux.org
  IdentityFile ~/.ssh/id_ed25519
  User aur
```

---

## 3. CMakeLists.txt の install ターゲット整備

AUR パッケージとしてシステムにインストールするには、CMake の `install()` コマンドで以下のファイルを適切なパスに配置する必要がある。

### 3.1 必要な変更

`CMakeLists.txt` の install セクション（`# ==== Install ====` 以降）を以下のように拡張する：

```cmake
# ==== Install ====
include(GNUInstallDirs)

# 実行ファイル
install(TARGETS ShogiBoardQ
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# デスクトップエントリ
install(FILES resources/platform/shogiboardq.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

# アプリケーションアイコン（256x256）
install(FILES resources/icons/shogiboardq.png
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
)

# 翻訳ファイル（.qm）
# .qm はビルド時に生成されるため、ビルドディレクトリからインストール
install(FILES ${QM_FILES}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
```

> **注意:** `.qm` ファイルの配置先は、アプリケーションが翻訳ファイルを検索するパスに合わせる。
> ShogiBoardQ は実行ファイルと同じディレクトリの `.qm` を読み込むため `${CMAKE_INSTALL_BINDIR}` に配置する。

### 3.2 .desktop ファイルの修正

`resources/platform/shogiboardq.desktop` の `Exec` 行にフルパスは不要（`$PATH` で解決されるため）。
現在の内容で問題ない：

```ini
[Desktop Entry]
Type=Application
Name=ShogiBoardQ
Comment=Shogi Board Application
Exec=ShogiBoardQ
Icon=shogiboardq
Categories=Game;BoardGame;
Terminal=false
```

---

## 4. PKGBUILD の作成

### 4.1 安定版（shogiboardq）

GitHub Release のソースアーカイブからビルドするパッケージ。

```bash
mkdir -p ~/aur/shogiboardq
```

`~/aur/shogiboardq/PKGBUILD` を作成：

```bash
# Maintainer: Your Name <your-email@example.com>
pkgname=shogiboardq
pkgver=0.1.0
pkgrel=1
pkgdesc='Shogi board application with USI engine support and CSA network play'
arch=('x86_64')
url='https://github.com/hnakada123/ShogiBoardQ'
license=('GPL-3.0-or-later')
depends=(
    'qt6-base'
    'qt6-charts'
)
makedepends=(
    'cmake'
    'ninja'
    'qt6-tools'   # lupdate, lrelease
)
source=("${pkgname}-${pkgver}.tar.gz::${url}/archive/refs/tags/v${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cmake -B build -S "ShogiBoardQ-${pkgver}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    DESTDIR="${pkgdir}" cmake --install build
}
```

> **`sha256sums` について:**
> 初回は `'SKIP'` で動作確認した後、`updpkgsums` コマンドで正しいハッシュに置き換える。

### 4.2 Git 最新版（shogiboardq-git）

Git リポジトリの最新コミットからビルドするパッケージ。

```bash
mkdir -p ~/aur/shogiboardq-git
```

`~/aur/shogiboardq-git/PKGBUILD` を作成：

```bash
# Maintainer: Your Name <your-email@example.com>
pkgname=shogiboardq-git
pkgver=0.1.0.r0.g1234567
pkgrel=1
pkgdesc='Shogi board application with USI engine support and CSA network play (git version)'
arch=('x86_64')
url='https://github.com/hnakada123/ShogiBoardQ'
license=('GPL-3.0-or-later')
depends=(
    'qt6-base'
    'qt6-charts'
)
makedepends=(
    'cmake'
    'ninja'
    'qt6-tools'
    'git'
)
provides=("${pkgname%-git}")
conflicts=("${pkgname%-git}")
source=("${pkgname}::git+${url}.git")
sha256sums=('SKIP')

pkgver() {
    cd "${pkgname}"
    git describe --long --tags 2>/dev/null | sed 's/^v//;s/-/.r/;s/-/./' \
        || printf "0.1.0.r%s.g%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
    cmake -B build -S "${pkgname}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    DESTDIR="${pkgdir}" cmake --install build
}
```

### 4.3 PKGBUILD の各フィールド説明

| フィールド | 説明 |
|---|---|
| `pkgname` | パッケージ名（小文字、AUR での検索名） |
| `pkgver` | バージョン番号 |
| `pkgrel` | パッケージリビジョン（同一 `pkgver` でパッケージを修正した場合にインクリメント） |
| `pkgdesc` | パッケージの説明（80 文字以内推奨） |
| `arch` | 対応アーキテクチャ |
| `url` | プロジェクトの URL |
| `license` | ライセンス（[SPDX 識別子](https://spdx.org/licenses/)） |
| `depends` | 実行時の依存パッケージ |
| `makedepends` | ビルド時のみ必要なパッケージ |
| `provides` | このパッケージが提供する仮想パッケージ（`-git` 版で使用） |
| `conflicts` | 競合するパッケージ（`-git` 版と安定版の同時インストールを防止） |
| `source` | ソースの URL |
| `sha256sums` | ソースの SHA256 チェックサム |

---

## 5. ローカルでのテストビルド

### 5.1 makepkg でビルド

```bash
cd ~/aur/shogiboardq-git   # または ~/aur/shogiboardq

# ビルド + インストール（依存パッケージも自動インストール）
makepkg -si

# ビルドのみ（インストールしない）
makepkg -s
```

| オプション | 説明 |
|---|---|
| `-s` | 不足している依存パッケージを自動インストール |
| `-i` | ビルド成功後にパッケージをインストール |
| `-f` | 既存のパッケージを上書き |
| `-c` | ビルド後にソースディレクトリをクリーンアップ |
| `-C` | ビルド前にソースディレクトリをクリーンアップ |

### 5.2 パッケージの内容確認

```bash
# 生成された .pkg.tar.zst の中身を確認
tar -tf shogiboardq-git-*.pkg.tar.zst | head -30
```

期待される内容：

```
usr/
usr/bin/
usr/bin/ShogiBoardQ
usr/bin/ShogiBoardQ_ja_JP.qm
usr/bin/ShogiBoardQ_en.qm
usr/share/
usr/share/applications/
usr/share/applications/shogiboardq.desktop
usr/share/icons/
usr/share/icons/hicolor/
usr/share/icons/hicolor/256x256/
usr/share/icons/hicolor/256x256/apps/
usr/share/icons/hicolor/256x256/apps/shogiboardq.png
```

### 5.3 インストール後の動作確認

```bash
# コマンドラインから起動
ShogiBoardQ

# パッケージ情報の確認
pacman -Qi shogiboardq-git

# インストールされたファイル一覧
pacman -Ql shogiboardq-git
```

### 5.4 namcap による検証

`namcap` は PKGBUILD とパッケージの品質をチェックするツール：

```bash
sudo pacman -S namcap

# PKGBUILD の検証
namcap PKGBUILD

# パッケージの検証
namcap shogiboardq-git-*.pkg.tar.zst
```

よくある警告と対処：

| 警告 | 対処 |
|---|---|
| `dependency X detected and not included` | `depends` に追加 |
| `dependency X included but already satisfied` | 不要な依存を `depends` から削除 |
| `ELF file ... has unstripped symbols` | 正常（`makepkg` が自動で strip する） |

### 5.5 アンインストール

```bash
sudo pacman -R shogiboardq-git
```

---

## 6. AUR への登録

### 6.1 AUR リポジトリの初期化

```bash
cd ~/aur/shogiboardq   # または shogiboardq-git

# AUR の Git リポジトリをクローン（パッケージ名で新規作成される）
git clone ssh://aur@aur.archlinux.org/shogiboardq.git aur-repo
cd aur-repo
```

> 新規パッケージの場合、空のリポジトリがクローンされる。

### 6.2 PKGBUILD と .SRCINFO の配置

```bash
# PKGBUILD をコピー
cp ../PKGBUILD .

# .SRCINFO を生成（AUR が解析するメタデータファイル）
makepkg --printsrcinfo > .SRCINFO
```

> **重要:** `.SRCINFO` は PKGBUILD を変更するたびに再生成する必要がある。

### 6.3 コミットとプッシュ

```bash
git add PKGBUILD .SRCINFO
git commit -m "Initial upload: shogiboardq 0.1.0"
git push
```

プッシュ後、https://aur.archlinux.org/packages/shogiboardq にパッケージページが作成される。

### 6.4 Git 版も同様に登録

```bash
cd ~/aur/shogiboardq-git
git clone ssh://aur@aur.archlinux.org/shogiboardq-git.git aur-repo
cd aur-repo
cp ../PKGBUILD .
makepkg --printsrcinfo > .SRCINFO
git add PKGBUILD .SRCINFO
git commit -m "Initial upload: shogiboardq-git"
git push
```

---

## 7. パッケージの更新

### 7.1 新バージョンをリリースしたとき

1. PKGBUILD の `pkgver` を新バージョンに更新
2. `pkgrel` を `1` にリセット
3. チェックサムを更新
4. `.SRCINFO` を再生成
5. コミット＆プッシュ

```bash
cd ~/aur/shogiboardq/aur-repo

# PKGBUILD を編集（pkgver を更新）
# pkgver=0.2.0
# pkgrel=1

# チェックサムを自動更新
updpkgsums

# .SRCINFO を再生成
makepkg --printsrcinfo > .SRCINFO

# テストビルド
makepkg -sf

# コミット＆プッシュ
git add PKGBUILD .SRCINFO
git commit -m "Update to 0.2.0"
git push
```

### 7.2 PKGBUILD のみ修正したとき（バージョン変更なし）

`pkgrel` をインクリメントする：

```bash
# pkgrel=2 に変更
makepkg --printsrcinfo > .SRCINFO
git add PKGBUILD .SRCINFO
git commit -m "Fix build dependencies"
git push
```

### 7.3 Git 版の場合

`-git` パッケージはバージョンが `pkgver()` 関数で自動計算されるため、PKGBUILD の `pkgver` を手動で更新する必要はない。ビルド時の依存関係やオプションを変更した場合のみ更新する。

---

## 8. ユーザー向けインストール手順

### 8.1 AUR ヘルパーを使用（推奨）

```bash
# yay の場合
yay -S shogiboardq        # 安定版
yay -S shogiboardq-git    # 最新開発版

# paru の場合
paru -S shogiboardq
paru -S shogiboardq-git
```

### 8.2 手動インストール

```bash
# リポジトリをクローン
git clone https://aur.archlinux.org/shogiboardq.git
cd shogiboardq

# ビルドしてインストール
makepkg -si
```

### 8.3 アップデート

```bash
# yay の場合（通常のシステム更新と一緒に AUR パッケージも更新される）
yay -Syu

# paru の場合
paru -Syu
```

### 8.4 アンインストール

```bash
sudo pacman -R shogiboardq
```

---

## 9. トラブルシューティング

### makepkg で Qt が見つからない

```
CMake Error: Could not find a package configuration file provided by "Qt6"
```

Qt6 の開発パッケージがインストールされているか確認：

```bash
sudo pacman -S qt6-base qt6-charts qt6-tools
```

### `==> ERROR: A failure occurred in build()`

ビルドログを確認する。よくある原因：

- **依存パッケージの不足**: `makedepends` に追加
- **CMake のバージョン不足**: `sudo pacman -S cmake`
- **ソースの URL が無効**: GitHub Release のタグ名を確認

### push が rejected される

```
remote: error: refusing to update checked out branch
```

AUR には `master` ブランチのみプッシュできる。ブランチ名を確認：

```bash
git branch -a
git push origin master
```

### .SRCINFO の生成エラー

```
==> ERROR: PKGBUILD does not exist.
```

PKGBUILD と同じディレクトリで `makepkg --printsrcinfo` を実行する。

### namcap の `dependency detected and not included` 警告

`namcap` がパッケージバイナリの動的リンクを解析し、`depends` に含まれていないライブラリを検出した場合に表示される。表示された依存を `depends` 配列に追加する。

ただし `qt6-base` が提供するライブラリ（`libQt6Core`, `libQt6Gui`, `libQt6Widgets`, `libQt6Network`）はすべて `qt6-base` への依存で解決されるため、個別に追加する必要はない。

### パッケージ削除リクエスト

AUR パッケージを削除したい場合は、AUR のパッケージページから **Deletion Request** を提出する。
