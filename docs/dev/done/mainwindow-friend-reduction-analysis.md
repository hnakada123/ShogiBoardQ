# MainWindow friend class 削減分析

## 現状: 11 friend classes

```cpp
friend class MainWindowUiBootstrapper;        // 1
friend class MainWindowRuntimeRefsFactory;    // 2
friend class MainWindowWiringAssembler;       // 3
friend class MainWindowServiceRegistry;       // 4
friend class MainWindowAnalysisRegistry;      // 5
friend class MainWindowBoardRegistry;         // 6
friend class MainWindowGameRegistry;          // 7
friend class MainWindowKifuRegistry;          // 8
friend class MainWindowUiRegistry;            // 9
friend class MainWindowDockBootstrapper;      // 10
friend class MainWindowLifecyclePipeline;     // 11
```

---

## 1. 各 friend class のアクセス分析

### 1.1 MainWindowUiBootstrapper

**呼び出し元**: MainWindowLifecyclePipeline のみ（buildGamePanels / restoreWindowAndSync / finalizeCoordinators）

| アクセス先 | 種別 | 用途 |
|---|---|---|
| `m_registry` | unique_ptr 読取 | ensureXxx 呼び出し |
| `m_gameSessionOrchestrator` | raw ptr 読取 | startNewShogiGame |
| `m_appearanceController` | unique_ptr 読取 | setupBoardInCenter, setupNameAndClockFonts |
| `ui` | protected unique_ptr | actionLockDocks, actionResetDockLayout 等 |
| `m_dockCreationService` | unique_ptr 読取 | thinkingDock, recordPaneDock |
| `m_dockLayoutManager` | unique_ptr 読取 | restoreStartupLayoutIfSet |
| `ensureDockLayoutManager()` | private メソッド | 遅延初期化 |

**分類**: **C（構造変更）** — LifecyclePipeline に統合可能

---

### 1.2 MainWindowRuntimeRefsFactory

**呼び出し元**: MainWindow::buildRuntimeRefs()（1行委譲）

| アクセス先 | 種別 | 用途 |
|---|---|---|
| `ui` | protected | statusbar |
| `m_match`, `m_gameController`, `m_shogiView` | raw ptr 読取 | Refs構築 |
| `m_kifuLoadCoordinator`, `m_csaGameCoordinator` | raw ptr 読取 | Refs構築 |
| `m_models.*` (全9フィールド) | struct 読取 | Refs構築 |
| `m_state.*` (全7フィールド) | struct 読取 | Refs構築 |
| `m_kifu.*` (全9フィールド) | struct 読取 | Refs構築 |
| `m_branchNav.*` (全6フィールド) | struct 読取 | Refs構築 |
| `m_player.*` (全4フィールド) | struct 読取 | Refs構築 |
| `m_recordPane` 〜 `m_analysisTab` | raw ptr 読取 (~20個) | Refs構築 |
| `m_evalGraphController`, `m_analysisPresenter` | unique_ptr .get() | Refs構築 |
| `m_playModePolicy` | unique_ptr 読取+更新 | updateDeps |
| `m_queryService` | unique_ptr 読取 | sfenRecord() |

**合計: ~50メンバ読取**（MainWindow の全状態のスナップショットを構築）

**分類**: **B（MainWindow::buildRuntimeRefs に直接実装可能）** — 1メソッドに過ぎないため、MainWindow.cpp に戻す

---

### 1.3 MainWindowWiringAssembler

**呼び出し元**: MainWindowGameRegistry::ensureMatchCoordinatorWiring() + MainWindowLifecyclePipeline::connectSignals()

