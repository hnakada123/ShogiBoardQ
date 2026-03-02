# Task 05: layer依存テストをCI可視化（ISSUE-005 / P0）

## 概要

`tst_layer_dependencies` を CI で単独実行ステップとして追加し、違反発生時に原因が即座にわかるようにする。

## 現状

- `tst_layer_dependencies` は全テストの一部として `ctest` で実行される
- 失敗時に違反一覧がCI画面で直接見えない
- `tst_structural_kpi` は既に Step Summary に出力される仕組みがある（参考にできる）

## 手順

### Step 1: CI ワークフローの修正

1. `.github/workflows/ci.yml` の `build-and-test` ジョブに専用ステップを追加:
   ```yaml
   - name: Layer dependency check
     if: always()
     run: |
       cd build
       ctest -V -R tst_layer_dependencies --output-on-failure 2>&1 | tee layer-deps-output.txt

       # Step Summaryに結果を出力
       echo "## Layer Dependency Check" >> $GITHUB_STEP_SUMMARY
       if grep -q "FAILED" layer-deps-output.txt; then
         echo "### :x: Violations found" >> $GITHUB_STEP_SUMMARY
         echo '```' >> $GITHUB_STEP_SUMMARY
         grep -A2 "violation" layer-deps-output.txt >> $GITHUB_STEP_SUMMARY || true
         echo '```' >> $GITHUB_STEP_SUMMARY
       else
         echo ":white_check_mark: No layer dependency violations" >> $GITHUB_STEP_SUMMARY
       fi
   ```

### Step 2: テスト出力の改善（必要に応じて）

1. `tst_layer_dependencies.cpp` の出力フォーマットを確認する
2. CI で grep しやすい形式になっているか確認する
3. 必要に応じて `qWarning()` や `QFAIL()` のメッセージを改善する

### Step 3: PR で確認

1. PR を作成して CI を実行する
2. Step Summary にレイヤ依存チェック結果が表示されることを確認する
3. 違反がある場合（ISSUE-001〜004 が未完了の場合）、違反一覧が読みやすく表示されることを確認する

## 完了条件

- PR で layer 違反発生時に原因が CI 画面で可視化される
- Step Summary に結果が表示される

## 注意事項

- `if: always()` を使い、ビルドが成功した場合は常に実行する
- ISSUE-001〜004 が完了していない段階では違反が表示されるが、それは期待動作
- 既存の `Structural KPI Summary` ステップのパターンを参考にする
- ISSUE-004 完了後に着手する（ただし並行可能）
