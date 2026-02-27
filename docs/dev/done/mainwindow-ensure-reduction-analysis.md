# MainWindow ensure* メソッド削減分析

## 概要

`mainwindow.h` に宣言されている 38個の `ensure*` メソッドすべてが `m_registry->ensureXxx()` への1行転送であることを確認。
全38メソッドを MainWindow から削除可能であり、**残存数 0** まで削減できる。

## 現状

- `mainwindow.h` に 38個の `ensure*` private メソッドが宣言
- `mainwindow.cpp` での実装は全て `m_registry->ensureXxx()` への1行転送
- `MainWindowServiceRegistry` が全ての実体ロジックを保持（5つの分割 .cpp ファイル）
- friend class: `MainWindowServiceRegistry`, `MainWindowLifecyclePipeline`

## 呼び出し元分析

### 呼び出しパターン一覧

| # | ensure* メソッド | 自己呼出し | Pipeline | connect/bind | 分類 |
|---|-----------------|-----------|----------|-------------|------|
| 1 | ensureTimeController | - | initializeEarlyServices | - | C |
| 2 | ensureMatchCoordinatorWiring | - | - | - | A |
| 3 | ensureReplayController | setReplayMode | - | - | B |
| 4 | ensureTurnSyncBridge | - | - | - | A |
| 5 | ensurePositionEditController | - | - | - | A |
| 6 | ensureBoardSyncPresenter | - | - | - | A |
| 7 | ensureBoardLoadService | loadBoardFromSfen, loadBoardWithHighlights | - | - | B |
| 8 | ensureConsiderationPositionService | - | - | - | A |
| 9 | ensureAnalysisPresenter | - | - | - | A |
| 10 | ensureGameStartCoordinator | - | - | - | A |
| 11 | ensureRecordPresenter | - | - | - | A |
| 12 | ensureLiveGameSessionStarted | - | - | - | A |
| 13 | ensureLiveGameSessionUpdater | - | - | - | A |
| 14 | ensureGameRecordUpdateService | - | - | - | A |
| 15 | ensureUndoFlowService | undoLastTwoMoves | - | - | B |
| 16 | ensureGameRecordLoadService | displayGameRecord | - | - | B |
| 17 | ensureTurnStateSyncService | onTurnManagerChanged, setCurrentTurn | - | - | B |
| 18 | ensureGameRecordModel | - | - | connect() | D |
| 19 | ensureDialogCoordinator | - | - | - | A |
| 20 | ensureKifuFileController | - | - | std::bind | D |
| 21 | ensureKifuExportController | - | - | - | A |
| 22 | ensureGameStateController | - | - | - | A |
| 23 | ensureBoardSetupController | - | - | std::bind | D |
| 24 | ensurePvClickController | - | - | - | A |
| 25 | ensurePositionEditCoordinator | - | - | - | A |
| 26 | ensureMenuWiring | - | - | - | A |
| 27 | ensurePlayerInfoWiring | - | - | - | A |
| 28 | ensurePreStartCleanupHandler | - | - | - | A |
| 29 | ensureConsecutiveGamesController | - | - | - | A |
| 30 | ensureLanguageController | - | finalizeAndConfigureUi | - | C |
| 31 | ensureDockLayoutManager | - | finalizeAndConfigureUi, runShutdown | - | C |
| 32 | ensureDockCreationService | - | - | - | A |
| 33 | ensureCommentCoordinator | - | - | - | A |
| 34 | ensureRecordNavigationHandler | onRecordPaneMainRowChanged | - | - | B |
| 35 | ensureUiStatePolicyManager | - | finalizeAndConfigureUi | - | C |
| 36 | ensureKifuNavigationCoordinator | - | - | - | A |
| 37 | ensureSessionLifecycleCoordinator | resetToInitialState, resetGameState | - | - | B |
| 38 | ensureBranchNavigationWiring | - | - | - | A |

### 補足: updateKifuExportDependencies（ensure* ではないが関連）

- `mainwindowkifuregistry.cpp` で `std::bind(&MainWindow::updateKifuExportDependencies, &m_mw)` としてコールバック設定
- `buildRuntimeRefs()` を呼ぶため MainWindow に存在
- ServiceRegistry に移動可能（`m_mw.buildRuntimeRefs()` 経由で呼び出し）

## 削減パターン分類

### パターン A: 完全削除（24メソッド）

MainWindow 上の wrapper は誰にも呼ばれていない。
ServiceRegistry の分割 .cpp ファイル内では `ensureXxx()` として直接呼んでいるため、MW wrapper は不要。

