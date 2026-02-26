# Task 61: MatchCoordinator配線とコールバックの移譲（C12）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C12 に対応。
推奨実装順序の第6段階（adapter化と補助機能）。

## 前提

Task 50（C13 ensure分離）、Task 60（C11 ポリシー判定/アクセサ）が完了していることが望ましい。

## 背景

MatchCoordinator への hook/callback 群（`initializeNewGameHook`, `renderBoardFromGc`, `showMoveHighlights`, `appendKifuLineHook`, `getRemainingMsFor` 等）が MainWindow のメンバ関数として定義され、`std::bind` で MatchCoordinatorWiring に渡されている。

## 目的

`MainWindowMatchAdapter`（新規・interface実装）に callback 群を集約し、MainWindow から Match hook メソッドを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `ensureMatchCoordinatorWiring` | MCW 生成配線 |
| `buildMatchWiringDeps` | Deps 構築 |
| `initMatchCoordinator` | MC 初期化 |
| `initializeGame` | 対局初期化 |
| `showGameOverMessageBox` | 終局メッセージ表示 |
| `initializeNewGameHook` | 新対局Hook |
| `renderBoardFromGc` | GC盤面描画 |
| `showMoveHighlights` | 着手ハイライト |
| `appendKifuLineHook` | 棋譜行追記Hook |
| `getRemainingMsFor` | 残り時間取得 |
| `getIncrementMsFor` | 加算時間取得 |
| `getByoyomiMs` | 秒読み取得 |
| `updateTurnAndTimekeepingDisplay` | 手番/時間表示更新 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/ui/wiring/matchcoordinatorwiring.h` / `.cpp`
- 新規: `src/app/mainwindowmatchadapter.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: MainWindowMatchAdapter 作成
1. `MainWindowMatchAdapter` を新規作成。
2. callback 群を interface メソッドとして実装。
3. 必要な UI 参照を Deps で受け取る。

### Phase 2: Deps 構築の移動
1. `buildMatchWiringDeps` を adapter 参照から deps を構築するように変更。
2. `MainWindow` のメンバ関数 bind を adapter 経由に置換。

### Phase 3: Hook メソッドの移動
1. `initializeNewGameHook`, `renderBoardFromGc`, `showMoveHighlights`, `appendKifuLineHook` を adapter に移動。
2. `showGameOverMessageBox` を adapter に移動。

### Phase 4: 時間取得の移動（C11 未完了の場合）
1. `getRemainingMsFor`, `getIncrementMsFor`, `getByoyomiMs` を adapter に移動。
2. C11 で MatchRuntimeQueryService が作成済みなら、adapter はそれに委譲。

### Phase 5: MainWindow からの削除
1. MainWindow から Match hook メソッド群を順次削除。
2. `ensureMatchCoordinatorWiring` は ServiceRegistry（C13）または adapter 経由に変更。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- Hook の呼び出しタイミングを変更しない
- `Qt::UniqueConnection` を維持

## 受け入れ条件

- callback 群が MatchAdapter に集約されている
- MainWindow から Match hook メソッドが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 対局開始時Hook（盤面初期化）
- 終局メッセージ表示
- 指し手ハイライト表示
- 時間取得（残り時間/加算/秒読み）
- 棋譜行追記

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