| メソッド | アクセス先 |
|---|---|
| `buildMatchWiringDeps` | `m_evalGraphController`, `m_match`, `m_matchAdapter`, `m_registry`, `m_gameRecordUpdateService`, `m_queryService`, `m_gameController`, `m_shogiView`, `m_usi1/2`, `m_models.*`, `m_state.*`, `m_kifu.*`, `m_timePresenter`, `m_boardController`, `m_signalRouter`, `m_timeController`, `m_uiStatePolicy`, `m_playerInfoWiring`, `m_kifuFileController`, `m_kifuNavCoordinator`, 各種 ensure* |
| `initializeDialogLaunchWiring` | `m_state.playMode`, `m_dialogCoordinator`, `m_gameController`, `m_match`, `m_shogiView`, `m_registry`, `m_csaGameWiring`, `m_boardSetupController`, `m_playerInfoWiring`, `m_analysisPresenter`, `m_usi1`, `m_analysisTab`, `m_models.*`, `m_kifuLoadCoordinator`, `m_evalChart`, `m_queryService`, `m_kifu.*`, `m_csaGameDialog`, `m_csaGameCoordinator`, `m_gameInfoController`, `m_docks.menuWindow`, `m_sfenCollectionDialog`, `m_dialogLaunchWiring`(書込), `m_kifuFileController`, 各種 ensure* |

**合計: ~40メンバ読取/書込**

**分類**: **C（構造変更）** — ServiceRegistry に統合可能

---

### 1.4 MainWindowServiceRegistry

**直接アクセス**: なし（コンストラクタで `m_mw` を保持し、サブレジストリに渡すのみ）

ServiceRegistry 自体は MainWindow の private メンバに直接アクセスしない。5つのサブレジストリが `m_mw` を経由してアクセスする。

**分類**: **D（friend 必須）** — 全 ensure* メソッドのファサード

---

### 1.5-1.9 サブレジストリ群（5クラス共通パターン）

5つのサブレジストリは同一パターン:

| クラス | アクセス対象メンバ数 | 主なアクセスパターン |
|---|---|---|
| **AnalysisRegistry** | ~15 | m_pvClickController, m_considerationPositionService, m_analysisPresenter, m_considerationWiring, m_usiCommandController, m_compositionRoot, m_state.*, m_player.*, m_models.*, m_analysisTab, m_shogiView, m_dialogCoordinator |
| **BoardRegistry** | ~30 | m_boardSetupController, m_posEditCoordinator, m_posEdit, m_boardSync, m_boardLoadService, m_boardController, m_compositionRoot, m_gameController, m_shogiView, m_state.*, m_kifu.*, m_models.*, m_branchNav.*, m_evalGraphController, m_timeController, m_commentCoordinator, m_queryService, m_kifuNavCoordinator, ui |
| **GameRegistry** | ~45 | m_timeController, m_replayController, m_matchWiring, m_matchAdapter, m_gameStateController, m_gameStart, m_csaGameWiring, m_preStartCleanupHandler, m_turnStateSync, m_liveGameSessionUpdater, m_undoFlowService, m_jishogiController, m_nyugyokuHandler, m_consecutiveGamesController, m_gameSessionOrchestrator, m_sessionLifecycle, m_coreInit, m_compositionRoot, m_state.*, m_kifu.*, m_player.*, m_models.*, m_branchNav.*, m_match, m_gameController, m_shogiView, m_usi1/2, 多数のensure*, buildRuntimeRefs, ui |
| **KifuRegistry** | ~30 | m_branchNavWiring, m_kifuFileController, m_kifuExportController, m_gameRecordUpdateService, m_gameRecordLoadService, m_kifuLoadCoordinator, m_models.*, m_kifuNavCoordinator, m_josekiWiring, m_compositionRoot, m_state.*, m_kifu.*, m_branchNav.*, m_recordPresenter, m_liveGameSessionUpdater, m_recordPane, m_docks.josekiWindow, m_commentCoordinator, m_queryService, m_tab, m_analysisTab, ui |
| **UiRegistry** | ~25 | m_evalGraphController, m_recordPresenter, m_playerInfoController, m_playerInfoWiring, m_dialogCoordinator, m_dialogCoordinatorWiring, m_menuWiring, m_dockLayoutManager, m_dockCreationService, m_uiStatePolicy, m_notificationService, m_recordNavWiring, m_languageController, m_compositionRoot, m_state.*, m_player.*, m_models.*, m_shogiView, m_evalChart, m_docks.*, m_appearanceController, m_queryService, ui |

**全サブレジストリ共通**:
- `m_mw.m_compositionRoot->ensureXxx()` で生成
- `m_mw.m_xxx = new Xxx(...)` で結果を書き込み
- `m_mw.buildRuntimeRefs()` で RuntimeRefs 取得
- `m_mw.m_state.*`, `m_mw.m_kifu.*` 等の struct メンバへのポインタ取得

