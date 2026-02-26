# MainWindow メソッド一覧と責務分類

作成日: 2026-02-25
対象: `src/app/mainwindow.h` / `src/app/mainwindow.cpp`

## ベースライン

| 指標 | 値 |
|---|---|
| `mainwindow.cpp` 行数 | 3,444 |
| `mainwindow.h` 行数 | 798 |
| `MainWindow::` メソッド数 | 171（コンストラクタ/デストラクタ含む） |

---

## 分類凡例

| 分類 | 説明 | 移譲先フェーズ |
|---|---|---|
| **(A)** | UIイベント受信・委譲（MainWindowに残す） | — |
| **(B)** | プレイモード判定ロジック | Phase 1 |
| **(C)** | 手番同期ロジック | Phase 2 |
| **(D)** | 棋譜追記・ライブ更新ロジック | Phase 3 |
| **(E)** | SFEN適用・分岐同期ロジック | Phase 4 |
| **(F)** | セッション制御ロジック | Phase 5 |
| **(G)** | 配線組み立てロジック | Phase 6 |
| **(H)** | UI構築ロジック | Phase 7 |

---

## 全メソッド一覧

### コンストラクタ / デストラクタ

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 147 | `MainWindow()` | **(H)** | UI構築オーケストレーション |
| 227 | `~MainWindow()` | **(A)** | デフォルト |

### public メソッド

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 134 | `evalChart()` | **(A)** | インライン getter |
| 1333 | `kifuExportController()` | **(A)** | ensure + getter |

### public slots — ファイル I/O

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 633 | `saveShogiBoardImage()` | **(A)** | BoardImageExporter へ委譲 |
| 639 | `saveEvaluationGraphImage()` | **(A)** | BoardImageExporter へ委譲 |
| 620 | `copyBoardToClipboard()` | **(A)** | BoardImageExporter へ委譲 |
| 626 | `copyEvalGraphToClipboard()` | **(A)** | BoardImageExporter へ委譲 |
| 782 | `openWebsiteInExternalBrowser()` | **(A)** | DialogCoordinator へ委譲 |

### public slots — エラー / 一般UI

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 547 | `displayErrorMessage()` | **(A)** | QMessageBox 表示 |
| 923 | `saveSettingsAndClose()` | **(F)** | 設定保存 + エンジン終了 + quit |
| 936 | `resetToInitialState()` | **(F)** | リセットシーケンス全体 |
| 956 | `resetEngineState()` | **(F)** | エンジン・CSA・連続対局停止 |
| 970 | `resetGameState()` | **(F)** | 状態変数クリア |
| 1013 | `resetModels()` | **(F)** | モデルクリア（ResetService委譲済み） |
| 1035 | `resetUiState()` | **(F)** | UI状態リセット（ResetService委譲済み） |
| 803 | `stopTsumeSearch()` | **(A)** | MC へ委譲 |
| 791 | `updateJosekiWindow()` | **(A)** | JosekiWiring へ委譲 |

### public slots — ツールバー / 操作

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 3259 | `onToolBarVisibilityToggled()` | **(A)** | ツールバー表示切替 + Settings保存 |
| 558 | `undoLastTwoMoves()` | **(B)** | PlayMode による switch 分岐あり |
| 1124 | `beginPositionEditing()` | **(A)** | PositionEditCoordinator へ委譲 |
| 1137 | `finishPositionEditing()` | **(A)** | PositionEditCoordinator へ委譲 |
| 887 | `initializeGame()` | **(F)** | GameStartService + GameStartCoordinator |
| 732 | `handleResignation()` | **(B)** | PlayMode 判定 + CSA/通常分岐 |
| 1223 | `onPlayerNamesResolved()` | **(F)** | 対局者名確定 + GameInfo設定 |
| 1859 | `onActionFlipBoardTriggered()` | **(A)** | MC へ委譲 |
| 1865 | `onActionEnlargeBoardTriggered()` | **(A)** | ShogiView へ委譲 |
| 1872 | `onActionShrinkBoardTriggered()` | **(A)** | ShogiView へ委譲 |
| 2167 | `handleBreakOffGame()` | **(A)** | GameStateController へ委譲 |
| 1149 | `movePieceImmediately()` | **(A)** | MC へ委譲 |
| 2955 | `onRecordPaneMainRowChanged()` | **(A)** | RecordNavigationHandler へ委譲 |

