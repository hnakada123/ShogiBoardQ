# Task 58: プレイヤー情報/表示同期の移譲（C09）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C09 に対応。
推奨実装順序の第5段階（表示・I/O・判定系）。

## 背景

プレイヤー名設定・エンジン名表示・矢印ボタン制御など、プレイヤー情報の表示同期が MainWindow に残っている。PlayerInfoController/PlayerInfoWiring/UiStatePolicyManager が既に存在するが、MainWindow 側の中間メソッドが残っている。

## 目的

プレイヤー表示同期を `PlayerPresentationCoordinator`（新規）または既存クラスに統合し、MainWindow からプレイヤー情報ロジックを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `setPlayersNamesForMode` | モード別プレイヤー名設定 |
| `setEngineNamesBasedOnMode` | モード別エンジン名設定 |
| `updateSecondEngineVisibility` | 2エンジン表示切替 |
| `onPlayerNamesResolved` | プレイヤー名解決 |
| `enableArrowButtons` | 矢印ボタン有効化 |
| `onPvDialogClosed` | PVダイアログ閉 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/ui/controllers/playerinfocontroller.h` / `.cpp`
- 既存拡張: `src/ui/wiring/playerinfowiring.h` / `.cpp`
- 既存拡張: `src/ui/coordinators/uistatepolicymanager.h` / `.cpp`
- 新規候補: `src/ui/coordinators/playerpresentationcoordinator.h` / `.cpp`
- `CMakeLists.txt`（変更がある場合）

## 実装手順

### Phase 1: PlayerPresentationCoordinator 作成（または既存クラスへ統合）
1. 名前・エンジン名・表示切替を統合するコーディネータを作成。
2. `setPlayersNamesForMode`, `setEngineNamesBasedOnMode`, `updateSecondEngineVisibility` を移動。

### Phase 2: プレイヤー名解決
1. `onPlayerNamesResolved` を `PlayerInfoWiring` または coordinator に移動。
2. MainWindow スロットから直接接続に変更。

### Phase 3: ナビゲーション制御
1. `enableArrowButtons` を `UiStatePolicyManager` 側に `enableNavigationIfAllowed()` として移動。

### Phase 4: PVダイアログ連携
1. `onPvDialogClosed` を `PvClickController` と `EngineAnalysisTab` 間の直接接続へ変更。
2. MainWindow の中間スロットを削除。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 対局モード別の名前表示を壊さない

## 受け入れ条件

- プレイヤー情報ロジックが既存/新規コーディネータに統合されている
- MainWindow から対象メソッドが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 対局モード別の名前表示
- EvE時の2エンジン表示切替
- PVダイアログ閉時の選択解除
- 矢印ボタンの有効/無効

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
