# Task 03: evaluationchartwidget.cpp 分割（テーマA: 巨大ファイル分割）

## 目的

`src/widgets/evaluationchartwidget.cpp`（1018行）を責務ごとに分割し、1000行以下にする。

## 背景

- evaluationchartwidget.cpp は評価グラフ描画ウィジェットで1018行
- チャートのデータ管理・描画制御・ユーザーインタラクションが混在
- テーマA（巨大ファイル分割）の第3優先対象

## 事前調査

### Step 1: 現状の責務分析

```bash
# メソッド一覧
rg -n "^void EvaluationChartWidget::|^bool EvaluationChartWidget::|^int EvaluationChartWidget::" src/widgets/evaluationchartwidget.cpp

# ヘッダの構造
cat src/widgets/evaluationchartwidget.h

# 依存関係
rg "#include.*evaluationchartwidget" src --type cpp -l
rg "EvaluationChartWidget" src --type-add 'header:*.h' --type header -l

# 関連Presenter
ls src/ui/presenters/evalgraph*
```

### Step 2: 責務の3分類

1. **チャートデータ管理**: 評価値シリーズの追加/更新/クリア
2. **チャート描画設定**: 軸設定、カラー、テーマ、レンジ管理
3. **ユーザーインタラクション**: クリック、ホバー、ツールチップ、カーソル移動

## 実装手順

### Step 3: 抽出先クラスの設計

- `src/widgets/evaluationchartdatamanager.h/.cpp` — チャートデータの管理ロジック
- `src/widgets/evaluationchartconfigurator.h/.cpp` — チャート見た目の設定

### Step 4-8: （Task 01と同じ手順）

ヘッダ作成 → メソッド移動 → CMakeLists.txt更新 → ビルド・テスト → 構造KPI更新

## 完了条件

- [ ] `evaluationchartwidget.cpp` が分割前の1018行から20%以上減少（目標: 800行以下）
- [ ] 全テスト通過
- [ ] 構造KPI例外値が最新化されている

## KPI変化目標

- 1000行超ファイル数: 1 → 0（Task 01,02完了後の値から）
- `evaluationchartwidget.cpp`: 1018 → 800以下

## 注意事項

- Qt Charts APIの使い方が複雑なので、チャートオブジェクトのライフサイクルに注意
- ウィジェットの描画性能に影響を与えないこと
- `evalgraphpresenter` との連携を維持する
