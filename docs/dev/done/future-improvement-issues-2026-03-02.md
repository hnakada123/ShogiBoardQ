# 今後改善のIssue分解版（チケット一覧, 2026-03-02）

対象分析: `docs/dev/future-improvement-analysis-2026-03-02.md`  
この文書は、上記分析をそのまま実行可能な Issue 単位へ分解した一覧である。

## 1. 運用前提

- 既実施の旧計画（`code-quality-execution-tasks-2026-02-27.md`, `future-improvement-implementation-guide-2026-02-28.md`）は本計画の前提に含めない。
- 本計画は 2026-03-02 の実測値を起点とする。
- 優先度は `P0 > P1 > P2` で固定する。
- 1PR 1目的で進める。

## 2. 現在値（開始ベースライン）

| 指標 | 現在値 |
|---|---:|
| テスト総数 | 57 |
| テスト結果 | 56 pass / 1 fail |
| layer 依存違反 | 17 |
| files_over_550 | 13 |
| files_over_500 | 29 |
| mainwindow_include_count | 40 |
| mw_friend_classes | 7 |
| sr_ensure_methods | 11 |
| long_functions_over_100 | 136 |

## 3. 実行順（着手順）

### Phase 1（P0: 最優先）

1. `ISSUE-001` engine 層の dialogs 直接依存を除去
2. `ISSUE-002` services 層の views 直接依存を除去
3. `ISSUE-003` game 層 UI依存のポート化（前半）
4. `ISSUE-004` game 層 UI依存のポート化（後半）
5. `ISSUE-005` layer依存テストをCI可視化

### Phase 2（P1: 構造密度削減）

6. `ISSUE-010` MainWindowRuntimeRefs を用途別分割
7. `ISSUE-011` CompositionRoot の create/refresh 分離
8. `ISSUE-012` mainwindow include 削減バッチ
9. `ISSUE-013` files_over_550 削減バッチ1
10. `ISSUE-014` 巨大コンストラクタ分割バッチ1
11. `ISSUE-015` KPI文書と実装値の同期運用を固定

### Phase 3（P2: 回帰耐性/CI強化）

12. `ISSUE-020` USI/CSA 異常系シナリオ拡張
13. `ISSUE-021` 棋譜変換プロパティテスト導入
14. `ISSUE-022` wiring 退行検知テスト拡張
15. `ISSUE-023` KPI差分レポートのCI追加
16. `ISSUE-024` Sanitizer 軽量PRジョブ導入

## 4. Issue一覧（作成用）

## ISSUE-001: engine 層の dialogs 直接依存を除去

- 優先度: P0
- 背景: `src/engine/enginesettingscoordinator.cpp` が `dialogs` に直接依存している。
- 対象ファイル:
- `src/engine/enginesettingscoordinator.cpp`
- `src/ui/coordinators/` 配下（新規/既存）
- 実施内容:
- ダイアログ起動責務を `ui` 層へ移管する。
- `engine` 層は「ダイアログ起動要求」APIのみ保持する。
- 完了条件:
- `tst_layer_dependencies` で `src/engine` 違反が 0 件。
- 挙動互換（エンジン設定ダイアログが従来通り起動）。
- 検証コマンド:
- `ctest --test-dir build -R tst_layer_dependencies --output-on-failure`
- `ctest --test-dir build -R tst_wiring_analysistab --output-on-failure`
- 依存: なし

## ISSUE-002: services 層の views 直接依存を除去

- 優先度: P0
- 背景: `appsettings.cpp` と `uinotificationservice.cpp` が `ShogiView` に依存している。
- 対象ファイル:
- `src/services/appsettings.cpp`
- `src/services/uinotificationservice.cpp`
- `src/ui/` 配下の通知/橋渡し実装
- 実施内容:
- `AppSettings` をプリミティブ値中心APIに変更する（例: squareSize を `int` 受け渡し）。
- 通知表示を interface 化し、`QMessageBox` 呼び出しを `ui` 層へ移管する。
- 完了条件:
- `tst_layer_dependencies` で `src/services` 違反が 0 件。
- 設定保存・エラー表示の既存挙動維持。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_layer_dependencies|tst_settings_roundtrip|tst_app_error_handling\" --output-on-failure`
- 依存: `ISSUE-001`（並行可）

