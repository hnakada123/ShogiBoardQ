# Task 24: MainWindow 責務分割

## 目的

MainWindow から少なくとも1つの責務群を外部クラスへ抽出し、行数と責務を削減する。

## 背景

- `src/app/mainwindow.cpp`: 約 4,569 行
- `src/app/mainwindow.h`: 約 825 行
- メンバ変数約111個、`ensure*()` 37個、`connect()` 71箇所

既に CommentCoordinator、DockCreationService、UiStatePolicyManager 等の抽出が行われているが、以下の責務群が残っている。

## 抽出候補（優先順）

### 候補A: リセット処理

以下のメソッド群を `ResetCoordinator` 等に抽出:
- `resetToInitialState()`（164行）
- `resetEngineState()`
- `resetGameState()`
- `resetModels(const QString&)`
- `resetUiState(const QString&)`

### 候補B: 初期化シーケンス

以下のメソッド群を `InitializationService` 等に抽出:
- `setupHorizontalGameLayout()`
- `setupRecordPane()`
- `setupEngineAnalysisTab()`（83行）
- `setupBoardInteractionController()`
- `setupBoardInCenter()`
- `setupNameAndClockFonts()`
- `setupCentralWidgetContainer()`
- `buildGamePanels()`

### 候補C: ダイアログ起動処理

以下の13個の `display*` メソッド群を `DialogLauncher` 等に抽出:
- `displaySfenCollectionViewer()`
- `displayEngineSettingsDialog()`
- `displayKifuAnalysisDialog()`
- `displayTsumeShogiSearchDialog()`
- `displayTsumeshogiGeneratorDialog()`
- `displayCsaGameDialog()`
- `displayJosekiWindow()`
- 他6個

## 作業

1. 上記候補から1つ以上を選び、外部クラスへ抽出する。
2. 既存の Deps 構造体パターン（Coordinator / Wiring クラスと同様）に従う。
3. MainWindow の public/private slot の数とメンバ依存を減らす。
4. 挙動維持を最優先し、段階的に移行する。

## 制約

- CLAUDE.md のコードスタイルに準拠。
- `connect()` にラムダを使わない。
- 新規クラスは `src/app/` または `src/ui/coordinators/` に配置。
- CMakeLists.txt にソースファイルを追加する（`scripts/update-sources.sh` を参照）。

## 受け入れ条件

- [ ] MainWindow の行数が減る（目安: 300行以上削減 or 1責務群の完全外出し）。
- [ ] 呼び出し元の可読性が向上し、初期化順序が崩れない。
- [ ] ビルド成功、既存テストが通る。
