# Task 07: matchcoordinator.cpp ユースケース単位分割

## 概要

`src/game/matchcoordinator.cpp`（789行）をユースケース単位で分割し、650行以下に削減する。

## 前提条件

- Task 01（ServiceRegistry再分割）完了推奨（Game系サブレジストリが存在する場合はそちらとの整合を取る）

## 現状

- `src/game/matchcoordinator.cpp`: 789行
- `src/game/matchcoordinator.h`: 576行
- ISSUE-060 MC責務移譲継続の対象
- 既に抽出済みのハンドラ:
  - `GameEndHandler`（472行）— 終局処理
  - `GameStartOrchestrator`（414行）— 対局開始
  - `EngineLifecycleManager`（368行）— エンジンライフサイクル
  - `MatchTimekeeper`（240行）— タイムキーピング
  - `AnalysisSessionHandler` — 解析セッション
- Hooks は Task 05 で4層ネスト化済み（UI/Time/Engine/Game）

## 分析すべき残存責務

ファイルを読み込んで以下を特定すること:

1. **ターン管理** — 手番進行、指し手適用
2. **ストラテジー管理** — 対局モード戦略の生成・切替
3. **エンジン通信中継** — USIコマンドの橋渡し
4. **状態リセット** — 対局終了後のクリーンアップ
5. **棋譜連携** — 指し手記録、SFEN更新

## 実施内容

### Step 1: 残存メソッドの責務分析

`matchcoordinator.cpp` の全メソッドを読み、ユースケースごとにグルーピングする。
既に抽出されているハンドラとの境界を明確にする。

### Step 2: 抽出候補の選定

以下を優先的に抽出を検討:

- **ターン進行ハンドラ** — 手番管理、指し手適用、棋譜追記の一連フロー
- **ストラテジーファクトリ** — `createAndStartModeStrategy` 等の戦略パターン関連

### Step 3: 新クラスの抽出

1. 新規ハンドラクラスを `src/game/` に作成
2. Refs/Hooks パターンを適用（既存ハンドラと同じ設計）
3. `MatchCoordinator` から委譲呼び出しに変更
4. シグナル/スロット接続を更新

### Step 4: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. CMakeLists.txt に新規ファイルを追加

## 完了条件

- `matchcoordinator.cpp` が 650行以下
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過（特に `tst_wiring_contracts` が通ること）

## 注意事項

- 既存ハンドラ（GameEndHandler, GameStartOrchestrator, ELM, MatchTimekeeper）との責務重複を避ける
- Hooks の4層ネスト構造（UI/Time/Engine/Game）を維持する
- `MatchCoordinator` のシグナルは signal-to-signal forwarding で維持する
- `createAndStartModeStrategy` は MC に強く結合しているため、移動困難な場合は残してよい
