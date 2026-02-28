# ensure* 呼び出しグラフ分析・再分割設計

**作成日**: 2026-02-28
**対象**: `MainWindowServiceRegistry` + `MainWindowCompositionRoot`

## 1. 現状の概要

### ファイル構成と行数

| ファイル | 行数 | 内容 |
|---------|------|------|
| `mainwindowserviceregistry.h` | 150 | 全ensure*宣言（1ヘッダに集約） |
| `mainwindowserviceregistry.cpp` | 10 | コンストラクタのみ |
| `mainwindowuiregistry.cpp` | 289 | UI系ensure*実装 |
| `mainwindowgameregistry.cpp` | 592 | Game系ensure*実装 |
| `mainwindowkifuregistry.cpp` | 369 | Kifu系ensure*実装 |
| `mainwindowanalysisregistry.cpp` | 111 | Analysis系ensure*実装 |
| `mainwindowboardregistry.cpp` | 307 | Board/共通系ensure*実装 |
| `mainwindowdockbootstrapper.cpp` | 243 | Dock初期化オーケストレーション |
| `mainwindowuibootstrapper.cpp` | 95 | UI起動オーケストレーション |
| `mainwindowwiringassembler.cpp` | 159 | Wiring組立・DialogLaunchWiring |
| `mainwindowcompositionroot.h` | 113 | CompositionRoot宣言（11メソッド） |
| `mainwindowcompositionroot.cpp` | 249 | CompositionRoot実装 |
| **合計** | **~2,700** | |

### メソッド数

- **ensure* メソッド（ServiceRegistry）**: 50件
- **非ensure* 操作メソッド（ServiceRegistry）**: 約33件
- **ensure* ファクトリ（CompositionRoot）**: 11件

---

## 2. 全 ensure* メソッド分類表

### 凡例

- **Tier**: 依存の深さ（0=リーフ、数字が大きいほど依存が深い）
- **直接呼出**: メソッド本体内で同期的に呼ぶ ensure*
- **遅延呼出**: Deps/callback/lambdaに格納して後で呼ばれる ensure*
- **CR委譲**: CompositionRoot に生成を委譲するか

### 2.1 UI系（mainwindowuiregistry.cpp — 12件）

| # | メソッド | Tier | 直接呼出 | 遅延呼出 | CR委譲 | 生成クラス |
|---|---------|------|---------|---------|--------|-----------|
| 1 | `ensureEvaluationGraphController` | 0 | — | — | — | `EvaluationGraphController` |
| 2 | `ensurePlayerInfoWiring` | 0 | — | — | — | `PlayerInfoWiring` |
| 3 | `ensureMenuWiring` | 0 | — | — | — | `MenuWindowWiring` |
| 4 | `ensureDockLayoutManager` | 0 | — | — | — | `DockLayoutManager` |
| 5 | `ensureDockCreationService` | 0 | — | — | — | `DockCreationService` |
| 6 | `ensureUiStatePolicyManager` | 0 | — | — | CR | `UiStatePolicyManager` |
| 7 | `ensureUiNotificationService` | 0 | — | — | — | `UiNotificationService` |
| 8 | `ensureLanguageController` | 0 | — | — | — | `LanguageController` |
| 9 | `ensurePlayerInfoController` | 1 | PlayerInfoWiring | — | CR | `PlayerInfoController` |
| 10 | `ensureRecordPresenter` | 1 | CommentCoordinator | — | — | `GameRecordPresenter` |
| 11 | `ensureDialogCoordinator` | 2 | KifuNavigationCoordinator | ConsiderationWiring, UiStatePolicyManager | CR | `DialogCoordinatorWiring` + `DialogCoordinator` |
| 12 | `ensureRecordNavigationHandler` | 2 | KifuNavigationCoordinator, UiStatePolicyManager, ConsiderationPositionService | — | CR | `RecordNavigationWiring` |

### 2.2 Game系（mainwindowgameregistry.cpp — 18件）

