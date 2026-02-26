# Task 02: 対局者名解決と対局情報反映の移譲

元ドキュメント: `docs/dev/mainwindow-responsibility-delegation-methods.md` セクション 4.4

## 対象メソッド

- `MainWindow::onPlayerNamesResolved`

## 現状問題

- 時間制御情報 (`TimeControlInfo`) の組み立てを `MainWindow` が保持している。
- `m_timeController` 有無で処理分岐しており、UI配線責務を越えている。

## 移譲先

- 既存 `PlayerInfoWiring::resolveNamesAndSetupGameInfo` を統一入口として使う。

## 実装手順

1. `PlayerInfoWiring` に `resolveNamesWithTimeController(...)` を追加する。
   `TimeControlController*` を受け、内部で `TimeControlInfo` 構築まで完結させる。
2. `MainWindow::onPlayerNamesResolved` は以下に縮小する。
   - `ensurePlayerInfoWiring()`
   - `m_playerInfoWiring->resolveNamesWithTimeController(human1, ..., playMode, m_state.startSfenStr, m_timeController);`
3. `clearGameEndTime()` 呼び出しも `PlayerInfoWiring` 側へ移す。

## 受け入れ条件

- `onPlayerNamesResolved` から `TimeControlInfo` 構築コードが消える。
- `MainWindow` は data pass-through のみになる。

## 回帰確認

- 新規対局開始時に開始日時/持ち時間/先手後手名が従来通り表示される。
- `m_timeController == nullptr` のケースでクラッシュしない。
