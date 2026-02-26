# Task 59: 棋譜/画像/通知I/Oの移譲（C10）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C10 に対応。
推奨実装順序の第5段階（表示・I/O・判定系）。

## 背景

クリップボード操作・画像保存・エラー通知・棋譜I/Oの入口が MainWindow に残っている。

## 目的

I/O・通知機能を専用サービスに移譲し、MainWindow からI/Oロジックを排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `copyBoardToClipboard` | 盤面クリップボードコピー |
| `copyEvalGraphToClipboard` | 評価グラフクリップボードコピー |
| `saveShogiBoardImage` | 盤面画像保存 |
| `saveEvaluationGraphImage` | 評価グラフ画像保存 |
| `displayErrorMessage` | エラーメッセージ表示 |
| `appendKifuLine` | 棋譜行追記 |
| `appendKifuLineHook` | 棋譜行追記Hook |
| `updateGameRecord` | 棋譜更新 |
| `createAndWireKifuLoadCoordinator` | 棋譜ロードコーディネータ生成配線 |
| `ensureKifuLoadCoordinatorForLive` | ライブ用棋譜ロードコーディネータ |
| `kifuExportController` | 棋譜エクスポートコントローラ取得 |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/board/boardimageexporter.h` / `.cpp`
- 既存拡張: `src/app/gamerecordupdateservice.h` / `.cpp`
- 新規候補: `src/services/uinotificationservice.h` / `.cpp`
- 新規候補: `src/kifu/kifuiofacade.h` / `.cpp`
- `CMakeLists.txt`

## 実装手順

### Phase 1: UiNotificationService 作成
1. `UiNotificationService` を新規作成。
2. `displayErrorMessage` のメッセージボックス表示と `errorOccurred` 更新を移動。

### Phase 2: 画像I/Oの移動
1. `copyBoardToClipboard`, `copyEvalGraphToClipboard` を `BoardImageExporter` に統合。
2. `saveShogiBoardImage`, `saveEvaluationGraphImage` を `BoardImageExporter` に統合。

### Phase 3: 棋譜I/Oの統合
1. `appendKifuLineHook`/`updateGameRecord` を `GameRecordUpdateService` へ一本化し hook を廃止。
2. `appendKifuLine` の MainWindow ラッパーを削除。

### Phase 4: KifuIoFacade 作成
1. `createAndWireKifuLoadCoordinator`, `ensureKifuLoadCoordinatorForLive` を facade に移動。
2. `kifuExportController` のアクセサも facade 経由に変更。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- 画像保存ダイアログの挙動を変更しない

## 受け入れ条件

- I/O・通知機能が専用サービスに移動している
- MainWindow から対象メソッドが削除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- 画像保存/コピー
- エラー通知ダイアログ
- 棋譜追記とモデル反映
- 棋譜ロード/エクスポート

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
