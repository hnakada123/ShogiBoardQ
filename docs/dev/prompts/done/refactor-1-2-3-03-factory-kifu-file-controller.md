# Step 03: ensureKifuFileController の factory 化

## 設計書

`docs/dev/mainwindow-refactor-1-2-3-plan.md` のフェーズ1・ステップ3に対応。

## 前提

Step 02 が完了済み（`ensureDialogCoordinator` が factory 化済み、`buildRuntimeRefs()` が存在する）。

## 背景

`MainWindow::ensureKifuFileController()` 内で `KifuFileController::Deps` を組み立てている。多数の `std::bind` / `std::function` コールバックで `MainWindow` のメソッドを参照しており、ここを factory に移す。

## タスク

### 1. ensureKifuFileController() の Deps 組み立てを factory に委譲

`src/app/mainwindow.cpp` の `ensureKifuFileController()` を変更:

**Before（現在のコード）:**
```cpp
KifuFileController::Deps deps;
deps.parentWidget = this;
deps.statusBar = ui->statusbar;
deps.saveFileName = &m_kifu.saveFileName;
deps.clearUiBeforeKifuLoad = std::bind(&MainWindow::clearUiBeforeKifuLoad, this);
deps.setReplayMode = std::bind(&MainWindow::setReplayMode, this, std::placeholders::_1);
deps.ensurePlayerInfoAndGameInfo = [this]() { ... };
// ... 他のコールバック ...
```

**After:**
```cpp
auto refs = buildRuntimeRefs();
auto deps = MainWindowDepsFactory::createKifuFileControllerDeps(refs, /* callbacks */);
```

### 2. コールバックの扱い

`KifuFileController::Deps` は多数のコールバックを持つ:
- `clearUiBeforeKifuLoad` → `std::bind(&MainWindow::clearUiBeforeKifuLoad, this)`
- `setReplayMode` → `std::bind(&MainWindow::setReplayMode, this, ...)`
- `ensurePlayerInfoAndGameInfo` → lambda（`ensurePlayerInfoWiring()` 等を呼ぶ）
- `ensureGameRecordModel` → `std::bind`
- `ensureKifuExportController` → `std::bind`
- `updateKifuExportDependencies` → `std::bind`
- `createAndWireKifuLoadCoordinator` → `std::bind`
- `ensureKifuLoadCoordinatorForLive` → `std::bind`
- `getKifuExportController` → lambda
- `getKifuLoadCoordinator` → lambda

これらは `MainWindow` の `this` に依存するため、ファクトリメソッドの引数として渡す設計にする。引数が多い場合は、コールバックをまとめた構造体を導入しても良い。

### 3. MainWindowDepsFactory の実装追加

- `createKifuFileControllerDeps` の実装を追加する
- `MainWindowRuntimeRefs` からの値マッピング + コールバック引数の受け渡し

## 制約

- `ensureKifuFileController()` の外部から見た挙動は一切変更しない
- `KifuFileController::Deps` 構造体自体は変更しない
- `KifuFileController` クラス自体は変更しない

## 回帰確認

ビルド成功後、以下を手動確認:
- 棋譜ファイル読み込み（KIF/KI2/CSA形式）
- 棋譜ファイル保存・上書き保存
- 棋譜エクスポート
- クリップボードからの棋譜貼り付け

## 完了条件

- `ensureKifuFileController()` 内の `Deps d; d.xxx = ...` の直接構築コードが消え、factory 呼び出しに置き換わっている
- `cmake -B build -S . && cmake --build build` が成功する
- コミットメッセージ: 「ensureKifuFileController の Deps 組み立てを MainWindowDepsFactory に移動」