### protected

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1109 | `closeEvent()` | **(A)** | ウィンドウライフサイクル |

### private slots — 対局終了 / 状態変化

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1829 | `onMatchGameEnded()` | **(F)** | 終局処理 + 連続対局判定 |
| 2158 | `onGameOverStateChanged()` | **(A)** | GameStateController へ委譲 |
| 813 | `onTurnManagerChanged()` | **(C)** | 手番UI更新 + GCへの同期 |
| 2076 | `flipBoardAndUpdatePlayerInfo()` | **(A)** | 盤反転 + 名前/時計再描画 |

### private slots — タブ / ナビゲーション

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 3236 | `onTabCurrentChanged()` | **(A)** | タブインデックスSettings保存 |
| 720 | `enableArrowButtons()` | **(A)** | UiStatePolicy ガード付き委譲 |

### private slots — 盤面・反転

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2122 | `onBoardFlipped()` | **(A)** | bottomIsP1トグル + 表示更新 |
| 2131 | `onBoardSizeChanged()` | **(A)** | セントラルウィジェットサイズ調整 |
| 2148 | `performDeferredEvalChartResize()` | **(A)** | デバウンス後のサイズ調整 |

### private slots — 司令塔通知

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1879 | `onRequestAppendGameOverMove()` | **(D)** | 終局手追記 + LiveGameSession commit |

### private slots — 移動要求

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1931 | `onMoveRequested()` | **(A)** | BoardSetupController へ委譲（状態同期あり） |

### private slots — リプレイ

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1951 | `setReplayMode()` | **(A)** | ReplayController へ委譲 |

### private slots — 定跡ウィンドウ

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 3248 | `onJosekiForcedPromotion()` | **(A)** | GameController へ委譲 |

### private slots — 内部配線

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1910 | `connectBoardClicks()` | **(G)** | BoardSetupController へ委譲 |
| 1919 | `connectMoveRequested()` | **(G)** | connect() 呼び出し |

### private slots — 棋譜表示 / 同期

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2037 | `onMoveCommitted()` | **(D)** | USI指し手記録 + currentSfen更新 + 定跡更新 |
| 1050 | `displayGameRecord()` | **(D)** | 棋譜読み込み時のモデル初期化 + Presenter委譲 |
| 1319 | `syncBoardAndHighlightsAtRow()` | **(E)** | KifuNavigationCoordinator へ委譲 |
| 2870 | `onRecordRowChangedByPresenter()` | **(A)** | 未保存コメント確認 + コメント表示 |
| 3210 | `onPvRowClicked()` | **(A)** | PvClickController へ委譲（状態同期あり） |
| 3229 | `onPvDialogClosed()` | **(A)** | AnalysisTab selection clear |

### private slots — 分岐ノード

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1962 | `onBranchNodeActivated()` | **(E)** | BranchNavigationWiring へ転送 |
| 1969 | `onBranchNodeHandled()` | **(E)** | KifuNavigationCoordinator へ委譲 |
| 2962 | `onBuildPositionRequired()` | **(D)** | 検討モード局面解決 |
| 1978 | `onBranchTreeBuilt()` | **(E)** | BranchNavigationWiring へ転送 |
| 1984 | `loadBoardFromSfen()` | **(E)** | 盤面SFEN適用 + 手番更新 + currentSfen更新 |
| 2013 | `loadBoardWithHighlights()` | **(E)** | BoardSyncPresenter委譲 + ガード管理 |

### private slots — エラー / 前準備

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 473 | `onErrorBusOccurred()` | **(A)** | displayErrorMessage() へ転送 |
| 2694 | `onPreStartCleanupRequested()` | **(F)** | PreStartCleanupHandler + UI クリア |
| 2750 | `onApplyTimeControlRequested()` | **(F)** | 時間設定保存 + TC適用 + GameInfo更新 |

### private slots — 投了

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2907 | `onResignationTriggered()` | **(A)** | handleResignation() へ転送 |

### private slots — 連続対局

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1253 | `onConsecutiveGamesConfigured()` | **(F)** | ConsecutiveGamesController 設定 |
| 1263 | `onGameStarted()` | **(F)** | playMode同期 + 定跡更新 + 連続対局通知 |
| 1282 | `startNextConsecutiveGame()` | **(F)** | 次局開始判定 + 委譲 |
| 1293 | `onRequestSelectKifuRow()` | **(A)** | 棋譜行選択 + 盤面同期 |
| 1312 | `onBranchTreeResetForNewGame()` | **(F)** | BranchNavigationWiring へ委譲 |

