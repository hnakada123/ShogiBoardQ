# macOS ビルド・リリース手順

ShogiBoardQ を macOS でビルドし、DMG ファイルとしてリリースする手順。

---

## 目次

1. [前提条件](#1-前提条件)
2. [開発環境のセットアップ](#2-開発環境のセットアップ)
3. [ビルド](#3-ビルド)
4. [アプリバンドルの作成](#4-アプリバンドルの作成)
5. [DMG ファイルの作成](#5-dmg-ファイルの作成)
6. [コード署名と公証](#6-コード署名と公証)
7. [トラブルシューティング](#7-トラブルシューティング)

---

## 1. 前提条件

| 項目 | バージョン |
|---|---|
| macOS | 12 (Monterey) 以降推奨 |
| Xcode | 14 以降（Command Line Tools 含む） |
| CMake | 3.16 以上 |
| Qt | 6.x（Widgets, Charts, Network, LinguistTools） |
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

`scripts/build-macos.sh` を使うと、Release ビルドから DMG 作成まで一括実行できる：

```bash
# 通常ビルド + DMG 作成
./scripts/build-macos.sh

# Universal Binary (arm64 + x86_64) でビルド
./scripts/build-macos.sh --universal

# クリーンビルド、DMG なし
./scripts/build-macos.sh --clean --skip-dmg
```

| オプション | 説明 |
|---|---|
| `--universal` | Universal Binary (arm64 + x86_64) をビルド |
| `--skip-dmg` | DMG 作成をスキップ（.app バンドルのみ生成） |
| `--clean` | build ディレクトリを削除してからビルド |
| `--help` | ヘルプを表示 |

スクリプトは以下の処理を自動実行する：

1. 前提ツールの存在確認（cmake, ninja, macdeployqt, create-dmg）
2. CMake Configure + Ninja ビルド
3. ビルド成果物の確認（.app、.qm 翻訳ファイル）
4. macdeployqt によるフレームワークバンドル + 検証
5. create-dmg による DMG 作成

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
│   │   └── ShogiBoardQ_en.qm        ← 英語翻訳
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

1. 依存する Qt フレームワーク（QtWidgets, QtCharts, QtNetwork, QtGui, QtCore 等）を `Contents/Frameworks/` にコピー
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
ls build/ShogiBoardQ.app/Contents/MacOS/*.qm
```

---

## 5. DMG ファイルの作成

> **Note:** `scripts/build-macos.sh` を使用した場合、方法B (create-dmg) で自動作成されるため手動での実行は不要。`--skip-dmg` オプションで DMG 作成のみスキップすることも可能。

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

## 7. トラブルシューティング

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
cp build/ShogiBoardQ_ja_JP.qm build/ShogiBoardQ.app/Contents/MacOS/
cp build/ShogiBoardQ_en.qm build/ShogiBoardQ.app/Contents/MacOS/
```

### Universal Binary (Intel + Apple Silicon) の作成

両アーキテクチャに対応したバイナリを作成する場合：

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

cmake --build build --config Release
```

> **注意**: Qt 自体も Universal Binary 版がインストールされている必要がある。