| # | メソッド | Tier | 直接呼出 | 遅延呼出 | CR委譲 | 生成クラス |
|---|---------|------|---------|---------|--------|-----------|
| 13 | `ensureTimeController` | 0 | — | — | — | `TimeControlController` |
| 14 | `ensureReplayController` | 0 | — | — | — | `ReplayController` |
| 15 | `ensurePreStartCleanupHandler` | 0 | — | — | — | `PreStartCleanupHandler` |
| 16 | `ensureTurnSyncBridge` | 0 | — | — | — | (static wire) |
| 17 | `ensureTurnStateSyncService` | 0 | — | — | — | `TurnStateSyncService` |
| 18 | `ensureLiveGameSessionUpdater` | 0 | — | — | — | `LiveGameSessionUpdater` |
| 19 | `ensureUndoFlowService` | 0 | — | — | — | `UndoFlowService` |
| 20 | `ensureJishogiController` | 0 | — | — | — | `JishogiScoreDialogController` |
| 21 | `ensureNyugyokuHandler` | 0 | — | — | — | `NyugyokuDeclarationHandler` |
| 22 | `ensureLiveGameSessionStarted` | 1 | LiveGameSessionUpdater | — | — | (delegates) |
| 23 | `ensureCoreInitCoordinator` | 1 | — | PlayerInfoWiring, TurnSyncBridge | — | `MainWindowCoreInitCoordinator` |
| 24 | `ensureGameStateController` | 1 | UiStatePolicyManager | KifuNavigationCoordinator | CR | `GameStateController` |
| 25 | `ensureCsaGameWiring` | 2 | UiStatePolicyManager, GameRecordUpdateService, UiNotificationService | — | — | `CsaGameWiring` |
| 26 | `ensureConsecutiveGamesController` | 2 | GameSessionOrchestrator | — | — | `ConsecutiveGamesController` |
| 27 | `ensureMatchCoordinatorWiring` | 3 | EvaluationGraphController, PlayerInfoWiring, CoreInitCoordinator, GameSessionOrchestrator, KifuNavigationCoordinator, BranchNavigationWiring | DialogCoordinator, TurnStateSyncService | — | `MatchCoordinatorWiring` + `MainWindowMatchAdapter` |
| 28 | `ensureGameStartCoordinator` | 4 | MatchCoordinatorWiring | — | — | (delegates to MCW) |
| 29 | `ensureGameSessionOrchestrator` | 1* | — | GameStateController, SessionLifecycleCoordinator, ConsecutiveGamesController, GameStartCoordinator, PreStartCleanupHandler, DialogCoordinator, ReplayController | — | `GameSessionOrchestrator` |
| 30 | `ensureSessionLifecycleCoordinator` | 2* | GameSessionOrchestrator | PlayerInfoWiring | — | `SessionLifecycleCoordinator` |

> \* Tier 1/2は直接呼出のみで計算。遅延呼出を含めると循環のため計算不能（後述）

### 2.3 Kifu系（mainwindowkifuregistry.cpp — 10件）

| # | メソッド | Tier | 直接呼出 | 遅延呼出 | CR委譲 | 生成クラス |
|---|---------|------|---------|---------|--------|-----------|
| 31 | `ensureJosekiWiring` | 0 | — | — | — | `JosekiWindowWiring` |
| 32 | `ensureCommentCoordinator` | 0 | — | — | CR | `CommentCoordinator` |
| 33 | `ensureKifuNavigationCoordinator` | 1 | — | BoardSyncPresenter | — | `KifuNavigationCoordinator` |
| 34 | `ensureGameRecordModel` | 1 | CommentCoordinator | — | — | `GameRecordModel` |
| 35 | `ensureBranchNavigationWiring` | 2 | KifuNavigationCoordinator | CommentCoordinator | — | `BranchNavigationWiring` |
| 36 | `ensureKifuExportController` | 1 | — | GameRecordModel | — | `KifuExportController` |
| 37 | `ensureGameRecordUpdateService` | 1 | — | RecordPresenter, LiveGameSessionUpdater | — | `GameRecordUpdateService` |
| 38 | `ensureGameRecordLoadService` | 1 | — | RecordPresenter, GameRecordModel | — | `GameRecordLoadService` |
| 39 | `ensureKifuLoadCoordinatorForLive` | 3 | createAndWireKifuLoadCoordinator (private) | — | — | `KifuLoadCoordinator` |
| 40 | `ensureKifuFileController` | 3 | — | PlayerInfoWiring, GameRecordModel, KifuExportController, createAndWire…, KifuLoadCoordinator | CR | `KifuFileController` |

