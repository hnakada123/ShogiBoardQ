# CSAサーバープロトコル V1.2.1 準拠監査（2026-10-13）

対象は `src/network/`（CsaClient・CsaGameCoordinator・CsaMoveProgressHandler）と CSA 接続ダイアログ。
参照仕様は [CSA TCP/IPプロトコル1.2.1](https://www.computer-shogi.org/protocol/tcp_ip_server_121.html)。
仕様の 3.1〜3.5 を項目ごとに実装と突き合わせ、見つかった12件をすべて修正した。

## 環境

| 項目 | 使用したもの |
|---|---|
| サーバー | `/home/nakada/shogi/shogi-server`（shogi-server、Ruby 3.4.10） |
| 偽サーバー | `mcp/tests/test_csa_game.py` の `FakeServer`（asyncio。shogi-server が出さないメッセージを再現） |
| USIエンジン | Hayanagi（`/home/nakada/GitHub/Hayanagi/build/hayanagi`）と Python 製の擬似エンジン |
| ビルド | Ninja / Release / Qt 6、`-DBUILD_TESTING=ON` |

## 監査で見つかった問題と修正

| 仕様 | 問題 | 修正 |
|---|---|---|
| 3.4 不正な着手・書式 | 相手の指し手行を解釈できないと即「中断」で切断し、続く `#ILLEGAL_MOVE` `#WIN` を捨てていた | 解釈できない指し手行は裁定待ちにし、結果行で勝敗を決める。5秒以内に裁定が無いときだけ中断（`CsaMoveProgressHandler::deferVerdict`） |
| 3.2.3 再開局面 | Position 内の指し手の `,T` を捨て、持時間を初期値に戻していた | 各手の消費時間を保持（`GameSummary::moveConsumedTimes`）。消費時間と各手番の加算を順に適用して残り時間を求め、棋譜欄にも時間を付ける |
| 3.1 / 3.4 終局後 | `GameOver` に留まり、次の Game_Summary を無視し、切断時に LOGOUT も送らなかった | `GameOver` を対局待ちとして扱い、次の対局条件を受けて同じ接続で対局を続ける。エンジンは終局で quit するので次局で起動し直す。切断時は LOGOUT を送る |
| 3.2.2 時間無制限 | Time 階層の省略を「持時間0秒の制限あり」として扱い、エンジンには `btime 0` を渡していた | 双方無制限なら消費時間を数える時計にする。エンジン対局ではローカル対局と同じ方針で REJECT する |
| 3.3 REJECT | 拒否する経路が無く、同意前のキャンセルや初期化失敗で無言で切断していた | 同意前の中止・失敗・切断は REJECT を送ってから LOGOUT する。Format が将棋以外のときも REJECT |
| 3.3 Game_ID 省略 | `REJECT: by <rejector>` を不正行として扱っていた | 空の Game_ID を受け付ける |
| 3.2.2 Time+/Time- | Increment・Delay・Least_Time_Per_Move・Time_Unit・Time_Roundup を先後別に持たず後勝ちだった | `TimeControl` を先後別に保持。共通 Time の値を引き継ぎ、Time+/Time- で書かれた項目だけ上書き。消費時間は指した側の単位で換算 |
| 3.2.2.1 遅延時間ほか | Delay・Max_Moves・Declaration・Format・Protocol_Version を解析のみで使っていなかった | Delay はエンジンの思考時間に加える。Max_Moves・Declaration・Delay・最少消費はログに出す。Declaration が無い入玉宣言は警告。Format／Protocol_Version／Protocol_Mode を確認 |
| 3.1 ログインID | 文字種と32バイト制限を検証していなかった | ID は英数字・`_`・`-` で32文字以内、パスワードは空白を含まない7ビットASCIIで32文字以内に制限（shogi-server の「ゲーム名,パスワード」はそのまま使える） |
| 3.1 LOGOUT | LOGOUT を送ってすぐ切断していた | `LOGOUT:completed` を最長3秒待ってから切断。応答待ちのまま再接続するときは待たずに切り替える |
| 3.4 手番外送信 | 送った指し手の確認が届く前に次を送れた。生コマンドにも保護が無かった | 確認が届くまで指し手・`%` コマンドを送らない（`commandRejected` で通知）。生コマンドも同じガードを通し、`%%` 拡張だけ手番に関係なく送る |
| 3.4 キープアライブ | 未実装 | 対局中は直前の送信から60秒で LF だけの行を送る。手動の空行送信は直前の送信から30秒以上空ける |

shogi-server の実装との差も記録しておく。shogi-server（CSA モード）は終局後に接続を閉じ、不正着手の指し手行はエコーせず `#ILLEGAL_MOVE` だけを送り、手番外の指し手は次の手番まで保留する。`%TORYO` の結果通知に `,T` は付かない。空行には LF を返す。

## 変更したファイル

- `src/network/csaclient.h/.cpp`、`csaclient_parser.cpp` — `TimeControl`・`moveConsumedTimes`、対局待ち状態の扱い、LOGOUT 応答待ち、確認待ちガード、キープアライブ、`commandRejected`
- `src/network/csagamecoordinator.h/.cpp`、`csagamecoordinator_setup.cpp` — 対局条件の確認と REJECT、エンジン再起動、再開局面の時間計算、`initialMoveElapsedText`
- `src/network/csamoveprogresshandler.h/.cpp` — 裁定待ち、先後別加算、遅延時間、時間無制限の時計（`applyClockTimes`）
- `src/ui/wiring/csagamewiring.cpp/.h` — 2局目以降のプレイモード復帰、再開局面の時間表記
- `src/dialogs/csagamedialog.cpp` — ID・パスワードの検証
- `tests/tst_csaprotocol.cpp` — 先後別持時間、再開局面、空 Game_ID、終局後の対局待ち、実ソケット（QTcpServer）での LOGOUT 待ち・確認待ちガード・キープアライブ
- `mcp/tests/test_csa_game.py` — `FakeServer` と仕様準拠テスト7件（エコーされた不正着手、再開局面の時間、時間無制限、同一接続の連続対局と LOGOUT、同意前の REJECT、空 Game_ID、先後別加算）

## 確認

- `ninja -C build` は警告なし。`tst_csaprotocol` 196件、`tst_wiring_csagame` 16件、`tst_app_error_handling` 27件、`tst_translation_files` 13件すべて成功。
- CTest 132件（`tst_structural_kpi` の550行超ファイル数は分割で解消）。
- CSA 結合テスト（shogi-server + Hayanagi + 偽サーバー）: **39成功、3スキップ**（ShogiHome 関連）。
- 翻訳は4言語とも未翻訳0。ベースラインを更新した。

## 再実行

```bash
cmake --build build
ctest --test-dir build --output-on-failure -j4
SHOGIBOARDQ_CLI="$PWD/build/shogiboardq-cli" \
SHOGIBOARDQ_EXECUTABLE="$PWD/build/ShogiBoardQ" \
SHOGIBOARDQ_TEST_CSA_SERVER=/home/nakada/shogi/shogi-server/shogi-server \
SHOGIBOARDQ_TEST_USI_ENGINE=/home/nakada/GitHub/Hayanagi/build/hayanagi \
python3 -m pytest -q -p no:cacheprovider --basetemp=/tmp/sbq mcp/tests/test_csa_game.py
```

偽サーバーのテストは外部サーバー不要。`--basetemp` は自動化ソケットのパス長制限のため短いパスにする。
対局後にもう一度接続するテストは、未保存の棋譜の確認ダイアログを避けるため先に `save_kifu` を呼ぶ。

## 範囲外

- Floodgate 拡張（`x1` ログイン、`%%GAME` など）は仕様外のため対象にしていない。
- 実運用の Floodgate での長時間対局（キープアライブの効果）は確認していない。

## 追記（2026-10-13）: 接続ダイアログの詳細設定

通信対局（CSA）ダイアログに「詳細」タブを追加し、接続を安定させる設定と連続対局の設定を入れた。
設定は `NetworkSettings`（`CsaGameSettings/...`）に保存する。パスワードは従来どおり保存しない。

| 設定 | 既定 | 動作 |
|---|---|---|
| 対局中に空行を送って接続を維持する／間隔 | 有効、40秒 | 直前の送信から指定秒で LF だけの行を送る。下限は仕様の30秒（`CsaClient::setKeepAliveIntervalMs`） |
| TCPキープアライブを使う | 有効 | 接続時に SO_KEEPALIVE と、無通信30秒・間隔10秒・5回の探査をソケットに設定する（Linux: TCP_KEEPIDLE/INTVL/CNT、macOS: TCP_KEEPALIVE、Windows: SIO_KEEPALIVE_VALS）。Nagle は常に無効（TCP_NODELAY） |
| 思考時間の余裕 | 1秒 | エンジンに渡す秒読み（または残り時間）から差し引く。USI 層の250msの余裕とは別に効く |
| 対局数（1局／指定回数／無制限） | 1局 | 上限に達したら LOGOUT し、その後の対局条件は REJECT する。続けるときは同じ接続で次の対局条件を受ける |
| 終局後にサーバーが切断したら再接続して次の対局を待つ | 無効 | サーバーの裁定で終局したあとの切断に限り、3秒後に接続し直す。接続失敗やログイン失敗では再試行しない |
| 終局時に棋譜を自動保存する／保存先／形式 | 無効、KIF | 終局行を記録した直後に `日時_先手_vs_後手.拡張子` で保存する。同じ秒に終わった対局は連番を付ける |
| 後手のときは盤を反転する | 有効 | 対局開始時に割り当てられた手番に合わせて、通常対局と同じ経路（`MatchCoordinator::flipBoard`）で反転する |
| ゲーム名／手番の希望（shogi-server） | 空／指定なし | LOGIN のパスワード欄を「ゲーム名[-b\|-w],パスワード」の形にする。従来のパスワード欄に「ゲーム名,パスワード」と書く方法も使える |

対局を続けるときの終局ダイアログは非モーダルにし、次の対局の開始や LOGOUT を止めない。
対局数を数えて LOGOUT の要否を決めてから終局を通知するので、最後の対局の結果ダイアログが開いていても LOGOUT は送られる。

テストは `tst_settings_roundtrip`（設定の往復）、`tst_csaprotocol`（キープアライブの設定）、
`mcp/tests/test_csa_game.py` の `test_game_limit_logs_out_after_last_game`・`test_reconnects_after_server_disconnect`・
`test_auto_flip_follows_assigned_side`・`test_game_name_and_turn_preference_in_login`・`test_thinking_margin_reduces_engine_budget`。
結合テストで「詳細」タブの部品を操作するときは、先に `set_widget_value(widget="tabWidget", value=1)` でタブを切り替える
（自動化 API は非表示の部品を操作しない）。
