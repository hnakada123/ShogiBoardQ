# Task 20: MatchCoordinator さらなる削減の分析

## 目的
`MatchCoordinator`（1444行 + 606行ヘッダー）の残存責務を分析し、さらなる分割で 800行以下にする計画を策定する。

## 背景
`matchcoordinator.cpp` は 2329行 → 1857行 → 1444行 と段階的に削減されてきた。
`GameEndHandler`（472行）と `GameStartOrchestrator`（414行）が既に抽出済みだが、まだ 1444行が残っている。

既に抽出済みのクラス:
- `GameEndHandler` - 終局処理
- `GameStartOrchestrator` - 対局開始フロー
- `GameSessionOrchestrator` - 対局ライフサイクル管理

## 作業内容

### 1. 残存メソッドの責務分類
`matchcoordinator.cpp` の全メソッドを以下のカテゴリに分類する:

#### A: Game Flow Control（対局進行制御）
- 手番管理
- 指し手の受け付け・適用
- 時間管理との連携

#### B: Engine Interaction（エンジン連携）
- エンジンへの局面送信
- エンジン思考結果の処理
- ponder 管理

#### C: Strategy Dispatch（ストラテジー管理）
- GameModeStrategy の生成・管理
- モード別の分岐処理

#### D: State Management（状態管理）
- 対局状態の保持・更新
- シグナル発行
- 各種フラグ管理

#### E: Hooks/Callbacks（外部連携）
- GUI コールバック
- 他コーディネーターへの通知

### 2. 抽出候補の特定
各カテゴリについて:
- 行数を集計する
- 独立して抽出可能かを判定する
- 抽出した場合の依存関係を分析する

### 3. 分割計画の策定
- 抽出対象のクラス名・責務・メソッド一覧を決定する
- 既存の `GameEndHandler` / `GameStartOrchestrator` パターン（Refs/Hooks pattern）に合わせた設計にする
- `matchcoordinator.h` のシグナル・スロット整理計画も含める

## 出力
`docs/dev/done/matchcoordinator-reduction-analysis.md` に分析結果と計画を記載する。

## 完了条件
- [ ] 全メソッドがカテゴリ分類されている
- [ ] 抽出候補が具体的に特定されている
- [ ] 分割後の各クラスの責務が明確に定義されている
- [ ] 段階的な実装計画がある

## 注意事項
- この段階ではコード変更は行わない（分析のみ）
- `MatchCoordinator` は多数のシグナル・スロットを持つ中核クラスのため、シグナル互換性を重視する
- 既に抽出済みの `GameEndHandler` / `GameStartOrchestrator` との重複や循環依存が生じないよう注意する
