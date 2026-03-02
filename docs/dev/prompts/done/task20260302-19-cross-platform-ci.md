# Task 19: Windows / macOS CI ジョブ追加（P1 / §5.1）

## 概要

全 CI ジョブが `ubuntu-latest` のみで実行されている。macOS と Windows のビルドジョブを追加し、クロスプラットフォームの互換性を検証する。

## 現状

- 既存 CI: `ubuntu-latest` のみ（build-and-test, build-debug, static-analysis, sanitize, coverage, translation-check）
- `scripts/build-macos.sh`, `scripts/build-windows.ps1` が存在するが CI に統合されていない
- プラットフォーム固有コード: `WIN32_EXECUTABLE`, macOS バンドル, `app.rc`

## 手順

### Step 1: 既存ビルドスクリプトの調査

1. `scripts/build-macos.sh` を読み、ビルド手順を確認する
2. `scripts/build-windows.ps1` を読み、ビルド手順を確認する
3. プラットフォーム固有の依存関係を確認する

### Step 2: macOS CI ジョブの追加

1. `.github/workflows/ci.yml` に `build-macos` ジョブを追加:
   ```yaml
   build-macos:
     runs-on: macos-latest
     steps:
       - uses: actions/checkout@v4
       - name: Install Qt
         uses: jurplel/install-qt-action@v4
         with:
           version: '6.7.*'
           modules: 'qtcharts'
       - name: Configure
         run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
       - name: Build
         run: cmake --build build
       - name: Test
         run: ctest --test-dir build --output-on-failure
   ```

### Step 3: Windows CI ジョブの追加

1. `.github/workflows/ci.yml` に `build-windows` ジョブを追加:
   ```yaml
   build-windows:
     runs-on: windows-latest
     steps:
       - uses: actions/checkout@v4
       - name: Install Qt
         uses: jurplel/install-qt-action@v4
         with:
           version: '6.7.*'
           modules: 'qtcharts'
       - name: Configure
         run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
       - name: Build
         run: cmake --build build --config Release
       - name: Test
         run: ctest --test-dir build --config Release --output-on-failure
   ```

### Step 4: テスト実行

1. PR を作成して CI を実行する
2. ビルドエラーがあれば修正する（プラットフォーム固有のヘッダ不足等）
3. テストが全パスすることを確認する

## 注意事項

- macOS / Windows CI は初回実行時にビルドエラーが出る可能性が高い。段階的に修正する
- 最初はビルド成功のみを目標とし、テストは段階的に追加する
- CI の実行時間を考慮し、`push` イベントでは Linux のみ、`pull_request` で全プラットフォームの実行も検討する
- GitHub Actions の macOS / Windows ランナーは Linux より時間課金が高いため注意
