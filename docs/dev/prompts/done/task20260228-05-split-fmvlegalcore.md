# Task 05: fmvlegalcore.cpp 分割（テーマA: 巨大ファイル分割）

## 目的

`src/core/fmvlegalcore.cpp`（930行）を責務ごとに分割し、600行以下にする。

## 背景

- fmvlegalcore.cpp はFast Move Validator（高速合法手生成）のコアで930行
- 駒種別の移動生成・合法性チェック・王手判定が密結合
- テーマA（巨大ファイル分割）の第5優先対象

## 事前調査

### Step 1: 現状の責務分析

```bash
# メソッド一覧
rg -n "^.*FmvLegalCore::" src/core/fmvlegalcore.cpp | head -30

# ヘッダの構造
cat src/core/fmvlegalcore.h

# 関連ファイル
ls src/core/fmv*
rg "#include.*fmvlegalcore" src -l

# テスト
cat tests/tst_fmvlegalcore.cpp | head -30
```

### Step 2: 責務の分類

1. **駒移動生成**: 各駒種ごとの移動先リスト生成
2. **合法性検証**: 王手回避・自殺手判定
3. **盤面状態クエリ**: 効き筋判定・ピン判定

## 実装手順

### Step 3: 抽出先クラスの設計

調査結果に基づき、例えば:

- `src/core/fmvmovegenerator.h/.cpp` — 駒種別移動生成
- `src/core/fmvcheckdetector.h/.cpp` — 王手・効き判定

既存の `fmvbitboard81.h/.cpp`、`fmvposition.h/.cpp` との整合性を確認する。

### Step 4-8: （Task 01と同じ手順）

ヘッダ作成 → メソッド移動 → CMakeLists.txt更新 → ビルド・テスト → 構造KPI更新

## 完了条件

- [ ] `fmvlegalcore.cpp` が分割前の930行から20%以上減少（目標: 700行以下）
- [ ] 全テスト通過（特に `tst_fmvlegalcore`, `tst_fmv_perft`）
- [ ] 構造KPI例外値が最新化されている

## KPI変化目標

- 600行超ファイル数: 減少方向
- `fmvlegalcore.cpp`: 930 → 700以下

## 注意事項

- パフォーマンスクリティカルなコード。分割でオーバーヘッドを増やさない
- `tst_fmv_perft` のベンチマーク結果が大幅に劣化しないこと
- インライン化やヘッダ内定義が必要な場合がある
- ビットボード操作の正確性を維持する
