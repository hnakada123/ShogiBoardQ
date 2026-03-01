# 今後の改善点 詳細分析（2026-03-01）

## 1. 目的

本書は、ShogiBoardQ の現行コードベースに対して「今後どこに改善投資すべきか」を、
実測値ベースで整理した実行計画ドキュメントである。

- 対象: 保守性、設計健全性、品質保証、CI 運用
- 非対象: 新機能仕様、UI デザイン刷新、配布戦略の全面変更

---

## 2. 現状ベースライン（2026-03-01 実測）

### 2.1 規模・テスト・構造KPI

| 指標 | 実測値 |
|---|---:|
| `src` 配下 `.cpp/.h` ファイル数 | 565 |
| `src` 配下 総行数 | 104,996 |
| 600行超ファイル数 | 15 |
| 800行超ファイル数 | 2 |
| 1000行超ファイル数 | 0 |
| ctest 登録テスト数 | 55 |
| ctest 実行結果 | 55/55 pass |
| `MainWindow` friend class 数 | 5（上限 5） |
| `MainWindow` public slots 数 | 13（上限 15） |
| `MainWindow` ensure* 数 | 0（上限 0） |
| `MainWindowServiceRegistry` ensure* 数 | 11（上限 11） |
| `delete` 文数（実コード） | 1（`src/widgets/flowlayout.cpp`） |
| lambda `connect` 数 | 0 |
| 旧式 `SIGNAL/SLOT` 数 | 0 |

補足:

- `tst_structural_kpi` と `tst_layer_dependencies` は pass。
- 量的品質は高水準だが、複雑性が一部ファイルへ集中している。

### 2.2 モジュール別規模（`src/*` 集計）

| モジュール | ファイル数 | 行数 | 600行超 |
|---|---:|---:|---:|
| `kifu` | 89 | 18,226 | 1 |
| `ui` | 96 | 16,247 | 1 |
| `dialogs` | 47 | 11,473 | 2 |
| `game` | 51 | 10,175 | 1 |
| `widgets` | 54 | 9,833 | 1 |
| `app` | 73 | 9,058 | 1 |
| `engine` | 24 | 5,870 | 3 |
| `core` | 25 | 4,677 | 2 |
| `analysis` | 20 | 4,675 | 1 |
| `views` | 13 | 4,062 | 0 |
| `services` | 33 | 3,237 | 0 |
| `network` | 10 | 3,079 | 1 |

### 2.3 600行超ファイル一覧（要重点管理）

| 行数 | include数 | 関数数（概算） | ファイル |
|---:|---:|---:|---|
| 930 | 4 | 14 | `src/core/fmvlegalcore.cpp` |
| 822 | 3 | 33 | `src/core/shogiboard.cpp` |
| 781 | 7 | 64 | `src/engine/usi.cpp` |
| 720 | 16 | 37 | `src/ui/coordinators/kifudisplaycoordinator.cpp` |
| 714 | 14 | 44 | `src/engine/usiprotocolhandler.cpp` |
| 697 | 43 | 22 | `src/app/gamesubregistry.cpp` |
| 693 | 2 | 30 | `src/network/csaclient.cpp` |
| 689 | 8 | 44 | `src/engine/shogiengineinfoparser.cpp` |
| 676 | 19 | 14 | `src/analysis/analysisflowcontroller.cpp` |
| 657 | 32 | 23 | `src/dialogs/tsumeshogigeneratordialog.cpp` |
| 654 | 28 | 24 | `src/kifu/kifuexportcontroller.cpp` |
| 642 | 7 | 21 | `src/navigation/kifunavigationcontroller.cpp` |
| 640 | 16 | 63 | `src/game/matchcoordinator.cpp` |
| 638 | 14 | 54 | `src/dialogs/startgamedialog.cpp` |
| 635 | 15 | 36 | `src/widgets/evaluationchartwidget.cpp` |

---

## 3. 現状評価（要点）

### 3.1 強い点

1. **品質ゲートが制度化されている**  
   構造KPIと依存方向テストが継続運用され、設計の崩壊を検知できる。
2. **MainWindow の薄型化が進んでいる**  
   LifecyclePipeline と Registry/SubRegistry への責務移譲が進行済み。
3. **テスト実行コストが低く、回しやすい**  
   55 テストを短時間で回せるため、日次の回帰検知に向く。

### 3.2 主要リスク

1. **複雑性の局所集中**  
   15 ファイルに大型化が残り、変更時の影響範囲予測が難しい。
