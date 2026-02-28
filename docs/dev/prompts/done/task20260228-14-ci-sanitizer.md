# Task 14: CI Sanitizerジョブ追加（テーマE: CI品質ゲート拡張）

## 目的

CIに ASan/UBSan ジョブを追加し、ランタイム不具合（未定義動作、メモリ境界違反）を自動検出する。

## 背景

- 現在のCIは Build/Test/Static Analysis/Translation の4ジョブ
- ランタイム不具合を直接拾うゲートがない
- テーマE（CI品質ゲート拡張）の主作業
- `workflow_dispatch` または PRラベル起動で実行負荷を抑える

## 対象ファイル

- `.github/workflows/ci.yml`

## 事前調査

### Step 1: 現在のCI構成確認

```bash
cat .github/workflows/ci.yml
```

現在のジョブ:
1. `build-and-test` (Release)
2. `build-debug` (Debug)
3. `static-analysis` (clang-tidy)
4. `translation-check`

### Step 2: Qt6 + Sanitizerの互換性確認

- ASan + Qt6 の既知の問題を確認
- UBSan + Qt6 の既知の問題を確認
- 必要な環境変数（`ASAN_OPTIONS` 等）を特定

## 実装手順

### Step 3: sanitize ジョブの追加

`.github/workflows/ci.yml` に以下のジョブを追加:

```yaml
  sanitize:
    runs-on: ubuntu-latest
    # 手動トリガーまたはラベル付きPRでのみ実行
    if: >
      github.event_name == 'workflow_dispatch' ||
      contains(github.event.pull_request.labels.*.name, 'sanitize')
    steps:
      - uses: actions/checkout@v4

      - name: Install Qt6
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.7.*'
          modules: 'qtcharts'

      - name: Configure with Sanitizers
        run: |
          cmake -B build -S . \
            -DBUILD_TESTING=ON \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
            -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"

      - name: Build
        run: cmake --build build --parallel 2

      - name: Test with Sanitizers
        env:
          QT_QPA_PLATFORM: offscreen
          ASAN_OPTIONS: "detect_leaks=0:halt_on_error=1"
          UBSAN_OPTIONS: "halt_on_error=1:print_stacktrace=1"
        run: cd build && ctest --output-on-failure
```

### Step 4: workflow_dispatch トリガーの追加

`on:` セクションに `workflow_dispatch` を追加:

```yaml
on:
  push:
    branches: [main]
  pull_request:
  workflow_dispatch:  # 追加
```

### Step 5: ローカルでの動作確認

```bash
# ローカルでSanitizer付きビルドを確認
cmake -B build-san -S . \
  -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"

cmake --build build-san --parallel $(nproc)

ASAN_OPTIONS="detect_leaks=0:halt_on_error=1" \
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
QT_QPA_PLATFORM=offscreen \
ctest --test-dir build-san --output-on-failure
```

### Step 6: 通常CIの確認

既存の4ジョブに影響がないことを確認。

## 完了条件

- [ ] `sanitize` ジョブが `.github/workflows/ci.yml` に追加されている
- [ ] `workflow_dispatch` または PRラベルで起動できる
- [ ] 既存のCIジョブに影響がない
- [ ] ローカルでSanitizer付きビルド・テストが通過する（またはQt既知問題を除外）

## KPI変化目標

- Sanitizerジョブ: 0 → 1

## 注意事項

- Qt6自体のASan検出を抑制する必要がある場合、`ASAN_OPTIONS` で `detect_leaks=0` を設定
- UBSan でQt内部の警告が出る場合、suppressionファイルを検討
- CI実行時間が長くなるため、常時実行ではなく任意トリガーにする
- `detect_leaks=0` はLinux環境でQt6の誤検知を回避するため
