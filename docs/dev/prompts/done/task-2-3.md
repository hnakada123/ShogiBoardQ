# Task 2-3: SessionLifecycleCoordinator - 終局処理の移植

## 背景

MainWindow 責務移譲の領域2（セッション制御の分離）ステップ3。
`SessionLifecycleCoordinator` に終局処理ロジックを追加する。

## 前提

- Task 2-1 が完了していること（`SessionLifecycleCoordinator` が存在すること）。

## 対象メソッド

- `MainWindow::onMatchGameEnded`（`src/app/mainwindow.cpp`）

## やること

1. `onMatchGameEnded` の終局時刻反映と連続対局判定ロジックを `SessionLifecycleCoordinator` に移植する。
2. `MainWindow::onMatchGameEnded` は `SessionLifecycleCoordinator` への委譲に書き換える。
3. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- 連続対局への遷移判定はドメインロジックなので coordinator に含める。
- UI操作が必要な部分はコールバックまたは signal で MainWindow に戻す設計にする。
- clazy 警告が出ないようにする。

## 完了条件

- `onMatchGameEnded` が「イベント受信 -> coordinator 呼び出し」に統一されている。
- ビルドが通る。