### private — UI / 表示更新

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2704 | `clearSessionDependentUi()` | **(F)** | ResetService 委譲済み |
| 2727 | `clearUiBeforeKifuLoad()` | **(F)** | ResetService 委譲済み |
| 3141 | `updateGameRecord()` | **(D)** | 棋譜1手追記 + LiveSession更新 |
| 766 | `updateTurnStatus()` | **(C)** | Clock手番設定 + View手番表示 |
| 748 | `ensureEvaluationGraphController()` | **(G)** | 遅延初期化 |

### private — 初期化 / セットアップ

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 479 | `initializeComponents()` | **(H)** | GC/View/Board/Turn初期化オーケストレーション |
| 499 | `initializeGameControllerAndKifu()` | **(H)** | GC生成 + 棋譜リストクリア |
| 517 | `initializeOrResetShogiView()` | **(H)** | ShogiView 生成/再初期化 |
| 529 | `initializeBoardModel()` | **(H)** | SFEN正規化 + newGame + View接続 |
| 667 | `setupHorizontalGameLayout()` | **(H)** | 空メソッド（レガシー） |
| 673 | `initializeCentralGameDisplay()` | **(H)** | 空メソッド（レガシー） |
| 1819 | `ensureTimeController()` | **(G)** | 遅延初期化 |
| 1659 | `ensureMatchCoordinatorWiring()` | **(G)** | 遅延初期化 + Deps更新 |
| 1678 | `buildMatchWiringDeps()` | **(G)** | ~80行の Deps 構築 |
| 1758 | `wireMatchWiringSignals()` | **(G)** | 転送シグナル接続 |
| 1802 | `initMatchCoordinator()` | **(G)** | MC/GSC生成 + 配線 |
| 1380 | `setupRecordPane()` | **(H)** | RecordPaneWiring 生成 + 構築 |
| 1408 | `setupEngineAnalysisTab()` | **(H)** | AnalysisTabWiring 生成 + 構築 |
| 1440 | `connectAnalysisTabSignals()` | **(G)** | AnalysisTab シグナル接続 |
| 1486 | `configureAnalysisTabDependencies()` | **(G)** | AnalysisTab 依存設定 |
| 1893 | `setupBoardInteractionController()` | **(G)** | BoardSetupController 委譲 |
| 1514 | `createEvalChartDock()` | **(H)** | DockCreationService 委譲 |
| 1531 | `createRecordPaneDock()` | **(H)** | DockCreationService 委譲 |
| 1545 | `createAnalysisDocks()` | **(H)** | DockCreationService 委譲 |
| 1577 | `setupBoardInCenter()` | **(H)** | セントラルウィジェット配置 |
| 1604 | `createMenuWindowDock()` | **(H)** | DockCreationService 委譲 |
| 1620 | `createJosekiWindowDock()` | **(H)** | DockCreationService 委譲 |
| 1643 | `createAnalysisResultsDock()` | **(H)** | DockCreationService 委譲 |
| 1340 | `initializeBranchNavigationClasses()` | **(H)** | BranchNavigationWiring 委譲 |

### private — ゲーム開始 / 切替

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 598 | `initializeNewGame()` | **(F)** | GC newGame + resume適用 |
| 608 | `setPlayersNamesForMode()` | **(B)** | PlayerInfoController 委譲（PlayMode依存） |
| 647 | `setEngineNamesBasedOnMode()` | **(B)** | PlayerInfoController 委譲（PlayMode依存） |
| 657 | `updateSecondEngineVisibility()` | **(B)** | PlayerInfoController 委譲（PlayMode依存） |
| 681 | `startNewShogiGame()` | **(F)** | 評価グラフ初期化 + MC prepareAndStartGame |

### private — 入出力 / 設定

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1098 | `saveWindowAndBoardSettings()` | **(A)** | SettingsService + DockLayoutManager |
| 1092 | `loadWindowSettings()` | **(A)** | SettingsService 委譲 |

### private — ユーティリティ

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1166 | `appendKifuLine()` | **(D)** | lastMove設定 + updateGameRecord呼出 |
| 832 | `setCurrentTurn()` | **(C)** | TurnManager確保 + 手番ソース判定 |
| 1157 | `setGameOverMove()` | **(A)** | GameStateController へ委譲 |
| 876 | `sfenRecord()` | **(A)** | MC へのアクセサ |
| 881 | `sfenRecord() const` | **(A)** | MC へのアクセサ (const) |

