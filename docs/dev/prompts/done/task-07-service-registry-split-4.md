# Task 07: MainWindowServiceRegistry 分割 - 第4カテゴリの移動と最終整理

## フェーズ

Phase 1（短期）- P0-3 対応・実装フェーズ4

## 背景

Task 06 に続き、残りのカテゴリを移動し、ServiceRegistry 分割を完了する。

## 実施内容

1. `docs/dev/service-registry-split-plan.md` を読み、残りカテゴリを確認する
2. 残りの Registry クラスを作成し、対象メソッドを移動する
3. `MainWindowServiceRegistry` に残るメソッドを確認し、共通/基盤的なもののみ残す
4. 全呼び出し元を更新する
5. `CMakeLists.txt` に新ファイルを追加する
6. ビルド確認: `cmake --build build`
7. KPI 確認: `MainWindowServiceRegistry::` 定義数が 60 以下になっているか確認

## 完了条件

- 全カテゴリの分割が完了している
- `MainWindowServiceRegistry::` 実装数が 95 → 60 以下
- ビルドが通る
- `mainwindowserviceregistry.cpp` の行数が大幅に削減されている

## 前提

- Task 06 が完了していること

## 注意事項

- 分割計画から大きく乖離する場合は `docs/dev/service-registry-split-plan.md` も更新する
