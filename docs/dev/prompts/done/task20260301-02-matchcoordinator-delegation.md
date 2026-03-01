# Task 02: MatchCoordinator 委譲完了・600行以下化

## 概要

`src/game/matchcoordinator.cpp`（640行）の残存メソッドをユースケース単位で整理し、600行以下に削減する。

## 前提条件

- Task 01（GameSubRegistry分割）完了推奨

## 現状

- `src/game/matchcoordinator.cpp`: 640行
- `src/game/matchcoordinator.h`: 546行
- 既に抽出済みの5ハンドラ: GameEndHandler, GameStartOrchestrator, EngineLifecycleManager, MatchTimekeeper, AnalysisSessionHandler, MatchUndoHandler, MatchTurnHandler
- 現在の MC は各ハンドラへの薄い委譲ハブ
- ensure*() メソッドが7個（58〜311行）で全体の約40%

## 分析

残存メソッドのカテゴリ:

1. **ensure*() 群**（7メソッド、~253行）: ハンドラの遅延初期化。Refs/Hooks構築が冗長
2. **エンジン管理転送**（11メソッド、~65行）: ELMへの1行委譲
3. **終局転送**（10メソッド、~60行）: GEHへの1行委譲
4. **対局開始転送**（4メソッド、~30行）: GSOへの1行委譲
5. **ターン転送**（10メソッド、~50行）: MTHへの1行委譲
6. **時間管理転送**（14メソッド、~55行）: Timekeeperへの1行委譲
7. **解析転送**（4メソッド、~25行）: ASHへの1行委譲
8. **Undo転送**（2メソッド、~10行）: UndoHandlerへの1行委譲

## 実施内容

### Step 1: ensure*() のリファクタリング

1. `matchcoordinator.cpp` を読み込み、各 `ensure*()` の Refs/Hooks 構築ロジックを分析
2. Hooks 構築部分を共通ヘルパー `buildXxxHooks()` として private メソッドに集約
3. `ensure*()` 本体を簡潔化（create + buildHooks + set の3行パターン）

### Step 2: 転送メソッドのインライン化検討

1. .h の中で1行委譲メソッドを `inline` 化できるか検討（.cpp行数削減）
2. ただし .h が 546行あるため、行数移動にならない場合は見送る
3. 代替案: 転送メソッドを機能グループ別の `_impl.cpp` に分割
   - `matchcoordinator_engine.cpp` — エンジン管理転送
   - `matchcoordinator_time.cpp` — 時間管理転送

### Step 3: matchcoordinator.h のスリム化

1. 546行ある .h を分析し、不要な前方宣言・不要なメソッド宣言を洗い出す
2. 直接使われていない public メソッドがあれば削除（dead code）
3. nested struct/enum が .h に定義されている場合、別ヘッダへの分離を検討

### Step 4: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. CMakeLists.txt に新規ファイルがあれば追加

## 完了条件

- `matchcoordinator.cpp` が 600行以下
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過（特に `tst_wiring_contracts`, `tst_stubs_matchcoordinator`）

## 注意事項

- MatchCoordinator のシグナルは signal-to-signal forwarding で維持する
- Hooks の4層ネスト構造（UI/Time/Engine/Game）を維持する
- `createAndStartModeStrategy` は MC に強く結合しているため、移動困難な場合は残してよい
- .h の行数も意識する（現状546行は大きい）
