# Task 03: game 層 UI依存ポート化（前半）（ISSUE-003 / P0）

## 概要

`GameStateController`、`MatchUndoHandler`、`GameStartCoordinator` が `views/widgets/dialogs` 具体型へ依存している可能性がある。UI操作ポート（interface/callback）を導入して依存を切る。

## 現状

調査時点では以下のファイルの `.cpp` に直接の `views/widgets/dialogs` include は確認されなかった。ただし:
- ヘッダファイル側の前方宣言やポインタ保持
- 他のヘッダ経由の間接依存
- `tst_layer_dependencies` ルール: `src/game` → `dialogs/`, `widgets/`, `views/` は禁止

対象ファイル:
- `src/game/gamestatecontroller.cpp` / `.h`
- `src/game/matchundohandler.cpp` / `.h`
- `src/game/gamestartcoordinator.cpp` / `.h`

## 手順

### Step 1: 現在の違反箇所を正確に特定

1. レイヤ依存テストを実行して `src/game` の全違反を一覧化:
   ```bash
   ctest --test-dir build -R tst_layer_dependencies -V --output-on-failure 2>&1 | grep "game"
   ```
2. `.h` ファイルの前方宣言・include を確認:
   ```bash
   grep -n "views\|widgets\|dialogs\|ShogiView\|StartGameDialog" src/game/gamestatecontroller.h src/game/matchundohandler.h src/game/gamestartcoordinator.h
   ```
3. 間接依存を確認（include 先のヘッダが禁止層を参照していないか）

### Step 2: UI操作ポートの設計

1. game 層が必要とする UI操作を洗い出す（例: 盤面更新、プロモーション確認、メッセージ表示）
2. Hooks パターンで対応:
   ```cpp
   struct Hooks {
       std::function<void(...)> updateBoardDisplay;
       std::function<void(...)> showMessage;
       // ...
   };
   ```
3. 既に `Hooks` 構造体が存在するクラスはそこに追加する

### Step 3: 依存の除去

1. 対象ファイルから `views/widgets/dialogs` 関連の include / 前方宣言を削除
2. 具体型の参照をポート（Hooks のコールバック）経由に置き換え
3. 呼び出し元（`app/` 層）でコールバックを注入

### Step 4: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_layer_dependencies|tst_game_end_handler|tst_game_start_flow|tst_matchcoordinator" --output-on-failure
   ```

## 完了条件

- 対象3ファイルで `views/widgets/dialogs` include が 0
- 対局開始/終局系テストが通る

## 注意事項

- Step 1 で違反が見つからない場合、ISSUE-003 はスキップして ISSUE-004 に進む
- 既存の Hooks/Deps パターンに従うこと
- ISSUE-002 完了後に着手する（依存関係）