**分類**: **C（構造変更）** — ServiceRegistry に統合し、サブレジストリをなくす

---

### 1.10 MainWindowDockBootstrapper

**呼び出し元**: MainWindowUiBootstrapper のみ + MainWindowUiRegistry::createMenuWindowDock

| アクセス先 | 種別 | 用途 |
|---|---|---|
| `m_models.*` (8フィールド) | struct 読取/書込 | モデル生成・取得 |
| `m_recordPaneWiring`, `m_recordPane` | raw ptr 読取/書込 | RecordPane 構築 |
| `m_analysisWiring`, `m_analysisTab`, `m_tab` | raw ptr 読取/書込 | AnalysisTab 構築 |
| `m_evalChart`, `m_evalGraphController` | raw ptr / unique_ptr | EvalChart 構築 |
| `m_playerInfoController`, `m_playerInfoWiring` | raw ptr 読取/書込 | PlayerInfo 設定 |
| `m_gameInfoController` | raw ptr 書込 | GameInfo 設定 |
| `m_docks.*` (11フィールド) | struct 書込 | ドック登録 |
| `m_central` | raw ptr 読取 | 親ウィジェット |
| `m_dockCreationService` | unique_ptr 読取 | ドック生成 |
| `m_registry` | unique_ptr 読取 | ensure* |
| `m_branchNavWiring`, `m_josekiWiring` 等 | unique_ptr 読取 | 配線 |
| `m_commentCoordinator`, `m_usiCommandController` 等 | raw ptr / unique_ptr 読取 | 外部依存 |
| 各種 `ensureXxx()` | private メソッド | 遅延初期化 |

**合計: ~35メンバ読取/書込**

**分類**: **C（構造変更）** — LifecyclePipeline に統合可能

---

### 1.11 MainWindowLifecyclePipeline

**呼び出し元**: MainWindow コンストラクタ・デストラクタ・closeEvent・saveSettingsAndClose

| アクセス先 | 種別 | 用途 |
|---|---|---|
| `m_compositionRoot` | unique_ptr 生成 | 初期化 |
| `m_registry` | unique_ptr 生成 | 初期化 |
| `m_signalRouter` | unique_ptr 生成+使用 | 配線 |
| `m_models.commLog1/2` | raw ptr 生成 | 初期化 |
| `m_queryService` | unique_ptr 生成/reset | 初期化/終了 |
| `ui` | protected 使用 | setupUi, UI参照 |
| `m_appearanceController` | unique_ptr 生成+使用 | UI骨格 |
| `m_central` | raw ptr 書込 | central設定 |
| `m_shogiView`, `m_player.*`, `m_match` | ダブルポインタ | Deps設定 |
| `m_playModePolicy` | unique_ptr 生成/reset | 初期化/終了 |
| `m_state.*`, `m_gameController` | struct/ptr 読取 | 依存設定 |
| `m_timeController`, `m_timePresenter` | raw ptr 読取/書込 | 時計初期化 |
| `m_coreInit` | unique_ptr 使用 | コア初期化 |
| `m_dockLayoutManager` | unique_ptr 使用 | 設定保存 |
| `m_evalChart`, `m_boardController` | raw ptr 読取 | SignalRouter Deps |
| `m_dialogCoordinatorWiring`, `m_dialogLaunchWiring` 等 | ダブルポインタ | SignalRouter Deps |
| `m_evalChartResizeTimer` | unique_ptr 生成 | タイマー初期化 |
| `m_uiStatePolicy` | raw ptr 使用 | 状態初期化 |
| `m_kifuExportController` | unique_ptr 読取 | SignalRouter |
| `m_debugScreenshotWiring` | unique_ptr 生成 | デバッグ |
| 各種 `ensureXxx()` | private メソッド | 遅延初期化 |
| `setDockNestingEnabled()` | public (QMainWindow) | ← friend不要 |

**合計: ~40メンバ読取/書込 + 生成**

**分類**: **D（friend 必須）** — 起動/終了フローは MainWindow の内部構造への深いアクセスが本質的に必要

