# Task 3-1: GameRecordLoadService の作成

## 背景

MainWindow 責務移譲の領域3（棋譜・局面更新の分離）ステップ1。
棋譜表示時の初期化とコメント同期を専用サービスに分離する。

## 対象メソッド

- `MainWindow::displayGameRecord`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/gamerecordloadservice.h/.cpp` を新規作成する。
2. `displayGameRecord` のデータ初期化部分（棋譜モデルへのデータ設定、コメント配列同期等）を `GameRecordLoadService` に移植する。
3. `MainWindow::displayGameRecord` は `GameRecordLoadService` への委譲に書き換える。
4. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
5. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- `m_kifu.commentsByRow` 等の直接操作を新サービスに集約する。
- UI依存なしで単体検証しやすい構造にする。
- clazy 警告が出ないようにする。

## 完了条件

- `displayGameRecord` の本体が「1回の委譲呼び出し」程度になっている。
- `MainWindow` での `m_kifu.commentsByRow` 等の直接操作が減っている。
- ビルドが通る。
