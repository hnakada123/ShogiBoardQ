# 包括的改善タスク一覧（task20260228-19〜32）

本インデックスは `docs/dev/comprehensive-improvement-analysis-2026-02-28.md` の改善項目を
実行可能なタスクに分解したものである。既存タスク（00〜18）を補完する。

## 出典との対応

| 出典 | 対応タスク |
|---|---|
| 包括的改善分析 §5.2.1 `[[nodiscard]]` | Task 19 |
| 包括的改善分析 §9.2.1 コードカバレッジ | Task 20 |
| 包括的改善分析 §9.2.4 Sanitizer cron | Task 21 |
| 包括的改善分析 §3.4 配線契約テスト | Task 22 |
| 包括的改善分析 §6.3 ErrorBus severity | Task 23 |
| 包括的改善分析 §3.4 MatchCoordinator テスト | Task 24 |
| 包括的改善分析 §3.4 Strategy群テスト | Task 25 |
| future-improvement テーマB + §2.4 | Task 26 |
| 包括的改善分析 §5.2.2 std::optional | Task 27 |
| 包括的改善分析 §11.2 Widget層 | Task 28 |
| ISSUE-060〜063 + §11.2 | Task 29 |
| 包括的改善分析 §5.2.4 QStringView | Task 30 |
| 包括的改善分析 §10.2 アーキテクチャ図 | Task 31 |
| 包括的改善分析 §10.2 パターンドキュメント | Task 32 |

---

## Phase 1: ガードレール強化（Week 1-2）

低コスト・即効果のクイックウィン。コード品質の自動検知基盤を整備。

| # | タスク | 工数 | 優先度 | ファイル |
|---|--------|------|--------|----------|
| 19 | `[[nodiscard]]` 一括追加 | 小 | P1 | [task20260228-19-nodiscard.md](task20260228-19-nodiscard.md) |
| 20 | コードカバレッジCI導入 | 小 | P1 | [task20260228-20-ci-code-coverage.md](task20260228-20-ci-code-coverage.md) |
| 21 | Sanitizer定期実行化 | 小 | P2 | [task20260228-21-ci-sanitizer-cron.md](task20260228-21-ci-sanitizer-cron.md) |

**Phase 1 完了条件:**
- `[[nodiscard]]` が主要関数20件以上に付与
- CI にカバレッジレポートが表示
- Sanitizer ジョブが週次cron実行

---

## Phase 2: テスト基盤拡充（Week 3-6）

テストカバレッジの拡大とエラーハンドリングの改善。

| # | タスク | 工数 | 優先度 | ファイル |
|---|--------|------|--------|----------|
| 22 | 配線契約テスト追加 | 中 | P1 | [task20260228-22-wiring-contract-tests.md](task20260228-22-wiring-contract-tests.md) |
| 23 | ErrorBus severity追加 | 小 | P2 | [task20260228-23-errorbus-severity.md](task20260228-23-errorbus-severity.md) |
| 24 | MatchCoordinator テスト | 中 | P1 | [task20260228-24-matchcoordinator-tests.md](task20260228-24-matchcoordinator-tests.md) |
| 25 | Strategy群テスト | 中 | P2 | [task20260228-25-strategy-tests.md](task20260228-25-strategy-tests.md) |

**Phase 2 完了条件:**
- テスト数 46 → 50 以上
- ErrorBus にレベル区分導入
- MatchCoordinator の主要フローがテストされている

---

## Phase 3: 大型ファイル削減・C++17活用（Week 7-10）

600行超ファイルの削減と、モダンC++機能の活用拡大。

| # | タスク | 工数 | 優先度 | ファイル |
|---|--------|------|--------|----------|
| 26 | csagamecoordinator 分割 | 中 | P1 | [task20260228-26-split-csagamecoordinator.md](task20260228-26-split-csagamecoordinator.md) |
| 27 | `std::optional` 拡大 | 中 | P2 | [task20260228-27-std-optional-expansion.md](task20260228-27-std-optional-expansion.md) |

**Phase 3 完了条件:**
- `csagamecoordinator.cpp` が 600行以下
- `std::optional` 使用数が 20 以上

---

## Phase 4: 品質仕上げ（Week 11-16）

Widget層の整理、ダイアログ共通化、ドキュメント整備。

| # | タスク | 工数 | 優先度 | ファイル |
|---|--------|------|--------|----------|
| 28 | Widget層ロジック抽出 | 大 | P2 | [task20260228-28-widget-logic-extraction.md](task20260228-28-widget-logic-extraction.md) |
| 29 | ダイアログ重複共通化 | 中 | P2 | [task20260228-29-dialog-dedup.md](task20260228-29-dialog-dedup.md) |
| 30 | QStringView 拡大 | 中 | P3 | [task20260228-30-qstringview-expansion.md](task20260228-30-qstringview-expansion.md) |
| 31 | アーキテクチャ概観図作成 | 小 | P2 | [task20260228-31-architecture-overview.md](task20260228-31-architecture-overview.md) |
| 32 | Deps/Hooks/Refs ドキュメント | 小 | P3 | [task20260228-32-deps-hooks-documentation.md](task20260228-32-deps-hooks-documentation.md) |

**Phase 4 完了条件:**
- 600行超ファイル 32 → 28 以下
- ダイアログ共通パターン使用率 20%以上
- アーキテクチャドキュメント完備

---

## 既存タスク（00〜18）との関係

既に `done/` に配置済みの Task 00〜18 は主に以下をカバー:

| 範囲 | タスク |
|---|---|
| ベースライン計測 | 00 |
| 巨大ファイル分割 (1000行超) | 01-06 |
| app司令塔層の分析・分割 | 07-09 |
| 所有権整理 | 10 |
| テスト拡充（フロー系） | 11-13 |
| CI強化 | 14-15 |
| 棋譜変換器共通化 | 16 |
| プロトコルエラー統一 | 17 |
| 異常系テスト | 18 |

本インデックスの Task 19〜32 は、これらを補完し、残りの改善領域をカバーする。
