# Task 64: ウィンドウ/設定復元フローの一本化（C15）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C15 に対応。
推奨実装順序の第7段階（初期化・設定復元・Facade化）。

## 前提

Task 50〜63 の大部分が完了していることが望ましい。

## 背景

コンストラクタ内の手続き順（UI構築→配線→設定復元→コーディネータ初期化）が MainWindow に直接記述されており、起動/終了フローが分散している。

## 目的

起動/終了の手続き順を `MainWindowLifecyclePipeline`（新規）に集約し、MainWindow のコンストラクタを pipeline 起動だけにする。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `buildGamePanels` | ゲームパネル構築 |
| `restoreWindowAndSync` | ウィンドウ復元と同期 |
| `finalizeCoordinators` | コーディネータ初期化完了 |
| `loadWindowSettings` | ウィンドウ設定読込 |
| `saveWindowAndBoardSettings` | ウィンドウ/盤設定保存 |
| `saveSettingsAndClose` | 設定保存して閉じる |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/app/mainwindowuibootstrapper.h` / `.cpp`
- 既存拡張: `src/ui/coordinators/docklayoutmanager.h` / `.cpp`
- 既存拡張: `src/services/settingsservice.h` / `.cpp`
- 新規: `src/app/mainwindowlifecyclepipeline.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: MainWindowLifecyclePipeline 作成
1. `MainWindowLifecyclePipeline` を新規作成。
2. コンストラクタ内の手続き順を `runStartup()` に移管。
3. 終了順を `runShutdown()` に移管。

### Phase 2: 起動フローの移動
1. `buildGamePanels` → パネル構築ステップ。
2. `restoreWindowAndSync` → 復元ステップ。
3. `finalizeCoordinators` → 初期化完了ステップ。
4. `loadWindowSettings` → 設定読込ステップ。

### Phase 3: 終了フローの統合
1. `saveWindowAndBoardSettings` → pipeline の shutdown ステップ。
2. `saveSettingsAndClose` → pipeline 経由に変更。
3. C06 で `MainWindowShutdownService` が作成済みなら、pipeline からそれを呼ぶ。

### Phase 4: MainWindow コンストラクタの簡素化
1. コンストラクタは `pipeline->runStartup()` だけにする。
2. デストラクタは `pipeline->runShutdown()` だけにする。

## 制約

- 既存挙動を変更しない
- 起動順序依存（null 参照）を回避する
- レイアウト復元の順序を変更しない
- 設定保存の順序を変更しない

## 受け入れ条件

- 起動/終了フローが pipeline に一本化されている
- MainWindow コンストラクタが簡素化されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 起動順序依存（null 参照）回避
- レイアウト復元
- 終了時保存
- 設定の保存/読込

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
