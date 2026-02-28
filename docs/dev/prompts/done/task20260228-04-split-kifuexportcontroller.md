# Task 04: kifuexportcontroller.cpp 分割（テーマA: 巨大ファイル分割）

## 目的

`src/kifu/kifuexportcontroller.cpp`（933行）を責務ごとに分割し、600行以下にする。

## 背景

- kifuexportcontroller.cpp は棋譜エクスポート処理で933行
- 複数フォーマット（KIF/KI2/CSA/SFEN/JKF/USEN）への変換ディスパッチ・ファイルI/O・クリップボード操作が混在
- テーマA（巨大ファイル分割）の第4優先対象

## 事前調査

### Step 1: 現状の責務分析

```bash
# メソッド一覧
rg -n "^void KifuExportController::|^bool KifuExportController::|^QString KifuExportController::" src/kifu/kifuexportcontroller.cpp

# ヘッダの構造
cat src/kifu/kifuexportcontroller.h

# エクスポートフォーマット
rg "export|Export" src/kifu/kifuexportcontroller.cpp -n | head -30

# 使用箇所
rg "KifuExportController" src -l
```

### Step 2: 責務の3分類

1. **フォーマット選択・ディスパッチ**: エクスポート先の形式選択
2. **ファイルI/O**: ファイル保存・クリップボードコピー
3. **変換委譲**: 各フォーマットconverterへの変換呼び出し

## 実装手順

### Step 3: 抽出先クラスの設計

調査結果に基づき、例えば:

- `src/kifu/kifuexportformatdispatcher.h/.cpp` — フォーマット判定・変換ディスパッチ
- 既存 `kifuioservice.h/.cpp` への統合も検討

### Step 4-8: （Task 01と同じ手順）

ヘッダ作成 → メソッド移動 → CMakeLists.txt更新 → ビルド・テスト → 構造KPI更新

## 完了条件

- [ ] `kifuexportcontroller.cpp` が分割前の933行から20%以上減少（目標: 700行以下）
- [ ] 全テスト通過
- [ ] 構造KPI例外値が最新化されている

## KPI変化目標

- 600行超ファイル数: 減少方向
- `kifuexportcontroller.cpp`: 933 → 700以下

## 注意事項

- 各フォーマットのconverterクラス（`src/kifu/formats/`）との関係を明確に保つ
- ファイルI/Oのエラーハンドリングを維持する
