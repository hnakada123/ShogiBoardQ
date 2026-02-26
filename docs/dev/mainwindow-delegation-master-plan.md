# MainWindow責務移譲マスタープラン

## 1. 目的
`MainWindow` クラスの肥大化を解消し、`MainWindow` を「画面ルートのFacade（配線入口）」に限定する。
本書は、**移譲すべき候補全件**と、各候補をどのように既存/新規クラスへ移すかを実装手順レベルで定義する。

## 2. 現状スナップショット
- `src/app/mainwindow.cpp`: 2754行
- `src/app/mainwindow.h`: 839行
- `MainWindow::` メソッド数: 183
- `ensure*` メソッド数: 47
- `#include`（`mainwindow.cpp`）: 113

## 3. 完了定義
以下を満たした時点を完了とする。
1. `MainWindow` に業務ロジック・UI詳細ロジック・依存生成ロジックが残っていない。
2. `MainWindow` の主責務が「画面起動時の最小配線」「Facade APIの公開」のみになっている。
3. 既存機能（対局、棋譜、分岐、評価値グラフ、各ダイアログ、設定復元）が回帰しない。
4. `MainWindow` の `#include` と privateメンバ、`ensure*` が大幅減少している。

## 4. 移譲対象全件（カテゴリ別）

### C01. UI外観/ウィンドウ表示制御
対象メソッド:
`setupCentralWidgetContainer`, `configureToolBarFromUi`, `installAppToolTips`, `setupBoardInCenter`, `onBoardSizeChanged`, `performDeferredEvalChartResize`, `setupNameAndClockFonts`, `flipBoardAndUpdatePlayerInfo`, `onBoardFlipped`, `onActionEnlargeBoardTriggered`, `onActionShrinkBoardTriggered`, `onToolBarVisibilityToggled`, `onTabCurrentChanged`

移譲先:
- 既存拡張: `DockLayoutManager`, `SettingsService`
- 新規: `MainWindowAppearanceController`（`src/ui/controllers/`）

移譲手順:
1. `MainWindowAppearanceController::Deps` を作成（`ShogiView`, `QWidget* central`, `QTimer* resizeTimer`, `QToolBar*`, `QAction* actionToolBar` など）。
2. ツールバー設定・ツールチップ設定・フォント設定・盤サイズ追従処理を移す。
3. 盤反転時の描画更新（`setFlipMode/setPieces/setPiecesFlip`）と時計表示更新を controller 側へ移す。
4. タブインデックス保存/ツールバー表示保存も controller 側で `SettingsService` を直接利用させる。
5. `MainWindow` では `m_appearanceController->...()` 呼び出しだけ残す。

検証観点:
- 起動時ツールバー表示状態復元
- Ctrl+ホイール後の盤サイズ追従
- 盤反転時の名前/時計/向き
- 最終選択タブの保存復元

---

### C02. 起動時コア初期化と局面初期化
対象メソッド:
`initializeComponents`, `initializeGameControllerAndKifu`, `initializeOrResetShogiView`, `initializeBoardModel`, `initializeNewGame`, `initializeNewGameHook`, `renderBoardFromGc`

移譲先:
- 既存拡張: `MainWindowUiBootstrapper`, `MainWindowGameStartService`
- 新規: `MainWindowCoreInitCoordinator`（`src/app/`）

移譲手順:
1. `MainWindowCoreInitCoordinator` に `ShogiGameController`/`ShogiView`/`m_state`/`m_kifu` の初期化ロジックを集約。
2. SFEN正規化と `newGame` 実行を同コーディネータへ移す。
3. `initializeNewGameHook`/`renderBoardFromGc` は `MatchCoordinatorWiring` から新コーディネータへ直接バインド。
4. `MainWindow` は起動時に `coreInit->initialize()` を1回呼ぶだけにする。

検証観点:
- 平手開始/途中局面開始/`startpos` 正規化
- 新規対局直後の盤表示
- Hook経由の盤再描画

---

### C03. アクション/シグナル配線
対象メソッド:
`initializeDialogLaunchWiring`, `connectAllActions`, `connectCoreSignals`, `connectBoardClicks`, `connectMoveRequested`, `connectAnalysisTabSignals`, `wireCsaGameWiringSignals`, `wireMatchWiringSignals`, `onErrorBusOccurred`

移譲先:
- 既存拡張: `MainWindowWiringAssembler`, `UiActionsWiring`, `RecordPaneWiring`, `AnalysisTabWiring`
- 新規: `MainWindowSignalRouter`（`src/ui/wiring/`）

