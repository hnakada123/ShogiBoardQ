# Task 04: game 層 UI依存ポート化（後半）（ISSUE-004 / P0）

## 概要

`prestartcleanuphandler`、`gamestartcoordinator_dialogflow`、`matchcoordinator`、`gamestartoptionsbuilder`、`promotionflow` 周辺に UI依存が残っている可能性がある。ISSUE-003 で導入した UIポートへ統一移行する。

## 現状

調査時点では以下のファイルの `.cpp` に直接の `views/widgets/dialogs` include は確認されなかった。ただし:
- ヘッダ側の前方宣言やポインタ保持による間接依存の可能性
- `matchcoordinator.h` に `class StartGameDialog;` の前方宣言がある（既知のレイヤ違反）

対象ファイル:
- `src/game/prestartcleanuphandler.cpp` / `.h`
- `src/game/gamestartcoordinator_dialogflow.cpp`
- `src/game/matchcoordinator.cpp` / `.h`
- `src/game/gamestartoptionsbuilder.cpp` / `.h`
- `src/game/promotionflow.cpp` / `.h`

## 手順

### Step 1: 現在の違反箇所を正確に特定

1. レイヤ依存テストを実行して `src/game` の全違反を一覧化:
   ```bash
   ctest --test-dir build -R tst_layer_dependencies -V --output-on-failure 2>&1 | grep "game"
   ```
2. 全 game 層のヘッダで前方宣言・include を確認:
   ```bash
   grep -rn "views\|widgets\|dialogs\|ShogiView\|StartGameDialog\|PromotionDialog" src/game/*.h src/game/*.cpp
   ```

### Step 2: matchcoordinator.h の StartGameDialog 前方宣言を除去

1. `matchcoordinator.h` の `class StartGameDialog;` 前方宣言を削除
2. `const StartGameDialog*` パラメータを `const GameStartOptions&` 等のデータ構造体に置換
3. 変換コードを `app/` 層に配置

### Step 3: その他のUI依存をポート化

1. `promotionflow` のプロモーションダイアログ依存（存在する場合）をコールバック化
2. `gamestartcoordinator_dialogflow` のダイアログ起動をポート経由に
3. `prestartcleanuphandler` のUI操作をコールバック化

### Step 4: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_layer_dependencies|tst_game_start_flow|tst_game_end_handler|tst_preset_gamestart_cleanup" --output-on-failure
   ```
3. `tst_layer_dependencies` で `src/game` 違反が 0 件であることを確認

## 完了条件

- `tst_layer_dependencies` で `src/game` 違反が 0 件
- ゲーム開始/中断/終局フローの回帰なし

## 注意事項

- ISSUE-003 で導入したポートパターンと一貫させること
- `matchcoordinator.h` の `StartGameDialog` 前方宣言除去は MEMORY に記載の「Two GameStartCoordinator instances」パターンに注意
- `buildStartOptions` が static メソッドである点を考慮
- ISSUE-003 完了後に着手する
