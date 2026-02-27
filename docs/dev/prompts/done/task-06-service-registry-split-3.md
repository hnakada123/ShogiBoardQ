# Task 06: MainWindowServiceRegistry 分割 - 第3カテゴリの移動

## フェーズ

Phase 1（短期）- P0-3 対応・実装フェーズ3

## 背景

Task 05 に続き、分割計画の第3カテゴリを移動する。

## 実施内容

1. `docs/dev/service-registry-split-plan.md` を読み、第3カテゴリを確認する
2. 対応する Registry クラスを新規作成する（`src/app/` に配置）
3. 対象の `ensure*` メソッドと関連コールバックを移動する
4. `MainWindowServiceRegistry` から移動済みメソッドを削除する
5. 呼び出し元を更新する
6. `CMakeLists.txt` に新ファイルを追加する
7. ビルド確認: `cmake --build build`

## 完了条件

- 第3カテゴリの Registry クラスが作成され、対象メソッドが移動済み
- ビルドが通る

## 前提

- Task 05 が完了していること
