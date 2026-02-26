# Task 05: Analysis タブ配線群の移譲

元ドキュメント: `docs/dev/mainwindow-responsibility-delegation-methods.md` セクション 4.1

## 対象メソッド

- `MainWindow::setupEngineAnalysisTab`
- `MainWindow::connectAnalysisTabSignals`
- `MainWindow::configureAnalysisTabDependencies`

## 現状問題

- `MainWindow` が UI生成、signal接続、依存設定を同時に持っている。
- `m_analysisTab` と `PlayerInfoWiring` と `CommentCoordinator` の接続順序知識が `MainWindow` に集中している。

## 移譲先

- 既存 `AnalysisTabWiring` を拡張する。
- 必要なら薄い新規 `AnalysisTabCoordinator` を `src/app` に追加し、`MainWindow` は coordinator の `ensureInitialized()` だけ呼ぶ。

## 実装手順

1. `AnalysisTabWiring::Deps` に以下を追加する。
   `QObject* mainWindowReceiver`, `CommentCoordinator*`, `UsiCommandController*`, `ConsiderationWiring*`, `PlayerInfoWiring*`, `ShogiEngineThinkingModel* considerationModel`
2. `AnalysisTabWiring::buildUiAndWire()` で、以下を一括接続する。
   `branchNodeActivated`, `commentUpdated`, `pvRowClicked`, `usiCommandRequested`, `startConsiderationRequested`, `engineSettingsRequested`, `considerationEngineChanged`
3. `configureAnalysisTabDependencies` 相当の処理を wiring 側メソッドに移し、`setAnalysisTab`, `addGameInfoTabAtStartup`, `setConsiderationThinkingModel` も移管する。
4. `MainWindow::setupEngineAnalysisTab()` は「生成と結果受け取り」のみにする。

## 受け入れ条件

- 上記3メソッドから `connect()` 呼び出しが消える。
- `MainWindow` 側の分岐と null-check が大幅に減る。

## 回帰確認

- 起動時に `対局情報` タブが正しく挿入される。
- コメント編集、PVクリック、検討開始、USIコマンド送信が従来通り動作する。
