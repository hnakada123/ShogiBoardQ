# 検証依頼：CSA通信対局の TCP キープアライブを Windows と macOS の実機で確かめる

ShogiBoardQ の通信対局（CSA）には、対局中の無通信で NAT やファイアウォールに接続を切られないよう、
OS の TCP キープアライブをソケットごとに設定するコードがあります。Linux では実装してビルド・動作を確認しましたが、
Windows と macOS の分岐は API の仕様に沿って書いただけで、コンパイルも動作も未確認です。
この分岐が「コンパイルできること」「設定に成功したとログに出ること」「実際にキープアライブのパケットが
30秒の無通信のあと10秒間隔で送られること」を、それぞれの実機で確かめてください。

この依頼は、過去の会話を参照できない状態でも実行できるようにまとめています。作業範囲は、ビルド・動作確認・
問題があったときの最小限の修正とテスト・結果の報告です。修正した場合はコミットしてよいですが、push は報告のあとで
指示を待ってください。説明・報告は日本語でお願いします。

## 作業環境

- リポジトリ：`https://github.com/hnakada123/ShogiBoardQ`（main）。検証対象のコードはコミット `225955c6`
  「通信対局（CSA）ダイアログに接続の維持と連続対局の設定を追加する」以降に含まれます。
- ビルド手順は `docs/dev/windows-build-and-release.md`（Visual Studio 2019/2022 の MSVC、Qt 6.x msvc 64-bit）と
  `docs/dev/macos-build-and-release.md`（Homebrew の cmake/ninja、Qt 6.x macos）にあります。
- 作業前にリポジトリの状態と適用される `AGENTS.md`・`CLAUDE.md` を確認し、既存の変更を保全してください。
- ビルドや一時ファイルは作業用のフォルダに置き、リポジトリに生成物を残さないでください
  （Debug ビルドのアプリは起動したフォルダに `debug.log` を書きます）。
- Python 3.10 以上（偽サーバーに使います）。パケットの観測には macOS では `tcpdump`、Windows では
  Wireshark と Npcap（Loopback Adapter を含めてインストール）を使います。

## 検証対象のコード

- `src/network/csaclient_commands.cpp` の `CsaClient::applySocketOptions()`。サーバーに接続した直後
  （`CsaClient::onSocketConnected()`）に呼ばれ、次を行います。
  1. `QAbstractSocket::LowDelayOption` を立てる（Nagle 無効化。常時）
  2. ダイアログの「TCPキープアライブを使う」が有効なら `QAbstractSocket::KeepAliveOption` を立てる（SO_KEEPALIVE）
  3. Qt は間隔を OS の既定値のままにするので、ソケット記述子に対して OS ごとに間隔を設定する
     - Windows：`<winsock2.h>`・`<mstcpip.h>` の `tcp_keepalive` 構造体を `WSAIoctl(SIO_KEEPALIVE_VALS)` で設定
       （`keepalivetime` = 無通信30秒、`keepaliveinterval` = 10秒。探査回数は Windows が決める。Windows 10 以降は10回）
     - macOS：`setsockopt(IPPROTO_TCP, TCP_KEEPALIVE, 30)`（無通信）、`TCP_KEEPINTVL` 10秒、`TCP_KEEPCNT` 5回
     - Linux：`TCP_KEEPIDLE` 30秒、`TCP_KEEPINTVL` 10秒、`TCP_KEEPCNT` 5回（確認済み）
- 値は `src/network/csaclient.h` の `kTcpKeepAliveIdleSec`（30）、`kTcpKeepAliveIntervalSec`（10）、
  `kTcpKeepAliveProbeCount`（5）。
- ログ（カテゴリ `shogi.network`）：
  - 成功：`TCP keep-alive enabled: idle 30 s interval 10 s`（qCDebug）
  - 失敗：`TCP keep-alive: setsockopt failed`（macOS/Linux）、`TCP keep-alive: WSAIoctl failed: <コード>`（Windows）、
    `TCP keep-alive: no socket descriptor`（qCWarning）
- 設定は通信対局（CSA）ダイアログの「詳細」タブにあります。「TCPキープアライブを使う」（既定：有効）と、
  「対局中に空行を送って接続を維持する」（既定：有効、40秒）。後者は CSA プロトコルの空行キープアライブで、
  今回の検証では TCP のパケットと区別しやすいよう**無効**にして観測してください。

## 手順

### 1. ビルド

Debug 構成でビルドし、`csaclient_commands.cpp` の分岐が警告なしでコンパイル・リンクできることを確かめてください。

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
```

起こりうる問題と直し方（必要なときだけ、最小限で）：

- Windows で `WSAIoctl` / `WSAGetLastError` が未解決：`CMakeLists.txt` の `SBQ_LINK_LIBS`（`target_link_libraries(shogiboardq_objects PUBLIC ${SBQ_LINK_LIBS})` の手前）に、`if(WIN32)` で `ws2_32` を足す。
- Windows で `winsock2.h` と `windows.h` の順序による再定義エラー：`csaclient_commands.cpp` は `<QtGlobal>` の直後に
  `<winsock2.h>` を含めています。Qt のヘッダーより前に来ていることを確かめ、必要なら `WIN32_LEAN_AND_MEAN` を検討する。
- macOS で `TCP_KEEPALIVE`・`TCP_KEEPINTVL`・`TCP_KEEPCNT` が未定義：`<netinet/tcp.h>` の include を確かめる
  （macOS 10.9 以降で定義されています）。

### 2. 設定成功のログを確かめる

偽の CSA サーバーを用意します（ログインに応答したあと何も送らず、接続を保つだけ）。作業用フォルダに保存して起動してください。

```python
# fake_csa_idle_server.py : LOGIN に応答して、あとは接続を保つだけのサーバー
import asyncio, sys

