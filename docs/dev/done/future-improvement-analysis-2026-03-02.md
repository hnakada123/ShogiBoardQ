# 今後の改善点 詳細分析（2026-03-02）

## 1. この文書の目的

本書は、ShogiBoardQ の現状コードを前提に、次の改善投資を「優先度」「順序」「完了条件」まで具体化するための実行計画である。  
対象は、設計健全性・保守性・回帰耐性・CI運用品質とする。

## 2. 現状スナップショット（2026-03-02 実測）

### 2.1 実行コマンド

- `cmake --build build -j`
- `ctest --test-dir build --output-on-failure`
- `ctest --test-dir build -V -R tst_structural_kpi`

### 2.2 結果サマリ

| 項目 | 結果 |
|---|---|
| Build | 成功 |
| テスト総数 | 57 |
| テスト結果 | 56 pass / 1 fail |
| failテスト | `tst_layer_dependencies` |
| 構造KPI | pass（しきい値到達項目あり） |

### 2.3 現在の最大リスク

`tst_layer_dependencies` が検出した違反は、アーキテクチャ境界違反であり、機能バグより先に解消すべき構造リスクである。

- `src/engine`: 1件
- `src/game`: 14件
- `src/services`: 2件
- 合計: 17件

主な例:

- `src/engine/enginesettingscoordinator.cpp` が `dialogs` に依存
- `src/game/gamestatecontroller.cpp` が `views/widgets` に依存
- `src/game/prestartcleanuphandler.cpp` が `views/widgets` に依存
- `src/services/appsettings.cpp` が `views` に依存
- `src/services/uinotificationservice.cpp` が `views` に依存

## 3. 改善方針（優先順位）

改善順序は次を厳守する。

1. 層境界を壊している依存を止血する（P0）
2. 変更コストを上げる構造密度を下げる（P1）
3. 長期的な退行を防ぐ品質ゲートを強化する（P2）

## 4. P0: 層依存違反 17件の解消（最優先）

### 4.1 目標

- `tst_layer_dependencies` を pass させる
- `game/engine/services` から UI 具体型への直接 include を 0 にする

### 4.2 原因の型

| 原因タイプ | 内容 |
|---|---|
| 直接UI型参照 | `ShogiView`、`RecordPane`、`QMessageBox` 相当の表示依存 |
| ダイアログ直呼び | `engine` 層が `dialogs` を直接起動 |
| UI更新混在 | ゲーム進行ロジック内でビュー更新/選択モード変更を実施 |

### 4.3 解消戦略

1. UIポート導入
- `game` 層が必要とするUI操作を interface 化し、実装は `ui` 層へ置く。
- 例: 盤面再描画、選択モード更新、マウス入力有効/無効、評価表示リセット。

2. ダイアログ起動責務の移管
- `engine` から `dialogs` 依存を削除し、`ui` 側コーディネータへ移管する。
- `engine` は「設定ダイアログを開く要求イベント」のみ発行する。

3. サービス層の純化
- `services/appsettings.cpp` から `ShogiView` 依存を除去し、設定保存はプリミティブ値ベースに変更する。
- `services/uinotificationservice.cpp` のエラー表示は notifier interface 経由にし、`QMessageBox` 呼び出しは `ui` 層へ移す。

### 4.4 実装タスク（推奨順）

1. `engine` 違反1件の解消  
影響面が狭いため最初に完了させる。ダイアログ起動を `ui/coordinators` に移し、`engine` は要求APIのみ保持。

2. `services` 違反2件の解消  
`AppSettings` の `saveWindowAndBoard` を `saveWindowAndSquareSize(QWidget*, int)` のような API へ変更。  
`UiNotificationService` は表示実装を外部注入に変更。

3. `game` 違反14件の解消  
`GameStateController`、`GameStartCoordinator`、`PreStartCleanupHandler` の順で UI 依存をポート化。  
`matchcoordinator` 系は最後にまとめて切り替える。

### 4.5 完了条件

- `ctest --test-dir build -R tst_layer_dependencies --output-on-failure` が pass
- 違反再発を防ぐため、同テストを必須ゲートとして維持

## 5. P1: 構造密度の削減（2〜6週間）

### 5.1 MainWindow依存ハブの縮退

現状:

- `mw_friend_classes = 7 (limit 7)`
- `sr_ensure_methods = 11 (limit 11)`
- `mw_public_slots = 13 (limit 13)`
- `mainwindow_include_count = 40 (limit 40)`

この状態は「passだが余白ゼロ」のため、機能追加で再肥大化しやすい。

改善策:

