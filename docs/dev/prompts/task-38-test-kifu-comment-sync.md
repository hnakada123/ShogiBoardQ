# Task 38: 棋譜コメント同期のテスト追加

## Workstream D-2: テスト拡充と品質ゲート

## 目的

`GameRecordModel` と `GameRecordPresenter` の同期整合をテストし、リファクタリング（特に Workstream C-1 の `const_cast` 除去）による回帰を検出する。

## 背景

- Workstream C-1 で `const_cast` 除去を行う際、`GameRecordModel` と `GameRecordPresenter` の関係が変わる可能性がある
- コメント・しおり更新の双方向反映が正しく動作することを保証するテストが必要

## 対象ファイル

- `tests/` 配下に新規テストファイルを追加
- テスト対象: `GameRecordModel`, `GameRecordPresenter`

## 実装内容

以下のシナリオについてテストを追加する:

1. **GameRecordModel と GameRecordPresenter の同期整合**
   - Presenter のデータ変更が Model に反映されること
   - Model のデータ変更が Presenter に反映されること

2. **コメント更新の双方向反映**
   - 棋譜の手にコメントを追加/変更した場合の反映
   - コメントの削除が正しく反映されること

3. **しおり更新の双方向反映**
   - しおり（ブックマーク）の追加/削除が反映されること

4. **bind() の動作確認**
   - `GameRecordModel::bind()` が正しくデータソースをバインドすること
   - バインド解除後の動作

## 制約

- 既存テストを壊さない
- GUI 環境がなくても実行可能（offscreen）
- テストは独立して実行可能であること

## 受け入れ条件

- 上記シナリオのテストが追加されている
- 全テスト（既存 + 新規）が通過する
- `const_cast` 除去後も同期が正常であることを検証できる

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 追加したテストファイル一覧
- テストケースの説明
- テスト実行結果
