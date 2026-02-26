# Phase 3: GameRecordUpdateService 導入 — 棋譜追記とライブ更新の分離

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` のフェーズ3。
棋譜追記・ライブセッション更新ロジックを `GameRecordUpdateService` に集約する。
実行優先順位はPhase 1の次（計画書セクション11による）。

## 対象メソッド

`src/app/mainwindow.cpp` / `src/app/mainwindow.h` から以下を移譲:

- `appendKifuLine()` — 棋譜1手追記
- `updateGameRecord()` — 棋譜モデル更新
- `onMoveCommitted()` の後半処理 — USI追記・`currentSfen` 更新

## 実装手順

1. **新規クラス作成**
   - `src/app/gamerecordupdateservice.h` / `src/app/gamerecordupdateservice.cpp` を作成
   - Deps struct パターンで依存関係を注入:
     - `GameRecordModel` の参照
     - `RecordPresenter` の参照
     - `LiveGameSessionUpdater` の参照
     - USI（エンジン）インターフェースの参照
     - `currentSfen` 等の状態アクセス

2. **ロジックの移植**
   - `appendKifuLine` の本体ロジックを移動
   - `updateGameRecord` の本体ロジックを移動
   - `onMoveCommitted` から棋譜更新・USI追記の部分を抽出して移動
   - `MainWindow` 側は1〜3行の委譲メソッドに変更

3. **CMakeLists.txt 更新**
   - `src/app/gamerecordupdateservice.cpp` を SOURCES に追加

4. **MainWindow での初期化**
   - `MainWindowCompositionRoot` に `ensureGameRecordUpdateService()` を追加
   - 依存関係が揃うタイミングで `updateDeps()` を呼び出し

5. **ビルド確認**
   ```bash
   cmake -B build -S . -G Ninja && ninja -C build
   ```

## 完了条件

- `MainWindow::updateGameRecord` が1〜3行の委譲になる
- `MainWindow::appendKifuLine` が1〜3行の委譲になる
- 棋譜更新と LiveGameSessionUpdater 連携が `GameRecordUpdateService` に集約
- ビルドが通る

## 注意

- `appendKifuLine` / `updateGameRecord` / `onMoveCommitted` が connect() のスロットターゲットとして使われている可能性があるため、MainWindow側のラッパーは残す必要がある
- `LiveGameSessionUpdater` との連携を壊さないこと
- `sfenRecord()` 参照先のライフタイムに注意（計画書 8.3 の重点回帰観点）
- Deps struct の `std::function` コールバックで lazy-init のギャップを埋めること
