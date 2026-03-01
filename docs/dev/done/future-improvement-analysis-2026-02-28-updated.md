# 今後の改善点 詳細分析（2026-02-28 更新版）

## 1. 目的

本書は、ShogiBoardQ の現行コードベースに対して、次に投資すべき改善テーマを
「効果」「実装コスト」「リスク」の3軸で整理し、実行可能な計画に落とし込むための分析レポートである。

対象は以下に限定する。

- 保守性（変更容易性、理解容易性）
- 品質保証コスト（テスト、CI、回帰検知）
- 障害予防（所有権、ライフサイクル、設計崩壊防止）

機能追加・UI仕様変更は対象外とする。

---

## 2. 現状ベースライン（2026-02-28 実測）

## 2.1 規模・品質ゲート

| 指標 | 実測値 |
|---|---:|
| `src` 配下 `.cpp/.h` ファイル数 | 538 |
| `src` 配下 総行数 | 104,833 |
| 600行超ファイル数 | 29 |
| 800行超ファイル数 | 6 |
| 1000行超ファイル数 | 0 |
| ctest 登録テスト数 | 50 |
| ctest 実行結果 | 50/50 pass |
| `MainWindow` friend class 数 | 3 |
| `MainWindow` ensure メソッド数 | 0 |
| `MainWindowServiceRegistry` ensure メソッド数 | 35 |
| `MainWindow` 生ポインタ風メンバ数（`m_`） | 47 |
| `MainWindow` `std::unique_ptr` メンバ数（`m_`） | 30 |
| `delete` 文（構造KPI測定） | 1 (`src/widgets/flowlayout.cpp`) |
| lambda `connect`（構造KPI測定） | 0 |
| 旧式 `SIGNAL/SLOT`（構造KPI測定） | 0 |

補足:

- `MainWindowServiceRegistry` の `ensure*` はテスト閾値（35）に到達済み。
- 構造KPIの例外リスト件数は 32、そのうち TODO 管理対象は 24。

## 2.2 モジュール別規模（上位）

| モジュール | ファイル数 | 行数 |
|---|---:|---:|
| `kifu` | 81 | 18,191 |
| `ui` | 96 | 16,245 |
| `dialogs` | 41 | 11,923 |
| `game` | 46 | 9,895 |
| `widgets` | 51 | 9,802 |
| `app` | 71 | 8,823 |

## 2.3 600行超ファイルの偏在

| モジュール | 件数 |
|---|---:|
| `kifu` | 5 |
| `dialogs` | 5 |
| `widgets` | 4 |
| `views` | 3 |
| `game` | 3 |
| `engine` | 3 |
| `core` | 2 |
| その他 | 4 |

## 2.4 現在の上位大型ファイル

| ファイル | 行数 |
|---|---:|
| `src/core/fmvlegalcore.cpp` | 930 |
| `src/dialogs/engineregistrationdialog.cpp` | 841 |
| `src/dialogs/josekimovedialog.cpp` | 838 |
| `src/core/shogiboard.cpp` | 822 |
| `src/views/shogiview_draw.cpp` | 808 |
| `src/views/shogiview.cpp` | 802 |
| `src/game/matchcoordinator.cpp` | 789 |
| `src/engine/usi.cpp` | 781 |
| `src/dialogs/changeenginesettingsdialog.cpp` | 774 |
| `src/kifu/formats/csaexporter.cpp` | 757 |

---

## 3. 総評

品質レベルは高い。

- 構造KPI・依存方向テスト・ライフサイクル契約テストが実運用されている。
- `MainWindow` は「薄いファサード化」が進み、主要ロジックは registry/pipeline に移譲されている。
- テストスイートは 50 件すべて成功している。

一方で、改善投資の重心は明確である。

- 大型ファイル群（特に `dialogs/kifu/widgets`）
- `MainWindow` 周辺の依存密度
- registry の成長余地不足（閾値張り付き）

---

## 4. 改善テーマ詳細（優先順）

