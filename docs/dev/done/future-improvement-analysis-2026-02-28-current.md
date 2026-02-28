# 今後の改善点 詳細分析（2026-02-28, Current）

## 1. 目的

本書は、ShogiBoardQ の次期改善投資先を「実測値ベース」で整理し、  
優先度・実施順・完了条件（KPI）を明確化するための分析レポートである。

対象は新機能追加ではなく、以下の品質属性改善に限定する。

- 保守性（変更容易性、依存関係の理解容易性）
- 安定性（クラッシュ/回帰の予防）
- 開発速度（レビュー容易性、CIでの早期検知）

---

## 2. エグゼクティブサマリ

現状は「品質ゲートが強い、ただし司令塔層の複雑度が高い」状態。

- 強み:
  - 構造KPIテスト・層依存テストが有効に機能
  - テストが広範囲に整備され、46/46 pass
  - `MainWindow` の処理本体は薄く、分割方針は明確
- 課題:
  - `MainWindowServiceRegistry` の `ensure*` が上限値に到達（35/35）
  - `MainWindow` の friend class が上限値（3/3）
  - 600行超ファイルが 32 件残存（UI/Dialogs/Kifu に偏在）

最優先は次の3点。

1. app司令塔層（`MainWindow`/Registry）の依存密度低減
2. 600行超ファイル群の計画的縮小（特にUI/Dialogs/Network）
3. 振る舞い契約テスト（配線・非同期フロー）の拡充

---

## 3. 現状スナップショット（実測）

## 3.1 コード規模

| 指標 | 値 |
|---|---:|
| `src/` 内 `.cpp/.h` ファイル数 | 532 |
| `src/` 総行数 | 104,455 |
| 600行超ファイル数 | 32 |
| 800行超ファイル数 | 8 |
| 1000行超ファイル数 | 0 |

## 3.2 テスト・品質ゲート

| 指標 | 値 |
|---|---:|
| テスト数（ctest） | 46 |
| テスト結果 | 46/46 pass |
| `delete` 件数（`src/*.cpp`） | 1 |
| lambda `connect` 件数 | 0 |
| 旧式 `SIGNAL/SLOT` 件数 | 0 |

## 3.3 app司令塔層の密度

| 指標 | 値 |
|---|---:|
| `MainWindowServiceRegistry` の `ensure*` 件数 | 35 |
| `MainWindowFoundationRegistry` の `ensure*` 件数 | 15 |
| `MainWindowCompositionRoot` の `ensure*` 件数 | 11 |
| `MainWindow` friend class 数 | 3 |
| `MainWindow` public slots 数 | 13 |
| `MainWindow` 生ポインタメンバ数 | 47 |
| `MainWindow` `std::unique_ptr` メンバ数 | 30 |

補足:

- `tests/tst_structural_kpi.cpp` の上限:
  - `sr_ensure_methods <= 35`
  - `mw_friend_classes <= 3`
- 現状はどちらも上限到達で、増分余地がない。

## 3.4 モジュール別行数（上位）

| モジュール | 行数 |
|---|---:|
| `kifu` | 18,192 |
| `ui` | 15,668 |
| `dialogs` | 12,467 |
| `game` | 9,895 |
| `widgets` | 9,736 |
| `app` | 8,821 |
| `engine` | 5,870 |

## 3.5 600行超ファイル（主要）

- `src/core/fmvlegalcore.cpp` (930)
- `src/dialogs/pvboarddialog.cpp` (884)
- `src/dialogs/engineregistrationdialog.cpp` (841)
- `src/dialogs/josekimovedialog.cpp` (838)
- `src/core/shogiboard.cpp` (820)
- `src/views/shogiview_draw.cpp` (808)
- `src/dialogs/changeenginesettingsdialog.cpp` (804)
- `src/views/shogiview.cpp` (802)
- `src/network/csagamecoordinator.cpp` (798)
- `src/game/matchcoordinator.cpp` (789)
- `src/engine/usi.cpp` (781)

---

## 4. 改善テーマ別分析

## 4.1 テーマA: app司令塔層の複雑度削減（最優先）

### 現状認識

