# Task 1-1: MainWindowRuntimeRefsFactory の作成

## 背景

MainWindow 責務移譲の領域1（配線・DI組み立ての分離）ステップ1。
`MainWindow` から「構築ロジック」を排除し、呼び出しのみにする方針の一環。

## 対象メソッド

- `MainWindow::buildRuntimeRefs`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/mainwindowruntimerefsfactory.h/.cpp` を新規作成する。
2. `buildRuntimeRefs` の中身（RuntimeRefs 構造体の組み立てロジック）を `MainWindowRuntimeRefsFactory` に移植する。
3. `MainWindow::buildRuntimeRefs` は `MainWindowRuntimeRefsFactory` への1行委譲に書き換える。
4. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
5. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- `MainWindow` の既存メンバ変数への参照が必要な場合は、引数として渡す設計にする。
- clazy 警告が出ないようにする。

## 完了条件

- `buildRuntimeRefs` の本体が「1回の委譲呼び出し」程度になっている。
- ビルドが通る。