## ISSUE-003: game 層 UI依存ポート化（前半）

- 優先度: P0
- 背景: `GameStateController` などが `views/widgets` 具体型へ直接依存している。
- 対象ファイル:
- `src/game/gamestatecontroller.cpp`
- `src/game/matchundohandler.cpp`
- `src/game/gamestartcoordinator.cpp`
- 実施内容:
- UI操作ポート（interface）を導入する。
- game 層は interface 経由で UI更新を要求し、具体型参照を削除する。
- 完了条件:
- 対象3ファイルで `views/widgets/dialogs` include が 0。
- 対局開始/終局系テストが通る。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_layer_dependencies|tst_game_end_handler|tst_game_start_flow|tst_matchcoordinator\" --output-on-failure`
- 依存: `ISSUE-002`

## ISSUE-004: game 層 UI依存ポート化（後半）

- 優先度: P0
- 背景: `prestartcleanuphandler`、`matchcoordinator` 周辺に UI依存が残っている。
- 対象ファイル:
- `src/game/prestartcleanuphandler.cpp`
- `src/game/gamestartcoordinator_dialogflow.cpp`
- `src/game/matchcoordinator.cpp`
- `src/game/gamestartoptionsbuilder.cpp`
- `src/game/promotionflow.cpp`
- 実施内容:
- `ISSUE-003` で導入した UIポートへ統一移行する。
- ダイアログ関連は UI層コーディネータ経由へ寄せる。
- 完了条件:
- `tst_layer_dependencies` で `src/game` 違反が 0 件。
- ゲーム開始/中断/終局フローの回帰なし。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_layer_dependencies|tst_game_start_flow|tst_game_end_handler|tst_preset_gamestart_cleanup\" --output-on-failure`
- 依存: `ISSUE-003`

## ISSUE-005: layer依存テストをCI可視化

- 優先度: P0
- 背景: layer依存違反を見逃さないため、CI上で失敗要因を即把握できる形にする。
- 対象ファイル:
- `.github/workflows/ci.yml`
- 実施内容:
- `tst_layer_dependencies` 単独実行ステップを追加する。
- 失敗時に違反一覧を Step Summary に出力する。
- 完了条件:
- PRで layer違反発生時に原因がCI画面で可視化される。
- 検証コマンド:
- GitHub Actions 上で対象ジョブの実行確認
- 依存: `ISSUE-004`

## ISSUE-010: MainWindowRuntimeRefs を用途別分割

- 優先度: P1
- 背景: MainWindow 依存集約点の面積が大きく、変更影響が広い。
- 対象ファイル:
- `src/app/mainwindowruntimerefs.h`
- `src/app/mainwindow.cpp`
- `src/app/mainwindowcompositionroot.cpp`
- 実施内容:
- `UiRuntimeRefs` / `GameRuntimeRefs` / `KifuRuntimeRefs` に分割する。
- 呼び出し側を段階移行し、不要参照を削除する。
- 完了条件:
- `MainWindowRuntimeRefs` の単一巨大構造体依存を縮小できている。
- 関連テスト通過。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_app_lifecycle_pipeline|tst_app_game_session|tst_app_kifu_load\" --output-on-failure`
- 依存: `ISSUE-005`

## ISSUE-011: CompositionRoot の create/refresh 分離