- `MainWindow` は薄い転送層になっているが、状態・依存の集約点としては依然大きい。
- `MainWindowServiceRegistry` へ責務集約は進んだ一方、`ensure*` と callback が高密度。
- `MainWindow` + Registry + Foundation + CompositionRoot の総計が約 2,690 行あり、変更影響の追跡が難しい。

### 根本課題

- 「分割済みだが、依存点がまだ集中している」状態。
- `ensure*` 呼び出し網に循環的な callback パスが存在し、修正時の認知負荷が高い。

### 改善方針

1. `ensure*` をさらに用途単位へ再分割（Game/Kifu/UI/Analysis のサブレジストリ化）。
2. `MainWindowRuntimeRefs` を目的別に分解し、不要な参照束ねを減らす。
3. `MainWindow` が保持する非UI状態を Context 構造へ段階移管。

### 期待効果

- 依存の見通し向上
- PRごとの変更範囲縮小
- 回帰原因追跡時間の短縮

---

## 4.2 テーマB: 大型ファイル削減（継続優先）

### 現状認識

- 1000行超は解消済み（0件）だが、600行超が 32 件残存。
- 偏在先は `dialogs` / `widgets` / `kifu` / `views` / `network`。

### 根本課題

- UIイベント処理・状態更新・I/Oが1ファイルに混在しやすい。
- 「局所変更で広範囲副作用」が起きやすい構造。

### 改善方針

1. 分割単位を「入力/判断/反映」または「UI/Domain/I/O」に固定。
2. 600行超ファイルは1スプリント2本を上限に継続分割。
3. `knownLargeFiles()` の上限値を分割PRで即時更新。

### 優先候補（順）

1. `src/network/csagamecoordinator.cpp`
2. `src/game/matchcoordinator.cpp`
3. `src/views/shogiview.cpp` + `src/views/shogiview_draw.cpp`
4. `src/kifu/kifuexportcontroller.cpp`
5. `src/core/fmvlegalcore.cpp`

---

## 4.3 テーマC: 所有権とライフサイクル境界の明示化

### 現状認識

- `delete` は実質1件まで削減され、全体として良好。
- 一方で `MainWindow` 配下は raw pointer が多く、所有責務の読み取りコストは高い。

### 根本課題

- `QObject` 親子管理とレジストリ管理の境界がコードリーディング時に追いづらい。

### 改善方針

1. `MainWindow` メンバを「所有」「参照」「遅延注入」の3群に明示整理（コメント/命名規約）。
2. raw pointer のうち `QObject` 非依存なものは `std::unique_ptr` 化を継続。
3. ライフサイクル境界（startup/shutdown/restart）を `LifecyclePipeline` 側で一元化。

---

## 4.4 テーマD: 配線契約テストの拡充

### 現状認識

- テストは46件あり、基礎層〜app層までカバー。
- ただし `wiring` / 非同期イベント連鎖の契約検証は、変更頻度に対して薄くなりやすい。

### 根本課題

- シグナル転送や callback 経由の遅延初期化は、構造テストだけでは回帰を捉えにくい。

### 改善方針

1. `wiring` ごとに「入力シグナル -> 期待副作用」を契約テスト化。
2. `GameSessionOrchestrator` 周辺は状態遷移表ベースでケースを追加。
3. 重要フロー（起動/終了/対局開始/棋譜ロード/分岐遷移）を固定回帰セット化。

---

## 4.5 テーマE: CI可観測性の強化

### 現状認識

- Build/Test/構造KPIの実行基盤は整っている。
- ただし「品質が改善しているか」の時系列可視化は弱い。

### 改善方針

1. 構造KPI結果（600行超件数、`ensure*` 件数など）をCI artifact化。
2. PRごとに前回比較を出力（増減表示）。
3. 任意トリガまたはnightlyで ASan/UBSan を追加。

---

## 4.6 テーマF: ドメイン境界（Kifu/Network/Core）の負債整理

### 現状認識

- `kifu`, `network`, `core` に高密度領域が残る。
- 形式変換・通信・盤面ロジックは仕様追加時の影響が広がりやすい。

### 改善方針

1. 変換系を `Parse -> Normalize -> Apply` に統一。
2. 例外系（不正入力・境界値）テストをテーブル駆動で増強。
3. `core` の巨大ロジック（`fmvlegalcore`, `shogiboard`）は補助クラス抽出で段階分割。

