# CSA通信対局のMCP検証（2026-09-27）

対象は [CSA通信対局ガイド](../guide/csa-game.html) の接続設定、対局待機、通信ログ、人間／エンジン対局。
ShogiBoardQ の操作と状態確認には Python MCP SDK から実際の stdio MCP サーバーを使用した。
GUIはテストごとに専用設定・ソケットで起動し、接続ダイアログと盤面クリックを通して操作する。

## 環境

| 項目 | 使用したもの |
|---|---|
| サーバー | [shogi-server](https://github.com/shogi-server/shogi-server)、コミット `86bf8135909d8b93b85b364bc75dbb2ff7717c1d` |
| 実行環境 | Linux、Ruby 3.4.10、WEBrick 1.9.2、Qt 6、Python 3.14 |
| 対戦クライアント | テスト用CSAクライアント、および `/opt/electron-shogi/shogihome`（ShogiHome 1.29.0） |
| USIエンジン | Hayanagi 1.0.1（`/home/nakada/GitHub/Hayanagi/build/hayanagi`） |
| ソース配置 | `/tmp/shogiboardq-csa-audit/shogi-server` |

ShogiHome は専用プロファイルで起動し、CDP経由で同製品の Electron IPC API を呼び出した。
CSA通信はインストール済みShogiHome本体が実行する。ShogiHome側の画面操作全体を自動テストしたものではない。

## 修正内容

| 発見した問題 | 修正と確認方法 |
|---|---|
| 終局後に次の通信対局を開始できない | 終局状態から再接続できるようにし、先後それぞれで連続2局を確認 |
| 通信対局の棋譜ツリー・現在手数・対局者名が共通モデルに同期されない | 対局開始時に記録を初期化し、着手ごとに同期。保存棋譜の再変換と終局後の手数移動で確認 |
| 接続を取り消すだけで既存の局面履歴が消える | 実際に対局が始まるまで既存棋譜を保持 |
| 成駒の移動をUSIで再度「成り」と記録する／SFEN手数がずれる | 新たな成りと成駒の移動を区別。先後それぞれで角成り→馬の移動→角打ちを確認 |
| Game_Summaryの初期手順を同じ最終局面で記録する | 各手に対応したSFENを棋譜モデルへ渡す |
| エンジンのUSI初期化中にCSA受信が停滞する | TCP受信をキュー接続に変更し、初期化完了までAGREEを保留。readyokを遅延させるエンジンで再現・確認 |
| 切断・単独の `#CHUDAN` で対局UIが復帰しない | 中断として一度だけ終局処理し、時計と操作状態を復帰 |
| 中断・入玉宣言が通常対局用の処理へ流れる | CSAの `%CHUDAN`・`%KACHI` に接続し、サーバー判定を待つ。投了等は自分の手番に制限 |
| 加算時間が残時間・USI時間指定に反映されない／表示時計が独自に終局させる | CSAの手番開始時加算を追跡し、時間切れ判定はサーバーに従う。個別持時間の省略値も共通設定を引き継ぐ |
| 待機ログを後から開くと過去ログが欠落する／LOGINパスワードが別経路に露出する | 最大5000行を保持し、送信ログを生成する時点でパスワードを伏せ字にする |
| エンジン対局中も人間の盤面入力が可能 | CSAの参加種別・手番に基づいて盤面入力を許可 |

## MCPの追加機能

- `get_app_state` に `csa` を追加。状態、先後、人間／エンジン、手番、先後の残時間・消費時間を取得できる。
  残時間は直近のサーバー確認時点の値であり、表示時計の秒単位カウントダウンではない。
- `set_widget_value(..., submit=true)` で単一行入力欄にEnterを送る。
  待機ログの `csaWaitingCommandInput` と対局中の `csaCommandInput` からコマンドを送信できる。
- モーダル待機画面に属する別ウィンドウの通信ログを操作できるようにした。
- パスワード入力欄の読み取りは伏せ字。非表示設定のままの書き込みは従来どおり拒否する。

## 再実行

Ruby・WEBrick・Node.js（WebSocket対応版）・Pythonの `mcp` / `pytest` が必要。
リポジトリのルートから実行する。

```bash
git clone https://github.com/shogi-server/shogi-server.git /tmp/shogiboardq-csa-audit/shogi-server
git -C /tmp/shogiboardq-csa-audit/shogi-server checkout 86bf8135909d8b93b85b364bc75dbb2ff7717c1d
ruby -rwebrick -e 'puts WEBrick::VERSION'
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build -j4
```

ShogiHomeを使う場合は、別端末で次を起動する。テスト専用インスタンスを指定すること。

```bash
xvfb-run -a /opt/electron-shogi/shogihome \
  --no-sandbox --disable-gpu --ozone-platform=x11 --remote-debugging-port=19223 \
  --user-data-dir=/tmp/shogiboardq-csa-audit/shogihome-profile
```

```bash
SHOGIBOARDQ_CLI="$PWD/build/shogiboardq-cli" \
SHOGIBOARDQ_EXECUTABLE="$PWD/build/ShogiBoardQ" \
SHOGIBOARDQ_TEST_CSA_SERVER=/tmp/shogiboardq-csa-audit/shogi-server/shogi-server \
SHOGIBOARDQ_TEST_USI_ENGINE=/home/nakada/GitHub/Hayanagi/build/hayanagi \
SHOGIBOARDQ_TEST_SHOGIHOME_CDP=http://127.0.0.1:19223 \
python3 -m pytest -q -p no:cacheprovider mcp/tests/test_csa_game.py

ctest --test-dir build --output-on-failure -j4
```

サーバーはテストごとに空きポートで `ruby shogi-server --least-time-per-move 0 audit PORT` を起動し、終了時に停止する。
設定、サーバーログ、棋譜、スクリーンショットはpytestの一時ディレクトリに出力される。
通常対局はID `BoardQ` / `Peer`、パスワードは `audit-300-5-b,audit-secret` / `audit-300-5-w`。
加算時間のテストでは `audit-300-5F` を使用する。これらはテスト専用の値。
外部サービス未指定のテストはskipとなる。単独中断・初期手順・接続失敗の補助テストは外部サーバー不要。

## 検証範囲

ビルドは警告なしで成功。CSA結合テストは **19/19成功**（49.21秒）、CTestは **100/100成功**（161.72秒）。
最終修正後に `tst_csaprotocol`・`tst_wiring_csagame`・`tst_automation_dispatcher` も再実行して成功した。
ローカルの実行ログ・保存棋譜・盤面スクリーンショットは `build/csa-mcp-audit/` に保存した。
`lupdate` では新しい翻訳文言がないことを確認し、変更したクラスの翻訳参照位置を更新した。

CSA結合テストは19ケース。先後の人間対局・再対局、待機取消、相手投了、ログ・コマンド送信、
宣言のサーバー判定、切断、時間切れ、加算時間、先後の実エンジン対局、USI初期化遅延、
成り・成駒・駒打ち、KIF/CSA/JKF/USI保存、棋譜移動、ShogiHome相互通信を含む。
単独 `#CHUDAN`、片側だけの持時間指定、初期手順付きGame_Summaryは補助サーバーで確認する。

参照仕様は [CSA TCP/IPプロトコル1.2.1](https://www.computer-shogi.org/protocol/tcp_ip_server_121.html)。
今回のshogi-serverでは `%CHUDAN` は不正コマンドとして反則判定されるため、
中断通知の受信テストと中断要求の送信テストを分けた。初期局面での `%KACHI` もサーバーの反則判定を確認する。
正当な入玉宣言の成立、千日手、長時間連続運転、外部floodgateでの実対局は今回の確認範囲に含まない。