2. **アプリ層の結合密度**  
   `MainWindow` と `GameSubRegistry` 周辺は依存が密で、抽象境界が崩れやすい。
3. **「閾値到達型」運用の行き詰まり**  
   friend/ensure など一部KPIが上限に近く、追加開発時に設計悪化を起こしやすい。

---

## 4. 改善テーマ詳細（優先順）

### 4.1 テーマA: 大型ファイル分割の第2フェーズ（最優先）

### 課題

- 600行超が 15 件残存。
- コア処理、プロトコル処理、UIオーケストレーションに混在責務がある。

### 改善方針

1. `core` 分割  
   `fmvlegalcore.cpp` を「生成」「合法性フィルタ」「打ち/成り判定」に分解する。  
   `shogiboard.cpp` を「盤面状態」「SFEN変換」「駒台処理」に分割する。
2. `engine/network` 分割  
   `usi.cpp`/`usiprotocolhandler.cpp` を「送受信」「状態遷移」「コマンド解釈」に分割する。
3. `game/ui` 分割  
   `matchcoordinator.cpp` と `kifudisplaycoordinator.cpp` をユースケース単位で抽出する。

### 完了条件（KPI）

- 600行超: `15 -> 9`（12週間）
- 800行超: `2 -> 0`（12週間）
- 600行超ファイルのうち `include >= 20` を持つもの: `3 -> 1` 以下

### 想定工数

- 中〜高（分割後の配線修正とテスト補強が必要）

---

### 4.2 テーマB: App層の結合点削減（最優先）

### 課題

- `MainWindow` はスリム化したが、依存メンバは依然多い。  
  - raw pointer 風メンバ: 42  
  - `std::unique_ptr` メンバ: 30  
  - `QPointer` メンバ: 6
- `GameSubRegistry` が 697 行、`#include` 43 と密結合。

### 改善方針

1. `MainWindowRuntimeRefs` を用途別に分割  
   `GameRuntimeRefs` / `KifuRuntimeRefs` / `UiRuntimeRefs` へ段階移行。
2. `GameSubRegistry` の再分割  
   - `GameSessionSubRegistry`
   - `GameWiringSubRegistry`
   - `GameStateSubRegistry`
3. `MainWindow` からの直接書き換え箇所を最小化  
   直接代入を Service API 経由へ置換し、可変状態の更新窓口を絞る。

### 完了条件（KPI）

- `gamesubregistry.cpp`: `697 -> 450` 以下
- `mainwindow.h` raw pointer 風メンバ: `42 -> 32`
- `MainWindow` friend class: `5 -> 4`（中期）

### 想定工数

- 高（影響範囲が広い）

---

### 4.3 テーマC: レジストリAPIの成長戦略（高優先）

### 課題

- `MainWindowServiceRegistry` の `ensure*` は 11（上限 11）で余白ゼロ。
- 実体はサブレジストリに分割済みだが、今後の追加余地が小さい。

### 改善方針

1. `ensure*` を「生成」と「依存更新」に分離  
   `ensureX()` + `refreshXDeps()` の2段構造を標準化。
2. 境界仕様を `docs` とテストで固定  
   「どのレイヤが何を `ensure` してよいか」を機械検証可能にする。
3. `ServiceRegistry` 直下メソッドをさらに削減  
   直接呼び出しは `orchestration` のみに限定。

### 完了条件（KPI）

- `MainWindowServiceRegistry` の公開 `ensure*`: 11 を維持しつつ新規追加ゼロ
- `GameSubRegistry`/`KifuSubRegistry` で依存更新APIを明示化

### 想定工数

- 中

---

### 4.4 テーマD: 所有権・寿命管理ルールの形式化（高優先）

### 課題

- `delete` は 1 件で健全だが、`new` の生成点が多数あり暗黙理解に依存する。
- QObject親子管理と `unique_ptr` 混在のため、寿命境界が追跡しづらい箇所がある。

### 改善方針

1. ルール表を `ownership-guidelines.md` に追記  
   - QObject 親所有
   - `unique_ptr` 所有
   - 非所有参照（`QPointer`/raw）  
   を判定フロー化。
2. 再生成可能オブジェクトへ寿命注釈を必須化  
   `Created once / Recreated / Destroyed by ...` をコメント規約として統一。
3. `QPointer` の採用基準を明確化  
   非所有かつ遅延破棄対象の参照は `QPointer` 優先。

### 完了条件（KPI）

- 明示的 `delete`: `<= 1` を維持
- 再生成対象の寿命注釈: 100%
- 新規追加クラスで所有権コメント未記載: 0

### 想定工数

- 低〜中

---