> `createAndWireKifuLoadCoordinator`（private helper）: 直接呼出 = BranchNavigationWiring, KifuNavigationCoordinator, UiStatePolicyManager, PlayerInfoWiring, UiNotificationService

### 2.4 Analysis系（mainwindowanalysisregistry.cpp — 5件）

| # | メソッド | Tier | 直接呼出 | 遅延呼出 | CR委譲 | 生成クラス |
|---|---------|------|---------|---------|--------|-----------|
| 41 | `ensurePvClickController` | 0 | — | — | CR | `PvClickController` |
| 42 | `ensureConsiderationPositionService` | 0 | — | — | — | `ConsiderationPositionService` |
| 43 | `ensureAnalysisPresenter` | 0 | — | — | — | `AnalysisResultsPresenter` |
| 44 | `ensureUsiCommandController` | 0 | — | — | — | `UsiCommandController` |
| 45 | `ensureConsiderationWiring` | 1 | — | DialogCoordinator | CR | `ConsiderationWiring` |

### 2.5 Board/共通系（mainwindowboardregistry.cpp — 5件）

| # | メソッド | Tier | 直接呼出 | 遅延呼出 | CR委譲 | 生成クラス |
|---|---------|------|---------|---------|--------|-----------|
| 46 | `ensurePositionEditController` | 0 | — | — | — | `PositionEditController` |
| 47 | `ensureBoardSyncPresenter` | 0 | — | — | — | `BoardSyncPresenter` |
| 48 | `ensurePositionEditCoordinator` | 1 | UiStatePolicyManager | PositionEditController | CR | `PositionEditCoordinator` |
| 49 | `ensureBoardSetupController` | 1 | — | PositionEditController, TimeController, GameRecordUpdateService, EvaluationGraphController | CR | `BoardSetupController` |
| 50 | `ensureBoardLoadService` | 1 | BoardSyncPresenter | KifuNavigationCoordinator | — | `BoardLoadService` |

---

## 3. 呼び出し依存グラフ

### 3.1 直接呼出の依存グラフ（テキスト）

```
                    ┌─── ensureGameStartCoordinator [28]
                    │            │
                    │            ▼
                    │   ensureMatchCoordinatorWiring [27]
                    │       │  │  │  │  │  │
                    │       │  │  │  │  │  └─ ensureBranchNavigationWiring [35]
                    │       │  │  │  │  │         │
                    │       │  │  │  │  │         ├─ ensureKifuNavigationCoordinator [33]
                    │       │  │  │  │  │         │       └─(cb) ensureBoardSyncPresenter [47]
                    │       │  │  │  │  │         └─(cb) ensureCommentCoordinator [32]
                    │       │  │  │  │  │
                    │       │  │  │  │  └─ ensureKifuNavigationCoordinator [33] (共有)
                    │       │  │  │  │
                    │       │  │  │  └─ ensureGameSessionOrchestrator [29]
                    │       │  │  │         └─(cb) 7メソッド (後述)
                    │       │  │  │
                    │       │  │  └─ ensureCoreInitCoordinator [23]
                    │       │  │         └─(cb) ensurePlayerInfoWiring [2], ensureTurnSyncBridge [16]
                    │       │  │
                    │       │  └─ ensurePlayerInfoWiring [2]
                    │       │
                    │       └─ ensureEvaluationGraphController [1]
                    │
ensureSessionLifecycleCoordinator [30]
        │
        └─ ensureGameSessionOrchestrator [29]

ensureConsecutiveGamesController [26]
        │
        └─ ensureGameSessionOrchestrator [29]

ensureCsaGameWiring [25]
        ├─ ensureUiStatePolicyManager [6]
        ├─ ensureGameRecordUpdateService [37]
        └─ ensureUiNotificationService [7]

ensureDialogCoordinator [11]
        └─ ensureKifuNavigationCoordinator [33]

ensureRecordNavigationHandler [12]
        ├─ ensureKifuNavigationCoordinator [33]
        ├─ ensureUiStatePolicyManager [6]
        └─ ensureConsiderationPositionService [42]

ensureRecordPresenter [10]
        └─ ensureCommentCoordinator [32]

ensurePlayerInfoController [9]
        └─ ensurePlayerInfoWiring [2]

ensureGameStateController [24]
        └─ ensureUiStatePolicyManager [6]

ensurePositionEditCoordinator [48]
        └─ ensureUiStatePolicyManager [6]

ensureBoardLoadService [50]
        └─ ensureBoardSyncPresenter [47]

ensureGameRecordModel [34]
        └─ ensureCommentCoordinator [32]

ensureLiveGameSessionStarted [22]
        └─ ensureLiveGameSessionUpdater [18]

createAndWireKifuLoadCoordinator (private)
        ├─ ensureBranchNavigationWiring [35]
        ├─ ensureKifuNavigationCoordinator [33]
        ├─ ensureUiStatePolicyManager [6]
        ├─ ensurePlayerInfoWiring [2]
        └─ ensureUiNotificationService [7]
```

