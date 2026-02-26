# Task 52: アクション/シグナル配線の移譲（C03）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C03 に対応。
推奨実装順序の第2段階（UI構築と配線）。

## 背景

MainWindow にアクション接続とシグナル配線のメソッドが集中しており、`connect()` 呼び出しが大量に存在する。

## 目的

配線ロジックを `MainWindowSignalRouter`（新規）と既存 Wiring クラスに分散し、`MainWindow` を配線の入口だけにする。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `initializeDialogLaunchWiring` | ダイアログ起動配線初期化 |
| `connectAllActions` | 全アクション接続 |
| `connectCoreSignals` | コアシグナル接続 |
| `connectBoardClicks` | 盤面クリック接続（connectCoreSignals内の場合あり） |
| `connectMoveRequested` | 着手要求接続（同上） |
| `connectAnalysisTabSignals` | 解析タブシグナル接続 |
| `wireCsaGameWiringSignals` | CSA配線シグナル接続 |
| `wireMatchWiringSignals` | Match配線シグナル接続 |
| `onErrorBusOccurred` | エラーバス処理 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/ui/wiring/uiactionswiring.h` / `.cpp`
- 既存拡張: `src/ui/wiring/analysistabwiring.h` / `.cpp`
- 既存拡張: `src/ui/wiring/csagamewiring.h` / `.cpp`
- 既存拡張: `src/ui/wiring/matchcoordinatorwiring.h` / `.cpp`
- 新規: `src/ui/wiring/mainwindowsignalrouter.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: MainWindowSignalRouter 作成
1. `src/ui/wiring/mainwindowsignalrouter.h/.cpp` を新規作成。
2. 接続のみの責務で、ロジックは持たない。

### Phase 2: 既存 Wiring への統合
1. `connectAnalysisTabSignals` のコメント/USI/検討系接続を `AnalysisTabWiring` 側に取り込む。
2. `wireCsaGameWiringSignals` を `CsaGameWiring` 側へ統合。
3. `wireMatchWiringSignals` は MatchCoordinatorWiring 側へ統合（task-45で一部実施済みの場合は差分のみ）。

### Phase 3: コア配線の移動
1. `connectAllActions` と `connectCoreSignals` を MainWindowSignalRouter へ移動。
2. `onErrorBusOccurred` を `ErrorNotificationService`（新規 or 既存）へ移動し、router から呼ぶ。

### Phase 4: MainWindow からの削除
1. MainWindow から配線メソッドを削除。
2. MainWindow は起動時に `signalRouter->connectAll()` を1回呼ぶだけにする。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 二重接続なし（`Qt::UniqueConnection` 維持）
- 初期化順序を変更しない

## 受け入れ条件

- 配線メソッドが SignalRouter/既存 Wiring に移動している
- MainWindow から対象メソッドが削除されている
- 二重接続がない
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 主要メニュー操作が全て有効
- ErrorBus 経由のエラー表示
- 解析タブのシグナル連携
- CSA対局中のナビゲーション制約

## 出力

- 変更ファイル一覧
- 移動した connect() の数
- MainWindow の行数変化
- 回帰リスク
- 残課題