1. `MainWindowRuntimeRefs` を用途別に分割する  
`UiRuntimeRefs` / `GameRuntimeRefs` / `KifuRuntimeRefs` へ分割し、受け渡し面積を減らす。

2. `MainWindowCompositionRoot` の ensure 系を生成と依存更新で分離  
`createX()` と `refreshXDeps()` を分け、生成責務と更新責務を明確化する。

3. include削減をKPIに反映  
`mainwindow_include_count` の目標を段階的に `40 -> 36 -> 32` に下げる。

### 5.2 長大ファイル・長関数の分割

実測:

- `files_over_550 = 13 (limit 13)` で上限到達
- `long_functions_over_100 = 136`（監視のみ）

優先対象:

- `src/kifu/formats/*.cpp` の lexer/exporter 群
- `src/ui/coordinators/kifudisplaycoordinator.cpp`
- `src/widgets/evaluationchartwidget.cpp`
- `src/network/csagamecoordinator.cpp`
- ダイアログの巨大コンストラクタ群

改善策:

1. コンストラクタ分解  
`buildUi()`, `bindSignals()`, `restoreSettings()`, `bindModels()` などに分割。

2. 変換器分割  
lexer / parser / normalize / validate を明示的に分ける。

3. 500/550行KPIのラチェット  
`files_over_550` を 13 から段階的に削減。  
`files_over_500` も 29 から継続削減。

### 5.3 文書と実装の整合性維持

`docs/dev/kpi-thresholds.md` の値とテスト実装値に差分がある。  
しきい値更新時は、テスト本体とドキュメントを同一PRで更新する運用へ統一する。

## 6. P2: 回帰耐性とCI運用品質の強化（1〜3か月）

### 6.1 非同期・境界系のテスト強化

1. USI/CSA の異常系シナリオを拡張  
タイムアウト、切断、部分応答、再接続を追加。

2. 変換器の頑健性強化  
フォーマット変換に対して、ランダム入力ベースのプロパティテストを段階導入。

3. UI配線の退行検知拡張  
`wiring` テストの対象を増やし、接続漏れと重複接続を検知。

### 6.2 CIゲートの改善

1. 層依存チェックの明示ジョブ化  
`tst_layer_dependencies` を単独ジョブで可視化し、失敗要因を早期把握。

2. Sanitizerジョブの定常運用  
週次だけでなくPRでも軽量セットを実行可能にする。

3. KPI差分レポート  
KPI JSON を前回値と比較し、悪化項目のみをサマリ表示。

## 7. 具体的ロードマップ

### Phase 1（1〜2週間）

- 層違反17件を0件にする
- `tst_layer_dependencies` pass 化

### Phase 2（2〜6週間）

- `files_over_550: 13 -> 10 以下`
- `mainwindow_include_count: 40 -> 36 以下`
- 巨大コンストラクタの分割開始

### Phase 3（1〜3か月）

- `files_over_550: 10 -> 8 以下`
- `files_over_500: 29 -> 24 以下`
- 非同期異常系テストの定常化

## 8. KPI目標（提案）

| KPI | 現在値 | 直近目標 | 中期目標 |
|---|---:|---:|---:|
| layer_dependency_violations | 17 | 0 | 0 |
| files_over_550 | 13 | 10 | 8 |
| files_over_500 | 29 | 26 | 24 |
| mainwindow_include_count | 40 | 36 | 32 |
| mw_friend_classes | 7 | 7以下維持 | 6以下 |
| sr_ensure_methods | 11 | 11以下維持 | 10以下 |
| long_functions_over_100 | 136 | 120 | 90 |

## 9. 実装時の注意点

1. 層違反の解消中に、逆に interface 過多で可読性を落とさない  
最初は「本当に必要なUI操作」だけポート化する。

2. 巨大ファイル分割は機械的に行わず、責務単位で切る  
単純分割は依存関係を悪化させる可能性がある。

3. KPIの数値だけを追わない  
「読みやすさ」と「変更時の影響範囲縮小」をレビュー観点として残す。

## 10. 直近で着手すべき3タスク

1. `engine` の `dialogs` 直接依存1件を除去するPRを作成する
2. `services/appsettings.cpp` と `services/uinotificationservice.cpp` の `views` 依存を除去する
3. `game/gamestatecontroller.cpp` の `ShogiView` / `RecordPane` 依存を UIポートへ置換する

---

この順で進めると、最短で「構造的に壊れにくい状態」に戻せる。  
新機能追加は、`tst_layer_dependencies` pass 復帰後に再開するのが安全である。