### 3.2 遅延呼出（callback）の依存グラフ

```
ensureGameSessionOrchestrator [29] ···(cb)···> ensureGameStateController [24]
                                   ···(cb)···> ensureSessionLifecycleCoordinator [30]
                                   ···(cb)···> ensureConsecutiveGamesController [26]
                                   ···(cb)···> ensureGameStartCoordinator [28]
                                   ···(cb)···> ensurePreStartCleanupHandler [15]
                                   ···(cb)···> ensureDialogCoordinator [11]
                                   ···(cb)···> ensureReplayController [14]

ensureMatchCoordinatorWiring [27]  ···(cb)···> ensureDialogCoordinator [11]
                                   ···(cb)···> ensureTurnStateSyncService [17]

ensureDialogCoordinator [11]       ···(cb)···> ensureConsiderationWiring [45]
                                   ···(cb)···> ensureUiStatePolicyManager [6]

ensureConsiderationWiring [45]     ···(cb)···> ensureDialogCoordinator [11]

ensureBoardSetupController [49]    ···(cb)···> ensurePositionEditController [46]
                                   ···(cb)···> ensureTimeController [13]
                                   ···(cb)···> ensureGameRecordUpdateService [37]
                                   ···(cb)···> ensureEvaluationGraphController [1]

ensureKifuFileController [40]      ···(cb)···> ensurePlayerInfoWiring [2]
                                   ···(cb)···> ensureGameRecordModel [34]
                                   ···(cb)···> ensureKifuExportController [36]
                                   ···(cb)···> createAndWireKifuLoadCoordinator
                                   ···(cb)···> ensureKifuLoadCoordinatorForLive [39]

ensureSessionLifecycleCoordinator [30] ···(cb)···> ensurePlayerInfoWiring [2]

ensureCoreInitCoordinator [23]     ···(cb)···> ensurePlayerInfoWiring [2]
                                   ···(cb)···> ensureTurnSyncBridge [16]

ensureGameRecordUpdateService [37] ···(cb)···> ensureRecordPresenter [10]
                                   ···(cb)···> ensureLiveGameSessionUpdater [18]

ensureGameRecordLoadService [38]   ···(cb)···> ensureRecordPresenter [10]
                                   ···(cb)···> ensureGameRecordModel [34]

ensureKifuExportController [36]    ···(cb)···> ensureGameRecordModel [34]

ensureKifuNavigationCoordinator [33] ···(cb)···> ensureBoardSyncPresenter [47]

ensureBranchNavigationWiring [35]  ···(cb)···> ensureCommentCoordinator [32]

ensureBoardLoadService [50]        ···(cb)···> ensureKifuNavigationCoordinator [33]

ensureGameStateController [24]     ···(cb)···> ensureKifuNavigationCoordinator [33]

ensurePositionEditCoordinator [48] ···(cb)···> ensurePositionEditController [46]
```

---

## 4. 循環依存の分析

### 4.1 検出された循環パターン（4件）

#### Cycle A: DialogCoordinator ↔ ConsiderationWiring

```
ensureDialogCoordinator ···(cb)···> ensureConsiderationWiring ···(cb)···> ensureDialogCoordinator
```

