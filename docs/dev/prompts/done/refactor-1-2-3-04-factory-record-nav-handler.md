# Step 04: ensureRecordNavigationHandler の factory 化

## 設計書

`docs/dev/mainwindow-refactor-1-2-3-plan.md` のフェーズ1・ステップ4に対応。

## 前提

Step 03 が完了済み（`ensureDialogCoordinator` と `ensureKifuFileController` が factory 化済み）。

## 背景

`MainWindow::ensureRecordNavigationHandler()` 内で `RecordNavigationWiring::Deps` を約20フィールド直接組み立てている。これを factory に移す。

## タスク

### 1. ensureRecordNavigationHandler() の Deps 組み立てを factory に委譲

`src/app/mainwindow.cpp` の `ensureRecordNavigationHandler()` を変更:

**Before（現在のコード）:**
```cpp
RecordNavigationWiring::Deps deps;
deps.mainWindow = this;
deps.navState = m_branchNav.navState;
deps.branchTree = m_branchNav.branchTree;
deps.displayCoordinator = m_branchNav.displayCoordinator;
deps.kifuRecordModel = m_models.kifuRecord;
deps.shogiView = m_shogiView;
deps.evalGraphController = m_evalGraphController;
deps.sfenRecord = sfenRecord();
deps.activePly = &m_kifu.activePly;
deps.currentSelectedPly = &m_kifu.currentSelectedPly;
deps.currentMoveIndex = &m_state.currentMoveIndex;
deps.currentSfenStr = &m_state.currentSfenStr;
deps.skipBoardSyncForBranchNav = &m_state.skipBoardSyncForBranchNav;
deps.csaGameCoordinator = m_csaGameCoordinator;
deps.playMode = &m_state.playMode;
deps.match = m_match;
```

**After:**
```cpp
auto refs = buildRuntimeRefs();
auto deps = MainWindowDepsFactory::createRecordNavigationDeps(refs);
```

### 2. MainWindowDepsFactory の実装追加

- `createRecordNavigationDeps` の実装を追加する
- `RecordNavigationWiring::Deps` は `std::function` コールバックを含まないため、`MainWindowRuntimeRefs` からの純粋なマッピングのみ
- `deps.mainWindow` には `refs.mainWindow` を `QMainWindow*` → `MainWindow*` にキャストする必要があるか確認すること
  - `RecordNavigationWiring::Deps` の `mainWindow` の型を確認して適切に対応する

### 3. MainWindowRuntimeRefs の不足フィールド追加

Step 01 で作成した `MainWindowRuntimeRefs` に不足しているフィールドがあれば追加する。特に:
- `evalGraphController` が含まれているか確認
- `csaGameCoordinator` が含まれているか確認

## 制約

- `ensureRecordNavigationHandler()` の外部から見た挙動は一切変更しない
- `RecordNavigationWiring::Deps` 構造体自体は変更しない
- 信号接続（`connect`）は既存の `RecordNavigationWiring::ensure()` 内のまま変更しない

## 回帰確認

ビルド成功後、以下を手動確認:
- 棋譜行クリックでの盤面追従
- 分岐ライン選択時の盤面同期
- 前後ボタンでの棋譜ナビゲーション

## 完了条件

- `ensureRecordNavigationHandler()` 内の `Deps d; d.xxx = ...` の直接構築コードが消え、factory 呼び出しに置き換わっている
- フェーズ1の対象3箇所すべてで factory 化が完了
- `cmake -B build -S . && cmake --build build` が成功する
- コミットメッセージ: 「ensureRecordNavigationHandler の Deps 組み立てを MainWindowDepsFactory に移動」