**該当メソッド:**
ensureMatchCoordinatorWiring, ensureTurnSyncBridge, ensurePositionEditController,
ensureBoardSyncPresenter, ensureConsiderationPositionService, ensureAnalysisPresenter,
ensureGameStartCoordinator, ensureRecordPresenter, ensureLiveGameSessionStarted,
ensureLiveGameSessionUpdater, ensureGameRecordUpdateService, ensureDialogCoordinator,
ensureKifuExportController, ensureGameStateController, ensurePvClickController,
ensurePositionEditCoordinator, ensureMenuWiring, ensurePlayerInfoWiring,
ensurePreStartCleanupHandler, ensureConsecutiveGamesController, ensureDockCreationService,
ensureCommentCoordinator, ensureKifuNavigationCoordinator, ensureBranchNavigationWiring

**作業:** `mainwindow.h` の宣言と `mainwindow.cpp` の実装を削除するだけ。

### パターン B: インライン化（7メソッド）

MainWindow のスロット/メソッド内から呼ばれている。
呼び出し箇所で `m_registry->ensureXxx()` に直接置き換える。

| ensure* メソッド | 呼び出し元スロット | 変更内容 |
|---|---|---|
| ensureUndoFlowService | undoLastTwoMoves() | `m_registry->ensureUndoFlowService()` に置換 |
| ensureTurnStateSyncService | onTurnManagerChanged(), setCurrentTurn() | 同上 |
| ensureSessionLifecycleCoordinator | resetToInitialState(), resetGameState() | 同上 |
| ensureGameRecordLoadService | displayGameRecord() | 同上 |
| ensureReplayController | setReplayMode() | 同上 |
| ensureBoardLoadService | loadBoardFromSfen(), loadBoardWithHighlights() | 同上 |
| ensureRecordNavigationHandler | onRecordPaneMainRowChanged() | 同上 |

**Before:**
```cpp
void MainWindow::undoLastTwoMoves()
{
    ensureUndoFlowService();
    m_undoFlowService->undoLastTwoMoves();
}
```

**After:**
```cpp
void MainWindow::undoLastTwoMoves()
{
    m_registry->ensureUndoFlowService();
    m_undoFlowService->undoLastTwoMoves();
}
```

### パターン C: Pipeline の呼び出し先変更（4メソッド）

`MainWindowLifecyclePipeline` が `m_mw.ensureXxx()` で呼んでいる。
`m_mw.m_registry->ensureXxx()` に変更する。

| ensure* メソッド | Pipeline 呼び出し箇所 |
|---|---|
| ensureTimeController | initializeEarlyServices() |
| ensureUiStatePolicyManager | finalizeAndConfigureUi() |
| ensureLanguageController | finalizeAndConfigureUi() |
| ensureDockLayoutManager | finalizeAndConfigureUi(), runShutdown() |

**Before (mainwindowlifecyclepipeline.cpp):**
```cpp
m_mw.ensureTimeController();
```

**After:**
```cpp
m_mw.m_registry->ensureTimeController();
```

### パターン D: connect/bind ターゲットの再配線（3メソッド）

`&MainWindow::ensureXxx` が connect() または std::bind のターゲットになっている。
ServiceRegistry へ再配線する。

#### D-1: ensureGameRecordModel（connect ターゲット）

**場所:** `mainwindowkifuregistry.cpp:229`

```cpp
// Before
connect(m_mw.m_commentCoordinator, &CommentCoordinator::ensureGameRecordModelRequested,
        &m_mw, &MainWindow::ensureGameRecordModel);

// After（ServiceRegistry は QObject なので connect 可能）
connect(m_mw.m_commentCoordinator, &CommentCoordinator::ensureGameRecordModelRequested,
        this, &MainWindowServiceRegistry::ensureGameRecordModel);
```

#### D-2: ensureKifuFileController（std::bind ターゲット）

**場所:** `mainwindowlifecyclepipeline.cpp:219`

```cpp
// Before
d.ensureKifuFileController = std::bind(&MainWindow::ensureKifuFileController, &m_mw);

// After
d.ensureKifuFileController = std::bind(&MainWindowServiceRegistry::ensureKifuFileController,
                                       m_mw.m_registry.get());
```

#### D-3: ensureBoardSetupController（std::bind ターゲット）

**場所:** `mainwindowlifecyclepipeline.cpp:226`

