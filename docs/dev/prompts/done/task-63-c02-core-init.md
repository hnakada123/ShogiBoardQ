# Task 63: 起動時コア初期化と局面初期化の移譲（C02）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C02 に対応。
推奨実装順序の第7段階（初期化・設定復元・Facade化）。

## 前提

Task 50〜62 の大部分が完了していることが望ましい。本タスクは初期化フローの最終整理。

## 背景

ShogiGameController/ShogiView/棋譜モデルの初期化ロジックが MainWindow の `initializeComponents` を起点に散在している。`MainWindowUiBootstrapper` が既に存在するが、コア初期化の一部は MainWindow に残っている。

## 目的

コア初期化ロジックを `MainWindowCoreInitCoordinator`（新規）に集約し、MainWindow から初期化ロジックを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `initializeComponents` | コンポーネント初期化 |
| `initializeGameControllerAndKifu` | GC/棋譜初期化 |
| `initializeOrResetShogiView` | ShogiView 初期化/リセット |
| `initializeBoardModel` | 盤面モデル初期化 |
| `initializeNewGame` | 新対局初期化 |
| `initializeNewGameHook` | 新対局Hook |
| `renderBoardFromGc` | GC盤面描画 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/app/mainwindowuibootstrapper.h` / `.cpp`
- 新規: `src/app/mainwindowcoreinitcoordinator.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: MainWindowCoreInitCoordinator 作成
1. `MainWindowCoreInitCoordinator` に `ShogiGameController`/`ShogiView`/`m_state`/`m_kifu` の初期化ロジックを集約。
2. Deps で必要な参照を受け取る。

### Phase 2: 初期化メソッドの移動
1. `initializeGameControllerAndKifu` を coordinator に移動。
2. `initializeOrResetShogiView`, `initializeBoardModel` を coordinator に移動。
3. SFEN 正規化と `newGame` 実行を coordinator へ移す。

### Phase 3: Hook の直接バインド
1. `initializeNewGameHook`/`renderBoardFromGc` は C12 で MatchAdapter に移動済みの場合はスキップ。
2. 未移動の場合、`MatchCoordinatorWiring` から新コーディネータへ直接バインド。

### Phase 4: MainWindow の簡素化
1. `MainWindow` は起動時に `coreInit->initialize()` を1回呼ぶだけにする。
2. `initializeComponents` を coordinator への委譲に変更。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 初期化順序を変更しない
- SFEN 正規化ロジックを変更しない

## 受け入れ条件

- コア初期化が coordinator に集約されている
- MainWindow から初期化ロジックが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 平手開始/途中局面開始/`startpos` 正規化
- 新規対局直後の盤表示
- Hook 経由の盤再描画
- ShogiView の初期化状態

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