移譲手順:
1. `MainWindowSignalRouter` に「接続のみ」を集約し、`MainWindow` スロット直結を削減。
2. `onErrorBusOccurred` は `ErrorNotificationService` へ移し、router から service を呼ぶ。
3. `connectAnalysisTabSignals` のコメント/USI/検討系接続を `AnalysisTabWiring` 側に取り込む。
4. `wireCsaGameWiringSignals`/`wireMatchWiringSignals` は assembler 側へ統合し `MainWindow` から削除。

検証観点:
- 二重接続なし（`Qt::UniqueConnection` 維持）
- 主要メニュー操作が全て有効
- ErrorBus 経由のエラー表示

---

### C04. ドック/パネル構築
対象メソッド:
`setupRecordPane`, `setupEngineAnalysisTab`, `configureAnalysisTabDependencies`, `createEvalChartDock`, `createRecordPaneDock`, `createAnalysisDocks`, `createMenuWindowDock`, `createJosekiWindowDock`, `createAnalysisResultsDock`, `updateJosekiWindow`, `initializeBranchNavigationClasses`

移譲先:
- 既存拡張: `MainWindowUiBootstrapper`, `DockCreationService`, `BranchNavigationWiring`, `PlayerInfoWiring`
- 新規: `MainWindowDockBootstrapper`（`src/app/`）

移譲手順:
1. ドック生成順序の責務を `MainWindowDockBootstrapper` に固定化。
2. `setupEngineAnalysisTab` と依存設定（`configureAnalysisTabDependencies`）を `AnalysisTabWiring` 側へ寄せる。
3. `updateJosekiWindow` は `JosekiWindowWiring` 側に遅延更新APIを追加して MainWindow 直判定を削除。
4. `MainWindowUiBootstrapper` は `DockBootstrapper` を呼ぶだけに簡素化。

検証観点:
- 起動時ドック構成と表示/非表示
- 定跡ドック表示時更新
- 解析タブと対局情報タブの連携

---

### C05. 対局開始/終了ライフサイクル
対象メソッド:
`initializeGame`, `startNewShogiGame`, `invokeStartGame`, `onGameStarted`, `onMatchGameEnded`, `onRequestAppendGameOverMove`, `onGameOverStateChanged`, `handleBreakOffGame`, `onPreStartCleanupRequested`, `onConsecutiveStartRequested`, `onApplyTimeControlRequested`, `startNextConsecutiveGame`, `onConsecutiveGamesConfigured`, `movePieceImmediately`, `handleResignation`, `onResignationTriggered`, `setGameOverMove`, `stopTsumeSearch`, `openWebsiteInExternalBrowser`

移譲先:
- 既存拡張: `SessionLifecycleCoordinator`, `ConsecutiveGamesController`, `GameStateController`, `DialogCoordinator`
- 新規: `GameSessionOrchestrator`（`src/game/` か `src/app/`）

移譲手順:
1. 開始前クリーンアップ～開始～終局後処理を `GameSessionOrchestrator` に一本化。
2. `SessionLifecycleCoordinator` は状態遷移の純粋処理、`GameSessionOrchestrator` はイベントオーケストレーションに分離。
3. `onRequestAppendGameOverMove` の `LiveGameSession::commit` を終局フロー側へ移す。
4. 投了/中断/詰み探索停止/連続対局遷移の入口を統合する。

検証観点:
- 通常対局/連続対局/CSA の終局遷移
- 投了手追記とライブセッション commit 順序
- 時間設定適用と対局情報更新

---

### C06. セッションリセット/終了保存
対象メソッド:
`saveSettingsAndClose`, `performShutdownSequence`, `saveWindowAndBoardSettings`, `closeEvent`, `resetToInitialState`, `resetEngineState`, `resetGameState`, `clearGameStateFields`, `resetModels`, `resetUiState`, `clearEvalState`, `unlockGameOverStyle`, `clearSessionDependentUi`, `clearUiBeforeKifuLoad`

移譲先:
- 既存拡張: `SessionLifecycleCoordinator`, `MainWindowResetService`
- 新規: `MainWindowShutdownService`, `MainWindowStateStore`

移譲手順:
1. `m_state/m_player/m_kifu` の直接操作を `MainWindowStateStore` 経由へ変更。
2. `clearGameStateFields` を state store の `resetForNewSession()` に置換。
3. 終了保存とエンジン破棄を `MainWindowShutdownService` に移す。
4. `closeEvent` は `shutdownService->beforeClose()` のみ呼ぶ形にする。

