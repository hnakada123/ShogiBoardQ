# 今後の改善点 詳細分析（2026-02-28）

## 1. 目的

この文書は、ShogiBoardQ の現行コードベースに対して「次の改善投資先」を明確化するための分析レポートである。  
対象は機能追加ではなく、保守性・変更容易性・品質保証コストの最適化である。

---

## 2. 現状スナップショット（実測）

### 2.1 計測値

| 指標 | 実測値 | 補足 |
|---|---:|---|
| `src/` 内 `.cpp/.h` ファイル数 | 518 | `find src ... | wc -l` |
| `src/` 総行数 | 103,800 | `wc -l` 合計 |
| 600行超ファイル数 | 33 | 構造KPI監視対象 |
| 800行超ファイル数 | 14 | 主要リファクタ対象候補 |
| 1000行超ファイル数 | 3 | `analysisflowcontroller.cpp`, `usi.cpp`, `evaluationchartwidget.cpp` |
| テスト数 | 41 | `ctest -N` |
| テスト結果 | 41/41 Pass | `ctest --output-on-failure` |
| `delete` 文 | 5 | `tst_structural_kpi` 実行ログ |
| lambda `connect` | 0 | `tst_structural_kpi` 実行ログ |
| 旧式 `SIGNAL/SLOT` | 0 | `tst_structural_kpi` 実行ログ |
| `MainWindow` friend class | 2 | 構造KPI上限内 |
| `MainWindow` public slots | 13 | 構造KPI上限内 |
| `MainWindowServiceRegistry` の `ensure*` | 50 | 遅延初期化ポイント数 |
| `MainWindow` の生ポインタメンバ | 47 | `m_` メンバのうち `*` |
| `MainWindow` の `std::unique_ptr` メンバ | 31 | 同上 |

### 2.2 モジュール別行数（上位）

| モジュール | 行数 |
|---|---:|
| `kifu` | 18,087 |
| `ui` | 15,802 |
| `dialogs` | 12,468 |
| `game` | 9,895 |
| `widgets` | 9,594 |
| `app` | 8,704 |
| `engine` | 5,582 |
| `core` | 4,686 |

### 2.3 600行超ファイルの偏在

| モジュール | 件数 |
|---|---:|
| `dialogs` | 6 |
| `widgets` | 5 |
| `kifu` | 5 |
| `views` | 3 |
| `game` | 3 |
| `engine` | 3 |
| その他 | 8 |

### 2.4 所見（現状）

- 品質ゲート（構造KPI、層依存テスト、CIでのclang-tidy/翻訳チェック）が整備されている。
- `MainWindow` の責務移譲は進んでいるが、依存の集約点としては依然大きい。
- ボトルネックは「巨大ファイル群」と「オーケストレーション層の複雑化」に集中している。

---

## 3. 改善テーマ別の詳細分析

## 3.1 テーマA: 巨大ファイル分割の第二段階

### 問題の本質

- 1000行超は3件まで減っているが、600行超が33件残存し、変更の局所性がまだ弱い。
- 特に `dialogs/widgets/kifu` に肥大が偏っており、UIイベント・状態更新・I/Oの混在が起きやすい。

### 優先対象（順）

1. `src/analysis/analysisflowcontroller.cpp` (1133)
2. `src/engine/usi.cpp` (1084)
3. `src/widgets/evaluationchartwidget.cpp` (1018)
4. `src/kifu/kifuexportcontroller.cpp` (933)
5. `src/core/fmvlegalcore.cpp` (930)
6. `src/engine/usiprotocolhandler.cpp` (902)

### 推奨アプローチ

- 分割単位を「ユースケース（入力/判断/反映）」で切る。
- 「状態更新」「表示反映」「外部I/O」を最低3層に分解する。
- 構造KPI例外値は、分割ごとに実測へ追従して即時更新する。

### KPI

- 1000行超: `3 -> 1`（最終的に `0` を目標）
- 600行超: `33 -> 24`（第1段階）

---

## 3.2 テーマB: MainWindow周辺の依存密度低減

### 問題の本質

- `MainWindow` はメソッド本体を薄く保てているが、依存ハブとしての複雑さは依然大きい。
- 生ポインタ47、`ensure*` 50、Registry分割ファイル群という形で「認知負荷」が残っている。

### 改善方針

- `MainWindowServiceRegistry` の `ensure*` をドメイン単位の子レジストリに再分割する。
- `MainWindowRuntimeRefs` をさらに用途別に分け、参照セットを最小化する。
- `MainWindow` から直接触る mutable state を段階的に専用Contextへ移管する。

### 具体施策

1. `GameContext`, `KifuContext`, `UiContext` を導入し、deps更新単位を縮小。
2. `ensure*` 呼び出しチェーンを可視化（依存グラフ化）し、循環リスクを監視。
3. `MainWindow` で直接保持する非UI依存を削減。

