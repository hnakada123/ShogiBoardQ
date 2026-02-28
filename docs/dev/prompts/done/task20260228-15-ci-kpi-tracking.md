# Task 15: KPI推移トラッキング（テーマE: CI品質ゲート拡張）

## 目的

`tst_structural_kpi` の結果をCIアーティファクトとして保存し、KPI推移を追跡可能にする。

## 背景

- 構造KPIは `tst_structural_kpi` で検証されているが、推移の可視化がない
- 改善の効果を定量的に示すために、計測値の時系列記録が必要
- テーマE（CI品質ゲート拡張）の補助作業

## 対象ファイル

- `.github/workflows/ci.yml`
- `tests/tst_structural_kpi.cpp`（既存、変更は最小限）

## 実装手順

### Step 1: KPI結果をJSON形式で出力

`tst_structural_kpi.cpp` の出力を構造化データとしても取得できるように:

```bash
# 現在の出力形式を確認
cat tests/tst_structural_kpi.cpp | head -100
```

必要に応じて、テスト実行後にログからKPI値を抽出するスクリプトを作成。

### Step 2: CIでアーティファクト保存

`.github/workflows/ci.yml` の `build-and-test` ジョブに追加:

```yaml
      - name: Save KPI Results
        if: always()
        run: |
          cd build
          # KPI結果をJSON形式で保存
          echo '{}' > kpi-results.json
          # tst_structural_kpiの出力からKPI値を抽出してJSONに格納
          # (具体的な抽出コマンドは出力形式に合わせて調整)

      - name: Upload KPI Artifact
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: structural-kpi-${{ github.sha }}
          path: build/kpi-results.json
          retention-days: 90
```

### Step 3: GitHub Step Summaryの拡張

既存のStep Summaryに前回比較を追加（可能であれば）:

```yaml
      - name: KPI Summary with Comparison
        if: always()
        run: |
          echo "## Structural KPI Trends" >> "$GITHUB_STEP_SUMMARY"
          echo "| Metric | Current |" >> "$GITHUB_STEP_SUMMARY"
          echo "|--------|---------|" >> "$GITHUB_STEP_SUMMARY"
          # KPI値を表形式で出力
```

### Step 4: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] KPI結果がCIアーティファクトとして保存される
- [ ] Step SummaryにKPI値が表示される
- [ ] 既存CIに影響がない

## KPI変化目標

- KPI推移グラフの自動生成: 基盤導入（アーティファクト保存）

## 注意事項

- アーティファクトの保存期間は90日程度に設定（ストレージ消費抑制）
- テスト出力のパース方法はtst_structural_kpiの出力形式に依存する
- 前回比較は将来的な拡張として、まずアーティファクト保存を優先する
