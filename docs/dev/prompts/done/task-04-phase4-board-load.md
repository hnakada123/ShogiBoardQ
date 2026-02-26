# Phase 4: BoardLoadService 導入 — SFEN適用/分岐同期の分離

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` のフェーズ4。
SFEN から盤面への適用と分岐ナビゲーション同期を `BoardLoadService` に集約し、`KifuNavigationCoordinator` を拡張する。

## 対象メソッド

`src/app/mainwindow.cpp` / `src/app/mainwindow.h` から以下を移譲:

- `loadBoardFromSfen()` — SFEN文字列からの盤面適用
- `loadBoardWithHighlights()` — 盤面適用 + ハイライト同期
- `syncBoardAndHighlightsAtRow()` — 棋譜行選択時の盤面同期
- `onBranchNodeHandled()` — 分岐ノード処理後のコールバック

## 実装手順

1. **事前調査**
   - 上記メソッドの実装を読み、依存する状態変数を特定
   - `skipBoardSyncForBranchNav` フラグの使用箇所を全て確認
   - `BoardSyncPresenter` の既存機能を確認（`loadBoardWithHighlights` は既に拡張済み）
   - connect() でのスロットターゲット使用箇所を確認

2. **新規クラス作成**
   - `src/app/boardloadservice.h` / `src/app/boardloadservice.cpp` を作成
   - Deps struct パターン:
     - `ShogiBoard` の参照
     - `ShogiView` の参照
     - `BoardSyncPresenter` の参照
     - `GameRecordModel` の参照
     - 手番表示更新用コールバック

3. **KifuNavigationCoordinator 拡張**
   - 「分岐ナビ中ガード（`skipBoardSyncForBranchNav`）」の管理を MainWindow から移動
   - `onBranchNodeHandled` の完全委譲

4. **ロジックの移植**
   - `loadBoardFromSfen` → `BoardLoadService::loadFromSfen`
   - `loadBoardWithHighlights` → `BoardLoadService::loadWithHighlights`
   - `MainWindow` 側は薄い委譲ラッパーに変更

5. **CMakeLists.txt 更新**

6. **ビルド確認**
   ```bash
   cmake -B build -S . -G Ninja && ninja -C build
   ```

## 完了条件

- 盤面適用とハイライト更新の本体が `MainWindow` から消える
- `skipBoardSyncForBranchNav` の管理が `KifuNavigationCoordinator` に移動
- ビルドが通る

## 注意

- **`skipBoardSyncForBranchNav` の競合**に注意（計画書 8.3 の重点回帰観点）
- 中〜高リスク: 盤面同期の多重発火を防ぐため、同期責務を一本化すること
- 手動テスト必須:
  - 棋譜行選択で盤面が正しく同期される
  - 分岐選択で盤面が正しく切り替わる
  - ハイライト（最終手）が正しく表示される