---

## 2. アクセスパターン分類サマリ

| 分類 | friend class | 代替手段 |
|---|---|---|
| **D: friend 必須** | MainWindowServiceRegistry, MainWindowLifecyclePipeline | 残す |
| **C: 構造変更** | 5つのサブレジストリ, WiringAssembler, UiBootstrapper, DockBootstrapper | ServiceRegistry/Pipeline に統合 |
| **B: インライン化** | RuntimeRefsFactory | MainWindow::buildRuntimeRefs に直接実装 |

---

## 3. 削減計画

### 目標: 11 → 2 (最終的に MainWindowServiceRegistry + MainWindowLifecyclePipeline のみ)

---

### Phase 1: RuntimeRefsFactory のインライン化 (11 → 10)

**作業内容**:
- `MainWindowRuntimeRefsFactory::build()` の実装を `MainWindow::buildRuntimeRefs()` に直接移動
- `mainwindowruntimerefsfactory.h/.cpp` を削除
- `friend class MainWindowRuntimeRefsFactory` を削除
- CMakeLists.txt から除去

**リスク**: 低。buildRuntimeRefs は MainWindow の private メソッドなのでアクセス権問題なし。
**コード変更量**: ~100行の移動 + ファイル2つ削除

---

### Phase 2: 5つのサブレジストリを ServiceRegistry に統合 (10 → 5)

**背景**: 5つのサブレジストリ（Analysis, Board, Game, Kifu, Ui）は ServiceRegistry の内部実装詳細。ServiceRegistry がファサードとして全メソッドを1行転送している。

**作業内容**:
1. 各サブレジストリの全メソッドを ServiceRegistry に直接移動
2. ServiceRegistry.cpp が巨大になるため、**複数 .cpp ファイルに分割**して管理:
   - `mainwindowserviceregistry.cpp` — コンストラクタ + 転送（そのまま）
   - `mainwindowserviceregistry_analysis.cpp` — Analysis系メソッド
   - `mainwindowserviceregistry_board.cpp` — Board系メソッド
   - `mainwindowserviceregistry_game.cpp` — Game系メソッド
   - `mainwindowserviceregistry_kifu.cpp` — Kifu系メソッド
   - `mainwindowserviceregistry_ui.cpp` — UI系メソッド
3. ヘッダーは `mainwindowserviceregistry.h` 1つに統合（private セクションにメソッド追加）
4. 5つのサブレジストリの .h/.cpp を削除
5. `friend class MainWindowAnalysisRegistry` 〜 `MainWindowUiRegistry` (5つ) を削除

**利点**:
- ServiceRegistry は既に friend なので、サブレジストリのコードが ServiceRegistry のメソッドになれば追加の friend 不要
- 転送メソッド（現在の1行委譲）が不要になり、直接実装になる
- ファイル分割により可読性を維持

**リスク**: 中。メソッド数が多い（~65メソッド）が、各メソッドの実装はそのまま移動。
**コード変更量**: ~2000行の移動 + ファイル10つ削除 + ServiceRegistry.h に ~65 private メソッド追加

---

### Phase 3: UiBootstrapper + DockBootstrapper を LifecyclePipeline に統合 (5 → 3)

**背景**:
- UiBootstrapper は LifecyclePipeline からのみ呼ばれる（3メソッド: buildGamePanels, restoreWindowAndSync, finalizeCoordinators）
- DockBootstrapper は UiBootstrapper からのみ呼ばれる（+ UiRegistry::createMenuWindowDock が1箇所）

**作業内容**:
1. UiBootstrapper の3メソッドを LifecyclePipeline の private メソッドとして移動
2. DockBootstrapper の全メソッドを LifecyclePipeline の private メソッドとして移動
3. UiRegistry::createMenuWindowDock の1箇所は ServiceRegistry から LifecyclePipeline 由来メソッドを呼ぶ形に調整（または ServiceRegistry に DockBootstrapper のロジックを残す）
4. `mainwindowuibootstrapper.h/.cpp` と `mainwindowdockbootstrapper.h/.cpp` を削除
5. `friend class MainWindowUiBootstrapper` と `friend class MainWindowDockBootstrapper` を削除