検証観点:
- 設定保存（ウィンドウ、盤、ドック）
- 新規対局時の完全初期化
- 既存エンジン・CSA停止漏れなし

---

### C07. 盤面操作/着手入力
対象メソッド:
`setupBoardInteractionController`, `onMoveRequested`, `onMoveCommitted`, `onJosekiForcedPromotion`, `loadBoardFromSfen`, `loadBoardWithHighlights`, `showMoveHighlights`

移譲先:
- 既存拡張: `BoardSetupController`, `BoardLoadService`, `GameRecordUpdateService`
- 新規: `BoardMoveOrchestrator`

移譲手順:
1. `onMoveRequested` の依存再設定処理を `BoardMoveOrchestrator::updateDepsFromRuntimeRefs()` に切り出す。
2. `onMoveCommitted` の「盤更新 + 棋譜更新 + 定跡更新」を orchestrator 側へ移す。
3. `forcedPromotion` は `JosekiWindowWiring` から `BoardSetupController` に直接接続。

検証観点:
- 人間着手→棋譜更新→盤ハイライト
- 検討モード/CSAモードの着手分岐
- 成り強制の反映

---

### C08. 棋譜ナビゲーション/分岐同期
対象メソッド:
`onRequestSelectKifuRow`, `onRecordPaneMainRowChanged`, `syncBoardAndHighlightsAtRow`, `navigateKifuViewToRow`, `onBranchNodeActivated`, `onBranchNodeHandled`, `onBranchTreeBuilt`, `onBranchTreeResetForNewGame`, `syncNavStateToPly`, `onBuildPositionRequired`, `onRecordRowChangedByPresenter`, `displayGameRecord`

移譲先:
- 既存拡張: `KifuNavigationCoordinator`, `RecordNavigationWiring`, `RecordNavigationHandler`, `BranchNavigationWiring`, `GameRecordLoadService`
- 新規: `KifuNavigationFacade`

移譲手順:
1. `onRequestSelectKifuRow` の直接 `QTableView` 操作を `KifuNavigationCoordinator::navigateToRow()` へ統合。
2. `syncNavStateToPly` の探索ロジックを `KifuNavigationCoordinator` 側へ移す。
3. `onRecordRowChangedByPresenter` のコメント連携を `CommentCoordinator` と `RecordNavigationHandler` の責務へ寄せる。
4. `MainWindow` は `recordNavFacade->onXxx()` の転送のみ残す。

検証観点:
- 本譜/分岐ラインでの行選択同期
- `currentSfenStr` 不整合再発防止
- コメント表示/未保存確認の挙動

---

### C09. プレイヤー情報/表示同期
対象メソッド:
`setPlayersNamesForMode`, `setEngineNamesBasedOnMode`, `updateSecondEngineVisibility`, `onPlayerNamesResolved`, `enableArrowButtons`, `onPvDialogClosed`

移譲先:
- 既存拡張: `PlayerInfoController`, `PlayerInfoWiring`, `UiStatePolicyManager`
- 新規: `PlayerPresentationCoordinator`

移譲手順:
1. 名前・エンジン名・表示切替を `PlayerPresentationCoordinator` に統合。
2. `enableArrowButtons` は `UiStatePolicyManager` 側に `enableNavigationIfAllowed()` を追加し呼び出す。
3. `onPvDialogClosed` は `PvClickController` と `EngineAnalysisTab` 間の直接接続へ変更。

検証観点:
- 対局モード別の名前表示
- EvE時の2エンジン表示切替
- PVダイアログ閉時の選択解除

---

### C10. 棋譜/画像/通知I/O
対象メソッド:
`copyBoardToClipboard`, `copyEvalGraphToClipboard`, `saveShogiBoardImage`, `saveEvaluationGraphImage`, `displayErrorMessage`, `appendKifuLine`, `appendKifuLineHook`, `updateGameRecord`, `createAndWireKifuLoadCoordinator`, `ensureKifuLoadCoordinatorForLive`, `kifuExportController`

移譲先:
- 既存拡張: `BoardImageExporter`, `GameRecordUpdateService`, `KifuLoadCoordinatorFactory`, `KifuExportDepsAssembler`
- 新規: `UiNotificationService`, `KifuIoFacade`

