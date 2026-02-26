# Task 01: 終了処理の重複解消

元ドキュメント: `docs/dev/mainwindow-responsibility-delegation-methods.md` セクション 4.6

## 対象メソッド

- `MainWindow::saveSettingsAndClose`
- `MainWindow::closeEvent`

## 現状問題

- 設定保存 + エンジン終了が二重管理されている。
- 今後の終了フロー変更で不整合を起こしやすい。

## 移譲先

- 新規 `AppShutdownService` か、既存 `SessionLifecycleCoordinator` へ「アプリ終了前処理」を追加する。

## 実装手順

1. 共通メソッド `performShutdownSequence()` を作成する。
   中身は `saveWindowAndBoardSettings()`, `m_match->destroyEngines()`。
2. `saveSettingsAndClose` は
   `performShutdownSequence(); QCoreApplication::quit();`
3. `closeEvent` は
   `performShutdownSequence(); QMainWindow::closeEvent(e);`
4. 将来的には `SessionLifecycleCoordinator` に `shutdownApp()` を生やして `MainWindow` は委譲のみでもよい。

## 受け入れ条件

- 終了時処理の実体が1か所になる。

## 回帰確認

- メニュー終了、ウィンドウクローズ双方で設定保存とエンジン終了が必ず実行される。
