# 現在ソースコードの改善点と修正方法（2026-03-03）

## 1. 目的

この文書は、2026-03-03 時点の `ShogiBoardQ` 現行ソースを対象に、改善すべき点と具体的な修正方法を「実行可能な手順」で整理したものです。  
対象は品質劣化の予防、保守性向上、将来の機能追加時の開発速度維持です。

---

## 2. 現状サマリ（実測）

- テスト: `ctest --test-dir build --output-on-failure` は **70/70 pass**
- `src/` の C++ ファイル数（`.cpp/.h`）: **604**
- C++ 総行数（`src/`、`wc -l`）: **106,941**
- 550行超ファイル数: **8**
- 500行超ファイル数: **24**
- `MainWindow` include 数: KPI 上限 **37**（上限値と同値）
- `MainWindow` friend class 数: KPI 上限 **7**（上限値と同値）

関連根拠:

- `tests/tst_structural_kpi.cpp`
- `tests/tst_layer_dependencies.cpp`
- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`

---

## 3. 優先度一覧

| ID | 優先度 | 改善テーマ | 期待効果 |
|---|---|---|---|
| A | P0 | `MainWindow` 境界の縮退（依存密度の低減） | 変更影響範囲の削減、レビュー容易化 |
| B | P0 | `UsiProtocolHandler` 再接続安全性 | 多重接続/重複処理リスク低減 |
| C | P1 | `buildRuntimeRefs()` 分割と契約明文化 | 依存注入の可読性向上 |
| D | P1 | `MatchCoordinator` の Hook 構築分離 | 対局制御の保守性向上 |
| E | P1 | `KifuApplyService` の責務分離 | 棋譜ロード系の回帰防止 |
| F | P2 | 大型ファイル削減（KPI に余白を作る） | 閾値引き上げ不要化、拡張余地確保 |
| G | P2 | 静的解析ゲートの段階強化 | 不具合の早期発見 |
| H | P2 | ドキュメント実測同期の自動化 | 仕様と実態の乖離防止 |

---

## 4. 詳細改善案と修正方法

## A. `MainWindow` 境界の縮退（P0）

### 問題

- `MainWindow` が多数の参照・状態を直接保持しており、責務境界が広い。
- `friend class` 数、include 数が KPI 上限に張り付いている。

根拠:

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `tests/tst_structural_kpi.cpp`（`mainWindowFriendClassLimit` / `mainWindowIncludeCount`）

### 修正方法

1. `MainWindowRuntimeRefs` を機能別に分割する。
   - 例: `UiRuntimeRefs`, `GameRuntimeRefs`, `KifuRuntimeRefs`, `AnalysisRuntimeRefs`
2. `MainWindowServiceRegistry` 側 API を `buildRuntimeRefs()` 一括渡しから、機能別渡しへ段階移行する。
3. `MainWindow` 本体メンバの直接参照を減らし、`RegistryParts` 経由アクセスへ寄せる。
4. `friend class` を減らすため、`MainWindow` private 直接アクセスをラップ API に置き換える。
5. 1回で大規模変更せず、サブレジストリ単位で移行する。

### 受け入れ基準

- `mainWindowFriendClassLimit` の上限を 7 -> 6 へ下げても通る。
- `mainWindowIncludeCount` の上限を 37 -> 34 程度に下げても通る。
- 既存 70 テストがすべて通る。

---

## B. `UsiProtocolHandler` 再接続安全性（P0）

### 問題

- `setProcessManager()` で既存接続の解除なしに `connect` を追加している。
- 同インスタンスに対する再設定で `onDataReceived` が重複接続される余地がある。

根拠:

- `src/engine/usiprotocolhandler.cpp`（`setProcessManager`）

### 修正方法

1. `m_processManager` が既に設定済みの場合、旧オブジェクトとの接続を明示切断する。
2. 新規接続には `Qt::UniqueConnection` を使用する。
3. 同一ポインタ再設定時は早期 return する。
4. 単体テストを追加する。
   - 2回 `setProcessManager()` 呼び出し後、`dataReceived` 1回でハンドラ実行が1回であること。

### 受け入れ基準

- 再接続系テストが追加され、安定して通る。
- 既存 `tst_usiprotocolhandler` が回帰なし。

---

## C. `buildRuntimeRefs()` の分割（P1）

### 問題

- `buildRuntimeRefs()` が巨大で、多数ドメインの参照を同時に収集している。
- 変更時に関係のないレイヤまで差分が波及しやすい。

根拠:

- `src/app/mainwindow.cpp`（`buildRuntimeRefs`）

### 修正方法

1. `buildRuntimeRefs()` を 4〜6 個の private ヘルパに分割する。
   - `buildUiRefs()`
   - `buildModelRefs()`
   - `buildGameRefs()`
   - `buildKifuRefs()`
   - `buildAnalysisRefs()`
2. 各ヘルパに「null 許容ポリシー」をコメントで明記する。
3. 参照構築関数は副作用を持たない契約を明文化する。

### 受け入れ基準

- `buildRuntimeRefs()` 本体が「ヘルパ呼び出しのみ」に近い形になる。
- 既存テスト全通過。

---

## D. `MatchCoordinator` Hook 構築分離（P1）

### 問題

- `ensure*` メソッド内で `Refs` 構築と `Hooks` 構築と接続が混在している。
- `std::function` ラムダの密度が高く、依存の追跡が難しい。

根拠:

- `src/game/matchcoordinator.cpp`（`ensureEngineManager`, `ensureMatchTurnHandler`, `ensureAnalysisSession`）

### 修正方法

1. Hook 構築を専用関数へ分割する。
   - `buildEngineLifecycleHooks()`
   - `buildMatchTurnHooks()`
   - `buildAnalysisSessionHooks()`
2. `Refs` 構築と `connect()` を別関数に分ける。
3. 依存が null のときの fallback を統一ルール化する（`0` / `nullptr` / fail-fast）。

### 受け入れ基準

- `matchcoordinator.cpp` の関数単位複雑度が低下。
- `tst_matchcoordinator` / `tst_gamestrategy` / `tst_game_end_handler` が回帰なし。

---

## E. `KifuApplyService` の責務分離（P1）

### 問題

- `applyParsedResult()` が「検証」「変換」「モデル反映」「UI選択」「分岐ツリー反映」を一括処理している。
- 単一関数内で変更点が集中し、回帰を招きやすい。

根拠:

- `src/kifu/kifuapplyservice.cpp`（`applyParsedResult`）

### 修正方法

1. フェーズ関数へ分割する。
   - `validateParsedResult(...)`
   - `rebuildMainlineState(...)`
   - `rebuildPositionCommands(...)`
   - `applyToRecordView(...)`
   - `applyBranchTree(...)`
2. UI 副作用がない部分を pure 関数化し、`tests/` で直接検証可能にする。
3. パフォーマンスログはフェーズ関数単位で統一する。

### 受け入れ基準

- `kifuapplyservice.cpp` 行数を 584 -> 500 未満に削減。
- 棋譜ロード関連テスト（`tst_app_kifu_load`, `tst_kifu_comment_sync`, `tst_navigation`）が回帰なし。

---

## F. 大型ファイル削減（P2）

### 問題

- 500行超ファイル数が KPI 上限と同値（24）で、機能追加時にすぐ閾値超過する。

現状候補（上位）:

- `src/kifu/kifuapplyservice.cpp` (584)
- `src/dialogs/josekiwindowui.cpp` (581)
- `src/views/shogiview.cpp` (579)
- `src/engine/usiprotocolhandler.cpp` (579)
- `src/analysis/tsumeshogigenerator.cpp` (575)

### 修正方法

1. 影響の大きい順で 1ファイルずつ分割する（同時並行で複数分割しない）。
2. 分割軸は「入出力境界」で定義する。
   - UIイベント処理
   - ドメイン計算
   - 変換/フォーマット
3. 分割ごとに構造KPIテストを回し、閾値を引き下げる。

### 目標値（段階）

- `files_over_550`: 8 -> 6 -> 4
- `files_over_500`: 24 -> 20 -> 16

---

## G. 静的解析ゲートの段階強化（P2）

### 問題

- `.clang-tidy` で有効化しているチェックは多いが、`WarningsAsErrors` は限定的。
- 「検出はするが失敗にはしない」項目が多く、劣化を止めきれない可能性がある。

根拠:

- `.clang-tidy`

### 修正方法

1. 毎週 1カテゴリずつ `WarningsAsErrors` へ昇格する運用を導入。
2. 既存ノイズの多いチェックは局所 suppress ではなく、根本修正優先で消す。
3. CI に `-DENABLE_CLANG_TIDY=ON` ジョブを nightly 追加し、段階的に PR 必須化する。

### 受け入れ基準

- 1か月で `WarningsAsErrors` 項目を 2〜3カテゴリ拡大。
- 新規コードが未対応警告を増やさない。

---

## H. ドキュメント実測同期の自動化（P2）

### 問題

- テスト数などの運用値が文書間でズレやすい。
- 過去文書に古い本数が残ると、開発判断を誤る。

### 修正方法

1. `ctest -N` の結果を抽出して `docs/dev/test-summary.md` を更新するスクリプトを追加。
2. 他ドキュメントは本数を直接書かず、`test-summary.md` 参照へ統一する。
3. PR チェックに「テスト本数差分検知（任意警告）」を追加する。

### 受け入れ基準

- テスト本数の主データが 1か所に集約される。
- 本数不一致がレビュー時に検出できる。

---

## 5. 実施順序（推奨）

1. A: `MainWindow` 境界縮退（P0）
2. B: `UsiProtocolHandler` 再接続安全化（P0）
3. C: `buildRuntimeRefs` 分割（P1）
4. D: `MatchCoordinator` Hook 分離（P1）
5. E: `KifuApplyService` 分割（P1）
6. F: 大型ファイル削減（P2）
7. G: 静的解析ゲート強化（P2）
8. H: ドキュメント同期自動化（P2）

---

## 6. 実装時チェックリスト

各項目実施時は以下を必須にする。

1. `cmake --build build`
2. `ctest --test-dir build --output-on-failure`
3. 影響範囲テストを個別再実行（例: `-R usiprotocolhandler`）
4. `tests/tst_structural_kpi` の閾値見直し（改善時は下げる）
5. ドキュメント（必要なら `docs/dev/*`）を更新

---

## 7. 付録: まず着手する最小変更セット（1週間）

### Day 1-2

- `UsiProtocolHandler::setProcessManager()` 再接続安全化（B）
- 対応テスト追加

### Day 3-4

- `buildRuntimeRefs()` 分割（C）の最小版
- `MainWindow` include 数削減を 2〜3 件実施（A）

### Day 5

- `ctest` 全件
- KPI 閾値見直し（可能なら `mainWindowIncludeCount` を引き下げ）
- 実施ログを `docs/dev/done/` に追記

---

## 8. 変更履歴

- 2026-03-03: 初版作成
