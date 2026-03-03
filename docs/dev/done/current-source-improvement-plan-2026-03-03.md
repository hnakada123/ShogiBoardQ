# 現行ソースコードの改善点と修正方法（2026-03-03）

## 1. 目的

この文書は、2026-03-03 時点の `main` ワークツリーを対象に、現行コードで優先度高く改善すべき点と具体的な修正方法を整理する。

対象は「設計上の破綻予防」「将来の回帰防止」「保守性向上」であり、現時点の致命的不具合の断定ではない。

---

## 2. 現状サマリ

- テストは現行 `build` で `57/57` pass。
- 層依存・構造KPI・配線カバレッジをテストで監視できており、品質基盤は強い。
- 一方で、`MainWindow` 周辺には「ライフサイクル終端時の安全性」「生成経路の一意性」「責務純度」の改善余地がある。

参考:
- `src/app/mainwindow.cpp:170`
- `src/app/mainwindowlifecyclepipeline.cpp:88`
- `src/ui/wiring/mainwindowsignalrouter.cpp:55`
- `tests/tst_structural_kpi.cpp:96`
- `tests/tst_layer_dependencies.cpp:26`

---

## 3. 優先度付き改善項目

## P0: シャットダウン後アクセスの安全化（Null/Dangling 予防）

### 問題

- `runShutdown()` で `m_queryService.reset()` している。
- `buildRuntimeRefs()` は `m_queryService->sfenRecord()` を無条件参照している。
- 終了処理中に遅延シグナルや非同期処理経由で `buildRuntimeRefs()` が再入すると、クラッシュ余地がある。

根拠:
- `src/app/mainwindowlifecyclepipeline.cpp:106`
- `src/app/mainwindow.cpp:194`

### 修正方針

- `MainWindow` に「終了中フラグ」を導入し、終了フェーズの参照構築を安全化する。
- `buildRuntimeRefs()` を `m_queryService == nullptr` でも成立する実装にする。

### 実装手順

1. `MainWindow` に `bool m_isShuttingDown = false;` を追加。
2. `MainWindowLifecyclePipeline::runShutdown()` 冒頭で `m_mw.m_isShuttingDown = true;` を設定。
3. `buildRuntimeRefs()` で以下をガード。
   - `refs.kifu.sfenRecord = m_queryService ? m_queryService->sfenRecord() : nullptr;`
4. `m_queryService` 依存先で `nullptr` を許容するガードを追加。
5. 終了中は `ensure*` の新規生成を抑制するガードを主要レジストリに追加。

### 受け入れ基準

- `closeEvent()` 後にキュー処理が残っていてもクラッシュしない。
- `ctest --test-dir build --output-on-failure` が全通過。
- 新規テスト: 「終了中に `buildRuntimeRefs()` を呼んでも例外/クラッシュしない」。

---

## P1: `DialogCoordinatorWiring` 生成経路の一本化

### 問題

- `MainWindowSignalRouter` が `DialogCoordinatorWiring` を直接 `new` する経路を持つ。
- 同時に `MainWindowCompositionRoot` でも同クラスを生成管理する設計になっている。
- 生成責務が分散し、依存更新漏れや設計逸脱の温床になる。

根拠:
- `src/ui/wiring/mainwindowsignalrouter.cpp:57`
- `src/app/mainwindowcompositionroot.h:47`
- `src/app/mainwindowuiregistry.cpp:58`

### 修正方針

- 「生成は CompositionRoot/Registry 経由のみ」に統一する。
- SignalRouter は生成せず、取得済み依存を使うだけに限定する。

### 実装手順

1. `MainWindowSignalRouter::connectAllActions()` から `new DialogCoordinatorWiring(...)` を削除。
2. `connectAllActions()` 冒頭で `ensureDialogCoordinator()` を呼ぶコールバックを必須化。
3. `UiActionsWiring` の依存注入前に `dcw != nullptr` を検証し、未初期化時は fail-fast ログを出して処理中断。
4. 生成ルールを `docs/dev/ownership-guidelines.md` に追記。

### 受け入れ基準

- `DialogCoordinatorWiring` の `new` は CompositionRoot 由来のみになる。
- `tst_wiring_contracts` と `tst_wiring_slot_coverage` が通過。
- 解析中止 (`actionCancelAnalyzeKifu`) が従来どおり動作。

---

## P1: `buildRuntimeRefs()` の副作用分離（純粋化）

### 問題

- `buildRuntimeRefs()` は参照構築関数だが、内部で `m_playModePolicy->updateDeps()` を呼んでいる。
- 読み取り関数に副作用があるため、呼び出し順依存の理解負荷が高い。

根拠:
- `src/app/mainwindow.cpp:170`
- `src/app/mainwindow.cpp:253`

### 修正方針

- 参照構築と依存更新を分離し、関数責務を明確化する。

### 実装手順

