# Task 01: analysisflowcontroller.cpp 分割（テーマA: 巨大ファイル分割）

## 目的

`src/analysis/analysisflowcontroller.cpp`（1133行）を責務ごとに分割し、1000行以下にする。

## 背景

- analysisflowcontroller.cpp は現在1133行で、プロジェクト最大のファイル
- 解析フローの入力受理・状態遷移・表示反映が混在している
- テーマA（巨大ファイル分割）の最優先対象

## 事前調査

### Step 1: 現状のメソッド一覧と責務分類

```bash
# public/private メソッド一覧
rg -n "^void |^bool |^int |^QString |^QList|^std::" src/analysis/analysisflowcontroller.cpp | head -50

# ヘッダーのメソッド宣言
cat src/analysis/analysisflowcontroller.h

# このファイルを使っている箇所
rg "AnalysisFlowController" src --type cpp --type-add 'header:*.h' --type header -l

# シグナル/スロット接続
rg "analysisFlowController" src --type cpp -C 2
```

### Step 2: 責務の3分類設計

ファイルを読み、メソッドを以下の3カテゴリに分類する:

1. **解析セッション管理**: 解析の開始/停止/状態遷移
2. **解析結果処理**: エンジン出力の受信・パース・格納
3. **解析UI反映**: 解析結果のUI更新・表示制御

分類結果をコード内コメントやメモとして記録する。

## 実装手順

### Step 3: 抽出先クラスの設計

責務分析の結果に基づき、以下のようなクラスを新規作成する（名前は調査結果で調整）:

- `src/analysis/analysisresulthandler.h/.cpp` — 解析結果の受信・処理
- `src/analysis/analysissessionmanager.h/.cpp` — セッションライフサイクル管理

### Step 4: ヘッダファイルの作成

抽出先クラスのヘッダを先に定義する。依存注入は `Deps` struct パターンまたは明示setter。

### Step 5: メソッド移動

1. 対象メソッドを新クラスに移動
2. 元ファイルには転送実装（委譲）を配置
3. シグナル/スロット接続を維持

### Step 6: CMakeLists.txt 更新

```bash
# ソースリスト更新
scripts/update-sources.sh
```

新規ファイルを `CMakeLists.txt` のソースリストに追加する。

### Step 7: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
QT_QPA_PLATFORM=offscreen build/tests/tst_layer_dependencies
```

### Step 8: 構造KPI更新

`tests/tst_structural_kpi.cpp` の例外上限を実測値に合わせて更新する。
`analysisflowcontroller.cpp` の行数が減少した場合は上限値を下げる。

## 完了条件

- [ ] `analysisflowcontroller.cpp` が分割前の1133行から20%以上減少（目標: 800行以下）
- [ ] 新規クラスが適切な責務で分離されている
- [ ] 全テスト通過（41テスト）
- [ ] 構造KPI例外値が最新化されている
- [ ] `tst_layer_dependencies` が通過している

## KPI変化目標

- 1000行超ファイル数: 3 → 2
- `analysisflowcontroller.cpp`: 1133 → 800以下

## 注意事項

- `connect()` でラムダを使用しない（CLAUDE.md準拠）
- シグナル/スロットの完全修飾型を維持する
- 既存の `tst_analysisflow.cpp` テストが通過することを確認する
