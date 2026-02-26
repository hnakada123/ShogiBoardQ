# Task 06: Dock 生成後のレイアウト調整の移譲

元ドキュメント: `docs/dev/mainwindow-responsibility-delegation-methods.md` セクション 4.3

## 対象メソッド

- `MainWindow::setupBoardInCenter`
- `MainWindow::onBoardSizeChanged`
- `MainWindow::performDeferredEvalChartResize`
- `MainWindow::createJosekiWindowDock` の visibility 接続

## 現状問題

- ボードサイズ同期 (`m_central->setFixedSize`) が分散。
- dock 作成後の副作用接続が `MainWindow` に残っている。

## 移譲先

- 既存 `DockCreationService` と `MainWindowUiBootstrapper` を拡張する。
- 必要なら新規 `BoardViewportLayoutService` を追加する。

## 実装手順

1. `DockCreationService` に後処理フックを追加する。
   例: `setJosekiVisibilityReceiver(QObject* receiver, const char* slot)` は使わず、型安全のため `setJosekiVisibilityHandler(std::function<void(bool)>)` よりも `QObject* + member function pointer` 方式の専用 setter を定義する。
2. `setupBoardInCenter` と `onBoardSizeChanged` の重複ロジックを `BoardViewportLayoutService::syncCentralSizeToBoard()` に移す。
3. `performDeferredEvalChartResize` は上記サービス1呼び出しだけにする。
4. `MainWindowUiBootstrapper` で初期レイアウト調整を呼ぶ。

## 受け入れ条件

- ボードサイズ同期の本体実装が `MainWindow` から消える。
- Joseki ドック可視化時更新の接続責務が `DockCreationService` 側に移る。

## 回帰確認

- 盤サイズ変更時に中央領域サイズが追従する。
- Joseki ドック表示時に内容が更新される。
