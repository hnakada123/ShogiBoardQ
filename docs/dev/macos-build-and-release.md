# macOS ビルド・リリース手順

ShogiBoardQ を macOS でビルドし、DMG ファイルとしてリリースする手順。

配布前に [Qt 文書とリリース添付の方針](qt-licensing.md) に従って
アプリ内のライセンス文書を準備してください。配布スクリプトには Python 3 も必要です。
Release の添付は配布 ZIP（`ShogiBoardQ-macos.zip`）のみとし、Qt ソースや関連文書を別添付しません。

DMG には ShogiBoardQ の実行に必要なファイルだけを入れる（最小構成）。
Linux 版と同様に、通常対局用の Hayanagi（USI エンジン）と詰将棋問題集は DMG の外に置き、DMG と一緒に配布 ZIP に入れる（[5.2](#52-配布-zip)）。
UI 言語は日本語・英語・中国語（簡体字・繁体字）の 4 言語で、翻訳ファイル 4 個を同梱する。

---

## 目次

1. [前提条件](#1-前提条件)
2. [開発環境のセットアップ](#2-開発環境のセットアップ)
3. [ビルド](#3-ビルド)
4. [アプリバンドルの作成](#4-アプリバンドルの作成)
5. [DMG ファイルの作成](#5-dmg-ファイルの作成)
6. [コード署名と公証](#6-コード署名と公証)
7. [GitHub Release での公開](#7-github-release-での公開)
8. [トラブルシューティング](#8-トラブルシューティング)

---

## 1. 前提条件

| 項目 | バージョン |
|---|---|
| macOS | 12 (Monterey) 以降推奨 |
| Xcode | 14 以降（Command Line Tools 含む） |
| CMake | 3.16 以上 |
| Qt | 6.x（Widgets, Charts, Network, Multimedia, LinguistTools） |
| C++ | C++17 対応コンパイラ |

---

## 2. 開発環境のセットアップ

### 2.1 Xcode Command Line Tools

```bash
xcode-select --install
```

### 2.2 Homebrew で依存ツールをインストール

```bash
brew install cmake ninja
```

### 2.3 Qt 6 のインストール

#### 方法A: Qt Online Installer（推奨）

[Qt 公式サイト](https://www.qt.io/download-qt-installer)からインストーラをダウンロードし、以下のコンポーネントを選択：

- Qt 6.x > macOS
- Qt 6.x > Qt Charts
- Qt 6.x > Qt Multimedia
- Developer and Designer Tools > CMake

インストール後、Qt のパスを環境変数に設定：

```bash
# ~/.zshrc に追加（Qt のインストールパスは環境に合わせて変更）
export Qt6_DIR="$HOME/Qt/6.x.x/macos/lib/cmake/Qt6"
export PATH="$HOME/Qt/6.x.x/macos/bin:$PATH"
```

#### 方法B: Homebrew

```bash
brew install qt@6
```

Homebrew の場合、CMake が自動検出するため環境変数の設定は不要な場合が多い。

### 2.4 インストール確認

```bash
cmake --version       # 3.16 以上
qmake --version       # Qt 6.x
clang++ --version     # Apple Clang
```

---

## 3. ビルド

### ビルドスクリプト（推奨）

`scripts/build-macos.sh` を使うと、Release ビルドからコード署名・DMG 作成まで一括実行できる：

```bash
# 通常ビルド + DMG 作成
./scripts/build-macos.sh

# Universal Binary (arm64 + x86_64) でビルド
./scripts/build-macos.sh --universal

# クリーンビルド、DMG なし
./scripts/build-macos.sh --clean --skip-dmg

# Qt ライセンス文書を準備していないときの試験用 DMG（配布には使わない）
./scripts/build-macos.sh --clean --skip-qt-licenses

# Developer ID で署名（既定はアドホック署名）
./scripts/build-macos.sh --sign-identity "Developer ID Application: Your Name (TEAMID)"
```

| オプション | 説明 |
|---|---|
| `--universal` | Universal Binary (arm64 + x86_64) をビルド |
| `--deployment-target VER` | 最小対応 macOS バージョン（既定: 環境変数 `MACOSX_DEPLOYMENT_TARGET`、未設定なら `26.0`） |
| `--sign-identity ID` | コード署名 ID（既定: `-` = アドホック署名）。Developer ID を指定すると Hardened Runtime とタイムスタンプを付けて署名する |
| `--skip-dmg` | DMG と配布 ZIP の作成をスキップ（.app バンドルのみ生成） |
| `--skip-qt-licenses` | [Qt 文書](qt-licensing.md) の追加をスキップし、ビルド時に同梱される簡易文書（`Contents/MacOS/licenses`）のみにする。配布用には付けない |
| `--clean` | build ディレクトリを削除してからビルド |
| `--help` | ヘルプを表示 |

スクリプトは以下の処理を自動実行する：

1. 前提ツールの存在確認（cmake, ninja, macdeployqt, codesign, vtool, create-dmg）
2. CMake Configure（`CMAKE_OSX_DEPLOYMENT_TARGET` を指定）+ Ninja ビルド
3. ビルド成果物の確認（.app、実行ファイルの最小 macOS バージョン、.qm 翻訳ファイル）
4. macdeployqt によるフレームワークバンドル
5. 未使用の Qt 部品の削除と、Qt バイナリの arm64 化（[4.4](#44-未使用の-qt-部品の削除最小構成)。`--universal` 時は arm64 化しない）+ 検証
6. Qt ライセンス文書の同梱（`--skip-qt-licenses` でスキップ）
7. バンドル全体のコード署名 + `codesign --verify --deep --strict` による検証
8. create-dmg による DMG 作成
9. DMG・Hayanagi・詰将棋問題集を入れた配布 ZIP（`ShogiBoardQ-macos.zip`）の作成（[5.2](#52-配布-zip)）

> **最小 macOS バージョンについて:** デプロイメントターゲットを指定しないと、ビルドホストの macOS バージョンが最小対応バージョンになる（例: macOS 27 でビルドすると macOS 26 で起動できない）。スクリプトは既定で `26.0` を指定し、ビルド後に実行ファイルの `minos` が一致しなければ停止する。
>
> **コード署名について:** macdeployqt はバイナリを書き換えるため、実行後のバンドルはリンカ署名のみの状態になり、厳格な署名検証に通らない。スクリプトは macdeployqt の後にバンドル全体を署名し直してから DMG を作成する。
>
> **macdeployqt の ERROR 表示について:** `.qm: is not an object file`（翻訳ファイルに otool を実行したもの）、ODBC / PostgreSQL / Mimer の SQL ドライバーの依存先が見つからないという表示、macdeployqt 自身の署名エラーが出るが、いずれも無害。これらのドライバーは次の手順で削除し、署名はスクリプトがやり直して厳格に検証する。

以下は個別のコマンドを手動で実行する場合の手順。

### 3.1 Release ビルド

```bash
cd /path/to/ShogiBoardQ

# Configure（Release ビルド）
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# ビルド（並列ビルド）
cmake --build build --config Release -- -j$(sysctl -n hw.ncpu)
```

Ninja を使う場合（より高速）：

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
```

### 3.2 ビルド成果物の確認

```bash
# .app バンドルが生成されていることを確認
ls -la build/ShogiBoardQ.app/

# バンドル内の構造を確認
find build/ShogiBoardQ.app -type f | head -20
```

ビルド後のバンドル構造：

```
ShogiBoardQ.app/
├── Contents/
│   ├── Info.plist
│   ├── MacOS/
│   │   ├── ShogiBoardQ              ← 実行ファイル
│   │   ├── ShogiBoardQ_ja_JP.qm     ← 日本語翻訳
│   │   ├── ShogiBoardQ_en.qm        ← 英語翻訳
│   │   ├── ShogiBoardQ_zh_CN.qm     ← 中国語（簡体字）翻訳
│   │   ├── ShogiBoardQ_zh_TW.qm     ← 中国語（繁体字）翻訳
│   │   └── licenses/                ← ライセンス文書（簡易版）
│   └── Resources/
│       └── shogiboardq.icns          ← アプリアイコン
```

### 3.3 動作確認

```bash
# ビルドしたアプリを起動
open build/ShogiBoardQ.app
```

---

## 4. アプリバンドルの作成

> **Note:** `scripts/build-macos.sh` を使用した場合、このセクションの手順は自動実行されるため手動での実行は不要。

### 4.1 macdeployqt で Qt フレームワークをバンドル

ビルドしただけでは Qt の動的ライブラリがバンドルに含まれない。
`macdeployqt` を使って、必要な Qt フレームワークとプラグインをバンドル内にコピーする。

```bash
macdeployqt build/ShogiBoardQ.app -verbose=2
```

`macdeployqt` が自動で行う処理：

1. 依存する Qt フレームワーク（QtWidgets, QtCharts, QtNetwork, QtMultimedia, QtGui, QtCore 等）を `Contents/Frameworks/` にコピー
2. Qt プラグイン（platforms/cocoa, imageformats/svg 等）を `Contents/PlugIns/` にコピー
3. ライブラリの `@rpath` を書き換え（`install_name_tool`）
4. `qt.conf` を `Contents/Resources/` に生成

### 4.2 バンドル後の構造確認

```bash
# フレームワークが正しくバンドルされているか確認
ls build/ShogiBoardQ.app/Contents/Frameworks/

# プラグインが含まれているか確認
ls build/ShogiBoardQ.app/Contents/PlugIns/

# 依存関係に外部パスが残っていないか確認
otool -L build/ShogiBoardQ.app/Contents/MacOS/ShogiBoardQ
```

`otool` の出力に `/usr/local/` や `/opt/homebrew/` のパスが残っている場合、バンドルが不完全。

### 4.3 翻訳ファイルの確認

CMake のポストビルドステップにより `.qm` ファイルは `Contents/MacOS/` に配置される。
`macdeployqt` 実行後もファイルが残っていることを確認：

```bash
ls build/ShogiBoardQ.app/Contents/MacOS/*.qm   # 4 個（ja_JP / en / zh_CN / zh_TW）
```

Qt 標準ダイアログの翻訳（`qtbase_ja` / `qtbase_zh_CN` / `qtbase_zh_TW`）は実行ファイルのリソースに埋め込まれるため、別ファイルは不要。

### 4.4 未使用の Qt 部品の削除（最小構成）

macdeployqt は種類ごとに Qt のプラグインをすべて配置し、仮想キーボードのプラグイン経由で Qt Quick / QML まで取り込む（約 187MB）。
ShogiBoardQ が使わないものを削除し、Apple Silicon 専用の配布では Qt の x86_64 部分も取り除く（約 89MB、DMG は約 50MB）。

| 削除するもの | 理由 |
|---|---|
| `PlugIns/platforminputcontexts/`、`QtQml*` / `QtQuick` / `QtVirtualKeyboard*` | 仮想キーボード・QML を使わない |
| `PlugIns/tls/`、`PlugIns/networkinformation/` | CSA 通信は平文 TCP のみ |
| `PlugIns/multimedia/libffmpegmediaplugin.dylib`、`libav*` / `libsw*` | 駒音は macOS 標準の darwin バックエンドで再生する |
| `PlugIns/sqldrivers/` の SQLite 以外 | SQLite のみ使用 |
| `PlugIns/imageformats/` の gif / wbmp / macheif / icns / tga / macjp2 | SVG / ICO（アイコン）と JPEG / TIFF / WebP（盤面画像出力）のみ使用 |

```bash
C=build/ShogiBoardQ.app/Contents
rm -rf $C/PlugIns/platforminputcontexts $C/PlugIns/tls $C/PlugIns/networkinformation \
  $C/PlugIns/multimedia/libffmpegmediaplugin.dylib $C/Frameworks/libav*.dylib $C/Frameworks/libsw*.dylib \
  $C/Frameworks/QtQml*.framework $C/Frameworks/QtQuick.framework $C/Frameworks/QtVirtualKeyboard*.framework
find $C/PlugIns/sqldrivers -name '*.dylib' ! -name libqsqlite.dylib -delete
for p in gif wbmp macheif icns tga macjp2; do rm -f $C/PlugIns/imageformats/libq$p.dylib; done

# Qt を arm64 のみにする（Universal Binary を配布する場合は行わない）
find $C -type f \( -perm +111 -o -name '*.dylib' \) | while read -r f; do
  if lipo -archs "$f" 2>/dev/null | grep -q x86_64; then
    lipo "$f" -thin arm64 -output "$f.thin" && chmod "$(stat -f %Lp "$f")" "$f.thin" && mv "$f.thin" "$f"
  fi
done
```

残るのは Qt フレームワーク 12 個（Charts, Concurrent, Core, DBus, Gui, Multimedia, Network, OpenGL, OpenGLWidgets, Sql, Svg, Widgets）と、プラグイン 9 個（cocoa, macstyle, svgicon, darwin multimedia, sqlite, ico / jpeg / tiff / webp）。
削除後はバンドル全体を署名し直す（[6.4](#64-手動でのコード署名)。アドホック署名なら `codesign --force --deep --sign - build/ShogiBoardQ.app`）。

起動後、読み込まれたライブラリがすべてバンドル内のものか確認する：

```bash
open build/ShogiBoardQ.app
lsof -p "$(pgrep -x ShogiBoardQ)" | grep -E '/Qt/|homebrew'   # 何も表示されなければよい
```

---

## 5. DMG ファイルの作成

> **Note:** `scripts/build-macos.sh` を使用した場合、方法B (create-dmg) で自動作成されるため手動での実行は不要。`--skip-dmg` オプションで DMG 作成のみスキップすることも可能。
>
> 手動で作成する場合も、DMG に入れる前に [4.4](#44-未使用の-qt-部品の削除最小構成) の削除と署名を済ませる。方法A は macdeployqt が DMG を直接作るため、最小構成にはならない。

### 方法A: macdeployqt の -dmg オプション（簡易）

```bash
macdeployqt build/ShogiBoardQ.app -dmg
```

`build/ShogiBoardQ.dmg` が生成される。シンプルだが、背景画像や Applications フォルダへのリンクはない。

### 方法B: create-dmg（推奨 - カスタム DMG）

ユーザーが Applications フォルダにドラッグ＆ドロップできる見栄えの良い DMG を作成する。

#### インストール

```bash
brew install create-dmg
```

#### DMG 作成

```bash
# macdeployqt でバンドルを作成（-dmg なし）
macdeployqt build/ShogiBoardQ.app

# DMG を作成
create-dmg \
  --volname "ShogiBoardQ" \
  --volicon "resources/icons/shogiboardq.icns" \
  --window-pos 200 120 \
  --window-size 600 400 \
  --icon-size 100 \
  --icon "ShogiBoardQ.app" 150 190 \
  --hide-extension "ShogiBoardQ.app" \
  --app-drop-link 450 190 \
  "ShogiBoardQ.dmg" \
  "build/ShogiBoardQ.app"
```

#### オプション説明

| オプション | 説明 |
|---|---|
| `--volname` | マウント時のボリューム名 |
| `--volicon` | ボリュームアイコン |
| `--window-pos` | DMG ウィンドウの表示位置 (x y) |
| `--window-size` | DMG ウィンドウのサイズ (幅 高さ) |
| `--icon-size` | アイコンサイズ (px) |
| `--icon` | アプリアイコンの表示位置 (名前 x y) |
| `--hide-extension` | .app 拡張子を非表示 |
| `--app-drop-link` | Applications フォルダへのリンク位置 (x y) |

### 方法C: hdiutil（手動 - 完全制御）

```bash
# 一時ディレクトリを作成
mkdir -p dmg_staging
cp -R build/ShogiBoardQ.app dmg_staging/
ln -s /Applications dmg_staging/Applications

# DMG を作成
hdiutil create -volname "ShogiBoardQ" \
  -srcfolder dmg_staging \
  -ov -format UDZO \
  "ShogiBoardQ.dmg"

# クリーンアップ
rm -rf dmg_staging
```

### 5.1 DMG の検証

```bash
# マウントして動作確認
hdiutil attach ShogiBoardQ.dmg
open /Volumes/ShogiBoardQ/ShogiBoardQ.app

# アンマウント
hdiutil detach /Volumes/ShogiBoardQ
```

### 5.2 配布 ZIP

スクリプトは DMG の後に、リポジトリ直下に `ShogiBoardQ-macos.zip` を作る。構成は Linux 版の ZIP に合わせる。

```
ShogiBoardQ-macos/
├── ShogiBoardQ.dmg
├── README.md                      ← resources/platform/README-macos.md
├── LICENSE
├── Hayanagi/
│   ├── hayanagi                   ← 通常対局用の USI エンジン（arm64、最小 macOS はアプリと同じ）
│   └── README.md
└── data/tsumeshogi/
    ├── tsume_{3,5,7,9,11,13}ply_1000_YYYYMMDD.txt
    └── README.md
```

- 問題集は各手数1ファイルに限る。旧版などが残っているとスクリプトは停止する。
- `hayanagi` は `build/Hayanagi/hayanagi` を `strip -x` し、アプリと同じ ID（既定はアドホック）で署名し直す。
- ダウンロードした `hayanagi` には隔離属性が付くため、README で `xattr -d com.apple.quarantine Hayanagi/hayanagi` を案内している。

```bash
unzip -l ShogiBoardQ-macos.zip
# Hayanagi が応答するか
(printf 'usi\nisready\nposition startpos\ngo movetime 300\n'; sleep 1.5; echo quit) | build/ShogiBoardQ-macos/Hayanagi/hayanagi | grep -E 'usiok|readyok|bestmove'
```

---

## 6. コード署名と公証

配布する場合、Apple の Gatekeeper を通過するためにコード署名と公証が必要。
署名なしでも動作するが、ダウンロード時に「開発元が未確認」の警告が表示される。

### 6.1 Apple Developer Program への加入

コード署名と公証には [Apple Developer Program](https://developer.apple.com/programs/)（年額 $99）への加入が必要。

### 6.2 署名用証明書の取得

Xcode > Settings > Accounts から以下の証明書を作成：

- **Developer ID Application** — アプリ本体の署名用
- **Developer ID Installer** — DMG / pkg の署名用（オプション）

### 6.3 macdeployqt でコード署名付きバンドル

```bash
macdeployqt build/ShogiBoardQ.app \
  -sign-for-notarization="Developer ID Application: Your Name (TEAMID)"
```

このオプションにより、`macdeployqt` は：
1. Qt フレームワークとプラグインをバンドル
2. 全フレームワーク・プラグイン・実行ファイルにコード署名
3. `--options=runtime`（Hardened Runtime）を有効化

### 6.4 手動でのコード署名

`macdeployqt` の署名オプションを使わない場合：

```bash
# バンドル内の全バイナリに再帰的に署名
codesign --deep --force --verify --verbose \
  --sign "Developer ID Application: Your Name (TEAMID)" \
  --options runtime \
  build/ShogiBoardQ.app

# 署名の検証
codesign --verify --deep --strict --verbose=2 build/ShogiBoardQ.app
```

### 6.5 公証 (Notarization)

```bash
# DMG を作成（署名済みバンドルから）
# ... (手順5の方法で DMG を作成)

# 公証に提出
xcrun notarytool submit ShogiBoardQ.dmg \
  --apple-id "your@email.com" \
  --team-id "TEAMID" \
  --password "app-specific-password" \
  --wait

# ステープル（公証結果をDMGに埋め込み）
xcrun stapler staple ShogiBoardQ.dmg

# ステープルの検証
xcrun stapler validate ShogiBoardQ.dmg
```

> **注意**: `--password` には Apple ID のパスワードではなく、[App 用パスワード](https://appleid.apple.com/account/manage)を使用する。

### 6.6 公証なしで配布する場合

署名や公証を行わずに配布する場合、ユーザーは初回起動時に以下の手順が必要：

1. Finder でアプリを右クリック → 「開く」を選択
2. 「開発元が未確認」の警告で「開く」をクリック

または、ターミナルから Gatekeeper の隔離属性を解除：

```bash
xattr -cr /Applications/ShogiBoardQ.app
```

---

## 7. GitHub Release での公開

### 7.1 タグの作成

版番号は、リリースする日にだけその日の日付に上げる（[版番号とリリースの運用](versioning.md)）。配布物を作る前に `CMakeLists.txt` の `APP_VERSION` を変えてコミットし、そのコミットから配布物を作ってから、同じコミットに同じ版番号のタグを付ける。タグには `v` を付けない。

```bash
git tag -a 2026.10.07 -m "ShogiBoardQ 2026.10.07"
git push origin main 2026.10.07
```

### 7.2 GitHub CLI でリリース作成

[GitHub CLI (gh)](https://cli.github.com/) を使用：

```bash
# インストール
brew install gh

# ログイン（初回のみ）
gh auth login
```

リリース作成とアセットのアップロード：

```bash
gh release create 2026.10.07 \
  --title "ShogiBoardQ 2026.10.07" \
  --notes-file RELEASE_NOTES.md \
  ShogiBoardQ-macos.zip
```

> Windows の ZIP も同時に公開する場合は、アセットを追加：
> ```bash
> gh release create 2026.10.07 \
>   --title "ShogiBoardQ 2026.10.07" \
>   --notes-file RELEASE_NOTES.md \
>   ShogiBoardQ-macos.zip \
>   ShogiBoardQ-windows.zip
> ```

#### リリースノートの自動生成

リリースノートファイルを用意しない場合、GitHub が自動生成する：

```bash
gh release create 2026.10.07 \
  --title "ShogiBoardQ 2026.10.07" \
  --generate-notes \
  ShogiBoardQ-macos.zip
```

#### 既存リリースにアセットを追加

Windows 側で先にリリースを作成済みの場合、macOS の ZIP を追加：

```bash
gh release upload 2026.10.07 ShogiBoardQ-macos.zip
```

### 7.3 Web UI からリリース作成（代替）

1. GitHub リポジトリ → **Releases** → **Draft a new release**
2. **Choose a tag** → 新しいタグ（例: `2026.10.07`）を入力して作成
3. **Release title** を入力（例: `ShogiBoardQ 2026.10.07`）
4. **Description** にリリースノートを記入
5. **Attach binaries** に `ShogiBoardQ-macos.zip` をドラッグ＆ドロップ
6. **Publish release** をクリック

### 7.4 リリースノートの書き方（テンプレート）

```markdown
## ShogiBoardQ 2026.10.07

### ダウンロード

| OS | ファイル |
|---|---|
| macOS | `ShogiBoardQ-macos.zip` |
| Windows (64-bit) | `ShogiBoardQ-windows.zip` |

### macOS での起動方法

1. ZIP を展開し、`ShogiBoardQ.dmg` を開く
2. ShogiBoardQ.app を Applications フォルダにドラッグ＆ドロップ
3. Applications から起動

> 署名なしの場合、初回起動時に「開発元が未確認」と表示されます。
> Finder でアプリを右クリック →「開く」で起動できます。

### 変更点

- ...
```

---

## 8. トラブルシューティング

### Qt が見つからない

```
CMake Error: Could not find a package configuration file provided by "Qt6"
```

Qt のインストールパスを明示的に指定する：

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.x.x/macos"
```

### macdeployqt が見つからない

```bash
# Qt のインストールパスを確認
which macdeployqt

# 見つからない場合、パスを通す
export PATH="$HOME/Qt/6.x.x/macos/bin:$PATH"

# Homebrew の場合
export PATH="$(brew --prefix qt@6)/bin:$PATH"
```

### Qt Charts が macdeployqt でバンドルされない

Qt Charts は `macdeployqt` が自動検出できない場合がある。手動でコピーが必要：

```bash
# バンドル漏れの確認
otool -L build/ShogiBoardQ.app/Contents/MacOS/ShogiBoardQ | grep Charts

# 手動コピー（必要な場合）
cp -R "$HOME/Qt/6.x.x/macos/lib/QtCharts.framework" \
  build/ShogiBoardQ.app/Contents/Frameworks/

# install_name_tool で参照パスを修正
install_name_tool -change \
  @rpath/QtCharts.framework/Versions/A/QtCharts \
  @executable_path/../Frameworks/QtCharts.framework/Versions/A/QtCharts \
  build/ShogiBoardQ.app/Contents/MacOS/ShogiBoardQ
```

### 「開発元が未確認」の警告（署名なしの場合）

配布先のユーザーに以下のいずれかを案内する：

- Finder で右クリック → 「開く」（初回のみ）
- `xattr -cr /Applications/ShogiBoardQ.app` をターミナルで実行

### 翻訳が読み込まれない

`.qm` ファイルがバンドル内に存在するか確認：

```bash
find build/ShogiBoardQ.app -name "*.qm"
```

`Contents/MacOS/` 内に `.qm` ファイルがない場合、手動でコピー：

```bash
cp build/ShogiBoardQ_{ja_JP,en,zh_CN,zh_TW}.qm build/ShogiBoardQ.app/Contents/MacOS/
```

### Universal Binary (Intel + Apple Silicon) の作成

両アーキテクチャに対応したバイナリを作成する場合：

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

cmake --build build --config Release
```

> **注意**: Qt 自体も Universal Binary 版がインストールされている必要がある。