- **種別**: 相互遅延（双方ともcallbackに格納）
- **安全性**: `if (m_mw.m_xxx) return;` ガードにより無限再帰なし
- **理由**: DialogCoordinatorが検討モード開始時にConsiderationWiringを要求し、ConsiderationWiringがダイアログ起動時にDialogCoordinatorを要求する相互依存

#### Cycle B: GameSessionOrchestrator ↔ ConsecutiveGamesController

```
ensureGameSessionOrchestrator ···(cb)···> ensureConsecutiveGamesController ──> ensureGameSessionOrchestrator
```

- **種別**: 片方直接・片方遅延
- **安全性**: CGC→GSOは直接呼出だが、GSOは先に生成されDeps更新のみ。ガードにより安全
- **理由**: 連続対局コントローラが対局オーケストレータのシグナルに接続する必要がある

#### Cycle C: GameSessionOrchestrator ↔ SessionLifecycleCoordinator

```
ensureGameSessionOrchestrator ···(cb)···> ensureSessionLifecycleCoordinator ──> ensureGameSessionOrchestrator
```

- **種別**: 片方直接・片方遅延（Cycle Bと同パターン）
- **安全性**: SLC→GSOは直接呼出だが、GSOのDeps更新のみ。安全

#### Cycle D: MatchCoordinatorWiring → GameSessionOrchestrator → GameStartCoordinator → MatchCoordinatorWiring

```
ensureMatchCoordinatorWiring ──> ensureGameSessionOrchestrator
    ···(cb)···> ensureGameStartCoordinator ──> ensureMatchCoordinatorWiring
```

- **種別**: 3ノード循環（直接+遅延混合）
- **安全性**: MCW→GSOは初回のみ直接呼出。GSO→GSCは遅延。GSC→MCWはガードでスキップ。安全
- **理由**: MCWがGSOにシグナル接続し、GSOが対局開始にGSCを遅延で取得、GSCはMCW経由で取得

### 4.2 循環依存の評価

| 循環 | リスク | 対策状況 | 分割時の影響 |
|------|--------|---------|------------|
| A | 低 | ガード+遅延で安全 | 同一レジストリ内に配置要 |
| B | 低 | ガード+遅延で安全 | Game内で閉じる |
| C | 低 | ガード+遅延で安全 | Game内で閉じる |
| D | 中 | ガード+遅延で安全だが複雑 | Game内で閉じる |

**結論**: 全循環はガードパターンにより実行時安全。ただし認知的複雑度が高いため、ドキュメント化が重要。

---

## 5. ドメイン間依存の分析

### 5.1 クロスドメイン直接呼出マトリクス

呼出元＼呼出先 | UI | Game | Kifu | Analysis | Board
:---|:---:|:---:|:---:|:---:|:---:
**UI** | 内部 | — | KifuNavCoord, CommentCoord | ConsiderationPositionSvc | —
**Game** | EvalGraphCtrl, PlayerInfoWiring, UiStatePolicyMgr, UiNotificationSvc | 内部 | KifuNavCoord, BranchNavWiring, GameRecordUpdateSvc | — | —
**Kifu** | UiStatePolicyMgr, PlayerInfoWiring, UiNotificationSvc | — | 内部 | — | —
**Analysis** | — | — | — | 内部 | —
**Board** | UiStatePolicyMgr | — | — | — | 内部

> Analysis→UIはensureConsiderationWiring→ensureDialogCoordinator（遅延のみ）のため直接呼出には含めない

### 5.2 依存方向の概要

```
                    ┌─────────────┐
                    │  Board系(5) │
                    └──┬──┬───────┘
                       │  │
              ┌────────┘  └────────┐
              ▼                    ▼
    ┌─────────────┐      ┌────────────────┐
    │  Game系(18) │──────>│  Kifu系(10)    │
    └──┬──────────┘      └──┬─────────────┘
       │                    │
       │    ┌───────────┐   │
       │    │Analysis(5)│   │
       │    └──┬────────┘   │
       │       │(遅延のみ)   │
       ▼       ▼            ▼
    ┌──────────────────────────┐
    │       UI系(12)           │
    └──────────────────────────┘
             │
             └──> Kifu (KifuNavCoord, CommentCoord)
                  ※UI→Kifuは逆方向依存
```

### 5.3 双方向依存の検出