---

## 5. 優先度マトリクス（効果/コスト/リスク）

| テーマ | 効果 | コスト | 実装リスク | 優先度 |
|---|---|---|---|---|
| A: app司令塔複雑度削減 | 高 | 中 | 中 | P1 |
| B: 大型ファイル削減 | 高 | 中〜高 | 中 | P1 |
| D: 配線契約テスト拡充 | 高 | 中 | 低〜中 | P1 |
| E: CI可観測性強化 | 中 | 低〜中 | 低 | P2 |
| C: 所有権明示化 | 中 | 中 | 低〜中 | P2 |
| F: ドメイン境界整理 | 高 | 高 | 中〜高 | P3 |

---

## 6. 実行ロードマップ（12週間）

## Phase 1（1-2週）

- `ensure*` 依存グラフの最新化と循環パスの棚卸し
- `MainWindowRuntimeRefs` の分割設計
- 600行超ファイルを2本分割
- 配線契約テストを2件追加

完了条件:

- `sr_ensure_methods` を 35 -> 33 以下
- 600行超ファイルを 32 -> 30 以下

## Phase 2（3-6週）

- appサブレジストリ導入（Game/Kifu/UI）
- `csagamecoordinator` と `matchcoordinator` の責務抽出
- CIでKPI差分表示を導入

完了条件:

- `MainWindow` 生ポインタ 47 -> 42 以下
- 600行超ファイル 30 -> 27 以下

## Phase 3（7-12週）

- `views` / `kifu` / `core` の大型ファイル分割継続
- 非同期系契約テストをフロー単位で追加
- Sanitizerジョブの導入（定期 or 任意トリガ）

完了条件:

- 600行超ファイル 27 -> 22 以下
- 800行超ファイル 8 -> 5 以下
- app/wiring回帰テストを +6 件

---

## 7. 改善KPI（推奨ターゲット）

| KPI | 現状 | 4週間目標 | 12週間目標 |
|---|---:|---:|---:|
| `MainWindowServiceRegistry ensure*` | 35 | <=33 | <=28 |
| `MainWindow friend class` | 3 | <=3 | <=2 |
| 600行超ファイル数 | 32 | <=30 | <=22 |
| 800行超ファイル数 | 8 | <=7 | <=5 |
| app/wiring系テスト数（追加分） | 0 | +2 | +6 |

---

## 8. 直近スプリントの具体タスク

1. `docs/dev/done/ensure-graph-analysis.md` を現行実装へ追従更新し、循環依存を再可視化。
2. `MainWindowRuntimeRefs` 分割案（`GameRefs`, `KifuRefs`, `UiRefs`）の設計メモ作成。
3. `src/network/csagamecoordinator.cpp` 分割の準備PR（責務境界の抽出だけ先行）。
4. `wiring` 契約テストを1本追加（対象は変更頻度の高い箇所から）。
5. `tst_structural_kpi` の上限に余裕を作るPRを優先（`sr_ensure_methods` 低減）。

---

## 9. リスクと対策

| リスク | 内容 | 対策 |
|---|---|---|
| 分割による回帰 | 信号配線/状態同期の取りこぼし | 契約テスト追加 + 分割PR粒度を小さく維持 |
| 改善停滞 | 大型ファイル分割が後回し化 | 1スプリント2本を固定KPI化 |
| 設計過剰 | 抽象化の過多で可読性低下 | 1責務1抽象を維持し、薄いラッパ増殖を避ける |
| KPI形骸化 | 上限だけ維持して実質改善が進まない | 目標は「上限維持」ではなく「段階的引き下げ」に設定 |

---

## 10. 総括

ShogiBoardQ はすでに高い品質基盤（テスト・構造KPI・分割方針）を持っている。  
次段階では「司令塔複雑度の圧縮」と「大型ファイルの計画的縮小」を同時に進めることで、  
変更速度と回帰耐性を両立できる。

短期は `app` 層の依存密度低減と配線契約テスト、  
中長期は `network/kifu/core` の負債圧縮を主軸に進めるのが最も費用対効果が高い。
