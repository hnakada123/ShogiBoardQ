# Task 03: 連続対局開始リクエストの組み立て移譲

元ドキュメント: `docs/dev/mainwindow-responsibility-delegation-methods.md` セクション 4.5

## 対象メソッド

- `MainWindow::ensureConsecutiveGamesController` 内の `requestStartNextGame` 接続ラムダ

## 現状問題

- `MainWindow` が `GameStartCoordinator::StartParams` を組み立てている。
- イベント受信者なのに次局開始の詳細手順まで持っている。

## 移譲先

- 既存 `ConsecutiveGamesController` に「次局起動委譲」責務を寄せる。

## 実装手順

1. `ConsecutiveGamesController` 側の signal を変更する。
   現在: `requestStartNextGame(opt, tc)`
   変更案: `requestStartNextGame(const GameStartCoordinator::StartParams& params)`
2. `ConsecutiveGamesController::startNextGame()` で `StartParams` を組み立て、`autoStartEngineMove = true` もそこで設定する。
3. `MainWindow` 側接続は lambda をやめ、専用スロット `onConsecutiveStartRequested(const StartParams&)` を追加して `m_gameStart->start(params)` のみ行う。

## 受け入れ条件

- `ensureConsecutiveGamesController` から `StartParams` 組み立てコードが消える。
- `MainWindow` 側で lambda 接続が1つ減る。

## 回帰確認

- 連続対局で次局が自動開始される。
- 先後入れ替え設定が維持される。