| ペア | 方向A（直接） | 方向B（直接） | 評価 |
|------|------------|------------|------|
| UI ↔ Kifu | UI→Kifu（KifuNavCoord, CommentCoord） | Kifu→UI（UiStatePolicyMgr, PlayerInfoWiring, UiNotifSvc） | **双方向** |
| Game ↔ Kifu | Game→Kifu（KifuNavCoord, BranchNavWiring, GameRecordUpdateSvc） | Kifu→Game（なし*） | 単方向 |

> \* KifuのensureGameRecordUpdateServiceはGame系のensureLiveGameSessionUpdaterを遅延呼出するが、直接呼出はない

**UI ↔ Kifu の双方向依存が、クラス分割の最大の障壁**。

---

## 6. 共有度分析（頻繁に呼ばれるensure*）

### 被呼出回数ランキング（直接+遅延合算、内部相互除く外部からの呼出含む）

| メソッド | 直接 | 遅延 | 合計 | 呼出元ドメイン数 |
|---------|------|------|------|----------------|
| `ensureUiStatePolicyManager` | 6 | 3 | 9 | 4 (UI,Game,Kifu,Board) |
| `ensurePlayerInfoWiring` | 3 | 5 | 8 | 4 (UI,Game,Kifu,Dock) |
| `ensureKifuNavigationCoordinator` | 5 | 3 | 8 | 4 (UI,Game,Kifu,Board) |
| `ensureCommentCoordinator` | 4 | 2 | 6 | 2 (UI,Kifu) |
| `ensureGameSessionOrchestrator` | 3 | 1 | 4 | 2 (Game,Dock) |
| `ensureBranchNavigationWiring` | 3 | 0 | 3 | 3 (Game,Kifu,Dock) |
| `ensureEvaluationGraphController` | 2 | 2 | 4 | 3 (UI,Game,Board) |
| `ensureGameRecordModel` | 1 | 3 | 4 | 1 (Kifu) |
| `ensureRecordPresenter` | 0 | 2 | 2 | 1 (Kifu) |
| `ensureDockCreationService` | 7 | 0 | 7 | 1 (Dock) |

**頻出Top3**（UiStatePolicyManager, PlayerInfoWiring, KifuNavigationCoordinator）は全ドメインから横断的に利用されており、**共通基盤層**に位置づけられる。

---

## 7. Tier（依存深度）分析

### Tier 0: リーフメソッド（25件）— 他のensure*を一切呼ばない

| ドメイン | メソッド |
|---------|---------|
| UI (8) | EvaluationGraphController, PlayerInfoWiring, MenuWiring, DockLayoutManager, DockCreationService, UiStatePolicyManager, UiNotificationService, LanguageController |
| Game (9) | TimeController, ReplayController, PreStartCleanupHandler, TurnSyncBridge, TurnStateSyncService, LiveGameSessionUpdater, UndoFlowService, JishogiController, NyugyokuHandler |
| Kifu (2) | JosekiWiring, CommentCoordinator |
| Analysis (4) | PvClickController, ConsiderationPositionService, AnalysisPresenter, UsiCommandController |
| Board (2) | PositionEditController, BoardSyncPresenter |

### Tier 1: 1段依存（16件）— Tier 0のみに依存

PlayerInfoController, RecordPresenter, GameRecordModel, KifuNavigationCoordinator, KifuExportController, GameRecordUpdateService, GameRecordLoadService, LiveGameSessionStarted, CoreInitCoordinator, GameStateController, ConsiderationWiring, PositionEditCoordinator, BoardSetupController, BoardLoadService, GameSessionOrchestrator*, BranchNavigationWiring*

> \* 直接呼出はTier 0のみだが、遅延呼出に高Tier含む

### Tier 2: 2段依存（5件）

DialogCoordinator, RecordNavigationHandler, CsaGameWiring, ConsecutiveGamesController, SessionLifecycleCoordinator

### Tier 3: 3段依存（3件）

MatchCoordinatorWiring, KifuLoadCoordinatorForLive, KifuFileController

### Tier 4: 4段依存（1件）

GameStartCoordinator (→MCW→多数)

---

## 8. 再分割設計案

### 8.1 案A: 共通基盤層抽出（推奨）

最も実用的かつ段階的移行が容易な案。

#### 構造

