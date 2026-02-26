# Task 56: セッションリセット/終了保存の移譲（C06）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C06 に対応。
推奨実装順序の第4段階（ゲームライフサイクルとリセット）。

## 背景

セッション終了・設定保存・状態リセットのロジックが MainWindow に集中しており、`m_state/m_player/m_kifu` の直接操作が多数残っている。

## 目的

状態管理を `MainWindowStateStore`（新規）、終了処理を `MainWindowShutdownService`（新規）に移し、MainWindow から直接的な状態操作を排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `saveSettingsAndClose` | 設定保存して閉じる |
| `performShutdownSequence` | シャットダウンシーケンス |
| `saveWindowAndBoardSettings` | ウィンドウ/盤設定保存 |
| `closeEvent` | 閉じるイベント |
| `resetToInitialState` | 初期状態リセット |
| `resetEngineState` | エンジン状態リセット |
| `resetGameState` | ゲーム状態リセット |
| `clearGameStateFields` | ゲーム状態フィールドクリア |
| `resetModels` | モデルリセット |
| `resetUiState` | UI状態リセット |
| `clearEvalState` | 評価状態クリア |
| `unlockGameOverStyle` | 終局スタイル解除 |
| `clearSessionDependentUi` | セッション依存UIクリア |
| `clearUiBeforeKifuLoad` | 棋譜ロード前UIクリア |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/app/sessionlifecyclecoordinator.h` / `.cpp`（一部吸収先候補）
- 新規: `src/app/mainwindowstatestore.h` / `.cpp`
- 新規: `src/app/mainwindowshutdownservice.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: MainWindowStateStore 作成
1. `MainWindowStateStore` を新規作成。
2. `m_state/m_player/m_kifu` の直接操作を state store 経由に変更。
3. `clearGameStateFields` を `stateStore->resetForNewSession()` に置換。

### Phase 2: リセット系メソッドの移動
1. `resetToInitialState`, `resetEngineState`, `resetGameState` を `SessionLifecycleCoordinator` または state store へ移動。
2. `resetModels`, `resetUiState` を state store / UI リセットサービスに移動。
3. `clearEvalState`, `unlockGameOverStyle`, `clearSessionDependentUi`, `clearUiBeforeKifuLoad` を移動。

### Phase 3: MainWindowShutdownService 作成
1. `MainWindowShutdownService` を新規作成。
2. `performShutdownSequence` のエンジン破棄・CSA停止を移動。
3. `saveWindowAndBoardSettings` の設定保存を移動。

### Phase 4: closeEvent の簡素化
1. `closeEvent` は `shutdownService->beforeClose()` のみ呼ぶ形にする。
2. `saveSettingsAndClose` は shutdown service に委譲。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 設定保存の順序を変更しない
- エンジン/CSA停止漏れを起こさない

## 受け入れ条件

- 状態管理が StateStore に集約されている
- 終了処理が ShutdownService に集約されている
- MainWindow から対象メソッドが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 設定保存（ウィンドウ、盤、ドック）
- 新規対局時の完全初期化
- 既存エンジン・CSA停止漏れなし
- 棋譜ロード前のUIクリア

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
