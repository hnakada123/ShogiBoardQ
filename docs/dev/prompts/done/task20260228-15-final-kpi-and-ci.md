# Task 15: 最終KPI反映・CI sanitizer 強化

## 概要

全改善タスク完了後の最終KPI計測を行い、閾値を引き下げる。
CI に ASan+UBSan ジョブを追加し、ランタイム品質を強化する。

## 前提条件

- Task 01〜14 のうち、主要タスク（01, 02, 03, 07, 08）が完了していること

## 現状

- CI 構成（`.github/workflows/ci.yml`）:
  - 6ジョブ: build-and-test, build-debug, static-analysis, sanitize, coverage, translation-check
  - sanitize ジョブは既に存在するが、内容の確認が必要
- KPI テスト:
  - 600行超ファイル数: 29件（目標: 20件以下）
  - 800行超ファイル数: 6件（目標: 3件以下）
  - sr_ensure_methods: 35（目標: 28以下）
  - 例外リスト TODO: 24件（目標: 15件以下）

## 実施内容

### Step 1: KPI実測と閾値更新

```bash
# 実測
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | sed '$d' | awk '$1>600{a++} $1>800{b++} $1>1000{c++} END{print "600超:" a, "800超:" b, "1000超:" c}'

# KPIテスト実行
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi -o -,txt
```

`tst_structural_kpi.cpp` の全閾値を新実測値に更新:
- `maxEnsureMethods` — 新しい ensure* 数
- `kMaxLargeFiles` — 新しい 1000行超ファイル数
- `knownLargeFiles()` — 新しい例外リストと上限値

### Step 2: CI sanitizer ジョブの確認・強化

1. `.github/workflows/ci.yml` の `sanitize` ジョブの内容を確認
2. 不足があれば以下を追加:
   - ASan（AddressSanitizer）: メモリ破壊、use-after-free 検知
   - UBSan（UndefinedBehaviorSanitizer）: 未定義動作検知
3. CMake 設定例:

```cmake
# ASan+UBSan ビルド
cmake -B build-sanitize -S . \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize
ctest --test-dir build-sanitize
```

### Step 3: 長時間シナリオテストの追加（オプション）

CI に nightly またはラベル起動の長時間テストジョブを追加:
- 連続対局シミュレーション
- 連続棋譜ロード
- エンジン連続起動/停止

### Step 4: KPI アーティファクト出力

CI で KPI 値を JSON として出力し、アーティファクトに保存する設定を確認:
- `kpi-results.json` が既に出力されている場合はフォーマット確認
- 時系列で比較可能なフォーマットであること

### Step 5: 翻訳 THRESHOLD 更新

```bash
# 現在の未翻訳数を確認
grep -c 'type="unfinished"' resources/translations/*.ts
```

CI の `translation-check` ジョブの `THRESHOLD` を新しい未翻訳数に合わせて更新。

## 完了条件

- KPI テスト全件通過（更新された閾値で）
- 600行超ファイル数が 20件以下
- KPI TODO 件数が 15件以下
- CI sanitizer ジョブが正常に動作すること
- ビルド成功
- 全テスト通過

## 注意事項

- sanitizer ビルドはデバッグビルドで行う（最適化を無効化）
- ASan は macOS/Linux で利用可能。Windows の場合は別のツール（Dr. Memory 等）を検討
- CI 実行時間が過度に長くならないよう、sanitizer ジョブは nightly またはラベル起動にすることを検討
- KPI 閾値は「実測値 + 最小マージン」に設定し、余裕を持たせすぎない