```cpp
// Before
d.ensureBoardSetupController = std::bind(&MainWindow::ensureBoardSetupController, &m_mw);

// After
d.ensureBoardSetupController = std::bind(&MainWindowServiceRegistry::ensureBoardSetupController,
                                         m_mw.m_registry.get());
```

## 削減結果サマリー

| パターン | メソッド数 | 作業内容 |
|---------|----------|---------|
| A: 完全削除 | 24 | 宣言+実装を削除 |
| B: インライン化 | 7 | 呼び出し元で `m_registry->` に置換、宣言+実装を削除 |
| C: Pipeline変更 | 4 | Pipeline で `m_mw.m_registry->` に変更、宣言+実装を削除 |
| D: 再配線 | 3 | connect/bind のターゲットを ServiceRegistry に変更、宣言+実装を削除 |
| **合計** | **38** | **残存: 0** |

### 追加: updateKifuExportDependencies の移動

- `MainWindowServiceRegistry` に新メソッド `updateKifuExportDependencies()` を追加
- 実装: `KifuExportDepsAssembler::assemble(m_mw.m_kifuExportController.get(), m_mw.buildRuntimeRefs())`
- MainWindow 側の宣言・実装を削除
- `mainwindowkifuregistry.cpp` の std::bind を ServiceRegistry に向ける

## 実装順序

### Step 1: パターン A（24メソッド削除）— 影響範囲最小

1. `mainwindow.h` から 24個の ensure* 宣言を削除
2. `mainwindow.cpp` から 24個の ensure* 実装を削除
3. ビルド確認（コンパイル時に漏れがあれば検出される）

### Step 2: パターン B（7メソッドのインライン化）

1. 各スロット/メソッドで `ensureXxx()` を `m_registry->ensureXxx()` に置換
2. `mainwindow.h` から 7個の ensure* 宣言を削除
3. `mainwindow.cpp` から 7個の ensure* 実装を削除
4. ビルド確認

### Step 3: パターン C（4メソッドの Pipeline 変更）

1. `mainwindowlifecyclepipeline.cpp` の 5箇所で `m_mw.ensureXxx()` → `m_mw.m_registry->ensureXxx()` に変更
2. `mainwindow.h` から 4個の ensure* 宣言を削除
3. `mainwindow.cpp` から 4個の ensure* 実装を削除
4. ビルド確認

### Step 4: パターン D（3メソッドの再配線）

1. `mainwindowkifuregistry.cpp:229` の connect() ターゲットを ServiceRegistry に変更
2. `mainwindowlifecyclepipeline.cpp:219,226` の std::bind ターゲットを ServiceRegistry に変更
3. `mainwindow.h` から 3個の ensure* 宣言を削除
4. `mainwindow.cpp` から 3個の ensure* 実装を削除
5. ビルド確認

### Step 5: updateKifuExportDependencies の移動

1. `MainWindowServiceRegistry` に `updateKifuExportDependencies()` メソッドを追加
2. `mainwindowkifuregistry.cpp` のコールバック設定を ServiceRegistry 向けに変更
3. `mainwindow.h/.cpp` から宣言・実装を削除
4. ビルド確認

## Task 04（friend 削減）との依存関係

- 現在の friend は `MainWindowServiceRegistry` と `MainWindowLifecyclePipeline` の2つ
- パターン C は Pipeline が `m_mw.m_registry` にアクセスする前提。Pipeline が friend を外される場合は、Pipeline に Registry へのポインタを渡すアクセサが必要
- パターン D-2, D-3 は Pipeline 内の std::bind。Pipeline の friend アクセスに依存
- 本タスクの実施により friend 経由で ensure* を呼ぶ必要がなくなるため、friend 削減と相互補完的

## 影響範囲

### 変更対象ファイル

- `mainwindow.h` — 38個の ensure* 宣言 + updateKifuExportDependencies 宣言を削除
- `mainwindow.cpp` — 38個の ensure* 実装 + updateKifuExportDependencies 実装を削除
- `mainwindowlifecyclepipeline.cpp` — 5箇所の呼び出しを m_registry 経由に変更 + 2箇所の std::bind 変更
- `mainwindowkifuregistry.cpp` — 1箇所の connect() ターゲット変更 + 1箇所の std::bind 変更
- `mainwindowserviceregistry.h` — updateKifuExportDependencies メソッド追加

### 削減効果

- `mainwindow.h`: ensure* 宣言 38行 + コメント・空行 → 約80行削減
- `mainwindow.cpp`: ensure* 実装 38メソッド（各4行程度）→ 約160行削減
- MainWindow の private メソッド数: 40 → 1（buildRuntimeRefs のみ）
