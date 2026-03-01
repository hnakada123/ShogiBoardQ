# Task 15: KPI閾値最終更新とCI運用強化

## 概要

全タスク完了後のKPI閾値を更新し、CI の KPI差分自動可視化を導入する。

## 前提条件

- Task 01〜14 の全て（または大部分）が完了していること

## 現状

- `tst_structural_kpi.cpp` の `knownLargeFiles()`: 15件の例外ファイル
- KPIテスト: 9テストスロット
- CI: build, test, sanitize, translation-check, structural-kpi の各ジョブ

## 実施内容

### Step 1: KPI閾値の全面更新

1. 分割後の全ファイルの行数を実測
2. `tst_structural_kpi.cpp` の `knownLargeFiles()` を更新:
   - 分割で600行以下になったファイルを例外リストから削除
   - 残存する600行超ファイルの上限値を実測値に更新
3. 各KPI閾値を更新:
   - `mw_friend_classes`: 実測値に更新
   - `sr_ensure_methods`: 11を維持
   - `delete_count`: 実測値に更新
   - `lambda_connect_count`: 実測値に更新
   - `files_over_1000`: 0を目標

### Step 2: 追加テストの実施

1. 全テストを実行し、pass を確認
2. テスト数を計測（目標: +10以上、うち異常系 +6以上）
3. 長時間シナリオテストの追加（1本以上）:
   - `tst_lifecycle_scenario.cpp` を作成
   - 「初期化→対局開始→指し手→保存→リセット→再対局」のシーケンスを検証
   - ソースコード構造検証方式（実際のウィンドウ生成なし）

### Step 3: CI KPI差分可視化

1. `ci.yml` に KPI差分表示ステップを追加:
   ```yaml
   - name: KPI Summary
     if: always()
     run: |
       echo "## KPI Results" >> $GITHUB_STEP_SUMMARY
       echo "| Metric | Value | Limit |" >> $GITHUB_STEP_SUMMARY
       echo "|--------|------:|------:|" >> $GITHUB_STEP_SUMMARY
       # 各KPI値を表示
       files_over_600=$(find src -name '*.cpp' -exec wc -l {} + | awk '$1 > 600' | wc -l)
       echo "| Files > 600 lines | $files_over_600 | 9 |" >> $GITHUB_STEP_SUMMARY
       # ... 他のKPI
   ```
2. KPI結果を `kpi-results.json` として artifact 保存（任意）

### Step 4: Sanitizer 定期実行の確認

1. `ci.yml` の `sanitize` ジョブが `schedule` イベントで定期実行されることを確認
2. sanitizer 結果の artifact 保存が設定されていることを確認

### Step 5: 最終確認

1. 全テストを実行:
   ```bash
   cmake -B build -S . && cmake --build build && ctest --test-dir build
   ```
2. 600行超ファイル数を計測:
   ```bash
   find src -name '*.cpp' -exec wc -l {} + | sort -rn | awk '$1 > 600'
   ```
3. 目標達成の確認:
   - 600行超: 15 → 9以下
   - 800行超: 2 → 0
   - テスト数: 55 → 65以上（+10）

## 完了条件

- `tst_structural_kpi.cpp` の全閾値が実測値に更新されている
- CI の KPI Summary が GitHub Step Summary に表示される
- 全テスト通過
- 600行超ファイル: 9以下
- 800行超ファイル: 0
- テスト数: 65以上

## 注意事項

- このタスクは全体のまとめであり、前提タスクの完了状況によって内容を調整する
- KPI閾値は「現状値 + 少しの余裕」ではなく「現状値そのもの」を設定する（上限ギリギリで運用）
- 長時間シナリオテストは GUI を起動しないソースコード構造検証方式で実装する