```
MainWindowServiceRegistry (ファサード)
    │
    ├── FoundationRegistry (共通基盤 — 新規クラス)
    │     ensureUiStatePolicyManager
    │     ensurePlayerInfoWiring / ensurePlayerInfoController
    │     ensureKifuNavigationCoordinator
    │     ensureCommentCoordinator
    │     ensureBoardSyncPresenter
    │     ensureUiNotificationService
    │     ensureEvaluationGraphController
    │     (7〜8件)
    │
    └── MainWindowServiceRegistry (残り42件、現行構造を維持)
          UI系 / Game系 / Kifu系 / Analysis系 / Board系
```

#### メリット
- **双方向依存の解消**: UI↔Kifuの双方向依存を引き起こすメソッド（UiStatePolicyManager, KifuNavigationCoordinator, PlayerInfoWiring等）をFoundationに移動。各ドメインはFoundationにのみ依存
- **最小変更**: ヘッダ分割は1クラス追加のみ。呼出元は `m_foundation->ensureXxx()` に変更
- **段階的移行**: 1メソッドずつFoundationに移動可能

#### デメリット
- FoundationRegistry も `MainWindow&` が必要（フィールドアクセスのため）
- メソッド呼出が1段深くなる（`m_registry->m_foundation->ensureXxx()`、またはファサードで委譲）

#### 移行順序
1. FoundationRegistryクラスを作成（MainWindow& を保持）
2. Tier 0 かつ被呼出4ドメイン以上のメソッドから移動開始
3. ServiceRegistryからFoundationRegistryへの参照を追加
4. 呼出元を段階的に書き換え

### 8.2 案B: 完全ドメイン分割（5クラス）

#### 構造

```
MainWindowServiceRegistry (ファサード)
    ├── UiSubRegistry (12件)
    ├── GameSubRegistry (18件)
    ├── KifuSubRegistry (10件)
    ├── AnalysisSubRegistry (5件)
    └── BoardSubRegistry (5件)
```

#### メリット
- 各ドメインの責務が明確に分離
- テスト時にドメイン単位でモック可能

#### デメリット
- **UI↔Kifu双方向依存が未解決**: UiSubRegistry→KifuSubRegistryとKifuSubRegistry→UiSubRegistryの相互参照が必要
- 5クラス間の相互参照管理が複雑
- ファサードの委譲コードが大量に必要（50メソッド×2行）
- 現行の.cppファイル分割と実質同じ効果

### 8.3 案C: 現行維持 + ドキュメント強化

#### 構造
変更なし。本ドキュメントを維持・更新する。

#### メリット
- コード変更ゼロ
- 現行の.cppファイル分割が既にドメイン境界を表現
- 遅延呼出パターンにより実質的なデカップリングが達成済み

#### デメリット
- 150行の単一ヘッダは新規開発者にとって認知負荷が高い
- ドメイン境界がヘッダレベルで強制されない

### 8.4 推奨案の比較

| 評価軸 | 案A (Foundation抽出) | 案B (完全分割) | 案C (現行維持) |
|--------|:---:|:---:|:---:|
| 双方向依存解消 | ○ | △ | × |
| 変更量 | 小〜中 | 大 | なし |
| 段階的移行 | ○ | △ | — |
| 認知負荷低減 | ○ | ○ | × |
| updateDeps()複雑化 | 低 | 高 | — |
| 循環依存リスク | 低 | 中 | 低（現行安定） |

**推奨: 案A（共通基盤層抽出）**

---

## 9. 案A実施時の詳細設計

### 9.1 FoundationRegistry に移動する候補メソッド

| # | メソッド | 被呼出ドメイン数 | 移動理由 |
|---|---------|----------------|---------|
| 1 | `ensureUiStatePolicyManager` | 4 | 最頻出。全ドメインが依存 |
| 2 | `ensurePlayerInfoWiring` | 4 | 全ドメインが依存 |
| 3 | `ensureKifuNavigationCoordinator` | 4 | UI↔Kifu双方向依存の原因 |
| 4 | `ensureCommentCoordinator` | 2 | UI, Kifu双方が直接呼出 |
| 5 | `ensureBoardSyncPresenter` | 2 | KifuNavCoordが遅延依存 |
| 6 | `ensureUiNotificationService` | 2 | Game, Kifuが直接呼出 |
| 7 | `ensureEvaluationGraphController` | 3 | UI, Game, Boardが依存 |

