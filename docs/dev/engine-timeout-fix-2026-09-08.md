# 先読みと時間切れ判定の修正（2026-09-08）

## 動作変更

- 先読み開始時に予測応手を保持する。実際の着手を含む局面と先読み局面が一致すれば `ponderhit`、不一致なら `stop` → `bestmove` 回収 → 新しい `position` / `go` の順に送信する。人間対エンジン・エンジン対エンジンで共通の経路を使用する。
- `go ponder` に持ち時間・秒読み・増加時間を付ける。停止して回収した予測局面の `resign` / `win` は実対局の終局として通知しない。
- 応答待ち失敗後に古い指し手を適用したり、次の先読みを開始したりしない。既に受信した `bestmove` を待機開始時に消さない。
- ローカル対局のUSI残予算をGUI時計から取得し、通常の `go` は送信直前に再計算する。先読み停止待ちやtick間の経過も差し引く。
- エンジンには最大250ms（残予算の半分を上限）の通信・停止余裕を差し引いて通知する。秒読み5秒なら通常は最大4,750msを通知し、エンジン固有のマージンはさらにエンジン側で適用される。
- GUI時計が時間切れ判定の基準となる。従来の「5,000−100＋250＝5,150ms」という独立した応答待ち猶予はローカル対局から除いた。時間切れ負けが無効な対局では、通信待ちだけでその設定を覆さない。
- 手番変更時に旧手番の未精算時間を計上する。`bestmove`受理時にその手番の時計を精算し、手番が変わるまで盤面・棋譜処理時間を加算しない。期限を過ぎた返答は受理しない。
- 先手・後手で異なる秒読み設定の場合、手番側の値を選ぶ。共通秒読みが0になることによるエンジン対エンジンの時間指定漏れも修正した。

GUI文言やユーザーの保存設定は変更していない。

## 検証

Windows / Qt 6.10.1 / MSVC Releaseでアプリケーションのビルドを実施。

以下の8つのCTest対象がすべて成功した。

- `tst_shogiclock`
- `tst_usiprotocolhandler`
- `tst_usimatchhandler`（追加）
- `tst_game_start_flow`
- `tst_game_end_handler`
- `tst_matchcoordinator`
- `tst_gamestrategy`
- `tst_structural_kpi`

追加した子プロセステストは、先読み中の二重goに応答しない模擬エンジンを使用し、先読みヒット・ミス・停止応答の投了破棄・人間着手API・期限前後の返答・返答後のUI遅延・非対称秒読みを確認する。時計・USI・プロセス通信は実装を使い、盤面表示はスタブにしている。

実Aperyは `SHOGIBOARDQ_TEST_ENGINE` に実行ファイルを指定して次のように検証できる。通常のCTestでは外部エンジンを必要とするこのケースだけスキップする。

```powershell
$env:SHOGIBOARDQ_TEST_ENGINE = 'C:\Users\hnaka\shogi\apery_wcsc30\apery_wcsc30.exe'
$env:QT_QPA_PLATFORM = 'offscreen'
./build/Desktop_Qt_6_10_1_MSVC2022_64bit-Release/tests/tst_usimatchhandler.exe aperyRealProcess
```

このPCではAperyの先読みを有効にした状態で、通常探索 → 先読みヒット → 先読みミス後の再探索がすべて成功した。実行ログは `build/apery-fixed-test.txt`。

実通信ログでは、最初の `byoyomi 4750` に加え、ミス時の `stop` 応答待ちを引いた時間（測定例: `byoyomi 4736`）を確認した。いずれもGUIの秒読み5秒以内に完了した。

修正版のWindows実行ファイルとQt・VC++ランタイムは `build/ShogiBoardQ-timeout-fix/` に配置した。起動対象はその中の `ShogiBoardQ.exe`。既存のリリースフォルダーは変更していない。

macOS・Linuxでの実行は未検証。OS固有の分岐を加えず、共通の時間計測・通信経路を修正した。