**注意**: DockBootstrapper::createMenuWindowDock は UiRegistry（Phase 2 で ServiceRegistry に統合済み）からも呼ばれる。この1メソッドだけ ServiceRegistry にも同等ロジックを持たせるか、LifecyclePipeline のメソッドを ServiceRegistry から呼び出す設計にする。

**リスク**: 中。LifecyclePipeline.cpp が大きくなるが、ドック生成は起動時の一回きり処理。
**コード変更量**: ~350行の移動 + ファイル4つ削除

---

### Phase 4: WiringAssembler を ServiceRegistry に統合 (3 → 2)

**背景**:
- `buildMatchWiringDeps` は GameRegistry（Phase 2 で ServiceRegistry に統合済み）から呼ばれる
- `initializeDialogLaunchWiring` は LifecyclePipeline の connectSignals() から呼ばれる

**作業内容**:
1. `buildMatchWiringDeps` を ServiceRegistry の private メソッドとして移動
2. `initializeDialogLaunchWiring` を ServiceRegistry の public メソッドとして追加し、LifecyclePipeline から `m_registry->initializeDialogLaunchWiring()` で呼ぶ
3. `mainwindowwiringassembler.h/.cpp` を削除
4. `friend class MainWindowWiringAssembler` を削除

**リスク**: 低。2メソッドの移動のみ。
**コード変更量**: ~160行の移動 + ファイル2つ削除

---

## 4. 最終結果

### 残す friend class（2つ）

| friend class | 理由 |
|---|---|
| **MainWindowServiceRegistry** | 全 ensure* メソッド（~65個）の実装を持ち、MainWindow の全メンバ変数の読取/書込が必要。依存注入・遅延初期化の中核。 |
| **MainWindowLifecyclePipeline** | 起動（8段階）/終了フローを管理。unique_ptr メンバの生成・reset、UI構築、シグナル配線など MainWindow のライフサイクル全体を制御。 |

### 削除する friend class（9つ）

| 削除対象 | Phase | 代替手段 |
|---|---|---|
| MainWindowRuntimeRefsFactory | 1 | MainWindow::buildRuntimeRefs に直接実装 |
| MainWindowAnalysisRegistry | 2 | ServiceRegistry に統合 |
| MainWindowBoardRegistry | 2 | ServiceRegistry に統合 |
| MainWindowGameRegistry | 2 | ServiceRegistry に統合 |
| MainWindowKifuRegistry | 2 | ServiceRegistry に統合 |
| MainWindowUiRegistry | 2 | ServiceRegistry に統合 |
| MainWindowUiBootstrapper | 3 | LifecyclePipeline に統合 |
| MainWindowDockBootstrapper | 3 | LifecyclePipeline に統合 |
| MainWindowWiringAssembler | 4 | ServiceRegistry に統合 |

---

## 5. 実装順序と依存関係

```
Phase 1 (RuntimeRefsFactory)  ── 依存なし、独立実施可能
    ↓
Phase 2 (5サブレジストリ)     ── Phase 1 とは独立だが、先に Phase 1 を済ませると buildRuntimeRefs の呼び出しが整理される
    ↓
Phase 3 (UiBootstrapper/DockBootstrapper) ── Phase 2 完了後（DockBootstrapper の createMenuWindowDock が ServiceRegistry を経由するため）
    ↓
Phase 4 (WiringAssembler)     ── Phase 2 完了後（buildMatchWiringDeps が ServiceRegistry のメソッドになるため）
```

Phase 3 と Phase 4 は独立して実施可能（Phase 2 完了後であれば順不同）。

---

## 6. リスク評価

| Phase | リスク | 理由 |
|---|---|---|
| 1 | **低** | 1メソッドの移動、コンパイルエラーがあれば即座にわかる |
| 2 | **中** | ~65メソッドの大量移動だが、ロジック変更なし。CMakeLists.txt の更新が重要 |
| 3 | **中** | LifecyclePipeline の肥大化。DockBootstrapper の createMenuWindowDock の扱いに注意 |
| 4 | **低** | 2メソッドの移動のみ |

全 Phase ともロジック変更は不要（移動のみ）。ビルド確認で十分検証可能。
