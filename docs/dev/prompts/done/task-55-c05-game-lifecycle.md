# Task 55: 対局開始/終了ライフサイクルの移譲（C05）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C05 に対応。
推奨実装順序の第4段階（ゲームライフサイクルとリセット）。

## 背景

対局開始・終了・連続対局遷移のロジックが MainWindow に集中しており、GameStartCoordinator/GameStateController/ConsecutiveGamesController が既に存在するが、MainWindow 側に多数の中間処理が残っている。

## 目的

対局ライフサイクル（開始前クリーンアップ → 開始 → 終局後処理）を `GameSessionOrchestrator`（新規）に一本化し、MainWindow から対局ロジックを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `initializeGame` | 対局初期化 |
| `startNewShogiGame` | 新規対局開始 |
| `invokeStartGame` | 対局開始呼び出し |
| `onGameStarted` | 対局開始イベント処理 |
| `onMatchGameEnded` | 対局終了イベント処理 |
| `onRequestAppendGameOverMove` | 終局手追記要求 |
| `onGameOverStateChanged` | 終局状態変更 |
| `handleBreakOffGame` | 対局中断 |
| `onPreStartCleanupRequested` | 開始前クリーンアップ |
| `onConsecutiveStartRequested` | 連続対局開始要求 |
| `onApplyTimeControlRequested` | 時間設定適用 |
| `startNextConsecutiveGame` | 次の連続対局開始 |
| `onConsecutiveGamesConfigured` | 連続対局設定 |
| `movePieceImmediately` | 即時着手 |
| `handleResignation` | 投了処理 |
| `onResignationTriggered` | 投了トリガー |
| `setGameOverMove` | 終局手設定 |
| `stopTsumeSearch` | 詰み探索停止 |
| `openWebsiteInExternalBrowser` | 外部ブラウザ起動 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/game/gamestartcoordinator.h` / `.cpp`
- 既存拡張: `src/game/consecutivegamescontroller.h` / `.cpp`
- 既存拡張: `src/ui/controllers/gamestatecontroller.h` / `.cpp`
- 既存拡張: `src/game/gameendhandler.h` / `.cpp`
- 新規候補: `src/game/gamesessionorchestrator.h` / `.cpp` or `src/app/gamesessionorchestrator.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: GameSessionOrchestrator 作成
1. `GameSessionOrchestrator` を新規作成。
2. Deps/Hooks パターンで依存を注入。
3. `SessionLifecycleCoordinator` は状態遷移の純粋処理、`GameSessionOrchestrator` はイベントオーケストレーションに分離。

### Phase 2: 開始フローの移動
1. `initializeGame`, `startNewShogiGame`, `invokeStartGame`, `onGameStarted` を orchestrator に移動。
2. `onPreStartCleanupRequested`, `onApplyTimeControlRequested` を移動。

### Phase 3: 終了フローの移動
1. `onMatchGameEnded`, `onGameOverStateChanged`, `handleBreakOffGame` を orchestrator に移動。
2. `handleResignation`, `onResignationTriggered`, `setGameOverMove` を移動。
3. `onRequestAppendGameOverMove` の `LiveGameSession::commit` を終局フロー側へ移す。

### Phase 4: 連続対局と補助機能
1. `onConsecutiveStartRequested`, `startNextConsecutiveGame`, `onConsecutiveGamesConfigured` を orchestrator/ConsecutiveGamesController に移動。
2. `stopTsumeSearch`, `movePieceImmediately` を移動。
3. `openWebsiteInExternalBrowser` は適切な場所に移動（UiActionsWiring 等）。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 投了手追記とライブセッション commit 順序を維持
- 連続対局遷移を壊さない

## 受け入れ条件

- 対局ライフサイクルが orchestrator に一本化されている
- MainWindow から対象メソッドが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 通常対局/連続対局/CSA の開始・終局遷移
- 投了手追記とライブセッション commit 順序
- 時間設定適用と対局情報更新
- 投了/中断/時間切れ

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
