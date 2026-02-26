# Task 57: UI外観/ウィンドウ表示制御の移譲（C01）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C01 に対応。
推奨実装順序の第5段階（表示・I/O・判定系）。

## 背景

ツールバー設定・フォント設定・盤サイズ追従・盤反転処理など、UI外観に関するロジックが MainWindow に散在している。

## 目的

UI外観制御を `MainWindowAppearanceController`（新規）に集約し、MainWindow から表示制御ロジックを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `setupCentralWidgetContainer` | 中央ウィジェット構築 |
| `configureToolBarFromUi` | ツールバー設定 |
| `installAppToolTips` | ツールチップ設定 |
| `setupBoardInCenter` | 盤面中央配置 |
| `onBoardSizeChanged` | 盤サイズ変更追従 |
| `performDeferredEvalChartResize` | 遅延評価グラフリサイズ |
| `setupNameAndClockFonts` | 名前/時計フォント設定 |
| `flipBoardAndUpdatePlayerInfo` | 盤反転+プレイヤー情報更新 |
| `onBoardFlipped` | 盤反転イベント |
| `onActionEnlargeBoardTriggered` | 盤拡大 |
| `onActionShrinkBoardTriggered` | 盤縮小 |
| `onToolBarVisibilityToggled` | ツールバー表示切替 |
| `onTabCurrentChanged` | タブ切替 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/ui/coordinators/docklayoutmanager.h` / `.cpp`
- 既存拡張: `src/services/settingsservice.h` / `.cpp`
- 新規: `src/ui/controllers/mainwindowappearancecontroller.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: MainWindowAppearanceController 作成
1. `MainWindowAppearanceController::Deps` を作成（`ShogiView*`, `QWidget* central`, `QTimer* resizeTimer`, `QToolBar*`, `QAction* actionToolBar` など）。
2. Deps で必要なウィジェット参照を受け取る。

### Phase 2: ツールバー/フォント/ツールチップの移動
1. `configureToolBarFromUi`, `installAppToolTips`, `setupNameAndClockFonts` を controller へ移動。
2. `onToolBarVisibilityToggled` を controller へ移動。

### Phase 3: 盤面表示の移動
1. `setupCentralWidgetContainer`, `setupBoardInCenter` を controller へ移動。
2. `onBoardSizeChanged`, `performDeferredEvalChartResize` を controller へ移動。

### Phase 4: 盤反転/タブ/設定保存
1. `flipBoardAndUpdatePlayerInfo`, `onBoardFlipped` を controller へ移動。
2. `onTabCurrentChanged` を controller へ移動。
3. タブインデックス保存/ツールバー表示保存は controller 側で `SettingsService` を直接利用。

### Phase 5: MainWindow の簡素化
1. MainWindow では `m_appearanceController->xxx()` 呼び出しだけ残す。
2. 盤拡大/縮小アクションは controller に直接接続。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 盤サイズ追従のタイミングを変更しない
- フォント設定の適用順序を変更しない

## 受け入れ条件

- UI外観制御が AppearanceController に集約されている
- MainWindow から対象メソッドが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 起動時ツールバー表示状態復元
- Ctrl+ホイール後の盤サイズ追従
- 盤反転時の名前/時計/向き
- 最終選択タブの保存復元
- 盤拡大/縮小ボタン

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
