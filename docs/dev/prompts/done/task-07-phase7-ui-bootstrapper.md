# Phase 7: MainWindowUiBootstrapper 導入 — UIブート手順の分離

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` のフェーズ7。
`MainWindow` の UI 初期化手順（パネル構築・依存順序制御）を `MainWindowUiBootstrapper` にカプセル化する。

## 対象メソッド

`src/app/mainwindow.cpp` / `src/app/mainwindow.h` から以下を移譲:

- `buildGamePanels()` — ゲームパネル構築（約66行）
- `restoreWindowAndSync()` — ウィンドウ復元と同期
- `finalizeCoordinators()` の一部 — コーディネーター最終設定

## 実装手順

1. **事前調査**
   - `buildGamePanels` の実装を読み、UI部品の生成順序と依存関係を特定
   - `setupRecordPane`, `setupEngineAnalysisTab`, `create*Dock` 等の呼び出し順を整理
   - `restoreWindowAndSync` の処理内容を確認

2. **新規クラス作成**
   - `src/app/mainwindowuibootstrapper.h` / `src/app/mainwindowuibootstrapper.cpp` を作成
   - UI 構築の依存順序を明文化したクラス
   - MainWindow の UI フォーム（`ui->xxx`）へのアクセスは Refs struct 経由

3. **ロジックの移植**
   - `buildGamePanels` の本体を移動
   - 依存順序コメントをクラス内の仕様として正式に定義
   - MainWindow 側は1〜2行の委譲に変更

4. **CMakeLists.txt 更新**

5. **ビルド確認**
   ```bash
   cmake -B build -S . -G Ninja && ninja -C build
   ```

## 完了条件

- `buildGamePanels` が薄い委譲メソッドになる
- UI 初期化の依存順序が `MainWindowUiBootstrapper` に明文化
- ビルドが通る

## 注意

- **中リスク**: 初期化順序の変更は起動時クラッシュの原因になりうる
- UI 部品の生成順序は厳守（依存関係がある）
- `nullptr` 参照や UI 未初期化に注意（計画書 9. リスク表）
- 手動テスト: アプリケーションが正常に起動し、全パネルが表示されること
