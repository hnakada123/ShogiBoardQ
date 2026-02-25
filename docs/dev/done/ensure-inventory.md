# MainWindow ensure* メソッド棚卸し

## 概要

- **対象**: `src/app/mainwindow.cpp` (4,388行) / `src/app/mainwindow.h` (863行)
- **ensure* メソッド数**: 37個
- **調査日**: 2026-02-25

## ensure* メソッド分類表

| # | メソッド名 | 行数 | 責務 | connect数 | サブシステム | 既存wiring | 備考 |
|---|---|---|---|---|---|---|---|
| 1 | ensureEvaluationGraphController | 16 | create+bind | 0 | Analysis | — | deps設定 |
| 2 | ensureMatchCoordinatorWiring | 20 | create+bind+wire | 5 | Match | MatchCoordinatorWiring | wireMatchWiringSignals別 |
| 3 | ensureTimeController | 8 | create+bind | 0 | Time | — | シンプル |
| 4 | ensureReplayController | 11 | create+bind | 0 | Replay | — | シンプル |
| 5 | ensureDialogCoordinator | 11 | create+bind+wire | 4 | Dialog | — | bind/wire別ヘルパー |
| 6 | ensureKifuExportController | 54 | create+bind+wire | 2 | KifuExport | DialogLaunchWiring | callbacks多数 |
| 7 | ensureGameStateController | 18 | create+bind | 0 | GameState | — | callbacks |
| 8 | ensurePlayerInfoController | 17 | fetch+bind | 0 | PlayerInfo | PlayerInfoWiring | 委譲 |
| 9 | ensureBoardSetupController | 17 | create+bind | 0 | Board | — | callbacks |
| 10 | ensurePvClickController | 18 | create+bind+wire | 1 | PvClick | — | |
| 11 | ensurePositionEditCoordinator | 25 | create+bind | 0 | PositionEdit | — | callbacks |
| 12 | ensureCsaGameWiring | 27 | create+bind+wire | 7 | CSA | CsaGameWiring | wireCsaGameWiringSignals別 |
| 13 | ensureJosekiWiring | 25 | create+bind+wire | 2 | Joseki | JosekiWindowWiring | |
| 14 | ensureMenuWiring | 12 | create+bind | 0 | Menu | MenuWindowWiring | シンプル |
| 15 | ensurePlayerInfoWiring | 30 | create+bind+wire | 1 | PlayerInfo | PlayerInfoWiring | |
| 16 | ensurePreStartCleanupHandler | 28 | create+bind | 0 | GameCleanup | — | deps多数 |
| 17 | ensureTurnSyncBridge | 8 | wire(static) | 0 | TurnSync | TurnSyncBridge | 静的呼出 |
| 18 | ensurePositionEditController | 5 | create | 0 | PositionEdit | — | 最小 |
| 19 | ensureBoardSyncPresenter | 25 | create+bind | 0 | BoardSync | BoardSyncPresenter | |
| 20 | ensureAnalysisPresenter | 5 | create | 0 | Analysis | — | 最小 |
| 21 | ensureGameStartCoordinator | 14 | create+bind+wire | 11 | GameStart | — | wireGameStartCoordinatorSignals別 |
| 22 | ensureRecordPresenter | 19 | create+bind+wire | 1 | Record | — | |
| 23 | ensureLiveGameSessionStarted | 40+ | initialize | 0 | LiveSession | — | 複雑初期化 |
| 24 | ensureKifuLoadCoordinatorForLive | 8 | create | 0 | KifuLoad | — | wrapper |
| 25 | ensureGameRecordModel | 30 | create+bind+wire | 2 | GameRecord | — | |
| 26 | ensureJishogiController | 5 | create | 0 | Jishogi | — | 最小 |
| 27 | ensureNyugyokuHandler | 5 | create | 0 | Nyugyoku | — | 最小 |
| 28 | ensureConsecutiveGamesController | 23 | create+bind+wire | 2 | ConsecutiveGames | — | lambda connect違反あり |
| 29 | ensureLanguageController | 11 | create+bind | 0 | Language | — | |
| 30 | ensureConsiderationUIController | 7 | fetch+bind | 0 | Consideration | ConsiderationWiring | 委譲 |
| 31 | ensureConsiderationWiring | 40 | create+bind+wire | 1 | Consideration | ConsiderationWiring | lazy callback複雑 |
| 32 | ensureDockLayoutManager | 22 | create+bind | 0 | DockLayout | — | dock登録 |
| 33 | ensureDockCreationService | 7 | create+bind | 0 | DockCreation | — | |
| 34 | ensureCommentCoordinator | 20 | create+bind+wire | 1 | Comment | — | |
| 35 | ensureUsiCommandController | 8 | create+bind | 0 | USI | — | |
| 36 | ensureRecordNavigationHandler | 10 | create+bind+wire | 6 | RecordNav | — | wire/bind別ヘルパー |
| 37 | ensureUiStatePolicyManager | 13 | create+bind | 0 | UIState | — | |

