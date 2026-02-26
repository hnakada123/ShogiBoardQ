# Task 1-4: KifuExportDepsAssembler の作成

## 背景

MainWindow 責務移譲の領域1（配線・DI組み立ての分離）ステップ4。
`MainWindow` から棋譜エクスポート依存の収集ロジックを分離する。

## 対象メソッド

- `MainWindow::updateKifuExportDependencies`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/kifuexportdepsassembler.h/.cpp` を新規作成する。
2. `updateKifuExportDependencies` の依存収集・設定ロジックを `KifuExportDepsAssembler` に移植する。
3. `MainWindow::updateKifuExportDependencies` は `KifuExportDepsAssembler` への1行委譲に書き換える。
4. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
5. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- clazy 警告が出ないようにする。

## 完了条件

- `updateKifuExportDependencies` が1行委譲になっている。
- ビルドが通る。