### 9.2 FoundationRegistry クラス設計

```cpp
// mainwindowfoundationregistry.h
class MainWindowFoundationRegistry : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowFoundationRegistry(MainWindow& mw, QObject* parent = nullptr);

    void ensureUiStatePolicyManager();
    void ensurePlayerInfoWiring();
    void ensurePlayerInfoController();
    void ensureKifuNavigationCoordinator();
    void ensureCommentCoordinator();
    void ensureBoardSyncPresenter();
    void ensureUiNotificationService();
    void ensureEvaluationGraphController();

private:
    MainWindow& m_mw;
};
```

### 9.3 ServiceRegistry からの参照

```cpp
// mainwindowserviceregistry.h (変更後)
class MainWindowServiceRegistry : public QObject
{
    // ...
    MainWindowFoundationRegistry* foundation() const { return m_foundation; }

private:
    MainWindowFoundationRegistry* m_foundation;
    MainWindow& m_mw;
};
```

### 9.4 呼出元の変更パターン

```cpp
// Before (mainwindowgameregistry.cpp)
void MainWindowServiceRegistry::ensureMatchCoordinatorWiring()
{
    ensureEvaluationGraphController();
    ensurePlayerInfoWiring();
    // ...
}

// After
void MainWindowServiceRegistry::ensureMatchCoordinatorWiring()
{
    m_foundation->ensureEvaluationGraphController();
    m_foundation->ensurePlayerInfoWiring();
    // ...
}
```

### 9.5 移行ステップ（段階的）

1. **Step 1**: FoundationRegistry クラスの骨格を作成
2. **Step 2**: ensureUiStatePolicyManager を移動（最頻出、Tier 0でリスク低）
3. **Step 3**: ensurePlayerInfoWiring を移動
4. **Step 4**: ensureKifuNavigationCoordinator を移動（UI↔Kifu双方向解消）
5. **Step 5**: 残り4件を移動
6. **Step 6**: ServiceRegistryヘッダからFoundation委譲メソッドを追加（後方互換）
7. **Step 7**: 後方互換委譲を段階的に削除

---

## 10. CompositionRoot との関係

CompositionRoot の11メソッドは**純粋な生成ロジック**のみを担い、他の ensure* を一切呼ばない。このため:

- CompositionRoot は現行のまま変更不要
- ServiceRegistry（またはFoundationRegistry）が CompositionRoot を呼ぶ一方向依存は維持
- Foundation抽出後も CompositionRoot への依存パスは変わらない

```
FoundationRegistry ──> CompositionRoot ──> DepsFactory
ServiceRegistry ──> FoundationRegistry
                └──> CompositionRoot ──> DepsFactory
```

---

## 11. リスク評価まとめ

| リスク | 影響度 | 発生可能性 | 対策 |
|--------|--------|-----------|------|
| Foundation抽出時の循環依存 | 中 | 低 | Tier 0のみを移動すれば循環なし |
| updateDeps()チェーンの複雑化 | 中 | 低 | Foundation内メソッドはDeps不要（ガード+生成のみ） |
| コンパイル時間の増加 | 低 | 低 | ヘッダ1件追加のみ |
| 遅延callback内のthisキャプチャ変更 | 中 | 中 | `this`→`m_foundation`に書換えが必要な箇所あり |
| DockBootstrapper/UiBootstrapper/WiringAssemblerとの整合 | 中 | 中 | これらもServiceRegistryのメンバなので、m_foundationアクセス可 |

---

## 付録: Orchestration層のメソッド

DockBootstrapper / UiBootstrapper / WiringAssembler の各メソッドは、複数ドメインの ensure* を横断的に呼ぶオーケストレーション層。Foundation抽出後もServiceRegistryに残す。

| ファイル | メソッド数 | 主な呼出先ドメイン |
|---------|----------|-----------------|
| mainwindowdockbootstrapper.cpp | 11 | 全ドメイン |
| mainwindowuibootstrapper.cpp | 3 | Game, Board, Dock |
| mainwindowwiringassembler.cpp | 2 | 全ドメイン |
