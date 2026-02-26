# Task 53: 棋譜ナビゲーション/分岐同期の移譲（C08）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C08 に対応。
推奨実装順序の第3段階（棋譜ナビと盤操作）。

## 背景

棋譜行選択・分岐遷移・盤同期のロジックが MainWindow に集中しており、KifuNavigationCoordinator/RecordNavigationHandler/BranchNavigationWiring が既に存在するが、MainWindow 側の中間メソッドが多数残っている。

## 目的

棋譜ナビゲーション関連の中間メソッドを既存コーディネータに統合し、MainWindow からナビゲーションロジックを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `onRequestSelectKifuRow` | 棋譜行選択要求（QTableView 直操作） |
| `onRecordPaneMainRowChanged` | 棋譜ペイン行変更 |
| `syncBoardAndHighlightsAtRow` | 盤面/ハイライト同期 |
| `navigateKifuViewToRow` | 棋譜ビュー行移動 |
| `onBranchNodeActivated` | 分岐ノード選択 |
| `onBranchNodeHandled` | 分岐ノード処理完了 |
| `onBranchTreeBuilt` | 分岐ツリー構築 |
| `onBranchTreeResetForNewGame` | 新対局時分岐リセット |
| `syncNavStateToPly` | ナビ状態を手数に同期 |
| `onBuildPositionRequired` | 局面構築要求 |
| `onRecordRowChangedByPresenter` | Presenter経由の行変更 |
| `displayGameRecord` | 棋譜表示 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/app/kifunavigationcoordinator.h` / `.cpp`
- 既存拡張: `src/navigation/recordnavigationhandler.h` / `.cpp`
- 既存拡張: `src/ui/wiring/branchnavigationwiring.h` / `.cpp`
- 既存拡張: `src/ui/presenters/recordpresenter.h` / `.cpp`
- `CMakeLists.txt`（変更がある場合）

## 実装手順

### Phase 1: KifuNavigationCoordinator への統合
1. `onRequestSelectKifuRow` の QTableView 直接操作を `KifuNavigationCoordinator::navigateToRow()` へ統合。
2. `syncNavStateToPly` の探索ロジックを `KifuNavigationCoordinator` 側へ移す。
3. `syncBoardAndHighlightsAtRow` を `KifuNavigationCoordinator` に吸収。

### Phase 2: RecordNavigationHandler への統合
1. `onRecordRowChangedByPresenter` のコメント連携を `CommentCoordinator` と `RecordNavigationHandler` の責務へ寄せる。
2. `onRecordPaneMainRowChanged` を `RecordNavigationHandler` へ移動。

### Phase 3: BranchNavigationWiring への統合
1. `onBranchNodeActivated`/`onBranchNodeHandled`/`onBranchTreeBuilt`/`onBranchTreeResetForNewGame` を `BranchNavigationWiring` へ移動。

### Phase 4: 表示系の移動
1. `displayGameRecord` を `RecordPresenter` 側に吸収。
2. `navigateKifuViewToRow` を `KifuNavigationCoordinator` に吸収。
3. `MainWindow` は転送のみ残す、または直接接続に変更。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- `currentSfenStr` 不整合を起こさない
- 初期化順序を変更しない

## 受け入れ条件

- ナビゲーション関連ロジックが既存コーディネータ/ハンドラに統合されている
- MainWindow から対象メソッドが削除されている（1行転送は許容）
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 本譜/分岐ラインでの行選択同期
- `currentSfenStr` 不整合の再発防止
- コメント表示/未保存確認の挙動
- 新対局時の分岐リセット

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