### KPI

- `MainWindow` 生ポインタメンバ: `47 -> 35`
- `MainWindowServiceRegistry` の `ensure*`: `50 -> 35`

---

## 3.3 テーマC: 所有権モデルの最終統一

### 問題の本質

- `delete` 文は5まで削減済みだが、破棄順序依存が強い箇所では潜在的な保守リスクが残る。
- `QObject` 親子管理と手動解放の境界が暗黙の箇所がある。

### 改善方針

- 例外的に `delete` が必要な箇所のみ、理由コメントを必須化。
- 非 `QObject` は `std::unique_ptr`、`QObject` は parent ownership を原則化。
- ライフサイクル境界は `Lifecycle/Pipeline` 系クラスへ隔離。

### KPI

- `delete` 文: `5 -> 2`
- 例外 `delete` 箇所のコメント付与率: `100%`

---

## 3.4 テーマD: テスト戦略のギャップ補完

### 問題の本質

- 41テストで基盤は強いが、`MainWindow` / appオーケストレーション層の振る舞い検証は薄い。
- 構造テストはある一方、結線変更時の「意図したUI連鎖」を保証するテストが不足しやすい。

### 改善方針

- app層のシナリオテストを追加（起動、対局開始、棋譜ロード、終了）する。
- `Wiring` 単位で「シグナル入力 -> 期待スロット発火」の契約テストを追加する。
- 回帰頻度の高いフローを優先して fixture 化する。

### KPI

- app層テストを `+6` 追加（最小）
- 重要フロー（開始/終了/ロード/分岐移動）の回帰ケースを全て自動化

---

## 3.5 テーマE: CI品質ゲートの拡張（高コスト障害の予防）

### 現状

- CIは Build/Test/Static Analysis/Translation を実施済み。
- ただしランタイム不具合（UB、メモリ境界、データ競合）を直接拾うゲートは限定的。

### 改善方針

- 週次またはPRラベル起動で `ASan/UBSan` ジョブを追加する。
- 主要フローの長時間試験（耐久）を nightly で実行する。
- `tst_structural_kpi` の結果を時系列で保存し、改善トレンドを可視化する。

### KPI

- Sanitizerジョブ導入: 1本以上
- Nightly長時間テスト: 1本以上
- KPI推移グラフの自動生成: 導入

---

## 3.6 テーマF: 棋譜・通信系の仕様境界整理

### 問題の本質

- `kifu/formats` と `engine/network` の高密度領域は、仕様追加時に横断影響が出やすい。
- 変換器・通信処理で「入力検証」「変換」「副作用」が同居しやすい。

### 改善方針

- 変換層を `Parse -> Normalize -> Apply` に固定化する。
- 異常系（欠損、不正手、境界値）のテーブル駆動テストを拡充する。
- プロトコル境界（USI/CSA）のエラーコード方針を統一する。

### KPI

- `kifu/formats` の重複処理削減（重複関数/分岐を20%以上削減）
- 仕様境界の異常系ケースを最低30件追加

---

## 4. 優先度付きロードマップ（3か月）

## Phase 1（1-3週）

1. 1000行超3ファイルのうち1本を分割完了
2. app層シナリオテスト2本追加
3. `delete` 例外箇所の理由コメント整備

## Phase 2（4-8週）

1. `MainWindowServiceRegistry` を再分割し、`ensure*` を10件以上削減
2. 600行超ファイルを `33 -> 28` へ削減
3. Sanitizerジョブ（ASan/UBSan）をCIへ導入

## Phase 3（9-12週）

1. 600行超ファイルを `33 -> 24` 達成
2. app層テスト合計6本を達成
3. 構造KPI（例外リスト）を縮小方向へ更新

---

## 5. 実施順序の判断基準

- 先に「障害コストが高い場所」を削る（巨大ファイル・司令塔・所有権）。
- 次に「変更速度を上げる投資」を行う（テスト・CI・依存分割）。
- 1PRあたりの変更粒度は中規模までに制限し、KPIを毎回更新する。

---

## 6. 直近スプリントで着手すべき具体タスク

1. `analysisflowcontroller.cpp` を責務3分割する設計メモを作成する。
2. `MainWindowServiceRegistry` の `ensure*` 呼び出しグラフを可視化する。
3. app層の起動/終了シナリオテストを1本追加する。
4. CIに `sanitize` プロファイル（任意トリガ）を追加する。

---

## 7. 総括

現状のShogiBoardQは、品質ゲートとテストが機能しており、土台は堅牢である。  
次の改善効果を最大化するには、「巨大ファイル削減」「依存密度低減」「所有権の最終統一」を先行し、その後に app層テストとCI拡張で回帰耐性を高める順序が最適である。
