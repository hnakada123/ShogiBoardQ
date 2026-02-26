# Task 3-2: ConsiderationPositionService の作成

## 背景

MainWindow 責務移譲の領域3（棋譜・局面更新の分離）ステップ2。
検討モードでの局面解決ロジックを専用サービスに分離する。

## 対象メソッド

- `MainWindow::onBuildPositionRequired`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/considerationpositionservice.h/.cpp` を新規作成する。
2. `onBuildPositionRequired` の分岐・本譜統合局面解決ロジックを `ConsiderationPositionService` に移植する。
3. `MainWindow::onBuildPositionRequired` は `ConsiderationPositionService` への1行呼び出しに変更する。
4. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
5. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- 検討モード特有のロジック（分岐・本譜統合局面解決）を正確に移植する。
- UI依存なしで単体検証しやすい構造にする。
- clazy 警告が出ないようにする。

## 完了条件

- `onBuildPositionRequired` が `ConsiderationPositionService` 呼び出しのみになっている。
- ビルドが通る。
