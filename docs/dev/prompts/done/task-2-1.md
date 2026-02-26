# Task 2-1: SessionLifecycleCoordinator の作成 - リセット処理の移植

## 背景

MainWindow 責務移譲の領域2（セッション制御の分離）ステップ1。
セッションの状態遷移を `SessionLifecycleCoordinator` に集約し、`MainWindow` は入口のみ担当する方針。
まずリセット処理から移植を始める。

## 対象メソッド

- `MainWindow::resetToInitialState`（`src/app/mainwindow.cpp`）
- `MainWindow::resetGameState`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/sessionlifecyclecoordinator.h/.cpp` を新規作成する。
2. `resetToInitialState` のロジック（盤面リセット・UI状態リセット・モデルクリア等）を `SessionLifecycleCoordinator` に移植する。
3. `resetGameState` のロジック（ゲーム状態リセット・時計リセット等）を同クラスに移植する。
4. `MainWindow` 側の各メソッドは `SessionLifecycleCoordinator` への委譲に書き換える。
5. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
6. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- UI操作が必要な部分はコールバックまたは signal で MainWindow に戻す設計にする。
- `PlayMode` 条件分岐の削減を意識する。
- clazy 警告が出ないようにする。

## 完了条件

- 対象メソッドが「イベント受信 -> coordinator 呼び出し」に統一されている。
- ビルドが通る。