## 関連ヘルパーメソッド（ensure* から呼ばれる wire/bind）

| メソッド名 | 行数 | connect数 | 所属サブシステム |
|---|---|---|---|
| wireMatchWiringSignals | 18 | 5 | Match |
| buildMatchHooks | 33 | — | Match (hooks) |
| buildMatchUndoHooks | 9 | — | Match (hooks) |
| buildMatchWiringDeps | 50 | — | Match (deps) |
| initMatchCoordinator | 22 | — | Match (facade) |
| wireGameStartCoordinatorSignals | 40 | 11 | GameStart |
| wireDialogCoordinatorSignals | 17 | 4 | Dialog |
| bindDialogCoordinatorContexts | 42 | — | Dialog (contexts) |
| wireCsaGameWiringSignals | 18 | 7 | CSA |
| wireRecordNavigationHandlerSignals | 14 | 6 | RecordNav |
| bindRecordNavigationHandlerDeps | 18 | — | RecordNav |
| wireBranchNavigationSignals | 15+ | — | BranchNav |

## 分離候補の優先順位

### 推奨実施順序

| 順序 | タスク | 対象 | 関連コード行数 | connect数 | 難易度 | リスク |
|---|---|---|---|---|---|---|
| 1 | Task 44 | RecordNavigationHandler | ~42行 | 6 | 低 | 低 |
| 2 | Task 43 | DialogCoordinator | ~70行 | 4 | 中 | 中 |
| 3 | Task 45 | ConsiderationWiring拡充 | ~47行 | 1 | 中 | 中 |
| 4 | Task 42 | MatchCoordinator | ~152行 | 16 | 高 | 高 |

### 順序の根拠

1. **Task 44 (RecordNavigationHandler)** を最初にする理由:
   - 既に create/wire/bind のヘルパー分離済み → 移動が容易
   - 他サブシステムとの依存が少ない
   - 分離パターンの確立に最適

2. **Task 43 (DialogCoordinator)** を2番目にする理由:
   - Task 40（ConsiderationPositionResolver統合）完了後に実施すると効率的
   - 3つのコンテキスト設定は機械的だが量が多い
   - DialogOrchestrator経由の配線パターンが参考になる

3. **Task 45 (ConsiderationWiring拡充)** を3番目にする理由:
   - Task 43 完了後、DialogCoordinator の配線が整理済みで作業しやすい
   - `ensureDialogCoordinator` のlazy callbackがTask 43で改善されている前提
   - ConsiderationWiring 既存クラスの拡張なので設計負荷は低い

4. **Task 42 (MatchCoordinator)** を最後にする理由:
   - 最も複雑（hooks/undoHooks/deps構築 + GameStartCoordinator配線 = 計152行, 16 connect）
   - MainWindow メンバへの参照が最も多い
   - MatchCoordinatorWiring 既存だが、builder メソッドの移動に設計判断が必要
   - GameStartCoordinator（11 connect）も含めて一括処理が妥当
   - 他タスクで確立したパターンを活用して安全に実施

## 追加発見・注意点

### スコープ外だが将来の候補
- `wireBranchNavigationSignals` + 関連メソッド: 分岐ナビゲーション配線の分離
- `ensureCsaGameWiring` + `wireCsaGameWiringSignals`: CSA 配線の統合（7 connect、既存CsaGameWiring拡張）
- `ensureConsecutiveGamesController`: lambda connect 違反の修正が必要

### 依存関係の注意
- `ensureDialogCoordinator` → `ensureConsiderationWiring` → `ensureDialogCoordinator`（循環コールバック）
- `ensureGameStartCoordinator` は `GameSessionFacade` 経由で生成される（`initMatchCoordinator` 内）
- `ensureUiStatePolicyManager` は複数の wire* メソッドから呼ばれる共通依存

### 既存 wiring クラス一覧（`src/ui/wiring/`）

| クラス | 用途 | 対応する ensure* |
|---|---|---|
| MatchCoordinatorWiring | 対局オーケストレーション | ensureMatchCoordinatorWiring |
| PlayerInfoWiring | 対局者情報UI | ensurePlayerInfoWiring |
| ConsiderationWiring | 検討モード | ensureConsiderationWiring |
| CsaGameWiring | CSAネットワーク対局 | ensureCsaGameWiring |
| JosekiWindowWiring | 定跡ウィンドウ | ensureJosekiWiring |
| MenuWindowWiring | メニューバー | ensureMenuWiring |
| DialogLaunchWiring | ダイアログ起動 | — (Task 24で抽出済み) |
| AnalysisTabWiring | 解析タブ | — |
| RecordPaneWiring | 棋譜ペイン | — |
| UiActionsWiring | UIアクション | — |