- 優先度: P1
- 背景: ensure 系の生成責務と依存更新責務が混在し、見通しが悪い。
- 対象ファイル:
- `src/app/mainwindowcompositionroot.cpp`
- `src/app/mainwindowserviceregistry.*`
- 実施内容:
- `createX()` と `refreshXDeps()` を導入して責務分離する。
- `ensureX()` は薄いオーケストレーションにする。
- 完了条件:
- 主要な ensure 系で責務分離が適用される。
- 挙動互換 + KPI退行なし。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_structural_kpi|tst_app_lifecycle_pipeline|tst_wiring_contracts\" --output-on-failure`
- 依存: `ISSUE-010`

## ISSUE-012: mainwindow include 削減バッチ

- 優先度: P1
- 背景: `mainwindow_include_count = 40` で上限到達している。
- 対象ファイル:
- `src/app/mainwindow.cpp`
- 関連ヘッダ（前方宣言化対象）
- 実施内容:
- 前方宣言化 + 実装側include移動で依存を削減する。
- 目標を `40 -> 36` にする。
- 完了条件:
- `tst_structural_kpi` の `mainwindow_include_count` が 36 以下。
- ビルド通過。
- 検証コマンド:
- `ctest --test-dir build -R tst_structural_kpi --output-on-failure`
- 依存: `ISSUE-011`

## ISSUE-013: files_over_550 削減バッチ1

- 優先度: P1
- 背景: `files_over_550 = 13` で上限到達している。
- 対象ファイル候補:
- `src/kifu/formats/kiflexer.cpp`
- `src/kifu/formats/csalexer.cpp`
- `src/ui/coordinators/kifudisplaycoordinator.cpp`
- `src/widgets/evaluationchartwidget.cpp`
- `src/network/csagamecoordinator.cpp`
- 実施内容:
- 責務分割で 550行超ファイルを削減する。
- 目標を `13 -> 10` にする。
- 完了条件:
- `files_over_550 <= 10`
- 既存機能の回帰なし。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_structural_kpi|tst_kifconverter|tst_csaprotocol|tst_ui_display_consistency\" --output-on-failure`
- 依存: `ISSUE-012`

## ISSUE-014: 巨大コンストラクタ分割バッチ1

- 優先度: P1
- 背景: `long_functions_over_100 = 136` で可読性負債が大きい。
- 対象ファイル候補:
- `src/dialogs/sfencollectiondialog.cpp`
- `src/dialogs/startgamedialog.cpp`
- `src/dialogs/csagamedialog.cpp`
- 実施内容:
- `buildUi` / `bindSignals` / `restoreSettings` / `bindModels` に分解する。
- 初期化順序と状態復元順序を明文化する。
- 完了条件:
- 対象コンストラクタが 100行未満または責務分離され、可読性が改善。
- UI関連テスト通過。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_ui_display_consistency|tst_game_start_flow|tst_wiring_csagame\" --output-on-failure`
- 依存: `ISSUE-013`

## ISSUE-015: KPI文書と実装値の同期運用を固定

- 優先度: P1
- 背景: `docs/dev/kpi-thresholds.md` と実装値の差分が将来混乱を生む。
- 対象ファイル:
- `docs/dev/kpi-thresholds.md`
- `tests/tst_structural_kpi.cpp`
- 実施内容:
- しきい値変更時の更新ルールを明文化する。
- KPI変更PRテンプレートを追加する。
- 完了条件:
- テスト値と文書値の不一致が解消される。
- 運用ルールが開発フローに定着する。
- 検証コマンド:
- `ctest --test-dir build -R tst_structural_kpi --output-on-failure`
- 依存: `ISSUE-013`

## ISSUE-020: USI/CSA 異常系シナリオ拡張

- 優先度: P2
- 背景: 非同期境界の異常系は回帰コストが高い。
- 対象ファイル:
- `tests/tst_usiprotocolhandler.cpp`
- `tests/tst_csaprotocol.cpp`
- 実施内容:
- タイムアウト、切断、部分応答、再接続シナリオを追加する。
- ログ/通知の期待値を固定する。
- 完了条件:
- 異常系ケースの自動検知範囲が拡大する。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_usiprotocolhandler|tst_csaprotocol\" --output-on-failure`
- 依存: `ISSUE-005`

