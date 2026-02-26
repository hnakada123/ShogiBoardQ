# Task 54: 盤面操作/着手入力の移譲（C07）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C07 に対応。
推奨実装順序の第3段階（棋譜ナビと盤操作）。

## 背景

着手入力・盤面読込・ハイライト表示のロジックが MainWindow に残っており、BoardSetupController/BoardLoadService/GameRecordUpdateService が既に存在するが、MainWindow 側の中間メソッドが多い。

## 目的

盤面操作関連のロジックを既存コントローラ/サービスに統合し、MainWindow から着手処理ロジックを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `setupBoardInteractionController` | 盤面インタラクション設定 |
| `onMoveRequested` | 着手要求（未発見の場合は connectMoveRequested 内にある可能性） |
| `onMoveCommitted` | 着手確定 |
| `onJosekiForcedPromotion` | 定跡成り強制 |
| `loadBoardFromSfen` | SFEN盤面読込（BoardLoadService に既存） |
| `loadBoardWithHighlights` | ハイライト付き盤面読込（BoardSyncPresenter に既存） |
| `showMoveHighlights` | 着手ハイライト表示 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/board/boardinteractioncontroller.h` / `.cpp`
- 既存拡張: `src/app/boardloadservice.h` / `.cpp`
- 既存拡張: `src/app/gamerecordupdateservice.h` / `.cpp`
- 既存拡張: `src/board/boardsetupcontroller.h` / `.cpp`（`setupBoardInteractionController` の吸収先候補）
- 新規候補: `BoardMoveOrchestrator`（着手フロー統合）
- `CMakeLists.txt`（変更がある場合）

## 実装手順

### Phase 1: 着手フローの整理
1. `onMoveCommitted` の「盤更新 + 棋譜更新 + 定跡更新」を `BoardMoveOrchestrator`（新規）または `GameRecordUpdateService` に移す。
2. 依存再設定処理を orchestrator 側 `updateDepsFromRuntimeRefs()` に切り出す。

### Phase 2: 定跡強制成りの直接接続化
1. `onJosekiForcedPromotion` を `JosekiWindowWiring` から `BoardSetupController` に直接接続へ変更。
2. MainWindow の中間スロットを削除。

### Phase 3: ハイライトと盤面読込
1. `showMoveHighlights` を `BoardLoadService` または `BoardSyncPresenter` に移動。
2. `loadBoardFromSfen` は既に BoardLoadService にある場合、MainWindow 側のラッパーを削除。

### Phase 4: MainWindow からの削除
1. 対象メソッドを MainWindow から削除。
2. 接続を直接結線に変更。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 検討モード/CSAモードの着手分岐を壊さない
- 成り強制の反映を維持

## 受け入れ条件

- 盤面操作ロジックが既存コントローラ/サービスに統合されている
- MainWindow から対象メソッドが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 人間着手→棋譜更新→盤ハイライト
- 検討モード/CSAモードの着手分岐
- 成り強制の反映
- SFEN盤面読込

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
