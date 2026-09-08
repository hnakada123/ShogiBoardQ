# Windows Apery の時間切れ調査（2026-09-08）

> 以下は修正前の調査記録。修正内容と検証結果は [修正記録](engine-timeout-fix-2026-09-08.md) を参照。

## 結論と確認範囲

現在の ShogiBoardQ（HEAD `a3ebfff3`）に、先読み開始時に予測応手を消し、先読みを終了しないまま次の `go` を送る不具合がある。保存設定で先読みが有効な Apery_WCSC30 に同じ通信順序を送ると、6.5秒待っても `bestmove` が返らないことを実機で再現した。先に `stop` とその `bestmove` の受信を挟むと、同じ5秒設定で約4.5秒で応答した。

これは数msの時計誤差ではなく、先読みの状態管理・通信順序の不具合である。一方、GUI時計とUSI応答待ちの期限が一致していない問題も別途存在する。

調査はソース解析、保存設定の読み取り、実エンジンへのUSI通信による比較実験で行った。元の対局のUSIログ・棋譜は取得しておらず、GUIを操作して「5手」の同一対局を再現したわけではない。macOS arm64・Linuxの実行検証もしていないため、報告された全環境の症状を同一原因と断定しない。

## 対象と保存設定

- 実際に登録されている実行ファイル: `C:/Users/hnaka/shogi/apery_wcsc30/apery_wcsc30.exe`
- USI自己紹介: `Apery_WCSC30` / `HiraokaTakuya`
- 保存設定: 後手の持ち時間0、秒読み5秒、増加0。
- `Byoyomi_Margin=500`、`Time_Margin=500`、`USI_Ponder=true`。
- 実験では独立したエンジンプロセスだけに設定を送信し、ユーザーの保存設定は変更していない。

## 先読みが終了されない経路

1. 通常探索が `bestmove 8c8d ponder 2g2f` のように返す。`UsiProtocolHandler::handleBestMoveLine()` が予測応手を `m_predictedOpponentMove` に保存する。
2. `UsiMatchHandler::startPonderingAfterBestMove()` は予測局面を組み立て、`sendGoPonder()` を呼ぶ。
3. `UsiProtocolHandler::sendGoPonder()` は `go ponder` を送る前に **`m_predictedOpponentMove.clear()`** を実行する（`src/engine/usiprotocolhandler.cpp:231`）。エンジンは先読み中だが、GUIの予測応手は空になる。
4. 次の人間の着手後、`UsiMatchHandler::processEngineResponse()` は予測応手が空なので通常探索の分岐に入り、`stop` も `ponderhit` も送らず、`position` と新しい `go` を送る（`src/engine/usimatchhandler.cpp:259`）。
5. 同梱Aperyソースの `ThreadPool::start_thinking()` は、新しい探索の開始前に `wait_for_search_finished()` を呼ぶ（`C:/Users/hnaka/shogi/apery_wcsc30/src/thread.rs:1766`）。先読み側は `stop` または先読み解除を待つため、前の探索が終わらない。
6. 返答が来ない間もGUI時計は進む。`ShogiClock::updateClock()` が秒読み満了を判定して時間切れ・投了通知を出す。

この経路では、時計に数百msの猶予を追加しても根本的には解決しない。

## 実機比較

実験スクリプト: `build/probe-apery.ps1`。PowerShellから標準入出力を接続し、`Stopwatch` で応答待ち時間を計測した。以下は少数回の観測値であり、統計的な最大遅延保証ではない。

| 通信条件 | 観測結果 |
| --- | --- |
| 通常 `go btime 290000 wtime 0 byoyomi 5000`、マージン500ms | 約4,504〜4,508msで `bestmove` |
| 同じ通常探索、実験プロセス内だけマージン0ms | 約5,004msで `bestmove` |
| `go ponder` の後、`stop` なしで次の `position` / `go` | 6,500ms以内に `bestmove` なし |
| `go ponder` → `stop` → `bestmove`受信 → 次の `position` / `go` | 停止応答6ms、次の探索応答4,504ms |

先読みの比較には以下の合法局面を使用した。

```text
position startpos moves 7g7f 8c8d 8h7g
go ponder
（約300ms後）
position startpos moves 7g7f 8c8d 2g2f
go btime 285000 wtime 0 byoyomi 5000
```

比較条件では、2番目の `position` の前に `stop` とその応答受信を追加した。正常順序の実行ログは `build/probe-apery-correct.log`。異常順序で終了待ちになったプロセスは実験終了時に回収した。

## 別途確認した時間管理の問題

### 二つの期限が一致していない

- `ShogiClock::updateClock()` は秒読みの残りが0以下になれば時間切れにする。タイマー間隔は50msで、実経過時間を減算する。
- `UsiMatchHandler::waitAndCheckForBestMoveRemainingTime()` は5,000msから100msを引き、250msの猶予を加えて、USI応答を5,150msまで待つ。
- この100msはエンジンに送る `byoyomi` から引かれる値ではなく、GUI側の応答待ち時間の計算にだけ使われる。
- 応答待ち中のイベントループではGUI時計も処理されるため、USI側の猶予を使い切る前にGUI側が時間切れを宣告できる。
- `ShogiClock::setCurrentPlayer()` は手番フィールドを変更するだけで、直前tickから切り替え時までの時間を旧手番へ精算しない。さらに時計の手番変更と `go` 送信は別の処理時点である。このため両者の計測起点も完全には一致しない。

マージン0の実験結果は、こうした境界でタイマー処理・プロセス通信・描画処理のタイミングにより結果が変わり得ることと整合する。ただし、保存設定のAperyは500msのマージンを持ち、通常探索は約4.5秒で返っているため、今回確認した先読み停止漏れとは区別する。

### 先読み修正時に併せて確認すべき点

- `processEngineResponse()` はヒット判定で `bestMove()` と予測応手を比較している。`bestMove()` は前回のエンジンの着手であり、直近の人間の着手ではない。人間の着手は別のローカル変数で局面文字列に追記される。予測応手の消去だけを取り除いても、ヒット判定は正しくならない。
- `sendGoPonder()` は時間情報を付けず `go ponder` だけを送る。通常探索の5秒がそのまま先読みに引き継がれる保証はない。同梱Aperyは各 `go` で `LimitsType` を新しく作るので、この点も修正時に考慮する必要がある。

## 環境差について

停止漏れの発生条件には `USI_Ponder` の保存値、`bestmove` に予測応手が含まれるか、未終了探索中に次の `go` を受けた際のエンジンの処理が関係する。これらが違えば、OSによって症状が違うように見える。現状の証拠では、Windows固有の時計精度が主因とはいえない。

macOS側の別エンジンについては、その実行時のUSIログで先読み中の `go` 重複と、期限直前の `bestmove` の到着時刻を区別する必要がある。Linuxで発生しない理由も、当該環境のエンジン・設定・ログなしには確定できない。

## 対処の方向

暫定回避はAperyのエンジン設定で `USI_Ponder=false` にすること。これにより確認した先読み停止漏れの経路を避けられる。通常探索の直接実験は成功したが、GUIでの長時間対局による回避確認は未実施。

恒久修正では先読み中の状態と予測応手を保持し、人間の実際の着手に応じて `ponderhit` または `stop` → 応答回収 → 新しい `go` に遷移させる。時間管理は別件として、手番切り替え時の精算、エンジン送信時の残予算、応答受理と時間切れ判定の基準を揃える。

アプリケーションの実装は変更していない。既存Release構成で `cmake --build ... --target ShogiBoardQ` を実行し成功（`ninja: no work to do.`）。新しいアプリケーションバイナリは生成していない。
