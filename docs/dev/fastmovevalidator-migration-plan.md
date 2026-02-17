# FastMoveValidator 段階移行ガイド（AI依頼プロンプト付き）

## 目的

`MoveValidator` から `FastMoveValidator` へ安全に移行し、性能改善を得つつ機能回帰を防ぐ。

## 前提（現時点の確認済み事項）

- 同値比較テスト（通常局面 + 特殊局面 + 到達可能ランダム局面）は PASS。
- `MoveValidator` 比で以下を実測:
  - `generateLegalMoves`: 約 55.9 倍高速
  - `isLegalMove`: 約 54.0 倍高速
- ただし全面置換前に、実運用経路での段階検証を推奨。

## 推奨移行方針

1. 機能フラグ導入（`MoveValidator` / `FastMoveValidator` を切替可能にする）
2. `ShogiGameController` から先行移行
3. 安定後に王手判定利用箇所（千日手/持将棋/入玉判定）を移行
4. 全面移行後に `MoveValidator` の整理（即削除は避け、一定期間の退避）

## 作業手順

### Step 1: 切替ポイントの導入

- 設定値またはビルドオプションで `FastMoveValidator` を選択可能にする。
- 初期値は `FastMoveValidator` でも良いが、緊急時に `MoveValidator` に戻せる設計を維持する。

**完了条件**
- 1箇所の切替で全利用箇所のバリデータ選択が変わる。
- 既存テストがすべて通る。

### Step 2: `ShogiGameController` 先行移行

- 実際の着手検証の中核である `ShogiGameController` の `MoveValidator` 利用を `FastMoveValidator` に変更。
- 成り判定（成/不成の分岐）が既存どおり動くことを重点確認。

**完了条件**
- 対局中の指し手検証で挙動差分がない。
- 手動確認（新規対局開始→数十手→成り選択→終局操作）で問題なし。

### Step 3: 周辺判定ロジック移行

- 以下の `checkIfKingInCheck` 利用箇所を `FastMoveValidator` に移行:
  - `SennichiteDetector`
  - `JishogiScoreDialogController`
  - `NyugyokuDeclarationHandler`

**完了条件**
- 千日手判定・持将棋判定・入玉宣言判定が回帰しない。

### Step 4: 監視とロールバック運用

- 一定期間、以下をログ監視:
  - 合法手数が 0 になる頻度の急増
  - 着手拒否の増加
  - 王手判定関連のUI表示異常
- 異常時は機能フラグで即時 `MoveValidator` に戻せる状態を維持。

### Step 5: 最終整理

- 安定確認後、`MoveValidator` の依存参照を削除。
- 不要コード・不要テスト・不要ドキュメントを整理。

## テスト計画（移行中毎回実施）

1. `tst_movevalidator`
2. `tst_fastmovevalidator`
3. `tst_movevalidator_equivalence`
4. `tst_movevalidator_perf`
5. 必要に応じて `tst_integration`

## リスクと対処

- リスク: 特定局面でのみ発生する挙動差分
  - 対処: 差分局面を都度フィクスチャ化し、同値テストに追加
- リスク: UI/対局フローの想定外経路での不整合
  - 対処: 機能フラグで即ロールバック、再現手順を保存して回帰テスト化

## AIに依頼するためのプロンプト集

### 1. 機能フラグ導入

```text
MoveValidator と FastMoveValidator を切り替え可能な機能フラグを導入してください。
要件:
- 切替は1箇所の設定で反映されること
- 既存動作を壊さないこと
- 変更後に build と関連テスト（tst_movevalidator, tst_fastmovevalidator, tst_movevalidator_equivalence）を実行すること
- 変更ファイルとテスト結果を報告すること
```

### 2. ShogiGameController 先行移行

```text
ShogiGameController 内の MoveValidator 利用を FastMoveValidator に移行してください。
要件:
- 成り/不成の判定分岐を既存と同じに保つこと
- 既存の手番・駒台・座標変換ロジックを壊さないこと
- 変更後に build とテスト（tst_movevalidator_equivalence, tst_integration）を実行すること
- 差分があれば再現局面をテスト追加してから修正すること
```

### 3. 周辺判定ロジック移行

```text
SennichiteDetector, JishogiScoreDialogController, NyugyokuDeclarationHandler で使っている
MoveValidator::checkIfKingInCheck を FastMoveValidator へ移行してください。
要件:
- 判定結果が変わらないこと
- 既存UI表示文言や分岐を変更しないこと
- build と関連テストを実行し、結果を報告すること
```

### 4. 回帰テスト強化

```text
FastMoveValidator 移行で問題が出やすい局面（王手回避、打ち歩詰め、強制成り、二歩、両王手）を
追加でテスト化してください。
要件:
- MoveValidator と FastMoveValidator の結果を比較すること
- 不一致局面は最小再現SFENを残すこと
- build と新規テスト実行結果を報告すること
```

### 5. 最終クリーンアップ

```text
FastMoveValidator への全面移行後、MoveValidator の未使用参照を調査し、
安全に削除可能なコードを整理してください。
要件:
- 削除前に参照箇所を一覧化すること
- 影響範囲を説明すること
- build と全関連テストを実行すること
```

