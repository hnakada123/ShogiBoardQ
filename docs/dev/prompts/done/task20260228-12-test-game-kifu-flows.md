# Task 12: 対局開始/棋譜ロードフローテスト追加（テーマD: app層テスト補完）

## 目的

対局開始フロー（GameSessionOrchestrator）と棋譜ロードフロー（KifuFileController/KifuLoadCoordinator）のシナリオテストを追加する。

## 背景

- 対局開始は複数のコンポーネントが連携する複雑なフロー
- 棋譜ロードはユーザー操作の主要パスで回帰リスクが高い
- テーマD（app層テスト補完）の中核作業

## 事前調査

### Step 1: 対局開始フローの確認

```bash
# GameSessionOrchestrator
cat src/app/gamesessionorchestrator.h
cat src/app/gamesessionorchestrator.cpp

# GameStartCoordinator
cat src/game/gamestartcoordinator.h
head -100 src/game/gamestartcoordinator.cpp

# 既存テスト
cat tests/tst_game_start_flow.cpp
cat tests/tst_game_start_orchestrator.cpp
```

### Step 2: 棋譜ロードフローの確認

```bash
# KifuFileController
cat src/kifu/kifufilecontroller.h
cat src/kifu/kifufilecontroller.cpp

# KifuLoadCoordinator
cat src/kifu/kifuloadcoordinator.h
head -100 src/kifu/kifuloadcoordinator.cpp
```

## 実装手順

### Step 3: 対局開始テスト

`tests/tst_app_game_session.cpp` を作成:

テスト項目:
1. `GameSessionOrchestrator` が正しく初期化される
2. 対局開始シグナルが正しいハンドラに伝播する
3. 対局中の状態遷移が正しい
4. 対局終了後のクリーンアップが完了する

### Step 4: 棋譜ロードテスト

`tests/tst_app_kifu_load.cpp` を作成:

テスト項目:
1. KIF/KI2/CSA/JKF各形式の正常ロード
2. ロード完了後のナビゲーション状態
3. ロードエラー時のハンドリング
4. 空ファイル・不正ファイルのエラーハンドリング

### Step 5: テスト用リソース

テスト用の棋譜ファイルが必要な場合:
```bash
ls tests/data/ tests/testdata/ tests/fixtures/ 2>/dev/null
rg "testdata|test.*data|fixture" tests/ -l
```

### Step 6: CMakeLists.txt更新・ビルド・テスト

```bash
# CMakeLists.txtに追加後
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] 対局開始フローのテストが追加されている
- [ ] 棋譜ロードフローのテストが追加されている
- [ ] 全テスト通過
- [ ] CI通過

## KPI変化目標

- テスト数: +2（対局1本 + 棋譜1本）

## 注意事項

- 既存の `tst_game_start_flow.cpp` / `tst_game_start_orchestrator.cpp` と重複しないこと
- テスト用のスタブは既存パターン（`test_stubs`）を再利用する
- ファイルI/Oテストは一時ファイルを使用し、テスト後にクリーンアップする