async def handle(reader, writer):
    peer = writer.get_extra_info("peername")
    print("connected", peer, flush=True)
    try:
        while True:
            line = await reader.readline()
            if not line:
                break
            text = line.decode(errors="replace").rstrip("\r\n")
            print("recv", repr(text), flush=True)
            if text.startswith("LOGIN "):
                writer.write(f"LOGIN:{text.split()[1]} OK\n".encode()); await writer.drain()
            elif text == "LOGOUT":
                writer.write(b"LOGOUT:completed\n"); await writer.drain(); break
            elif text == "":
                writer.write(b"\n"); await writer.drain()   # 空行キープアライブへの応答
    finally:
        print("closed", peer, flush=True)
        writer.close()

async def main(port):
    server = await asyncio.start_server(handle, "127.0.0.1", port)
    print("listening", port, flush=True)
    async with server:
        await server.serve_forever()

asyncio.run(main(int(sys.argv[1]) if len(sys.argv) > 1 else 4081))
```

1. `python3 fake_csa_idle_server.py 4081` を起動する。
2. Debug ビルドの ShogiBoardQ を起動し、「対局」→「通信対局（CSA）…」で接続先 `127.0.0.1`、ポート `4081`、
   任意の ID とパスワードを入れる。「詳細」タブで「TCPキープアライブを使う」を有効、「対局中に空行を送って接続を維持する」を
   無効にして「接続」を押す。ログインに成功し、待機画面が「対局待ち」になることを確かめる。
3. アプリを起動したフォルダの `debug.log` に `TCP keep-alive enabled: idle 30 s interval 10 s` があり、
   `TCP keep-alive:` で始まる警告が無いことを確かめる（Release ビルドで試すときは、環境変数
   `QT_LOGGING_RULES=shogi.network.debug=true` を付けて標準エラー出力を見る）。

### 3. キープアライブのパケットを観測する

手順 2 の接続を保ったまま、無通信のあとに OS が送る探査パケットを観測します。待機画面を開いたまま 2 分以上待ってください。

- **macOS**：別の端末で `sudo tcpdump -i lo0 -nn -ttt 'tcp port 4081'`。接続から約30秒後に、ペイロード無し
  （`length 0`）の `[.]` ACK が ShogiBoardQ 側から送られ、サーバー側が ACK を返す対が、その後10秒ごとに続くことを確かめる。
  探査パケットは直前のシーケンス番号より1小さい位置で送られます（tcpdump では `seq` が前の値から進まない）。
- **Windows**：Wireshark を Npcap Loopback Adapter で起動し、表示フィルタ `tcp.port == 4081 && tcp.analysis.keep_alive`
  を使う。約30秒後から `[TCP Keep-Alive]` と `[TCP Keep-Alive ACK]` の対が10秒ごとに現れることを確かめる。
  ループバックを取れない環境では、偽サーバーを別の PC（Linux 可）で動かし、その PC で `tcpdump -nn -ttt 'tcp port 4081'` を取る。
- 比較のため、「TCPキープアライブを使う」を無効にして同じ観測を行い、探査パケットが出ないことも確かめる
  （OS の既定の無通信時間は Windows・macOS とも2時間なので、2分では現れないはずです）。

### 4. 対局中に接続が保たれることを確かめる（余力があれば）

`mcp/tests/test_csa_game.py` の `FakeServer` と同じ要領で、Game_Summary を送って `START:` のあと何も指さないサーバーを使い、
対局中の無通信が3分以上続いても ShogiBoardQ が「中断」にならず、相手の指し手を送れば続きが指せることを確かめてください。
このとき「対局中に空行を送って接続を維持する」を無効にしても切れないことが、TCP キープアライブ単体の確認になります。

### 5. 自動テスト

```bash
ctest --test-dir build --output-on-failure -R "tst_csaprotocol|tst_settings_roundtrip"
```

`tst_csaprotocol` には実ソケット（QTcpServer）を使うテストが含まれます。失敗した場合は、その OS 固有の原因かどうかを
切り分けて報告してください（`docs/dev/prompts/task20261008-mcp-tests-linux.md` に macOS での既知の失敗例があります）。

## 報告の形式

OS ごとに次の表を埋めてください。

| 項目 | Windows | macOS |
|---|---|---|
| OS・Qt・コンパイラの版 | | |
| ビルド（警告の有無） | | |
| `TCP keep-alive enabled` のログ | | |
| 探査パケットの開始（接続から何秒後か） | | |
| 探査パケットの間隔 | | |
| 無効にしたときに探査パケットが出ないこと | | |
| 手順4（任意） | | |
| `ctest` の結果 | | |

あわせて、直した箇所があればそのコミット（件名→空行→本文→`Co-Authored-By`）、残った問題、再現手順を書いてください。
観測に使った tcpdump／Wireshark の出力は、該当部分だけを抜粋して添えてください。

## やらないこと

- キープアライブの秒数やプロトコルの挙動（空行キープアライブの間隔、対局数、再接続など）の変更
- 観測のためのコード追加（ログは既にあります。足りない場合は報告にとどめる）
- push（報告のあとで指示します）
