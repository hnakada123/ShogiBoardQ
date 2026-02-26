# Task 51: ドック/パネル構築の移譲（C04）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C04 に対応。
推奨実装順序の第2段階（UI構築と配線）。

## 背景

ドック/パネルの構築メソッドが MainWindow に集中しており、UI構造の組み立てがコンストラクタ周辺に散在している。

## 目的

ドック生成・パネル構築・タブ設定の責務を `MainWindowDockBootstrapper`（新規）に集約し、`MainWindow` からUI組み立てロジックを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `setupRecordPane` | 棋譜ペイン構築 |
| `setupEngineAnalysisTab` | エンジン解析タブ構築 |
| `configureAnalysisTabDependencies` | 解析タブ依存設定 |
| `createEvalChartDock` | 評価値グラフドック生成 |
| `createRecordPaneDock` | 棋譜ペインドック生成 |
| `createAnalysisDocks` | 解析ドック群生成 |
| `createMenuWindowDock` | メニューウィンドウドック生成 |
| `createJosekiWindowDock` | 定跡ウィンドウドック生成 |
| `createAnalysisResultsDock` | 解析結果ドック生成 |
| `updateJosekiWindow` | 定跡ドック更新 |
| `initializeBranchNavigationClasses` | 分岐ナビ初期化 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/app/mainwindowuibootstrapper.h` / `.cpp`
- 既存拡張: `src/ui/wiring/analysistabwiring.h` / `.cpp`（`setupEngineAnalysisTab`/`configureAnalysisTabDependencies` の吸収先）
- 新規: `src/app/mainwindowdockbootstrapper.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: MainWindowDockBootstrapper 作成
1. `src/app/mainwindowdockbootstrapper.h/.cpp` を新規作成。
2. Deps 構造体で `MainWindow*`, `QMainWindow*`, `DockCreationService*`, `DockLayoutManager*` 等を受け取る。
3. `createAllDocks()` メソッドにドック生成順序を固定化。

### Phase 2: ドック生成メソッドの移動
1. `createEvalChartDock`, `createRecordPaneDock`, `createAnalysisDocks`, `createMenuWindowDock`, `createJosekiWindowDock`, `createAnalysisResultsDock` を DockBootstrapper へ移動。

### Phase 3: パネル構築の移動
1. `setupRecordPane` を DockBootstrapper へ移動。
2. `setupEngineAnalysisTab` と `configureAnalysisTabDependencies` を `AnalysisTabWiring` 側へ寄せる。

### Phase 4: 補助メソッドの移動
1. `updateJosekiWindow` の MainWindow 直判定を排除し、適切な wiring クラスへ移動。
2. `initializeBranchNavigationClasses` を DockBootstrapper か BranchNavigationWiring へ移動。
3. `MainWindowUiBootstrapper` は `DockBootstrapper` を呼ぶだけに簡素化。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- ドック生成順序を変更しない（レイアウト復元に影響）
- 遅延初期化パターンを維持する

## 受け入れ条件

- ドック生成・パネル構築が DockBootstrapper/既存 Wiring に移動している
- MainWindow から対象メソッドが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 起動時ドック構成と表示/非表示
- 定跡ドック表示時更新
- 解析タブと対局情報タブの連携
- ドック保存/復元/ロック

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
