# Task 11: KPI文書と実装値の同期運用を固定（ISSUE-015 / P1）

## 概要

`docs/dev/kpi-thresholds.md` と `tests/tst_structural_kpi.cpp` の値が乖離する問題を解決し、同期運用ルールを確立する。

## 現状

- `docs/dev/kpi-thresholds.md` に KPI 閾値一覧が記載されている
- `tests/tst_structural_kpi.cpp` に実装値（`kMaxFilesOver550 = 13` 等）がハードコードされている
- 両者の不一致が生じやすい

## 手順

### Step 1: 現在の乖離を特定

1. `docs/dev/kpi-thresholds.md` の全閾値を読み出す
2. `tests/tst_structural_kpi.cpp` の全閾値を読み出す
3. 差分を一覧化する

### Step 2: 値の同期

1. 乖離している値を統一する（テスト実装値を正とする）
2. `kpi-thresholds.md` を最新の実装値で更新する

### Step 3: 同期運用ルールの明文化

1. `kpi-thresholds.md` に運用ルールセクションを追加:
   ```markdown
   ## 運用ルール

   ### 閾値変更時の手順
   1. `tests/tst_structural_kpi.cpp` の閾値を変更する
   2. 本文書の対応する値を同時に更新する
   3. PR 説明に変更理由を記載する

   ### 値の正とする場所
   - 実装値（テストコード）を Single Source of Truth とする
   - 本文書は参照用ドキュメントとして同期を維持する
   ```

### Step 4: PR テンプレートの追加（任意）

1. `.github/PULL_REQUEST_TEMPLATE.md` に KPI 変更チェックリストを追加する（既存テンプレートがある場合は追記）:
   ```markdown
   ## KPI 変更
   - [ ] `tst_structural_kpi.cpp` の閾値変更あり → `kpi-thresholds.md` も更新済み
   ```

### Step 5: テスト

1. テスト実行:
   ```bash
   ctest --test-dir build -R tst_structural_kpi --output-on-failure
   ```

## 完了条件

- テスト値と文書値の不一致が解消される
- 運用ルールが開発フローに定着する

## 注意事項

- KPI 閾値はプロジェクトのフェーズに応じて変更される。「緩める」変更にも理由の記載を求める
- ISSUE-013 完了後に着手する（KPI 値が安定した段階で同期する）
