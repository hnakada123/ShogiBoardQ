# Task 2-2: SessionLifecycleCoordinator - 新規対局開始の移植

## 背景

MainWindow 責務移譲の領域2（セッション制御の分離）ステップ2。
Task 2-1 で作成した `SessionLifecycleCoordinator` に新規対局開始ロジックを追加する。

## 前提

- Task 2-1 が完了していること（`SessionLifecycleCoordinator` が存在すること）。

## 対象メソッド

- `MainWindow::startNewShogiGame`（`src/app/mainwindow.cpp`）

## やること

1. `startNewShogiGame` の再開判定・評価グラフ初期化・開始呼び出しを `SessionLifecycleCoordinator` に移植する。
2. `MainWindow::startNewShogiGame` は `SessionLifecycleCoordinator` への委譲に書き換える。
3. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- UI操作が必要な部分はコールバックまたは signal で MainWindow に戻す設計にする。
- clazy 警告が出ないようにする。

## 完了条件

- `startNewShogiGame` が「イベント受信 -> coordinator 呼び出し」に統一されている。
- ビルドが通る。
