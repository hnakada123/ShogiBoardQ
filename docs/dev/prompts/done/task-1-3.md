# Task 1-3: KifuLoadCoordinatorFactory の作成

## 背景

MainWindow 責務移譲の領域1（配線・DI組み立ての分離）ステップ3。
`MainWindow` から KifuLoadCoordinator の生成・配線ロジックを分離する。

## 対象メソッド

- `MainWindow::createAndWireKifuLoadCoordinator`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/kifuloadcoordinatorfactory.h/.cpp` を新規作成する。
2. `createAndWireKifuLoadCoordinator` の KifuLoadCoordinator 生成・依存設定・signal 接続ロジックを `KifuLoadCoordinatorFactory` に移植する。
3. `MainWindow::createAndWireKifuLoadCoordinator` は `KifuLoadCoordinatorFactory` への1行委譲に書き換える。
4. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
5. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- KifuLoadCoordinator の既存インターフェースは変更しない。
- clazy 警告が出ないようにする。

## 完了条件

- `createAndWireKifuLoadCoordinator` の本体が「1回の委譲呼び出し」程度になっている。
- ビルドが通る。
