# FastMoveValidator 組み込み・移行ガイド

## 目的

既存 `MoveValidator` を段階的に置き換え、`FastMoveValidator` を本番利用できる状態にするための残作業・手順を明確化する。

## 現在の実装状況（2026-02-17）

- 新規クラスを追加済み:
  - `src/core/fastmovevalidator.h`
  - `src/core/fastmovevalidator.cpp`
- 追加済みテスト:
  - `tests/tst_fastmovevalidator.cpp`
- テストビルド登録済み:
  - `tests/CMakeLists.txt`
- 基本機能は実装済み:
  - 合法手判定（`isLegalMove`）
  - 合法手数生成（`generateLegalMoves`）
  - 王手判定（`checkIfKingInCheck`）

## 完全組み込みまでの残作業

1. 呼び出し元の差し替え
   - `MoveValidator` を生成している箇所を `FastMoveValidator` に置換する。
   - 対象例:
     - `src/game/shogigamecontroller.cpp`
     - `src/game/sennichitedetector.cpp`
     - `src/ui/controllers/jishogiscoredialogcontroller.cpp`
     - `src/ui/controllers/nyugyokudeclarationhandler.cpp`

2. 互換性テストの拡充
   - `tests/tst_movevalidator.cpp` の主要ケースを `tst_fastmovevalidator` に移植する。
   - 追加すべき重点ケース:
     - 成り/不成の両立ケース
     - 強制成り
     - 王手回避（合い駒・玉移動・駒取り）
     - 打ち歩詰め
     - 両王手
     - 中終盤の複雑局面

3. ランダム局面の同値検証
   - 同一局面・同一着手に対し、`MoveValidator` と `FastMoveValidator` の結果一致率を検証する。
   - 不一致局面を固定フィクスチャ化して回帰テストへ追加する。

4. 性能計測の追加
   - 代表局面（初期・中盤・終盤）で以下を計測:
     - `isLegalMove` の平均/最大処理時間
     - `generateLegalMoves` の平均/最大処理時間
   - CIまたはローカル再現用コマンドを `docs/dev/test-summary.md` へ追記する。

5. ドキュメント更新
   - `docs/dev/movevalidator-usage.md` に併記または後継クラスとして追記する。
   - `docs/dev/developer-guide.md` の MoveValidator 説明章に移行方針を反映する。

## 推奨移行手順

1. `FastMoveValidator` のテストを既存 `MoveValidator` と同等カバレッジまで拡張
2. ランダム同値検証で不一致を潰す
3. `ShogiGameController` のみ先に差し替え（影響範囲が最も大きい）
4. `SennichiteDetector` / UI判定系を差し替え
5. 手動確認（対局開始→着手→成り→終局まで）
6. 既存 `MoveValidator` を互換期間だけ残し、安定後に削除判断

## FastMoveValidator が MoveValidator より優れている点

1. 軽量なクラス設計
   - `QObject` 非依存で、signal/slot 前提がない。
   - 生成コストと依存が小さい。

2. 実装の単純化
   - 複雑な多層ビットボード構築を避け、盤面シミュレーション中心の実装。
   - 読みやすく追跡しやすいため、不具合調査のコストが低い。

3. 置換しやすいAPI
   - 公開メソッド名・定数名を既存 `MoveValidator` と揃えている。
   - 呼び出し側の変更量を最小化できる。

4. 不要なエラー通知機構の排除
   - 未接続 signal ベースの通知に依存しない。
   - 戻り値中心の判定フローで処理の見通しが良い。

5. 将来の最適化余地が明確
   - 現在の実装を土台に、差分更新・キャッシュ導入・高速化ポイントを段階適用しやすい。

## 注意点（現時点）

- 既存 `MoveValidator` と完全同値であることは未証明。
- 特殊局面を含む網羅テストを追加しないまま全面置換すると、ルール差異が残る可能性がある。
- 本ドキュメントの「完全組み込み」は、テスト拡充と同値検証完了を前提とする。
