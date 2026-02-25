# ShogiBoardQ 品質改善リファクタリング計画

`code-review-improvements.md` の改善提案に基づく実行計画。
1回のセッション（`/clear` 区切り）で完了可能な粒度に分割している。

> **既存の `refactoring-plan.md`** はクラス分解（EngineAnalysisTab, ShogiView, MatchCoordinator 等の巨大クラス分割）に焦点を当てた計画である。本計画はそれとは独立した品質面の改善（型安全性、テスト、CI、エラーハンドリング等）を対象とする。

---

## 依存関係と実行順

```
Phase 1: 安全網の構築（テスト・CI）    ← 他の全 Phase の前提
  ├─ Task 1: CI パイプライン構築                [独立]
  ├─ Task 2: USI プロトコルハンドラのテスト追加  [独立]
  ├─ Task 3: CSA/USI フォーマット変換テスト拡充  [独立]
  └─ Task 4: AnalysisFlowController テスト追加   [独立]

Phase 2: 型安全性の強化                ← Phase 1 完了後推奨
  ├─ Task 5: Turn enum の導入                    [独立]
  ├─ Task 6: SpecialMove enum の導入             [独立, Task 5 と並行可]
  └─ Task 7: Piece enum の導入（大規模）         [Task 5 に依存]
      ├─ Step 7a: Piece enum + 変換ユーティリティ作成
      ├─ Step 7b: ShogiMove の Piece 化
      ├─ Step 7c: ShogiBoard の Piece 化
      └─ Step 7d: フォーマット変換器の Piece 対応

Phase 3: エラーハンドリング・リソース管理  ← Phase 2 の後が望ましい
  ├─ Task 8: std::optional 導入（SFEN パース）   [Task 5 に依存]
  ├─ Task 9: フォーマット変換の入力バリデーション [Task 3 に依存]
  ├─ Task 10: unique_ptr 移行                    [独立]
  └─ Task 11: Q_ASSERT → ランタイムチェック      [独立]

Phase 4: アーキテクチャ改善            ← Phase 2, 3 完了後推奨
  ├─ Task 12: MainWindow 長大メソッドの分解      [独立]
  ├─ Task 13: SettingsService キー一元管理       [独立]
  ├─ Task 14: Strategy の friend 依存解消        [Task 2 に依存]
  └─ Task 15: 棋譜変換の共通ユーティリティ化     [Task 3, 9 に依存]

Phase 5: ビルド・ツーリング整備        ← 任意のタイミング
  ├─ Task 16: CMake ソース明示列挙               [独立]
  ├─ Task 17: ヘッダ依存の削減                   [Phase 4 後推奨]
  └─ Task 18: ロギングカテゴリの一元管理         [独立]

Phase 6: シグナル/スロット整理         ← Phase 4 完了後推奨
  ├─ Task 19: デッドシグナル/スロット棚卸し      [独立]
  └─ Task 20: UiActionsWiring の機能別分割       [Task 19 に依存]

Phase 7: コード品質改善（Task 21 観察に基づく）← 任意のタイミング
  ├─ Task 22: ビルド警告ゼロ化                   [独立, 最優先]
  ├─ Task 23: 所有権の明確化・unique_ptr 移行    [独立]
  ├─ Task 24: MainWindow 責務分割               [Task 23 後推奨]
  ├─ Task 25: lambda connect の解消             [独立]
  ├─ Task 26: パーサーの複雑さ低減              [独立]
  └─ Task 27: エラーハンドリングの一貫性向上     [独立]
```

---

## Phase 1: 安全網の構築

以降のリファクタリングの安全性を担保するため、テストとCIを先に整備する。

### Task 1: CI パイプライン構築

| 項目 | 内容 |
|------|------|
| 対象 | `.github/workflows/ci.yml`（新規） |
| 参照 | §8.1, §8.7 |
| 規模 | 小（1ファイル新規作成） |
| 依存 | なし |

**ゴール**:
- Ubuntu + Qt6 環境でのビルド + テスト（`-DBUILD_TESTING=ON` + `ctest`）
- clang-tidy の警告をチェック（`-DENABLE_CLANG_TIDY=ON`）
- 翻訳の未翻訳件数をサマリ出力

**プロンプト**: → `prompts/task-01-ci.md`

---

### Task 2: USI プロトコルハンドラのテスト追加

