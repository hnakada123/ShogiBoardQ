# FastMoveValidator 移行完了ガイド（AI依頼プロンプト付き）

## 目的

MoveValidator から FastMoveValidator への移行を完了した現状を整理し、今後の保守作業を標準化する。

## 移行完了サマリ

- 本番利用箇所は FastMoveValidator に統一済み
- MoveValidator 本体と比較専用テストは削除済み
- 主要確認テストは以下で継続運用:
  - `tst_fastmovevalidator`
  - `tst_integration`
  - `tst_coredatastructures`

## 運用ルール

1. 新規ロジック追加時は FastMoveValidator のみを拡張する
2. ルール差分修正時は局面（SFEN）ベースで再現テストを先に追加する
3. リファクタ時は最低限 `tst_fastmovevalidator` と `tst_integration` を実行する

## 追加検証チェックリスト

- 王手回避（玉移動・合い駒・駒取り）
- 打ち歩詰め
- 強制成り（歩・桂・香）
- 二歩
- 両王手

## AI依頼プロンプト

### 1. FastMoveValidator 修正依頼

```text
FastMoveValidator の挙動修正を行ってください。
要件:
- まず不具合の最小再現SFENテストを追加すること
- 成り/不成、打ち歩詰め、二歩、王手回避への副作用を確認すること
- 変更後に build と tst_fastmovevalidator, tst_integration を実行すること
- 変更ファイルとテスト結果を報告すること
```

### 2. FastMoveValidator 最適化依頼

```text
FastMoveValidator のパフォーマンス最適化を行ってください。
要件:
- 振る舞いを変えないこと
- 測定コマンドと比較結果（変更前/変更後）を示すこと
- 変更後に build と tst_fastmovevalidator, tst_integration を実行すること
```

### 3. ドキュメント更新依頼

```text
docs/dev 内の FastMoveValidator 関連ドキュメントを現行実装に合わせて更新してください。
要件:
- 旧 MoveValidator 前提の記述を現行表現へ修正すること
- 削除済みファイル/テストへの参照を除去すること
- 変更したドキュメント一覧を報告すること
```
