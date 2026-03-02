# Task 18: `.clang-tidy` チェック拡充（P3 / §6.1+§6.2）

## 概要

`.clang-tidy` で未有効化の有用チェックを追加し、`WarningsAsErrors` を設定する。

## 現状

有効チェック: `clang-diagnostic-unused-*`, `misc-unused-*`, `clang-analyzer-deadcode.*`, `readability-redundant-*`, `modernize-*`, `performance-*`, `bugprone-*`（一部除外あり）

`WarningsAsErrors: ''`（空 = 全て警告止まり）

## 追加候補チェック

| チェック | 検出対象 | リスク |
|---|---|---|
| `cppcoreguidelines-slicing` | オブジェクトスライシング | 低 |
| `cppcoreguidelines-prefer-member-initializer` | メンバ初期化子の推奨 | 低 |
| `clang-analyzer-cplusplus.*` | use-after-free, double-free | 低 |
| `clang-analyzer-security.*` | セキュリティ関連 | 低 |
| `readability-container-size-empty` | `.size() == 0` → `.empty()` | 低 |
| `misc-const-correctness` | const 付与漏れ | 中（大量に出る可能性） |
| `readability-magic-numbers` | マジックナンバー | 高（大量に出る可能性） |

## 手順

### Step 1: 段階的にチェックを追加

1. `.clang-tidy` のチェックリストにまず低リスクのチェックを追加:
   ```yaml
   Checks: >
     ...,
     cppcoreguidelines-slicing,
     cppcoreguidelines-prefer-member-initializer,
     clang-analyzer-cplusplus.*,
     clang-analyzer-security.*,
     readability-container-size-empty
   ```
2. ビルドして警告数を確認する
3. 警告が0件または少数であれば追加を確定する

### Step 2: 中リスクチェックの評価

1. `misc-const-correctness` を追加して警告数を確認する
2. 大量に出る場合はスキップまたは段階的に対応する
3. `readability-magic-numbers` は Task 15 の後に検討する

### Step 3: WarningsAsErrors の設定

1. `.clang-tidy` の `WarningsAsErrors` を設定する:
   ```yaml
   WarningsAsErrors: '*'
   ```
2. ただし、まだ大量に警告が出るチェックがある場合は、対応済みのチェックのみ列挙する:
   ```yaml
   WarningsAsErrors: 'clang-diagnostic-unused-*,misc-unused-*,...'
   ```

### Step 4: ビルド・テスト

1. `cmake -B build -S . -DENABLE_CLANG_TIDY=ON && cmake --build build` で確認
2. テストを実行して全パスを確認
3. CI の `static-analysis` ジョブで新しいチェックが動作することを確認する

## 注意事項

- 一度に大量のチェックを追加すると修正が追いつかない。段階的に追加すること
- Qt のマクロに起因する false positive が出るチェックは除外リストに追加する
- `readability-magic-numbers` は UI 定数が多いプロジェクトでは大量に出るため、Task 15 の完了後に検討する