### private — 手番チェック / 判定

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 1186 | `isHumanTurnNow()` | **(B)** | PlayMode switch（~30行） |
| 3074 | `isGameActivelyInProgress()` | **(B)** | PlayMode switch |
| 3091 | `isHvH()` | **(B)** | GameStateController委譲 + フォールバック |
| 3100 | `isHumanSide()` | **(B)** | GameStateController委譲 + フォールバック switch |

### private — フォント

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2097 | `setupNameAndClockFonts()` | **(H)** | フォント設定 |

### private — リプレイ

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2176 | `ensureReplayController()` | **(G)** | 遅延初期化 |

### private — 遅延初期化（ensure系）

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2643 | `ensurePositionEditController()` | **(G)** | |
| 2650 | `ensureBoardSyncPresenter()` | **(G)** | |
| 2677 | `ensureAnalysisPresenter()` | **(G)** | |
| 2684 | `ensureGameStartCoordinator()` | **(G)** | |
| 2779 | `ensureRecordPresenter()` | **(G)** | |
| 2800 | `ensureLiveGameSessionStarted()` | **(G)** | |
| 2806 | `ensureLiveGameSessionUpdater()` | **(G)** | |
| 3061 | `ensureKifuLoadCoordinatorForLive()` | **(G)** | |
| 3168 | `ensureGameRecordModel()` | **(G)** | |
| 2268 | `ensureDialogCoordinator()` | **(G)** | CompositionRoot 委譲 |
| 2285 | `ensureKifuFileController()` | **(G)** | CompositionRoot 委譲 |
| 2312 | `ensureKifuExportController()` | **(G)** | |
| 2334 | `updateKifuExportDependencies()` | **(G)** | |
| 2368 | `ensureGameStateController()` | **(G)** | CompositionRoot 委譲 |
| 2417 | `ensurePlayerInfoController()` | **(G)** | CompositionRoot 委譲 |
| 2425 | `ensureBoardSetupController()` | **(G)** | CompositionRoot 委譲 |
| 2449 | `ensurePvClickController()` | **(G)** | CompositionRoot 委譲 |
| 2459 | `ensurePositionEditCoordinator()` | **(G)** | CompositionRoot 委譲 |
| 2483 | `ensureCsaGameWiring()` | **(G)** | |
| 2514 | `wireCsaGameWiringSignals()` | **(G)** | |
| 2530 | `ensureJosekiWiring()` | **(G)** | |
| 2557 | `ensureMenuWiring()` | **(G)** | |
| 2571 | `ensurePlayerInfoWiring()` | **(G)** | |
| 2603 | `ensurePreStartCleanupHandler()` | **(G)** | |
| 2633 | `ensureTurnSyncBridge()` | **(G)** | |
| 3271 | `ensureJishogiController()` | **(G)** | |
| 3278 | `ensureNyugyokuHandler()` | **(G)** | |
| 3285 | `ensureConsecutiveGamesController()` | **(G)** | |
| 3310 | `ensureLanguageController()` | **(G)** | |
| 3323 | `ensureConsiderationWiring()` | **(G)** | CompositionRoot 委譲 |
| 3338 | `ensureDockLayoutManager()` | **(G)** | |
| 3362 | `ensureDockCreationService()` | **(G)** | |
| 3371 | `ensureCommentCoordinator()` | **(G)** | CompositionRoot 委譲 |
| 3380 | `ensureUsiCommandController()` | **(G)** | |
| 3390 | `ensureRecordNavigationHandler()` | **(G)** | CompositionRoot 委譲 |
| 3397 | `ensureUiStatePolicyManager()` | **(G)** | CompositionRoot 委譲 |
| 3403 | `ensureKifuNavigationCoordinator()` | **(G)** | |
| 1346 | `ensureBranchNavigationWiring()` | **(G)** | |

### private — 配線ヘルパー

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2389 | `syncNavStateToPly()` | **(E)** | 分岐ナビ状態同期 |
| 1326 | `navigateKifuViewToRow()` | **(A)** | KifuNavigationCoordinator へ委譲 |

