# Task 2-4: SessionLifecycleCoordinator - 時間設定適用の移植

## 背景

MainWindow 責務移譲の領域2（セッション制御の分離）ステップ4。
`SessionLifecycleCoordinator` に時間設定適用ロジックを追加する。

## 前提

- Task 2-1 が完了していること（`SessionLifecycleCoordinator` が存在すること）。

## 対象メソッド

- `MainWindow::onApplyTimeControlRequested`（`src/app/mainwindow.cpp`）

## やること

1. `onApplyTimeControlRequested` の保存・適用・UI反映ロジックを `SessionLifecycleCoordinator` に移植する。
2. `MainWindow::onApplyTimeControlRequested` は `SessionLifecycleCoordinator` への委譲に書き換える。
3. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- 設定の保存は SettingsService 経由で行う。
- UI操作が必要な部分はコールバックまたは signal で MainWindow に戻す設計にする。
- clazy 警告が出ないようにする。

## 完了条件

- `onApplyTimeControlRequested` が「イベント受信 -> coordinator 呼び出し」に統一されている。
- `MainWindow` 内の `PlayMode` 条件分岐がさらに削減されている。
- ビルドが通る。