移譲手順:
1. メッセージボックス表示と `errorOccurred` 更新を `UiNotificationService` に移す。
2. `appendKifuLineHook`/`updateGameRecord` は `GameRecordUpdateService` へ一本化し hook を廃止。
3. Kifu load/export 入口を `KifuIoFacade` にまとめる。

検証観点:
- 画像保存/コピー
- エラー通知
- 棋譜追記とモデル反映

---

### C11. ポリシー判定/時間取得アクセサ
対象メソッド:
`isHumanTurnNow`, `isGameActivelyInProgress`, `isHvH`, `isHumanSide`, `getRemainingMsFor`, `getIncrementMsFor`, `getByoyomiMs`, `sfenRecord`, `sfenRecord() const`

移譲先:
- 既存拡張: `PlayModePolicyService`, `TimeControlController`
- 新規: `MatchRuntimeQueryService`

移譲手順:
1. 判定・取得系を `MatchRuntimeQueryService` に寄せ、`MainWindow` から状態参照を排除。
2. `MatchCoordinatorWiring` へは `MatchRuntimeQueryService` を注入し、`MainWindow` メンバ関数bindをやめる。

検証観点:
- 手番判定
- 対局中判定
- 持ち時間/加算/秒読み取得

---

### C12. MatchCoordinator配線とコールバック
対象メソッド:
`ensureMatchCoordinatorWiring`, `buildMatchWiringDeps`, `initMatchCoordinator`, `initializeGame`, `showGameOverMessageBox`, `initializeNewGameHook`, `renderBoardFromGc`, `showMoveHighlights`, `appendKifuLineHook`, `getRemainingMsFor`, `getIncrementMsFor`, `getByoyomiMs`, `updateTurnAndTimekeepingDisplay`

移譲先:
- 既存拡張: `MainWindowWiringAssembler`, `MainWindowMatchWiringDepsService`
- 新規: `MainWindowMatchAdapter`（interface実装）

移譲手順:
1. `MainWindowMatchAdapter` に callback 群を実装。
2. `MainWindowWiringAssembler::buildMatchWiringDeps` は `MainWindow` ではなく adapter 参照から deps を構築。
3. `MainWindow` から Match hook メソッド群を順次削除。

検証観点:
- 対局開始時Hook
- 終局メッセージ
- 指し手ハイライト
- 時間取得

---

### C13. `ensure*` 群の完全分離（DI/ライフサイクル）
対象メソッド（47件）:
`ensureEvaluationGraphController`, `ensureBranchNavigationWiring`, `ensureMatchCoordinatorWiring`, `ensureTimeController`, `ensureReplayController`, `ensureDialogCoordinator`, `ensureKifuFileController`, `ensureKifuExportController`, `ensureGameStateController`, `ensurePlayerInfoController`, `ensureBoardSetupController`, `ensurePvClickController`, `ensurePositionEditCoordinator`, `ensureCsaGameWiring`, `ensureJosekiWiring`, `ensureMenuWiring`, `ensurePlayerInfoWiring`, `ensurePreStartCleanupHandler`, `ensureTurnSyncBridge`, `ensurePositionEditController`, `ensureBoardSyncPresenter`, `ensureBoardLoadService`, `ensureConsiderationPositionService`, `ensureAnalysisPresenter`, `ensureGameStartCoordinator`, `ensureRecordPresenter`, `ensureLiveGameSessionStarted`, `ensureLiveGameSessionUpdater`, `ensureGameRecordUpdateService`, `ensureUndoFlowService`, `ensureGameRecordLoadService`, `ensureTurnStateSyncService`, `ensureKifuLoadCoordinatorForLive`, `ensureGameRecordModel`, `ensureJishogiController`, `ensureNyugyokuHandler`, `ensureConsecutiveGamesController`, `ensureLanguageController`, `ensureConsiderationWiring`, `ensureDockLayoutManager`, `ensureDockCreationService`, `ensureCommentCoordinator`, `ensureUsiCommandController`, `ensureRecordNavigationHandler`, `ensureUiStatePolicyManager`, `ensureKifuNavigationCoordinator`, `ensureSessionLifecycleCoordinator`

移譲先:
- 既存拡張: `MainWindowCompositionRoot`, `MainWindowDepsFactory`, `MainWindowRuntimeRefsFactory`
- 新規: `MainWindowServiceRegistry`