### 4.5 テーマE: 異常系テストと長時間回帰の強化（高優先）

### 課題

- 55 テストは強いが、異常系と長時間シナリオの比率は今後の改善余地がある。
- 通信、I/O、再初期化シーケンスは将来変更で壊れやすい。

### 改善方針

1. 異常系テストを追加  
   - 破損SFEN/破損棋譜入力
   - CSA切断・USI応答欠損
   - 設定ロード失敗時フォールバック
2. 長時間シナリオテストを追加  
   起動->対局->保存->リセット->再対局を反復するテスト。
3. App層の契約テストを追加  
   `MainWindowLifecyclePipeline` と SubRegistry の初期化順保証を固定化。

### 完了条件（KPI）

- 追加テスト: +10 以上（うち異常系 +6 以上）
- 長時間シナリオテスト: 1 本以上
- 失敗時ログの診断性向上（原因ステップが判別可能）

### 想定工数

- 中

---

### 4.6 テーマF: CIゲートの実運用強化（中優先）

### 課題

- 現在のCIは十分強いが、運用面で改善余地がある。
- 翻訳閾値 (`THRESHOLD=1202`) は高く、実質的に退行抑止が弱い。

### 改善方針

1. 翻訳チェックを「固定高閾値」から「ベースライン回帰禁止」へ変更。
2. `sanitize` ジョブ結果の可観測性強化  
   定期ジョブ結果を artifact で履歴化する。
3. KPI推移を時系列で比較可能にする  
   `kpi-results.json` の差分を Step Summary で自動表示。

### 完了条件（KPI）

- 翻訳未完了件数: 増加を常時禁止
- Sanitizer の定期実行継続率: 100%
- KPI差分の自動可視化: 導入完了

### 想定工数

- 低〜中

---

## 5. 12週間ロードマップ

### フェーズ1（Week 1-4）

1. `gamesubregistry.cpp` 分割開始（最優先）
2. `matchcoordinator.cpp` のユースケース抽出
3. 異常系テスト 4 件追加
4. 翻訳チェック方針のCI改修

目標:

- 600行超: `15 -> 13`
- 主要分割PR: 2本以上

### フェーズ2（Week 5-8）

1. `fmvlegalcore.cpp` / `shogiboard.cpp` 分割
2. RuntimeRefs の用途別分離
3. 長時間シナリオテスト導入

目標:

- 600行超: `13 -> 11`
- `gamesubregistry.cpp`: 550行以下

### フェーズ3（Week 9-12）

1. `usi.cpp` / `usiprotocolhandler.cpp` 分割
2. `kifuexportcontroller.cpp` / `analysisflowcontroller.cpp` 再編
3. 所有権注釈の全体適用

目標:

- 600行超: `11 -> 9`
- 800行超: `2 -> 0`

---

## 6. 直近2週間の着手タスク（PR単位）

1. `GameSubRegistry` を 3 ファイルへ分割し、挙動不変のまま移動のみ実施
2. `MatchCoordinator` の `start/end/turn` ハンドラ分離（`matchturnhandler` 方向へ寄せる）
3. `tst_app_error_handling` に設定破損・ロード失敗ケースを追加
4. `ci.yml` の translation-check を「前回値以上なら fail」へ変更
5. `ownership-guidelines.md` に所有権判定表とレビュー項目を追加

---

## 7. 優先度マトリクス

| テーマ | 効果 | 工数 | リスク低減 | 優先度 |
|---|---|---|---|---|
| A: 大型ファイル分割 | 高 | 高 | 高 | P1 |
| B: App層結合点削減 | 高 | 高 | 高 | P1 |
| C: レジストリ成長戦略 | 中〜高 | 中 | 高 | P1 |
| D: 所有権形式化 | 中 | 低〜中 | 中 | P1 |
| E: 異常系テスト強化 | 中〜高 | 中 | 高 | P1 |
| F: CI運用強化 | 中 | 低〜中 | 中 | P2 |

---

## 8. 結論

ShogiBoardQ は既に高品質だが、次の改善余地は「量」ではなく「結合密度と責務境界」に集中している。
特に `app/game/core/engine` の上位大型ファイル群を分割し、所有権と初期化契約を明文化することで、
将来の機能追加時にも設計品質を維持しやすくなる。

現時点で最も効果が高い順は以下。

1. `gamesubregistry.cpp` / `matchcoordinator.cpp` の分割
2. `fmvlegalcore.cpp` / `shogiboard.cpp` の責務分離
3. 異常系テストとCI回帰検知（翻訳・KPI差分）の強化
