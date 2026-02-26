# Task 1-2: MainWindowWiringAssembler の作成

## 背景

MainWindow 責務移譲の領域1（配線・DI組み立ての分離）ステップ2。
`MainWindow` から `MatchCoordinatorWiring::Deps` 生成と signal 接続を分離する。

## 対象メソッド

- `MainWindow::buildMatchWiringDeps`（`src/app/mainwindow.cpp`）
- `MainWindow::wireMatchWiringSignals`（`src/app/mainwindow.cpp`）
- `MainWindow::initializeDialogLaunchWiring`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/mainwindowwiringassembler.h/.cpp` を新規作成する。
2. `buildMatchWiringDeps` の `MatchCoordinatorWiring::Deps` 構築ロジックを `MainWindowWiringAssembler` に移植する。
3. `wireMatchWiringSignals` の signal 接続ロジックを同クラスに移植する。
4. `initializeDialogLaunchWiring` のロジックを同クラスに移植する。
5. `MainWindow` 側の各メソッドは `MainWindowWiringAssembler` への1行委譲に書き換える。
6. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
7. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- `std::bind` の塊がこのクラスに移動する形になるが、`MainWindow` からは消える。
- clazy 警告が出ないようにする。

## 完了条件

- 対象3メソッドの本体が「ensure + 1回の委譲呼び出し」程度になっている。
- `MainWindow` から `std::bind` の塊が大幅に減っている。
- ビルドが通る。
