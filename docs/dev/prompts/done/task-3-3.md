# Task 3-3: UndoFlowService の作成

## 背景

MainWindow 責務移譲の領域3（棋譜・局面更新の分離）ステップ3。
「待った」実行時の巻き戻し後処理を専用サービスに分離する。

## 対象メソッド

- `MainWindow::undoLastTwoMoves`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/undoflowservice.h/.cpp` を新規作成する。
2. `undoLastTwoMoves` の巻き戻し後処理（評価値グラフ・ply同期等）を `UndoFlowService` に移植する。
3. `MainWindow::undoLastTwoMoves` は `UndoFlowService` への委譲に書き換える。
4. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
5. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- 評価値グラフのカーソル位置同期を正確に移植する。
- clazy 警告が出ないようにする。

## 完了条件

- `undoLastTwoMoves` が `UndoFlowService` への委譲になっている。
- ビルドが通る。