移譲手順:
1. 生成責務を `MainWindowServiceRegistry` に移し、`MainWindow` では registry 参照のみ持つ。
2. `ensure*` は registry へラップ移動し、`MainWindow` 側は廃止または極小プロキシ化。
3. `MainWindowRuntimeRefs` は「状態ポインタ直公開」を段階的に減らし、用途別DTOへ分割。
4. `MainWindowDepsFactory` は用途別 `createXxxDeps` を細分化し、循環参照を禁止。

検証観点:
- 遅延初期化順序の維持
- 生成済み再利用の維持
- ダングリングポインタ不発生

---

### C14. CSA/考慮モード/補助機能の入口整理
対象メソッド:
`ensureCsaGameWiring`, `ensureJosekiWiring`, `ensureConsiderationWiring`, `ensureJishogiController`, `ensureNyugyokuHandler`, `ensureUsiCommandController`, `onJosekiForcedPromotion`, `onPvRowClicked`

移譲先:
- 既存拡張: `DialogLaunchWiring`, `ConsiderationWiring`, `CsaGameWiring`, `PvClickController`
- 新規: `AnalysisInteractionCoordinator`

移譲手順:
1. 考慮モード・PVクリック・USIコマンド入口を `AnalysisInteractionCoordinator` に統合。
2. `onPvRowClicked` の依存同期（名前/SFEN/index）を controller 側 `updateContext(...)` APIへ移す。
3. `MainWindow` からモード依存分岐を除去。

検証観点:
- PVクリックダイアログ
- 検討エンジン変更
- CSA中の操作制約

---

### C15. ウィンドウ/設定復元フローの一本化
対象メソッド:
`buildGamePanels`, `restoreWindowAndSync`, `finalizeCoordinators`, `loadWindowSettings`, `saveWindowAndBoardSettings`, `saveSettingsAndClose`

移譲先:
- 既存拡張: `MainWindowUiBootstrapper`, `DockLayoutManager`, `SettingsService`
- 新規: `MainWindowLifecyclePipeline`

移譲手順:
1. コンストラクタ内の手続き順を `MainWindowLifecyclePipeline::runStartup()` に移管。
2. 終了順を `runShutdown()` に移管。
3. `MainWindow` のコンストラクタは pipeline 起動だけにする。

検証観点:
- 起動順序依存（null参照）回避
- レイアウト復元
- 終了時保存

---

### C16. Facade最終化（MainWindowに残す責務の明確化）
対象メソッド:
`MainWindow::MainWindow`, `~MainWindow`, `evalChart`, （必要最小限の公開APIのみ）

実施内容:
1. `MainWindow` は「UIルート」「外部公開API」「トップレベルイベント受け口」のみ残す。
2. 受け口スロットは `controller/coordinator` への1行転送に限定。
3. privateデータは `MainWindowStateStore` と `ServiceRegistry` に退避し、`MainWindow` 直接所有を削減。

## 5. 実装順序（推奨）
1. C13（ensure分離）を先に実施し、依存注入の土台を安定化
2. C04/C03（UI構築と配線）を移動
3. C08/C07（棋譜ナビと盤操作）を移動
4. C05/C06（ゲームライフサイクルとリセット）を移動
5. C01/C09/C10/C11（表示・I/O・判定系）を移動
6. C12/C14（adapter化と補助機能）で最終整理
7. C16で `MainWindow` をFacade化

## 6. 変更単位（コミット粒度）
1コミットで1カテゴリを上限にし、以下を必ず同時に入れる。
1. 本体移動
2. 旧呼び出し箇所の差し替え
3. 接続/DIの修正
4. 回帰確認結果の記録（`docs/dev/manual-test-scenarios.md` 追記）

## 7. 回帰テストチェックリスト
1. 新規対局（平手/駒落ち/途中局面）
2. 棋譜ロード後の行選択・分岐遷移・コメント編集
3. PVクリック、検討開始/停止、USIコマンド送信
4. CSA対局中のナビゲーション制約
5. 投了/中断/時間切れ/連続対局遷移
6. ドック保存/復元/ロック、ツールバー表示保存
7. 起動直後と終了直前の設定保存

## 8. 補足
- 既存 `MainWindowCompositionRoot/MainWindowDepsFactory/MainWindowRuntimeRefsFactory` は「移譲の中核」として継続利用する。
- ただし `RuntimeRefs` が巨大化しているため、最終段階で用途別DTOへ分割する。
- 移譲中に `MainWindow` と移譲先で同じロジックを二重保持しない（フラグ切替期間を最短にする）。
