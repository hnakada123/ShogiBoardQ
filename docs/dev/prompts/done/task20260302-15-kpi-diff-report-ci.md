# Task 15: KPI差分レポートのCI追加（ISSUE-023 / P2）

## 概要

PR の基準ブランチとの KPI 差分を Step Summary に出力し、KPI 悪化をレビュー前に可視化する。

## 現状

- `tst_structural_kpi` は既に Step Summary に KPI テーブルを出力する
- ただし「前回値との差分」は表示されない
- KPI 悪化がレビュー前にわかりにくい

## 手順

### Step 1: KPI 抽出スクリプトの作成

1. `scripts/extract-kpi-json.sh` を作成する:
   ```bash
   #!/bin/bash
   # tst_structural_kpi のテスト出力から KPI 値を JSON で抽出
   cd build
   ctest -V -R tst_structural_kpi --output-on-failure 2>&1 | \
     grep -E "KPI:" | \
     # パース処理
     jq -n '{...}' > kpi.json
   ```
2. または、テストコード側で JSON 出力を追加する方法も検討

### Step 2: CI ワークフローの修正

1. `.github/workflows/ci.yml` に KPI 差分レポートステップを追加:
   ```yaml
   - name: KPI diff report
     if: github.event_name == 'pull_request'
     run: |
       # PR ブランチの KPI
       scripts/extract-kpi-json.sh > pr-kpi.json

       # ベースブランチの KPI（キャッシュまたは再ビルド）
       git stash
       git checkout ${{ github.base_ref }}
       cmake -B build-base -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
       cmake --build build-base
       cd build-base && ctest -V -R tst_structural_kpi 2>&1 > ../base-kpi-output.txt
       cd ..
       scripts/extract-kpi-json.sh build-base > base-kpi.json

       # 差分を Step Summary に出力
       python3 scripts/kpi-diff.py base-kpi.json pr-kpi.json >> $GITHUB_STEP_SUMMARY
   ```

### Step 3: 差分表示スクリプトの作成

1. `scripts/kpi-diff.py` を作成:
   - 2つの KPI JSON を比較
   - 悪化した指標を赤、改善を緑で表示
   - Markdown テーブル形式で出力

### Step 4: テスト

1. PR を作成して CI を実行する
2. Step Summary に KPI 差分テーブルが表示されることを確認する

## 完了条件

- KPI 悪化がレビュー前に視覚的にわかる
- PR の Step Summary に差分テーブルが表示される

## 注意事項

- ベースブランチの再ビルドは時間がかかるため、キャッシュ活用を検討する
- KPI の項目が増減した場合にもスクリプトが壊れないようにする
- ISSUE-015 完了後に着手する