1. `buildRuntimeRefs()` から `m_playModePolicy->updateDeps()` を削除。
2. 新規メソッド `refreshRuntimeServiceDeps()` を追加し、依存更新を移動。
3. `refreshRuntimeServiceDeps()` の呼び出しポイントを明示。
   - 起動完了直後
   - 対局開始直前
   - Match 再生成直後
4. 既存の `ensure*` 呼び出しとの順序契約をコメント化。

### 受け入れ基準

- `buildRuntimeRefs()` が状態変更を一切行わない。
- 依存更新の責務が単一箇所に集約される。
- `tst_app_lifecycle_pipeline` / `tst_lifecycle_scenario` が通過。

---

## P2: Wiring の null 依存前提を明文化し、fail-fast 化

### 問題

- `GameActionsWiring::wire()` は `dcw`/`gso`/`dlw` が `nullptr` でも `connect()` を呼びうる。
- 現状は前段の初期化順に依存して成立しているが、将来の変更で壊れやすい。

根拠:
- `src/ui/wiring/gameactionswiring.cpp:21`
- `src/ui/wiring/gameactionswiring.cpp:29`
- `src/ui/wiring/uiactionswiring.cpp:22`

### 修正方針

- Wiring 入力条件を明示し、未充足時は早期 return + 明確ログにする。

### 実装手順

1. `GameActionsWiring::wire()` 冒頭で必須依存を検証。
2. 必須不足時は `qCWarning` を出して配線を中止。
3. `UiActionsWiring::wire()` 側でも `Deps` 妥当性検証を追加（二重防衛）。
4. `tests/tst_wiring_contracts.cpp` に null 依存時の防御確認を追加。

### 受け入れ基準

- 必須依存不足時に原因がログで即判別できる。
- 予期せぬ null `connect` 警告が減る。

---

## P2: 大型ファイルの段階的分割（複雑性削減）

### 問題

- 600行超は解消済みだが、500行超ファイルがまだ多い（KPI上限ギリギリ）。
- 仕様追加時に再肥大化しやすい。

根拠:
- `tests/tst_structural_kpi.cpp:628`（`files_over_500`）
- 行数上位例:
  - `src/ui/coordinators/kifudisplaycoordinator.cpp`（592）
  - `src/kifu/formats/usentosfenconverter.cpp`（591）
  - `src/kifu/kifuapplyservice.cpp`（584）
  - `src/engine/usiprotocolhandler.cpp`（579）

### 修正方針

- 機能スライス単位で分割し、公開APIを維持したまま内部実装を分離する。

### 実装手順

1. 1ファイルずつ実施（並列分割しない）。
2. 分割単位は「イベントハンドラ」「変換処理」「状態同期」に分ける。
3. 分割ごとに `tst_structural_kpi` を確認し、上限値の引き下げも同時に行う。

### 受け入れ基準

- `files_over_550` を 10 -> 8 -> 6 と段階的に削減。
- 関連ユニットテストと統合テストが全通過。

---

## P3: ドキュメント/運用ルールの同期

### 問題

- 開発ガイドの一部に旧テスト本数（31）が残っているが、現行は 57。
- 認識差があるとレビュー・CI運用で齟齬が出る。

### 修正方針

- 実測値とドキュメントを同期し、更新元を一本化する。

### 実装手順

1. テスト本数の記載箇所を検索して最新化。
2. `docs/dev/test-summary.md` を正とし、他文書は参照リンクに統一。
3. 可能なら `ctest -N` 出力を自動転記するスクリプトを追加。

### 受け入れ基準

- テスト本数の表記差分がなくなる。
- リリース前チェックで手動確認項目が減る。

---

## 4. 実施順序（推奨）

1. P0: シャットダウン後アクセス安全化
2. P1: `DialogCoordinatorWiring` 生成経路一本化
3. P1: `buildRuntimeRefs()` 副作用分離
4. P2: Wiring fail-fast 化
5. P2: 大型ファイル分割（継続タスク）
6. P3: ドキュメント同期

---

## 5. テスト戦略

## 共通

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

## 追加推奨テスト

1. 終了中参照安全テスト
   - 対象: `MainWindowLifecyclePipeline` + `buildRuntimeRefs`
   - 観点: `runShutdown()` 後の参照構築が安全に失敗/スキップすること

2. 生成経路一意性テスト
   - 対象: `DialogCoordinatorWiring`
   - 観点: `new DialogCoordinatorWiring` が許可された層以外に存在しないこと

3. Wiring 依存妥当性テスト
   - 対象: `UiActionsWiring` / `GameActionsWiring`
   - 観点: 必須依存欠落時に fail-fast すること

---

## 6. 期待効果

- 終了時クラッシュ耐性の向上
- 生成責務の一貫性向上による保守コスト削減
- 依存更新順序の理解負荷軽減
- 構造KPIの持続的改善

---

## 7. 変更履歴

- 2026-03-03: 初版作成