## 4.1 テーマA: 大型ファイル分割の継続（最優先）

### 現状課題

- 600行超が 29 件残存し、変更局所性が下がる。
- `tests/tst_structural_kpi.cpp` でも TODO Issue が継続管理されている。
- UI/プロトコル/変換器で「入力解釈 + 変換 + 副作用」が同居しやすい。

### リスク

- 修正時に副作用が広がりやすく、回帰原因の切り分けコストが高い。
- レビューが差分追従中心になり、設計改善が遅れる。

### 施策

1. `dialogs` 上位3ファイルを「Model/Presenter/ActionHandler」に分割する。
2. `kifu/formats` は `parse -> normalize -> apply` の3段へ整理する。
3. `matchcoordinator.cpp` はユースケース単位（開始/終了/進行/例外）で抽出する。

### KPI

- 600行超: `29 -> 20`（12週間）
- 800行超: `6 -> 3`（12週間）
- KPI TODO件数: `24 -> 15`（12週間）

## 4.2 テーマB: MainWindow依存密度の低減（最優先）

### 現状課題

- `MainWindow` は 256行と薄いが、依存先メンバが多い（生ポインタ風 47）。
- `buildRuntimeRefs()` が多領域参照を束ね、結合点として大きい。
- friend class は閾値いっぱい（3/3）。

### リスク

- 機能追加時に `MainWindow` ヘッダ変更が連鎖しやすい。
- レジストリやワイヤリング側の責務境界が曖昧になりやすい。

### 施策

1. `MainWindowRuntimeRefs` を `GameRefs/KifuRefs/UiRefs` に分割する。
2. `MainWindow` の非UI依存メンバを段階的に Context に移管する。
3. `MainWindow` から直接参照する writable state を最小化する。

### KPI

- 生ポインタ風メンバ: `47 -> 35`
- `buildRuntimeRefs()` のフィールド数: 段階的に30%以上削減

## 4.3 テーマC: ServiceRegistry の再分割（高優先）

### 現状課題

- `MainWindowServiceRegistry` の `ensure*` が 35（閾値35に一致）。
- 成長余白がなく、追加時に設計負債か閾値緩和の二択になりやすい。

### リスク

- 依存チェーンの見通し悪化。
- “とりあえず ensure 追加” による中央集約の再肥大化。

### 施策

1. `Ui/Game/Kifu/Analysis/Board` ごとにサブレジストリ化。
2. `ensure*` を生成責務と更新責務に分離（`ensureX` と `refreshXDeps`）。
3. 依存グラフをテストまたはドキュメントで固定化。

### KPI

- `ServiceRegistry ensure*`: `35 -> 28`
- registry実装ファイル横断include数: 20%以上削減

## 4.4 テーマD: テスト戦略の「厚みの偏り」補正（高優先）

### 現状課題

- テストは50件あり強固だが、領域ごとの偏りは残る。
- app層の結線/ライフサイクルは追加済みだが、異常系と長期回帰が薄い。

### リスク

- 配線変更や再初期化シーケンス変更で潜在回帰を見逃す。
- 例外系（通信中断、設定破損、ロード途中失敗）を再現しにくい。

### 施策

1. app層シナリオテスト（起動→対局→保存→終了→再起動）を拡張。
2. wiring契約テストを主要配線に水平展開。
3. parser/protocol の異常系テーブルを増強。

### KPI

- app/wiring系テスト: `+8` ケース以上
- 異常系テスト（I/O・通信・変換）: `+20` ケース以上

## 4.5 テーマE: 所有権ルールの最終統一（中優先）

### 現状課題

- `delete` 件数は 1 と健全。
- ただし `new` が registry/compose 層に多く、暗黙 ownership 理解が必要。

### リスク

- 将来の移譲や再生成ロジックで寿命バグを埋め込みやすい。
- 参照保持先の寿命保証がコメント頼みになりやすい。

### 施策

1. QObject parent所有と `std::unique_ptr` の境界ルールを文書化し、全新規コードに適用。
2. 再生成されるオブジェクト（orchestrator/wiring）に寿命コメントを明記。
3. `QPointer` を「非所有参照」の標準として拡大。