## ISSUE-021: 棋譜変換プロパティテスト導入

- 優先度: P2
- 背景: フォーマット境界の想定外入力に対する耐性を高める。
- 対象ファイル:
- `tests/tst_kifconverter.cpp`
- `tests/tst_ki2converter.cpp`
- `tests/tst_csaconverter.cpp`
- `tests/tst_jkfconverter.cpp`
- `tests/tst_usiconverter.cpp`
- `tests/tst_usenconverter.cpp`
- 実施内容:
- ランダム入力ベースの検証を段階導入する。
- 破綻しやすい境界値（空入力、長大行、記号混在）を強化する。
- 完了条件:
- 既存変換器に異常入力カバレッジが追加される。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_kifconverter|tst_ki2converter|tst_csaconverter|tst_jkfconverter|tst_usiconverter|tst_usenconverter\" --output-on-failure`
- 依存: `ISSUE-020`

## ISSUE-022: wiring 退行検知テスト拡張

- 優先度: P2
- 背景: signal/slot 配線の抜け漏れは実行時に発覚しやすい。
- 対象ファイル:
- `tests/tst_wiring_contracts.cpp`
- `tests/tst_wiring_slot_coverage.cpp`
- 関連 `test_stubs_*`
- 実施内容:
- 接続漏れ、重複接続、依存未注入ケースの検知を追加する。
- 主要 wiring クラスの契約を明文化する。
- 完了条件:
- wiring 変更時の退行検知精度が向上する。
- 検証コマンド:
- `ctest --test-dir build -R \"tst_wiring_contracts|tst_wiring_slot_coverage|tst_wiring_csagame|tst_wiring_analysistab|tst_wiring_consideration|tst_wiring_playerinfo\" --output-on-failure`
- 依存: `ISSUE-011`

## ISSUE-023: KPI差分レポートのCI追加

- 優先度: P2
- 背景: KPIは値の推移が重要で、単発pass/failだけでは悪化を見逃す。
- 対象ファイル:
- `.github/workflows/ci.yml`
- `scripts/extract-kpi-json.sh`
- 実施内容:
- PRの基準ブランチとの差分を Step Summary に出力する。
- 悪化した指標のみを強調表示する。
- 完了条件:
- KPI悪化がレビュー前に視覚的にわかる。
- 検証コマンド:
- GitHub Actions 実行確認
- 依存: `ISSUE-015`

## ISSUE-024: Sanitizer 軽量PRジョブ導入

- 優先度: P2
- 背景: Sanitizer は週次だけでは検知が遅れる。
- 対象ファイル:
- `.github/workflows/ci.yml`
- 実施内容:
- PRで実行可能な軽量サニタイザジョブを追加する。
- 実行時間と失敗率を監視し、必要に応じて対象テストを絞る。
- 完了条件:
- PR段階でサニタイザの基本検知が有効化される。
- 検証コマンド:
- GitHub Actions 実行確認
- 依存: `ISSUE-023`

## 5. マイルストーンとKPI対応

| Milestone | 対象Issue | KPI目標 |
|---|---|---|
| M1（P0完了） | ISSUE-001〜005 | `layer_dependency_violations: 17 -> 0` |
| M2（P1前半） | ISSUE-010〜012 | `mainwindow_include_count: 40 -> 36` |
| M3（P1後半） | ISSUE-013〜015 | `files_over_550: 13 -> 10` |
| M4（P2） | ISSUE-020〜024 | `long_functions_over_100: 136 -> 120`（監視） |

## 6. Issueテンプレート（コピー用）

```md
## 概要
- Issue ID:
- 優先度:
- 目的:

## 対象
- 対象ファイル:

## 実施内容
- 

## 完了条件（DoD）
- 

## 検証
- 実行コマンド:

## 依存Issue
- 
```

