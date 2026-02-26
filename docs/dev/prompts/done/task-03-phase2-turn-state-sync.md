# Phase 2: TurnStateSyncService 導入 — 手番同期の分離

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` のフェーズ2。
TurnManager / GameController / View / 時計表示の手番同期ロジックを `TurnStateSyncService` に集約する。

## 対象メソッド

`src/app/mainwindow.cpp` / `src/app/mainwindow.h` から以下を移譲:

- `setCurrentTurn()` — 手番設定
- `onTurnManagerChanged()` — TurnManager変更通知ハンドラ
- `updateTurnAndTimekeepingDisplay()` の手番同期部分 — 手番表示の更新

## 実装手順

1. **事前調査**
   - 上記3メソッドの実装を読み、依存する状態変数・他メソッドを特定
   - `connect()` でスロットターゲットとして使われている箇所を `Grep` で特定
   - TurnManager / GameController / View の更新順序を確認

2. **新規クラス作成**
   - `src/app/turnstatesyncservice.h` / `src/app/turnstatesyncservice.cpp` を作成
   - Deps struct パターンで依存関係を注入:
     - `TurnManager` の参照
     - `ShogiGameController` の参照
     - `ShogiView`（手番表示用）の参照
     - `ShogiClock` の参照
     - 時計ラベル UI の参照

3. **ロジックの移植**
   - `setCurrentTurn` の本体ロジックを移動
   - `onTurnManagerChanged` の本体ロジックを移動
   - `updateTurnAndTimekeepingDisplay` のうち手番同期に関する部分を移動
   - `MainWindow` 側は薄いラッパーに変更

4. **CMakeLists.txt 更新**
   - `src/app/turnstatesyncservice.cpp` を SOURCES に追加

5. **MainWindow での初期化**
   - `MainWindowCompositionRoot` に `ensureTurnStateSyncService()` を追加

6. **ビルド確認**
   ```bash
   cmake -B build -S . -G Ninja && ninja -C build
   ```

## 完了条件

- TurnManager 関連ロジックの本体が `MainWindow` から除去
- `MainWindow` には薄い委譲ラッパーのみ残る
- ビルドが通る

## 注意

- **TurnManager と GameController の同期順序**に注意（計画書 8.3 の重点回帰観点）
- `setCurrentTurn` / `onTurnManagerChanged` は signal/slot ターゲットとして使われている可能性が高い。MainWindow 側のスロットラッパーは必ず残すこと
- ライブ対局中と盤面ナビゲーション時で情報源が切り替わるロジックを正しく移植すること
- 中リスク: 時計表示との同期タイミングが微妙なため、手動テストで以下を確認:
  - 対局中の手番表示が正しく切り替わる
  - 時計が正しく動作する
  - 棋譜ナビゲーション時に手番表示が追従する
