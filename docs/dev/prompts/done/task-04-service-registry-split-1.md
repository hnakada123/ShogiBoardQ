# Task 04: MainWindowServiceRegistry 分割 - 第1カテゴリの移動

## フェーズ

Phase 1（短期）- P0-3 対応・実装フェーズ1

## 背景

Task 03 で策定した分割計画に基づき、最も依存が少ないカテゴリから順に分割を開始する。

## 実施内容

1. `docs/dev/service-registry-split-plan.md` を読み、分割計画を確認する
2. 計画で「最初に分割すべき」と推奨されたカテゴリの Registry クラスを新規作成する
   - ヘッダーファイルとソースファイルを `src/app/` に作成
   - 対象の `ensure*` メソッドを `MainWindowServiceRegistry` から新クラスへ移動
   - 移動したメソッドが参照する `std::bind` コールバックも一緒に移動
3. `MainWindowServiceRegistry` から移動済みメソッドを削除する
4. `MainWindow` 側の呼び出しを新クラス経由に更新する
5. `CMakeLists.txt` に新ファイルを追加する
6. ビルド確認: `cmake --build build`

## 完了条件

- 新 Registry クラスが作成され、対象メソッドが移動済み
- `MainWindowServiceRegistry` から移動したメソッドが削除されている
- ビルドが通る
- 既存の動作が変わらない（リファクタのみ）

## 前提

- Task 03 の分割計画が完了していること

## 注意事項

- `connect()` 文でラムダを使わない（CLAUDE.md コードスタイル準拠）
- 移動時にメソッドのシグネチャは変更しない
- 依存関係が複雑な場合は、一部メソッドを残して次タスクに回してよい