| 項目 | 内容 |
|------|------|
| 対象 | `tests/tst_usiprotocolhandler.cpp`（新規） |
| 参照 | §4 |
| 規模 | 中 |
| 依存 | なし |

**ゴール**:
- `info` 行の解析テスト（depth, score cp, score mate, pv, multipv）
- `bestmove` 行の解析テスト（通常手、ponder 付き、resign、win）
- 不正入力に対する耐性テスト
- テスト用スタブ/モックを最小限で構成

**プロンプト**: → `prompts/task-02-usi-tests.md`

---

### Task 3: CSA/USI フォーマット変換テスト拡充

| 項目 | 内容 |
|------|------|
| 対象 | `tests/tst_csaconverter.cpp`、`tests/tst_usiconverter.cpp`（既存拡充） |
| 参照 | §4 |
| 規模 | 小〜中 |
| 依存 | なし |

**ゴール**:
- CSA: 駒打ち、成り、不成り、不正入力のテスト追加
- USI: 駒打ち（`P*5e`）、成り（`7g7f+`）、不正入力のテスト追加
- 各テスト5件以上の追加

**プロンプト**: → `prompts/task-03-converter-tests.md`

---

### Task 4: AnalysisFlowController テスト追加

| 項目 | 内容 |
|------|------|
| 対象 | `tests/tst_analysisflow.cpp`（新規） |
| 参照 | §4 |
| 規模 | 中 |
| 依存 | なし |

**ゴール**:
- 解析開始→進行→停止の状態遷移テスト
- エラー発生時の状態回復テスト

**プロンプト**: → `prompts/task-04-analysis-tests.md`

---

## Phase 2: 型安全性の強化

### Task 5: Turn enum の導入

| 項目 | 内容 |
|------|------|
| 対象 | `src/core/` 全体、`src/game/turnmanager.h/.cpp` |
| 参照 | §2-b |
| 規模 | 中（広範囲だが機械的置換が多い） |
| 依存 | Phase 1 完了推奨 |

**ゴール**:
- `enum class Turn { Black, White }` を定義
- `ShogiBoard::m_currentPlayer` を `QString` → `Turn` に変更
- SFEN 相互変換メソッド（`Turn::Black ↔ "b"`）を用意
- 全既存テスト通過

**プロンプト**: → `prompts/task-05-turn-enum.md`

---

### Task 6: SpecialMove enum の導入

| 項目 | 内容 |
|------|------|
| 対象 | `src/engine/usiprotocolhandler.cpp`、関連ファイル |
| 参照 | §2-c |
| 規模 | 小 |
| 依存 | なし（Task 5 と並行可能） |

**ゴール**:
- `enum class SpecialMove { None, Resign, Win, Draw }` を定義
- 文字列比較を enum に置換
- 全既存テスト通過

**プロンプト**: → `prompts/task-06-specialmove-enum.md`

---

### Task 7: Piece enum の導入（大規模）

| 項目 | 内容 |
|------|------|
| 対象 | `src/core/shogimove.h`、`src/core/shogiboard.h/.cpp`、広範囲 |
| 参照 | §2-a |
| 規模 | 大（4セッションに分割） |
| 依存 | Task 5 完了後 |

**Step 7a**（1セッション目）: Piece enum + QChar 変換ユーティリティ作成（既存コード変更なし）
**Step 7b**（2セッション目）: ShogiMove の Piece 化
**Step 7c**（3セッション目）: ShogiBoard の Piece 化
**Step 7d**（4セッション目）: フォーマット変換器の Piece 対応

**プロンプト**: → `prompts/task-07a-piece-enum.md` 〜 `prompts/task-07d-piece-converters.md`

---

## Phase 3: エラーハンドリング・リソース管理

### Task 8: std::optional 導入（SFEN パース）

| 項目 | 内容 |
|------|------|
| 対象 | `src/core/shogiboard.h/.cpp` |
| 参照 | §3-a |
| 規模 | 小〜中 |
| 依存 | Task 5（Turn enum 導入後） |

**プロンプト**: → `prompts/task-08-optional-sfen.md`

---

### Task 9: フォーマット変換の入力バリデーション強化

| 項目 | 内容 |
|------|------|
| 対象 | `src/kifu/formats/usitosfenconverter.cpp` 等 |
| 参照 | §3-b |
| 規模 | 小 |
| 依存 | Task 3（テストが先に存在すること） |

**プロンプト**: → `prompts/task-09-validation.md`

---