### private — コンストラクタ分割先

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 230 | `setupCentralWidgetContainer()` | **(H)** | Central Widget 構築 |
| 239 | `configureToolBarFromUi()` | **(H)** | ToolBar 設定 |
| 260 | `buildGamePanels()` | **(H)** | パネル構築オーケストレーション（~66行） |
| 326 | `restoreWindowAndSync()` | **(H)** | ウィンドウ設定復元 |
| 337 | `initializeDialogLaunchWiring()` | **(G)** | DialogLaunchWiring 生成（~56行） |
| 393 | `connectAllActions()` | **(G)** | UiActionsWiring 生成 + wire() |
| 417 | `connectCoreSignals()` | **(G)** | GC/View/ErrorBus 接続 |
| 447 | `installAppToolTips()` | **(H)** | ToolTipFilter 設定 |
| 463 | `finalizeCoordinators()` | **(G)** | MC + フォント + 各ensure |

### private — KifuLoadCoordinator

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 3003 | `createAndWireKifuLoadCoordinator()` | **(G)** | KLC 生成 + 全配線（~58行） |

### private — ファクトリ / フック

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2189 | `buildRuntimeRefs()` | **(G)** | RuntimeRefs構築（~79行） |
| 2822 | `initializeNewGameHook()` | **(F)** | MC→UI初期化コールバック |
| 2849 | `renderBoardFromGc()` | **(E)** | GC→View盤面反映 |
| 2857 | `showMoveHighlights()` | **(A)** | BoardController へ委譲 |
| 2863 | `appendKifuLineHook()` | **(D)** | appendKifuLine() への転送 |

### private — 時間取得ヘルパー

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2914 | `getRemainingMsFor()` | **(C)** | TimeController へ委譲 |
| 2925 | `getIncrementMsFor()` | **(C)** | TimeController へ委譲 |
| 2936 | `getByoyomiMs()` | **(C)** | TimeController へ委譲 |

### private — ゲームオーバー / 表示

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 2946 | `showGameOverMessageBox()` | **(A)** | DialogCoordinator へ委譲 |

### private — 手番同期

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 3124 | `updateTurnAndTimekeepingDisplay()` | **(C)** | setCurrentTurn + Clock再描画 |

### private — ガード / 判定

| 行 | メソッド | 分類 | 備考 |
|---|---|---|---|
| 3074 | `isGameActivelyInProgress()` | **(B)** | （上記「手番チェック/判定」にも記載） |
| 3091 | `isHvH()` | **(B)** | （上記「手番チェック/判定」にも記載） |
| 3100 | `isHumanSide()` | **(B)** | （上記「手番チェック/判定」にも記載） |
| 3074 | `isGameActivelyInProgress()` | **(B)** | |

---

## 分類別サマリ

| 分類 | メソッド数 | 備考 |
|---|---|---|
| **(A)** UIイベント受信・委譲 | 約48 | 最終形で MainWindow に残る |
| **(B)** プレイモード判定 | 約10 | Phase 1 → `PlayModePolicyService` |
| **(C)** 手番同期 | 約7 | Phase 2 → `TurnStateSyncService` |
| **(D)** 棋譜追記・ライブ更新 | 約8 | Phase 3 → `GameRecordUpdateService` |
| **(E)** SFEN適用・分岐同期 | 約9 | Phase 4 → `BoardLoadService` + `KifuNavigationCoordinator`拡張 |
| **(F)** セッション制御 | 約24 | Phase 5 → `SessionLifecycleCoordinator` |
| **(G)** 配線組み立て | 約56 | Phase 6 → `MainWindowWiringAssembler` |
| **(H)** UI構築 | 約19 | Phase 7 → `MainWindowUiBootstrapper` |

**合計: 171 メソッド**（一部のメソッドは重複分類を含む）

---

## 移譲による削減見込み

現状 3,444行 → 目標 2,200行以下（計画書 §10 の定量目標）

- **(B)** ~150行削減（判定ロジック）
- **(C)** ~120行削減（手番同期）
- **(D)** ~200行削減（棋譜更新）
- **(E)** ~150行削減（SFEN適用）
- **(F)** ~350行削減（セッション制御）
- **(G)** ~500行削減（配線の大部分は既にCompositionRoot経由だが、buildMatchWiringDeps/buildRuntimeRefs等が大きい）
- **(H)** ~200行削減（UI構築オーケストレーション）

合計想定削減: ~1,670行 → 残り ~1,774行（目標達成見込み）
