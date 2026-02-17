# FastMoveValidator アルゴリズム概要（移行後）

## 概要

FastMoveValidator は、盤面・持ち駒・手番を入力として以下を提供する。

- 単一手の合法判定（`isLegalMove`）
- 局面の合法手数生成（`generateLegalMoves`）
- 王手判定（`checkIfKingInCheck`）

## 現行の判定方針

1. 入力手の基本妥当性を検証
2. 駒種ごとの移動制約（成り/不成、行き所のない駒、駒打ち制約）を検証
3. 自玉が王手放置にならないことを確認
4. 禁手（例: 二歩、打ち歩詰め）を検証

## 重点ルール

- 二歩
- 打ち歩詰め
- 強制成り（歩・香・桂）
- 王手回避（単王手/両王手）

## 実装位置

- `src/core/fastmovevalidator.h`
- `src/core/fastmovevalidator.cpp`

## テスト対応

- `tests/tst_fastmovevalidator.cpp`
- `tests/tst_integration.cpp`

## 補足（履歴）

旧 MoveValidator の設計課題分析は移行時の参考情報であり、現在の実装判断は FastMoveValidator と現行テスト結果を基準とする。
