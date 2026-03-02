# Task 11: 翻訳ファイルロードパス修正（P2 / §8.1）

## 概要

`main.cpp` で QRC パス `:/i18n/ShogiBoardQ_ja_JP` を最初に試行するが、`.qm` ファイルは QRC に埋め込まれていないため常に失敗する。フォールバックパスのみに修正する。

## 現状

`src/app/main.cpp` で翻訳ファイルのロード:
1. `:/i18n/ShogiBoardQ_ja_JP`（QRC パス）→ 常に失敗
2. `applicationDirPath()/ShogiBoardQ_ja_JP.qm` → 実際にロードされる

## 手順

### Step 1: 翻訳ロードの現状確認

1. `src/app/main.cpp` の翻訳ロードコードを読む
2. `.qm` ファイルが QRC (`shogiboardq.qrc`) に含まれているか確認する
3. 実際の `.qm` ファイルの配置場所を確認する

### Step 2: 修正方針の決定

**方針A（推奨）: QRC パス試行を削除**
- 最初の QRC パス試行を削除し、`applicationDirPath()` からのロードのみにする
- シンプルで確実

**方針B: .qm を QRC に埋め込む**
- `shogiboardq.qrc` に `.qm` ファイルを追加する
- アプリケーションバイナリに翻訳を埋め込めるが、バイナリサイズが増加する
- 翻訳更新時にリビルドが必要

### Step 3: 実装

1. 選択した方針に基づいてコードを修正する
2. 方針Aの場合: QRC パス試行の行を削除
3. システムロケールの分岐（`QLocale::system().uiLanguages()` ループ）でも同様に QRC パスを削除

### Step 4: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. アプリを起動し、翻訳が正しくロードされることを確認

## 注意事項

- 翻訳ロードの失敗自体は起動をブロックしない（フォールバックがある）が、無駄な I/O とログノイズの原因になる
- `QLocale::system().uiLanguages()` ループ内の QRC パスも同様に修正すること
