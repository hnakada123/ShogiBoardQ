# Task 04: レイヤ違反修正 game/ → dialogs/（P1 / §3.1）

## 概要

`src/game/gamestartorchestrator.cpp` と `src/game/matchcoordinator.h` が `src/dialogs/startgamedialog.h` に依存しており、レイヤ違反している。データ構造体を `game/` 層に作成して依存を切る。

## 現状

- `src/game/gamestartorchestrator.cpp:6`: `#include "startgamedialog.h"`
- `src/game/matchcoordinator.h:30`: `class StartGameDialog;`（前方宣言）
- `GameStartOrchestrator` / `MatchCoordinator` が `const StartGameDialog*` を引数に取る
- `tst_layer_dependencies` はフラットインクルードパスのため検出できていない

## 手順

### Step 1: StartGameDialog の使用箇所を調査

1. `GameStartOrchestrator` が `StartGameDialog` のどのメンバにアクセスしているか洗い出す
2. `MatchCoordinator` が `StartGameDialog` をどう使っているか調査する
3. 必要なデータ項目（対局設定: 先手/後手エンジン、持ち時間等）をリストアップする

### Step 2: GameStartOptions データ構造体の作成

1. `src/game/` 配下に `GameStartOptions`（または既存の `StartOptions`）構造体を定義する
   - `StartGameDialog` から取得していたデータを全て含める
2. 既に `MatchCoordinator::StartOptions` が存在する場合は、それを拡張または活用する

### Step 3: 依存の切り替え

1. `GameStartOrchestrator` のメソッドシグネチャを `const StartGameDialog*` から `const GameStartOptions&` に変更
2. `MatchCoordinator` の前方宣言 `class StartGameDialog;` を削除
3. `StartGameDialog` → `GameStartOptions` への変換コードを `app/` 層または `dialogs/` 層に配置する（呼び出し元で変換）
4. `gamestartorchestrator.cpp` の `#include "startgamedialog.h"` を削除

### Step 4: tst_layer_dependencies の検出ロジック改善

1. テストのインクルード検出ロジックを確認する
2. パス文字列マッチングに加え、`startgamedialog.h` のようなフラットインクルードも検出できるよう改善する
   - 方法: 検出対象ファイル名リストを `src/dialogs/` のファイル一覧から動的に生成する

### Step 5: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行（特に `tst_layer_dependencies` が新ロジックでパスすることを確認）

## 注意事項

- `buildStartOptions` が `static` メソッドである点に留意（MEMORYに記載あり）
- `MatchCoordinator::StartOptions` が既に存在するか確認し、二重定義を避ける
- 変換コードの配置場所は、`app/` 層のコンポジションルート周辺が適切
