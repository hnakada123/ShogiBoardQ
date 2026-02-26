# Task 3-4: GameRecordModelBuilder の作成

## 背景

MainWindow 責務移譲の領域3（棋譜・局面更新の分離）ステップ4。
`ensureGameRecordModel` の構築処理を専用ビルダーに分離する。

## 対象メソッド

- `MainWindow::ensureGameRecordModel`（`src/app/mainwindow.cpp`）

## やること

1. `src/app/gamerecordmodelbuilder.h/.cpp` を新規作成する。
2. `ensureGameRecordModel` の bind/callback/connect 構築ロジックを `GameRecordModelBuilder` に移植する。
3. `MainWindow::ensureGameRecordModel` は `GameRecordModelBuilder` への委譲に書き換える（ensure パターンは維持）。
4. `CMakeLists.txt` に新規ファイルを追加する（`scripts/update-sources.sh` で生成）。
5. ビルドが通ることを確認する。

## 注意事項

- CLAUDE.md のコーディング規約に従う（lambda禁止、std::as_const、NSDMI等）。
- `ensure*` の遅延初期化パターンは MainWindow 側に残す（null チェック + 生成委譲）。
- 既存の `GameRecordUpdateService` との責務境界を明確にする（Builder は初期構築、UpdateService は1手追記・更新フック）。
- clazy 警告が出ないようにする。

## 完了条件

- `ensureGameRecordModel` の構築処理が `GameRecordModelBuilder` に移植されている。
- `ensure*` パターン（null チェック + 委譲）は維持されている。
- ビルドが通る。