### Task 10: unique_ptr 移行

| 項目 | 内容 |
|------|------|
| 対象 | 明示的 `delete` を使用している全ファイル |
| 参照 | §6 |
| 規模 | 小 |
| 依存 | なし |

**プロンプト**: → `prompts/task-10-unique-ptr.md`

---

### Task 11: Q_ASSERT → ランタイムチェック

| 項目 | 内容 |
|------|------|
| 対象 | `Q_ASSERT` 使用箇所全体 |
| 参照 | §3-c |
| 規模 | 小 |
| 依存 | なし |

**プロンプト**: → `prompts/task-11-runtime-checks.md`

---

## Phase 4: アーキテクチャ改善

### Task 12: MainWindow 長大メソッドの分解

| 項目 | 内容 |
|------|------|
| 対象 | `src/app/mainwindow.cpp` |
| 参照 | §1-a |
| 規模 | 中 |
| 依存 | Phase 2 完了推奨 |

**プロンプト**: → `prompts/task-12-mainwindow-methods.md`

---

### Task 13: SettingsService キー一元管理

| 項目 | 内容 |
|------|------|
| 対象 | `src/services/settingsservice.h/.cpp` |
| 参照 | §8.4 |
| 規模 | 中 |
| 依存 | なし |

**プロンプト**: → `prompts/task-13-settings-keys.md`

---

### Task 14: Strategy の friend 依存解消

| 項目 | 内容 |
|------|------|
| 対象 | `src/game/matchcoordinator.h/.cpp`、Strategy 各クラス |
| 参照 | §8.5 |
| 規模 | 中〜大 |
| 依存 | Task 2（テストが安全網） |

**プロンプト**: → `prompts/task-14-strategy-context.md`

---

### Task 15: 棋譜変換の共通ユーティリティ化

| 項目 | 内容 |
|------|------|
| 対象 | `src/kifu/formats/` 配下全体 |
| 参照 | §8.6 |
| 規模 | 中 |
| 依存 | Task 3, 9 完了後 |

**プロンプト**: → `prompts/task-15-notation-utils.md`

---

## Phase 5: ビルド・ツーリング整備

### Task 16: CMake ソース明示列挙への移行

| 項目 | 内容 |
|------|------|
| 対象 | `CMakeLists.txt` |
| 参照 | §8.2 |
| 規模 | 中 |
| 依存 | なし |

**プロンプト**: → `prompts/task-16-cmake-sources.md`

---

### Task 17: ヘッダ依存の削減

| 項目 | 内容 |
|------|------|
| 対象 | `src/app/mainwindow.h` 中心 |
| 参照 | §8.3 |
| 規模 | 中 |
| 依存 | Phase 4 完了後推奨 |

**プロンプト**: → `prompts/task-17-header-deps.md`

---

### Task 18: ロギングカテゴリの一元管理

| 項目 | 内容 |
|------|------|
| 対象 | 全モジュールの `Q_LOGGING_CATEGORY` |
| 参照 | §7-a |
| 規模 | 小〜中 |
| 依存 | なし |

**プロンプト**: → `prompts/task-18-log-categories.md`

---

## Phase 6: シグナル/スロット整理

### Task 19: デッドシグナル/スロット棚卸し・削除

| 項目 | 内容 |
|------|------|
| 対象 | コードベース全体 |
| 参照 | §5-b |
| 規模 | 中 |
| 依存 | Phase 4 完了後推奨 |

**プロンプト**: → `prompts/task-19-dead-signals.md`

---

### Task 20: UiActionsWiring の機能別分割

| 項目 | 内容 |
|------|------|
| 対象 | `src/ui/wiring/uiactionswiring.h/.cpp` |
| 参照 | §5-a |
| 規模 | 小〜中 |
| 依存 | Task 19 完了後 |

**プロンプト**: → `prompts/task-20-split-actions-wiring.md`

---

## 推定効果まとめ

| Phase | 主な改善 | 効果 |
|-------|---------|------|
| 1 | テスト + CI | リグレッション防止の基盤 |
| 2 | 型安全性 | コンパイル時バグ検出、自己文書化 |
| 3 | エラーハンドリング + RAII | 不正入力耐性、メモリリーク防止 |
| 4 | アーキテクチャ | 保守性向上、責務明確化 |
| 5 | ビルド整備 | ビルド時間短縮、変更追跡性 |
| 6 | シグナル整理 | デバッグ容易性 |
