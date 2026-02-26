# Task 50: ensure* 群の完全分離（C13）— DI/ライフサイクル基盤構築

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C13 に対応。
推奨実装順序の第1段階（全カテゴリの土台）。

## 背景

`MainWindow` に47個の `ensure*` メソッドが集中しており、遅延初期化・依存注入・ライフサイクル管理がすべて MainWindow に混在している。既に tasks 42-45 で一部の wiring ヘルパー（wire/bind）は分離済みだが、`ensure*` メソッド本体は MainWindow に残っている。

## 目的

`ensure*` 群の生成責務を `MainWindowServiceRegistry`（新規）に集約し、`MainWindow` から生成ロジックを排除する。

## 対象メソッド（47件）

`ensureEvaluationGraphController`, `ensureBranchNavigationWiring`, `ensureMatchCoordinatorWiring`, `ensureTimeController`, `ensureReplayController`, `ensureDialogCoordinator`, `ensureKifuFileController`, `ensureKifuExportController`, `ensureGameStateController`, `ensurePlayerInfoController`, `ensureBoardSetupController`, `ensurePvClickController`, `ensurePositionEditCoordinator`, `ensureCsaGameWiring`, `ensureJosekiWiring`, `ensureMenuWiring`, `ensurePlayerInfoWiring`, `ensurePreStartCleanupHandler`, `ensureTurnSyncBridge`, `ensurePositionEditController`, `ensureBoardSyncPresenter`, `ensureBoardLoadService`, `ensureConsiderationPositionService`, `ensureAnalysisPresenter`, `ensureGameStartCoordinator`, `ensureRecordPresenter`, `ensureLiveGameSessionStarted`, `ensureLiveGameSessionUpdater`, `ensureGameRecordUpdateService`, `ensureUndoFlowService`, `ensureGameRecordLoadService`, `ensureTurnStateSyncService`, `ensureKifuLoadCoordinatorForLive`, `ensureGameRecordModel`, `ensureJishogiController`, `ensureNyugyokuHandler`, `ensureConsecutiveGamesController`, `ensureLanguageController`, `ensureConsiderationWiring`, `ensureDockLayoutManager`, `ensureDockCreationService`, `ensureCommentCoordinator`, `ensureUsiCommandController`, `ensureRecordNavigationHandler`, `ensureUiStatePolicyManager`, `ensureKifuNavigationCoordinator`, `ensureSessionLifecycleCoordinator`

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- `src/app/mainwindowcompositionroot.h` / `.cpp`（既存拡張）
- `src/app/mainwindowdepsfactory.h`（既存拡張）
- `src/app/mainwindowruntimerefsfactory.h` / `.cpp`（既存拡張）
- 新規: `src/app/mainwindowserviceregistry.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: MainWindowServiceRegistry の作成
1. `src/app/mainwindowserviceregistry.h/.cpp` を新規作成。
2. `MainWindow*` とその `MainWindowRuntimeRefs` への参照を受け取るコンストラクタ。
3. 最初に5〜10個の小さい `ensure*`（依存が少ないもの）を移動して基本パターンを確立。
   - 候補: `ensureLanguageController`, `ensureJishogiController`, `ensureNyugyokuHandler`, `ensureConsecutiveGamesController`, `ensureCommentCoordinator`, `ensureUsiCommandController`

### Phase 2: 中規模 ensure* の移動
1. `ensureDockLayoutManager`, `ensureDockCreationService`, `ensureRecordNavigationHandler`, `ensureUiStatePolicyManager` を移動。
2. `ensureReplayController`, `ensureTimeController`, `ensureTurnSyncBridge`, `ensurePositionEditController`, `ensureBoardSyncPresenter` を移動。

### Phase 3: 大規模 ensure* の移動
1. `ensureMatchCoordinatorWiring`, `ensureGameStartCoordinator`, `ensureCsaGameWiring` を移動。
2. `ensureDialogCoordinator`, `ensureKifuFileController`, `ensureKifuExportController` を移動。
3. `ensurePlayerInfoWiring`, `ensureMenuWiring`, `ensureConsiderationWiring`, `ensureJosekiWiring`, `ensureBranchNavigationWiring` を移動。

### Phase 4: 残りの ensure* の移動
1. 残りの全 ensure* を移動。
2. `MainWindow` 側は `m_registry->ensureXxx()` への1行転送、または直接 registry 呼び出しに置換。

### Phase 5: MainWindow から ensure* 宣言を削除
1. MainWindow.h から private ensure* 宣言を削除。
2. 呼び出し元を registry 経由に切り替え。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 遅延初期化パターン（nullptr チェック → 生成）を維持する
- 生成済み再利用を維持する（二重生成しない）
- 初期化順序を変更しない
- Phase ごとにビルド確認する

## 受け入れ条件

- `MainWindowServiceRegistry` が作成され、全47個の ensure* メソッドの生成責務を持つ
- `MainWindow` から ensure* メソッドの実装が削除されている（1行転送プロキシは許容）
- 遅延初期化の順序と再利用が維持されている
- ダングリングポインタが発生しない
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 新規対局（平手/駒落ち）
- 棋譜ロード後の行選択
- 検討モード開始/停止
- CSA対局開始
- ドック表示/非表示
- 起動直後と終了直前の設定保存

## 出力

- 変更ファイル一覧
- MainWindowServiceRegistry に移動した ensure* の数
- MainWindow の行数変化（cpp/h）
- 残課題
