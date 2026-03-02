# Task 16: Sanitizer 軽量PRジョブ導入（ISSUE-024 / P2）

## 概要

現在の Sanitizer ジョブは週次/手動のみ。PR で実行可能な軽量バージョンを追加し、メモリ安全性の問題を早期検出する。

## 現状

`.github/workflows/ci.yml` の `sanitize` ジョブ:
- `schedule`（cron）または `workflow_dispatch` または `'sanitize'` ラベルで起動
- ASAN + UBSan を使用
- 全テストを実行

## 手順

### Step 1: 軽量ジョブの設計

1. 実行トリガー: `pull_request` イベントで常時実行
2. 実行時間制限: 10分以内を目標
3. テスト対象の絞り込み:
   - コア機能テスト（`tst_shogiboard`, `tst_movevalidator`, `tst_shogimove`）
   - プロトコルテスト（`tst_usiprotocolhandler`, `tst_csaprotocol`）
   - 変換テスト（`tst_kifconverter` 等）
   - wiring テスト（スタブベースで高速）

### Step 2: CI ジョブの追加

1. `.github/workflows/ci.yml` に `sanitize-pr` ジョブを追加:
   ```yaml
   sanitize-pr:
     runs-on: ubuntu-latest
     if: github.event_name == 'pull_request'
     steps:
       - uses: actions/checkout@v4
       - name: Install Qt
         uses: jurplel/install-qt-action@v4
         with:
           version: '6.7.*'
           modules: 'qtcharts'
       - name: Configure with sanitizers
         run: |
           cmake -B build -S . \
             -DCMAKE_BUILD_TYPE=Debug \
             -DBUILD_TESTING=ON \
             -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
       - name: Build
         run: cmake --build build -j$(nproc)
       - name: Run core tests with sanitizers
         run: |
           cd build
           ctest -R "tst_shogiboard|tst_movevalidator|tst_shogimove|tst_kifconverter|tst_csaconverter" \
             --output-on-failure --timeout 120
         env:
           ASAN_OPTIONS: "detect_leaks=1:halt_on_error=1"
           UBSAN_OPTIONS: "print_stacktrace=1:halt_on_error=1"
   ```

### Step 3: 実行時間の監視

1. 初回実行後、ジョブの実行時間を確認する
2. 10分を超える場合はテスト対象を絞る
3. 失敗率が高い場合は原因を調査・修正する

### Step 4: テスト

1. PR を作成して CI を実行する
2. Sanitizer ジョブが成功することを確認する
3. 既知の問題がないか確認する

## 完了条件

- PR 段階でサニタイザの基本検知が有効化される
- ジョブが安定して成功する

## 注意事項

- ASAN は Debug ビルドでのみ有効。Release ビルドでは効果が薄い
- Sanitizer ビルドは通常ビルドより2-3倍遅い
- Qt 内部の既知の false positive がある場合は suppressions ファイルで抑制する
- ISSUE-023 完了後に着手する