### KPI

- `delete` 件数: 維持（`<= 1`）
- ownershipコメント付与率: 100%（再生成対象）

## 4.6 テーマF: CIゲートのランタイム品質強化（中優先）

### 現状課題

- build/test/staticは整備済み。
- ASan/UBSan などの実行時破壊検知が常設されていない。

### リスク

- まれな未定義動作や寿命問題が通常CIでは顕在化しない。

### 施策

1. `ASan+UBSan` ジョブを nightly またはラベル起動で追加。
2. 長時間シナリオ（連続対局・連続ロード）を定期実行。
3. KPI値を artifact として時系列保存。

### KPI

- sanitizerジョブ: 最低1本導入
- 長時間テスト: 最低1本導入

---

## 5. 優先度マトリクス

| テーマ | 効果 | 工数 | 失敗時リスク | 優先度 |
|---|---|---|---|---|
| A: 大型ファイル分割 | 高 | 高 | 中 | P1 |
| B: MainWindow依存密度低減 | 高 | 中 | 高 | P1 |
| C: ServiceRegistry再分割 | 高 | 中 | 高 | P1 |
| D: テスト偏り補正 | 中〜高 | 中 | 高 | P1 |
| E: 所有権統一 | 中 | 低〜中 | 中 | P2 |
| F: CIランタイム強化 | 中 | 中 | 中 | P2 |

---

## 6. 12週間ロードマップ

## Phase 1（1-4週）

1. `ServiceRegistry` の再分割設計と最小実装を完了。
2. `dialogs` 上位2ファイルの分割着手。
3. app/wiring 異常系テストを4ケース追加。

完了条件:

- `sr_ensure_methods <= 32`
- 回帰テスト全緑（50件 + 追加分）

## Phase 2（5-8週）

1. `matchcoordinator.cpp` と `kifu/formats` の分割を実施。
2. `MainWindowRuntimeRefs` を領域別に分割。
3. sanitizerジョブ導入（手動 or nightly）。

完了条件:

- 600行超 `<= 24`
- `MainWindow` 生ポインタ風メンバ `<= 40`

## Phase 3（9-12週）

1. `widgets/views` 上位大型ファイルを分割。
2. ownershipルール文書化 + チェック運用を固定。
3. KPI例外/TODOの縮小を反映。

完了条件:

- 600行超 `<= 20`
- KPI TODO件数 `<= 15`
- sanitizer + 長時間テスト運用開始

---

## 7. 直近スプリント（次の2週間）で実施すべき具体タスク

1. `MainWindowServiceRegistry` の `ensure*` を 3〜5 件削減するPRを作る。
2. `dialogs/engineregistrationdialog.cpp` を責務分割する設計メモを作る。
3. app層の異常系テスト（初期化失敗、依存未生成、二重終了）を3件追加する。
4. `tst_structural_kpi` の TODO縮小反映（実測に合わせた上限更新）を行う。

---

## 8. 測定ルール（週次）

以下を毎週同じ手順で記録する。

```bash
find src -type f \( -name '*.cpp' -o -name '*.h' \) | wc -l
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | tail -n 1
find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 wc -l | sed '$d' | awk '$1>600{a++} $1>800{b++} $1>1000{c++} END{print a,b,c}'
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi -o -,txt
```

記録フォーマット:

- 日付
- 各KPI実測値
- 前週比
- 逸脱理由
- 次週アクション

---

## 9. まとめ

ShogiBoardQ は、品質基盤がすでに高水準で運用されている。
次の改善は「新しい仕組みの導入」よりも、「既存改善方針の完遂」に集中するのが最も効果的である。

最短で効く順は以下。

1. `ServiceRegistry/MainWindow` 周辺の依存密度を下げる。
2. 大型ファイル分割を TODO 管理に沿って継続する。
3. 異常系テストと runtime CI を増やして回帰検知を厚くする。

