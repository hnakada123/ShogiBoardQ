# 対局・UI側と通信プロセスの IPC

実装仕様 10.5節の詳細。対局・UI側（C++ の `PeerProcess`）が通信プロセス `shogiboardq-peer` を QProcess で
起動し、標準入出力でやり取りする。

## 1. 起動

| 起動のしかた | 用途 |
|---|---|
| `shogiboardq-peer`（Linux と macOS では `--parent-pid <起動した側の PID>`、macOS では `--no-loopback` を付けることがある） | 対局・UI側から使う。標準入出力で IPC を行う |
| `shogiboardq-peer --service ...` | 協力ノード（7章）。IPC を使わない |
| `shogiboardq-peer --sandbox-self-test [試す相手...]` | 権限の制限を掛けてから（Windows では掛かっていることを確かめてから）、禁止した操作を試し、結果を JSON 1行で標準出力に書いて終わる（試験用。6章） |
| `shogiboardq-peer --version` | 版を1行で書いて終わる |

`--no-loopback` は macOS だけの引数で、引数のどこに置いてもよい（IPC と `--sandbox-self-test` に効く）。loopback への
発信をサンドボックスでも止める（6.3節）。対局・UI側は、ローカルのネットワーク（`sbq-local-1`）を使わないときに付ける。
ほかの OS では受け付けない（終了コード 3）。

`--parent-pid <PID>` は Linux と macOS だけの引数で、引数のどこに置いてもよい（どの起動のしかたにも効く）。値は起動した
側の PID（10進の正の整数。先頭の 0・符号は付けない）。通信プロセスは、起動の直後に読んだ親の PID がこの値と違えば、
ほかのことをする前に、標準エラーに `shogiboardq-peer: sandbox: the parent process has changed` の1行を書いて終了コード 3
で終わる（起動した側がもう終わっていて親が替わっていると、親の終了を見張っても意味がないため）。値の形が違う・2回ある・
値が無いときも、`shogiboardq-peer: sandbox: ...` の1行で終了コード 3。対局・UI側は、Linux と macOS ではいつも付ける。
Windows では受け付けない（終了コード 3。Windows では、起動した側が持つジョブのハンドルで終わる）。

- 対局・UI側は、実行ファイルをアプリと同じフォルダ（`QCoreApplication::applicationDirPath()`）から絶対パスで起動する。
  環境変数は渡さない（Linux と macOS は空の環境。Windows は、無いとプロセスを作れない `SystemRoot` と `LOCALAPPDATA` だけ）。
  標準入出力と標準エラー以外のファイル記述子・ハンドルを引き継がない
  （Unix は `QProcess::UnixProcessFlag::CloseFileDescriptors`、Windows は `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`）。
  起動のしかたは `PeerConfinement`（`src/network/p2p/peerconfinement.cpp`）の1か所で決める。
- 通信プロセスは、標準入力を読む前に権限の制限（6章）を掛ける（Windows では、起動する側が掛けた制限を確かめる）。
  掛けられなければ、標準エラーに理由を書いて終了コード 3 で終わる。
- 標準入力が終端に達したら、直ちに終了する（親の終了）。Linux では親の終了で SIGKILL を受ける設定もする。
  macOS では親の終了を kqueue で見張り、終わったら自分に SIGKILL を送る。
  Windows では、起動する側が持つジョブのハンドルが閉じると終わる。

| 終了コード | 意味 |
|---|---|
| 0 | `shutdown` または標準入力の終端による終了 |
| 1 | そのほかの失敗 |
| 2 | IPC の違反（上限超過・形式不正・知らないメソッド）。対局・UI側が送ったものが正しくない |
| 3 | 権限の制限を掛けられなかった（Windows では、制限なしで起動された） |

標準エラーは診断の文だけに使う（秘密・招待文字列・合言葉・IP アドレスを書かない）。対局・UI側は先頭から 64 KiB までを
読み、保存しない。

## 2. メッセージの形

どちらの向きも、4バイトのネットワークバイト順の長さに、UTF-8 の JSON オブジェクト1つを続ける。
長さは 2〜131072（128 KiB）。超えたら、受けた側は相手を IPC の違反として扱う（通信プロセスは終了コード 2 で終わり、
対局・UI側は通信プロセスを終了させる）。

```json
{"protocolVersion":6,"processGeneration":"3","requestId":"17","method":"sendFrame","params":{...}}
{"protocolVersion":6,"processGeneration":"3","requestId":"17","result":{...}}
{"protocolVersion":6,"processGeneration":"3","requestId":"17","error":{"code":"unknown_connection","message":"..."}}
{"protocolVersion":6,"processGeneration":"3","notification":"frameReceived","params":{...}}
```

- `protocolVersion` は 6（`hello` の応答の `ipcVersion` と同じ）。違えば IPC の違反。版 6 は、`connectionClosed` の
  `reason` に `refused` を足した版（2026-11-21。同じ LAN の合言葉の確認の値。`README.md` 3.1節）。
- `processGeneration` は対局・UI側が起動ごとに決める 10進の文字列。通信プロセスは最初の要求（`hello`）の値を覚え、
  以後の要求の値が違えば IPC の違反とし、応答と通知にはこの値を付ける。対局・UI側は、今の世代と違う値の
  メッセージを捨てる（古いプロセスの残り）。
- `requestId` は対局・UI側が要求ごとに 1 から増やす 10進の文字列。応答は同じ値を返す。対局・UI側は、送っていない・
  応答済み・取り消した要求の応答を捨てる。
- 64ビットの整数は 10進の文字列にする（JSON の数値の精度に頼らない）。件数などの小さな値（0〜1,000,000）だけは
  JSON の整数にする。
- バイト列は base64（RFC 4648 の標準の文字、`=` を付ける）、ハッシュは小文字の16進。
- どちらの側も、知らないメソッド・通知・フィールド、型や範囲の合わない値を拒否する（フィールドを足すときは版を上げる）。
- 任意のコマンドやファイルのパスを運ぶフィールドを置かない。署名を頼む要求、鍵を渡す要求を置かない。

エラーの `code` は次のいずれか。`message` は診断用の英語の短い文（秘密を含めない）。

| code | 意味 |
|---|---|
| `invalid_request` | 引数の形が違う（ほかの IPC の違反は終了で扱う） |
| `invalid_state` | 今は受け付けない（`start` の前など） |
| `network_not_allowed` | このビルドが参加できない networkName |
| `build_expired` | 使用期限を過ぎたビルドで、期限のあるネットワークに参加しようとした |
| `invite_invalid`・`invite_expired`・`invite_wrong_network`・`invite_own` | 招待文字列・合言葉が読めない・期限切れ・別のネットワーク（同じ LAN の合言葉は `local` 以外の profile）・自分の招待 |
| `invite_not_found`・`invite_no_node` | 協力ノードを使う合言葉で、協力ノードにその合言葉で読める招待が無い（合言葉が違う・期限が過ぎた・もう引き取られた・取り消された）・待ち合わせに対応した協力ノードにつながっていない（どの協力ノードも答えない） |
| `unknown_connection` | 知らない接続 |
| `invalid_frame` | 送るフレームの外枠が正しくない（`README.md` 2章） |
| `rate_limited`・`limit` | 頻度・件数の上限 |
| `internal` | そのほかの失敗 |

## 3. 要求（対局・UI側 → 通信プロセス）

| method | params | result |
|---|---|---|
| `hello` | `{"uiVersion": 文字列}` | `{"peerVersion": 文字列, "ipcVersion": 6, "sandbox": Sandbox}` |
| `start` | `{"networkName", "bootstrap": [文字列], "relayOnly": 真偽}` | `{"peerId", "peerIdBytes", "networkId", "profile"}` |
| `createInvite` | `{}` | `{"invite", "code", "expiresAt", "addressCount"}` |
| `cancelInvites` | `{}` | `{}` |
| `inspectInvite` | `{"invite"}` | `{"kind": "invite"\|"code"\|"meetCode", "peerId", "expiresAt", "addressCount", "usableAddressCount", "privateAddresses": [文字列]}` |
| `connectInvite` | `{"invite", "allowPrivate": 真偽}` | `{"connection"}` |
| `setGamePeer` | `{"peerId"}`（空文字で解除） | `{}` |
| `connectPeer` | `{"peerId"}` | `{"connection"}` |
| `sendFrame` | `{"connection", "frame"}` | `{}` |
| `closeConnection` | `{"connection"}` | `{}` |
| `createPresence` | `{}` | `{"presence", "id", "expiresAt", "addressCount"}` |
| `cancelPresence` | `{}` | `{}` |
| `connectPresence` | `{"presence", "allowPrivate": 真偽}` | `{"connection"}` |
| `meetPut` | `{"slot", "blob", "ttlSeconds": 整数}` | `{"stored": 整数}` |
| `meetGet` | `{"slot"}` | `{"blobs": [文字列]}` |
| `shutdown` | `{}` | `{}` |

- `hello` は最初に1回だけ。`Sandbox` は `{"applied": 真偽, "os": "linux" か "windows" か "macos", "mechanisms": [文字列], "landlockAbi": 整数}`
  （`landlockAbi` は Linux 以外では 0）。
  `mechanisms` には掛けたもの（Windows では、掛かっていると確かめたもの）を入れる（16 件まで、1件 32 文字まで）。
  Linux は `no_new_privs`・`landlock_fs`・`seccomp`・`parent_death_signal`・`no_core_dump` と、掛けられた場合だけ
  `landlock_scope`。macOS は `seatbelt`・`parent_death_watch`・`no_core_dump`・`deny_attach` と、`--no-loopback` を付けて
  起動されたときだけ `seatbelt_no_loopback`。Windows は `appcontainer`・`job_kill_on_close`・`job_single_process`・
  `no_child_process`・`no_error_dialog` と、起動の条件で決まるもの（6.2節）: トークンに同じ LAN との送受信の能力
  （privateNetworkClientServer）が無いときだけ `no_private_network`（確かめられなければ入れない）、起動した側が掛けた
  緩和ポリシーのうち読めたものごとに `no_dynamic_code`・`no_win32k`・`no_untrusted_images`・`prefer_system32_images`・
  `no_extension_points`・`no_non_system_fonts`。`applied` は必須のもの（`landlock_scope`・`seatbelt_no_loopback` と、Windows の起動の条件で決まる
  もの以外のすべて）を掛けたとき true。対局・UI側は、自分の OS の必須の名前（`PeerConfinement::requiredMechanisms()`）と、
  起動の条件で決まる名前（macOS の `--no-loopback` を付けたときの `seatbelt_no_loopback`、Windows の緩和ポリシーの6つと、
  ローカルのネットワークを使わないときの `no_private_network`）がすべて入っていることを確かめる（起動の処理が退行しても
  気づけるように）。
  通信プロセスは掛けられなければ終了しているので、`applied: false` を返すことはない。対局・UI側は値を確かめ直す。
- `start` は `hello` の後に1回だけ。`networkName` は `README.md` 1.1節の一覧にあるもの。
  `bootstrap` は利用者が指定した協力ノード（`/p2p/<PeerId>` で終わる multiaddr。16件まで）。今の profile で使えない
  宛先（5章）は捨てる。通信プロセスが自分から接続を始める相手は、この協力ノードと、招待・対局の相手だけである。
  `relayOnly` は「直接接続を使わない」（5.1節）。結果の `peerId` は文字列表記（base58btc）、
  `peerIdBytes` はバイナリの base64、`networkId` は16進、`profile` は `local` か `internet`。
- `createInvite` は 10 分有効の招待を作る（同時に4件まで。超えたら古いものから無効にする）。`expiresAt` は UNIX秒の
  文字列、`addressCount` は招待文字列に入れた到達候補の数。`code` は合言葉（同じ招待を指す短い形）で、`local` の profile で
  直接つなぐ招待には同じ LAN の合言葉（`README.md` 3.1節）、それ以外で、待ち合わせに対応した協力ノードに
  つながっていて到達候補のある招待には、協力ノードを使う合言葉（`README.md` 3.2節）が付く。付けられないときは
  空文字列。合言葉を付けるときは、鍵を作って（協力ノードを使う合言葉は、招待を協力ノードに預けて）から応答するので、
  応答は少し遅れる（その間に `cancelInvites`・`shutdown` を受けたら、招待を作らずに `invalid_state` を返す）。
  招待文字列も合言葉も、対局・UI側で記録しない。
- `inspectInvite`・`connectInvite` の `invite` は、招待文字列か合言葉。接頭辞 `sbqinv1.` で始まれば招待文字列、
  そうでなければ合言葉として読む（合言葉の種類は形で決まる。`README.md` 3.2節）。
- `inspectInvite` は読んだ結果を返し、接続はしない。`kind` は `invite`（招待文字列）・`code`（同じ LAN の合言葉）・
  `meetCode`（協力ノードを使う合言葉）。同じ LAN の合言葉には相手の PeerId も期限も入っていないので、`kind` が
  `code` のとき `peerId` は空文字列、`expiresAt` は `"0"`。`meetCode` は、協力ノードから招待を引き取ってから応答し
  （鍵を作る時間と、協力ノードとのやり取りの分だけ遅れる）、引き取った招待の `peerId`・`expiresAt`・到達候補を返す。
  協力ノードは1回引き取られたら消すので、引き取った招待は、続く `connectInvite` のために期限まで持つ（同じ合言葉を
  もう一度確かめたときも、持っている招待から答える）。引き取れなければ `invite_not_found`・`invite_no_node`。
  `addressCount` は宛先の数（同じ LAN の合言葉は、アドレス1件につき QUIC と TCP の2件。自分の待受を除く）。
  `privateAddresses` は今の profile が `internet` のときの、私設アドレスの到達候補（利用者に見せて許可を求める）。
- `connectInvite`・`connectPeer` は接続を始めて、すぐに番号 `connection`（10進の文字列）を返す。
  同じ LAN の合言葉の `connectInvite` は、合言葉の宛先へ相手の PeerId を決めずにつなぎ、最初につながった相手に、
  合言葉から作ったトークンで対局ストリームを開く（宛先へつながるのを 30 秒まで待ち、その後に下の 30 秒を数える）。
  相手が返す確認の値（`README.md` 2.1節・3.1節）を確かめてから `connectionOpened` を知らせる。確かめられなければ
  （合言葉が違う・期限切れ・使用済み・取り消された・相手が招待した本人でない。見分けない）、`connectionOpened` を
  出さずに `connectionClosed`（`reason` は `refused`）を知らせる。
  協力ノードを使う合言葉の `connectInvite` は、確かめたときに引き取った招待があればそれで、なければ協力ノードから
  引き取ってから、招待文字列のときと同じ手順でつなぐ（引き取れなければ `connectionClosed`、`reason` は
  `unreachable`。対応した協力ノードにつながっていなければ、要求を `invite_no_node` で断る）。
  ストリームが開けたら `connectionOpened`、開けなければ `connectionClosed`（`reason` は `unreachable`・`timeout`、
  `relayOnly` で招待に使える中継経由の宛先が無いときは `no_relay_route`。全体で 30 秒）が届く。
  `connectPeer` は `setGamePeer` の相手にだけ使える（再接続。つなぐのに使った宛先を覚えておいて使う）。
  ストリームは、相手との直接の接続があればその上で開く。中継経由でつながったときは、直接の接続ができるのを
  3 秒まで待つので、`connectionOpened` はその分だけ遅れる（5章「対局ストリームを開く接続」）。
- 受け付けた側（inbound）の `connectionOpened` は、相手の最初のフレームが届いてから出す（最初のフレームを 5 秒待つ）。
  `origin` が `game` なら、続けてそのフレームを `frameReceived` で渡す（`invite` の最初のフレームは
  INVITE_TOKEN なので渡さない）。したがって接続を開いた側（outbound）は、`connectionOpened` を受けたら先にフレームを
  送る（申込みの HELLO、再接続の CONTROL など）。
- 対局ストリームを受け付けるのは、有効な招待の INVITE_TOKEN（招待文字列のトークンか、同じ LAN の合言葉から作った
  トークン）を
  最初に送ってきた相手、在席の知らせのトークンを最初に送ってきた相手（友だちの申し込み）と、`setGamePeer` の相手だけ。
  ほかのストリームは閉じる。
- `setGamePeer` を設定している間は、その PeerId からの対局ストリームだけを受け付け（招待のトークンは要らない。
  `origin` は `game`）、ほかのピアからの対局ストリームは閉じる。
- `sendFrame` の `frame` はフレーム全体（長さの前置きを含めない）の base64。通信プロセスは外枠を確かめてから送る。
  接続ごとの送信の列は 64 件までで、あふれたら `rate_limited` を返す。
  INVITE_TOKEN（type 11）は送れない。
- `closeConnection` で閉じた接続には `connectionClosed` を出さない（相手には閉じたことが届く）。
- `createPresence`・`cancelPresence`・`connectPresence`・`meetPut`・`meetGet` は、友だち一覧（`README.md` 3.3節）に使う。
  秘密と暗号化は対局・UI側が扱い、通信プロセスは、知らせの中身（接続先とトークン）を作ることと、暗号化されたものを
  協力ノードとやり取りすることだけを受け持つ。
  - `createPresence` は在席の知らせを作る。`presence` は、自分の到達候補と申し込み用のトークンを入れた、招待の本体と
    同じ形のバイト列（base64）で、対局・UI側は読まずに暗号化する。`id` は知らせの番号（10進の文字列。1 から）、
    `expiresAt` は期限（作ってから 300 秒）、`addressCount` は入っている到達候補の数（0 なら、相手はつなげない）。
    トークンは、期限まで何度でも使える。同時に 256 件まで持ち、超えたら古いものから無効にする。
  - `cancelPresence` は、作った知らせをすべて無効にする（在席を知らせる設定を切ったとき・友だちを外したとき）。
  - `connectPresence` は、友だちの知らせ（協力ノードから引き取って復号した `presence`）の相手へ、`connectInvite` と
    同じ手順で接続を始める。読めない・期限切れ・別のネットワーク・自分の知らせは、招待と同じエラーで断る。
  - `meetPut` は、番号 `slot`（16進 64 文字）と預けるもの `blob`（base64。40〜1024 バイト）を、待ち合わせに対応した
    協力ノードのすべてに、`ttlSeconds`（1〜600）の間、何度でも引き取れる形で預ける。`stored` は預かった協力ノードの数
    （0 なら、どこにも預けられなかった）。
  - `meetGet` は、`slot` に預けられているものを、対応した協力ノードのすべてに尋ねる。`blobs` は協力ノードが返したもの
    （base64。16 件まで）で、通信プロセスは中身を読まない（鍵を知らない協力ノードが作ったものは、対局・UI側の復号で
    捨てられる）。
- `shutdown` は応答を返した後、招待と在席の知らせを無効にして終わる。

## 4. 通知（通信プロセス → 対局・UI側）

| notification | params |
|---|---|
| `networkStatus` | `{"state": "connecting"\|"online"\|"offline", "connectedPeers": 整数, "reachability": "unknown"\|"public"\|"private", "relayReservations": 整数, "meetNodes": 整数}` |
| `connectionOpened` | `{"connection", "peerId", "peerIdBytes", "route": "direct"\|"relay", "direction": "inbound"\|"outbound", "origin": "invite"\|"friend"\|"game", "presence"}` |
| `connectionRoute` | `{"connection", "route"}`（受け付けた側で、相手との接続が増減して経路が変わったとき） |
| `frameReceived` | `{"connection", "frame"}` |
| `connectionClosed` | `{"connection", "reason"}` |

- 自分や相手のアドレス（IP）は、通知に入れない（対局・UI側は IP を受け取らない。例外は `inspectInvite` の
  `privateAddresses` だけ。招待文字列と同じ LAN の合言葉には自分のアドレスが入っているが、対局・UI側は読まずに利用者へ渡す）。
- `state` は、接続しているピアがある（または協力ノードを指定せずに待受を始めた）とき `online`、協力ノードへ接続中のとき
  `connecting`、それ以外は `offline`。値が変わったときにだけ、1秒に1回まで出す。
- `reachability` は到達性の確認（AutoNAT v2）の結果（`relayOnly` では確かめないので `unknown` のまま）。`route` は対局ストリームの経路。
  接続を開いた側（outbound）は、ストリームを開いた接続の経路を知らせる（開いた後は変わらないので、`connectionRoute` は
  出さない）。受け付けた側（inbound）は、ストリームがどの接続の上にあるかを知れないので、その相手との接続がすべて
  中継経由なら `relay`、1つでも直接の接続があれば `direct` とし、接続が増減して変わったら `connectionRoute` で知らせる。
- `meetNodes` は、つながっている協力ノードのうち、待ち合わせに対応したものの数（0 なら、協力ノードを使う合言葉と
  友だち一覧は使えない）。
- `connectionOpened` の `origin` は、招待のトークンで開いた接続は `invite`、在席の知らせのトークンで開いた接続
  （友だちの申し込み。受け付けた側だけ）は `friend`、対局の相手のつなぎ直しは `game`。`presence` は、`origin` が
  `friend` のとき、使われた知らせの番号（`createPresence` の `id`）で、ほかは `"0"`。対局・UI側は、その番号の知らせを
  預けた友だちのプレイヤーID を、相手が名乗らなければ断る。
- `frameReceived` は外枠を確かめたフレームだけを運ぶ。INVITE_TOKEN は運ばない。
- `connectionClosed` の `reason` は `closed`・`reset`・`timeout`・`unreachable`・`no_relay_route`・`invalid_frame`・`rate_limited`・`limit`・
  `refused`。`refused` は、同じ LAN の合言葉の `connectInvite` で、相手が確認の値を返さなかった（`connectionOpened` の前だけ。
  対局・UI側は、招待した側に受け付けられなかったときと同じ知らせを出す）。
  `shutdown` の後は、開いている接続を知らせずに終わる（接続は通信プロセスとともに閉じる）。

## 5. 宛先の規則

| profile | 待受 | 使える宛先 |
|---|---|---|
| `local` | IPv4 の全インターフェースの TCP と QUIC。ポートは 48210（使用中なら空いているポート）。IPv6 では待ち受けない | loopback・私設アドレス・中継 |
| `internet` | `/ip4/0.0.0.0` と `/ip6/::` の TCP・QUIC（`relayOnly` なら待受を広告しない） | インターネット上のアドレスと中継 |

`internet` では、招待・Identify で得た宛先から次を除く:
loopback（127.0.0.0/8、::1）、私設（10/8、172.16/12、192.168/16）、link-local（169.254/16、fe80::/10）、
共有アドレス空間（100.64.0.0/10）、ULA（fc00::/7）、マルチキャスト（224.0.0.0/4、ff00::/8）、
未指定（0.0.0.0/8、::）、IPv4 射影（::ffff:0:0/96）、そのほか予約済み（240.0.0.0/4、255.255.255.255、
192.0.2/24・198.51.100/24・203.0.113/24・2001:db8::/32 の文書用、198.18/15、廃止された 6to4 の中継の 192.88.99/24）。
私設アドレスは `connectInvite` の `allowPrivate` が true の招待の宛先にだけ使う。
6to4（2002::/16）は埋め込んだ IPv4 がインターネット上のアドレスのときだけ使い、Teredo（2001::/32）・
サイトローカル（fec0::/10）・100::/64・NAT64（64:ff9b::/32）は使わない。

宛先は、どの profile でも決めた形だけを受け付ける: `ip/tcp`・`ip/udp/quic-v1`（末尾の `/p2p/<PeerId>` は許す）と、
中継経由の `[直接の形]/p2p/<中継>/p2p-circuit[/p2p/<宛先>]`・`/p2p/<中継>/p2p-circuit[/p2p/<宛先>]`。この形の中では
先頭の IP が唯一の IP になる。IP を2つ重ねた宛先（`/ip4/<A>/tcp/1/ip4/<B>/tcp/2`）・知らない部品を含む宛先・中継の
入れ子は使わない（libp2p の TCP は末尾の IP へつなぐので、先頭だけを確かめる規則を抜けられる）。

- DNS 名の宛先（`/dns4`・`/dns6`・`/dnsaddr`）は v1 では使わない（名前解決のためのファイルを読まない）。
- 1ピアあたりの宛先は8件まで。同時に試す接続は8件まで。失敗した宛先は 60 秒再試行しない。
- 自分が広告するアドレスからも私設アドレスを除く。Identify で配るのは対応するプロトコルとアプリの版
  （`shogiboardq-peer/<版>`）と、`internet` で到達性を確かめたアドレス・中継のアドレスだけにする。
- mDNS は使わない。
- `local` は、同じ PC と同じ LAN の相手と、協力ノードなしでつなぐ。待受のアドレスのうち私設アドレスを、自分のアドレス
  として招待（と在席の知らせ）に載せる（Identify には載せない。Identify は招待を持たない相手にも届くので、待受のポートに
  つないだだけの LAN の機器に、この PC の私設アドレスの一覧を配らない。つなぎ直すのは招待で接続した側で、宛先は
  招待のものを使う）。選び方は同じ LAN の合言葉と同じ（`README.md` 3.1節）: 回線のつながっていないインターフェース
  （コンテナのいない docker0 など。調べられる OS だけ）のアドレスは、私設アドレスがすべてそうでなければ載せず、
  loopback（127.0.0.1）は私設アドレスが1つもないときだけ載せる（同じ PC の相手も、私設アドレスでつなげる）。どの
  アドレスも、TCP と QUIC の両方の待受を載せる。届いた接続は、接続元が loopback・私設アドレスのものだけを受け付け、ほかは
  暗号の手続きを始める前に閉じる（Transport の層。中継経由の接続は接続元の IP を持たないので、ここでは断らない）。
  私設アドレスへの接続に、招待ごとの許可（`allowPrivate`）は求めない（このネットワークを選んだ利用者は、LAN 内の
  宛先へつなぐことを選んでいる）。
- `local` の待受のポートは TCP・UDP とも 48210 に決める。起動のたびに変わると、招待する側の PC のファイアウォールで
  許可するポートを決められないためである。ほかのプロセス（同じ PC の2つ目のアプリなど）が使っていれば、空いている
  ポートで待ち受ける。TCP は、使用中かを先に確かめる（libp2p の TCP の待受は同じポートを複数のプロセスで共有できる
  設定で開くので、確かめないと、届いた接続が2つのプロセスに振り分けられる）。確かめてから開くまでの間に、同時に
  起動したプロセスも同じことをすると両方が開けてしまうので、UDP（QUIC）を先に開け、48210 の UDP を開けたプロセス
  だけが TCP でも 48210 を試す（UDP の待受は共有できないので、48210 の持ち主が1つに決まる）。協力ノード（7章）の
  `--listen` にはこの規則を掛けず、指定したアドレスを、書いた順や UDP の有無によらずそのまま試す（TCP が使用中かは
  確かめる）。
  `internet` では、到達性の確認（AutoNAT v2）で確かめたインターネット上のアドレスと中継のアドレスだけを載せる。
- 到達性の確認は AutoNAT v2（`/libp2p/autonat/2/dial-request`）で行う。確かめてもらうのは、相手（協力ノード）が
  観測した自分のアドレスの候補のうちインターネット上の直接のアドレスだけで、1件ずつ頼む（v1 は待受のアドレス
  （私設・loopback・link-local を含む）を全部送るので使わない）。確認に応えるのは協力ノード（`--service`）だけで、
  ふつうのプレイヤーは応えない（ほかのピアの頼みで、そのピアへ接続を始めない）。協力ノードへは新しいポートで接続し、
  観測されたアドレスを待受のポートに直して候補にする。
- 中継の hop に対応する協力ノードには、自分の到達性によらず予約を取る（2件まで）。中継経由のアドレスは、利用者が
  指定した協力ノードの宛先から作り、招待に入れる（`relayOnly` の相手は、中継経由のアドレスでしかつなげない）。
- 対局ストリームを開く接続（`peer/src/node/outbound.rs`）: 対局ストリームは、開いた接続の上に乗ったまま動かせない。
  中継でつながった直後に開くと、その後に直接の接続（DCUtR）ができても、フレームは中継を通り続ける。接続を開く側
  （`connectInvite`・`connectPeer`）は、次の順でストリームを開く。
  - 相手との直接の接続があれば、中継経由の接続を閉じてから開く（libp2p-stream は、相手との接続のどれかを無作為に
    選んでストリームを開くので、直接の接続だけを残す）。
  - 中継経由の接続しかなければ、直接の接続ができるのを 3 秒まで待ち、できなければ中継経由の接続の上で開く。
    `relayOnly` では待たない。相手が DCUtR を使うかは前もって分からない（DCUtR は中継経由の接続を受けた側が始め、
    受けた側は Identify で名乗らない）ので、`relayOnly` の相手とも同じ時間だけ待つ。3 秒は、対局・UI側が切れた対局の
    相手へつなぎ直す間隔（5 秒）より短くしてある。
  - 中継経由の接続の上で開いた後にできた直接の接続は、使わないので閉じる。次につなぎ直すとき（中継の制限で
    切れたときなど）に、もう一度直接の接続を試す。
- ping（10秒ごと、30秒で失敗）に応えない接続は閉じる。
- 公開募集のための発見（Kademlia）と配信（GossipSub）は持たない。協力ノードと招待・対局の相手のほかへ、
  自分から接続を始めない。

### 5.1 直接接続を使わない（`relayOnly`）

対局の相手に自分の IP を見せない設定。自分の IP が見えるのは、利用者が指定した協力ノードだけになる。

- 直接つなぐ宛先は、`bootstrap` で指定した協力ノードの宛先（と、そのノードが Identify で名乗った別の宛先）だけ。
  ほかの宛先への直接の接続は、Transport の層で断る（どの経路で始まった接続にも掛かる）。
- 中継経由の宛先は、中継が指定した協力ノードのときだけ使う（指定していない中継には自分の IP が見えるので使わない）。
  招待に使える宛先が残らなければ、`no_relay_route` で閉じる。
- 自分のアドレスとして出すのは中継経由のアドレスだけ（招待・Identify）。待受のアドレスは Identify で配らない。
- AutoNAT（到達性の確認）と DCUtR（直接接続への切り替え）を行わない。どちらも、自分のアドレスの候補を
  相手に渡すため。

上限（実装仕様4章）: 接続元は2段で数える。1つの接続元（IPv4 はアドレス、IPv6 は /64）と、同じアドレス帯（IPv4 は /24、
IPv6 は /48）。`local` では LAN 全体が同じ帯に入るので、どちらの段も接続元の IP。IPv4 射影の IPv6 は IPv4 として数える。
接続は合計32（確立前は受け・掛けそれぞれ8、1つの接続元からの確立前の受けは3、1ピアあたり4）、接続の確立（暗号の
手続きと多重化の取り決め）は20秒まで、同じアドレス帯から受け付ける接続は8、1接続の yamux のストリームは64まで
（協力ノードの上限は7章）。
対局ストリームは合わせて8（`setGamePeer` の間は、その相手からのものだけを受け付け、ほかは最初のフレームを読まずに
閉じる）。届いた対局ストリームの最初のフレームは5秒まで待ち、待つのは全体で16本・相手ごとに1本まで。
接続ごとの受信キュー 256 KiB（対局・UI側へ書き終えていない受信。超えたら `limit` で閉じる）。
対局ストリームのフレームは、START_SIGNATURE（type 3）を双方が送るまでは 5件/秒、その後は 20件/秒（どちらも
バーストは2倍）。超えたらストリームを閉じる（`rate_limited`）。

## 6. 権限の制限

### 6.1 Linux

通信プロセスが自分で掛ける。標準入力を読む前に、次の順で掛ける。どれか必須のものが失敗したら終了コード 3 で終わる。

1. 親の終了で SIGKILL を受ける（`PR_SET_PDEATHSIG`）。設定の後に親がすでに替わっていれば終わる。
2. コアダンプを残さない（`RLIMIT_CORE` を0、`PR_SET_DUMPABLE` を0）。
3. 新しい権限を得ない（`PR_SET_NO_NEW_PRIVS`）。
4. Landlock でファイルの操作をすべて拒否する（ABI 1 以上が必須）。ABI 6 以上なら、抽象 Unix ソケットへの接続と
   ほかのプロセスへのシグナルも拒否する（`landlock_scope`）。
5. seccomp-bpf: プログラムの起動（execve・execveat）、プロセスの複製（clone は CLONE_THREAD のある呼び出しだけ許し、
   clone3 は ENOSYS）、ほかのプロセスへの干渉（ptrace・process_vm_readv・process_vm_writev・kill・pidfd_open・
   pidfd_send_signal・pidfd_getfd・tkill と、第1引数が自分のプロセスでない tgkill・rt_tgsigqueueinfo・
   rt_sigqueueinfo）、ファイルの属性の変更（chmod・fchmod・fchmodat・fchmodat2・chown・fchown・lchown・fchownat・
   utime・utimes・utimensat・futimesat・setxattr などの xattr。Landlock は属性の変更を止めない）、
   socket・socketpair の AF_INET・AF_INET6・AF_NETLINK 以外（AF_INET・AF_INET6 は TCP・UDP だけ、AF_NETLINK は
   NETLINK_ROUTE だけ）、カーネルの構成を変える呼び出し（mount・umount2・pivot_root・chroot・unshare・setns・bpf・
   perf_event_open・keyctl・add_key・request_key・init_module・finit_module・delete_module・kexec_load・reboot・
   swapon・swapoff・acct・userfaultfd・personality・io_uring_setup、x86_64 では fork・vfork も）を EPERM で拒否する。
   x86_64 では、x32 の呼び出し（番号に 0x40000000 を立てたもの）でプロセスを終わらせる（seccomp の表は
   アーキテクチャの値と番号で照らすので、x32 の番号では上の表に当たらない）。

`landlockAbi` は、カーネルと Landlock の crate の両方が知っている ABI のうち最も新しい版（crate が知らない新しい版の
カーネルでも、crate の知っている版で報告する）。

Linux の `--sandbox-self-test` は、上の制限を掛けた後に次を試し、`{"applied":..., "results":{"<名前>":"denied"|"allowed"}}` を書く。
`read_file`（/etc/hostname を開く）、`write_file`（/tmp に作る）、`exec`（/bin/true を起動）、`fork`（スレッドでない clone）、
`unix_socket`（AF_UNIX の socket）、`unix_connect_abstract`、`signal_parent`（親へ signal 0）、`connect_loopback_tcp`（127.0.0.1 の
閉じたポートへ接続。OS の側では止めないので `allowed` が出る。実装仕様 10.4節の「OS の側で止められなかった操作」）。

### 6.2 Windows

Windows では、プロセスを作る時点でしか掛けられない制限を使うので、**起動する側（対局・UI側の `PeerConfinement`）が
`CreateProcess` の属性で掛け**、通信プロセスは標準入力を読む前に、掛かっていることを確かめる（確かめるための Win32 API の
呼び出しは `peer/win32` のクレートだけに置く。unsafe を書くのはそこだけ）。必須のものが1つでも掛かっていなければ、
終了コード 3 で終わる（実行ファイルを直接起動しても動かない）。

起動する側が掛けるもの:

1. AppContainer（`PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES`）。名前は `io.github.hnakada123.ShogiBoardQ.peer`
   （この PC のどのプレイヤーも同じ。別々の AppContainer どうしは同じ PC の中でつながらない）。
   プロファイルを登録していない AppContainer ではプロセスを作れないので、起動ごとに `CreateAppContainerProfile` で
   登録し、通信プロセスが終わったら消す（利用者のレジストリと、`%LOCALAPPDATA%` の `Packages` の下の空のフォルダ。通信プロセスは何も書かない）。
   能力は `internetClient`・`internetClientServer` と、起動のたびの能力（`S-1-15-3-1024-` に乱数6つ。下の DACL に使う）で、
   ローカルのネットワーク（`sbq-local-1`）を使うときだけ `privateNetworkClientServer` を足す。AppContainer の中からは、
   利用者のファイルの読み書き、ほかのプロセスを開くこと、名前付きパイプ（自動化 API など）への接続、loopback への接続ができない。
   プロセスと最初のスレッドの DACL と、トークンの既定の DACL（後から作るスレッドなどに付く）は、利用者・SYSTEM・起動のたびの
   能力だけを許す（AppContainer の SID を入れない）。同じ AppContainer のほかの通信プロセス（同じ PC のほかのプレイヤー）が、
   このプロセスとスレッドを開けないようにするため（監査 2026-11-21 S13）。トークンの既定の DACL を決めるまで動かさない
   よう、止めたまま作り（`CREATE_SUSPENDED`）、決めてから動かす。
2. ジョブ（`PROC_THREAD_ATTRIBUTE_JOB_LIST`）: ハンドルが閉じたら終わる（`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`。
   起動した側の終了で終わる）、プロセスは1つだけ（`ActiveProcessLimit` 1）、処理されない例外でエラーの報告の画面を
   出さずに終わる（`JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION`）、UI の制限（クリップボード・デスクトップ・
   グローバルなアトム・ほかのプロセスの USER ハンドルなど）すべて。
3. 子プロセスの禁止（`PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY` の `PROCESS_CREATION_CHILD_PROCESS_RESTRICTED`）。
4. 緩和ポリシー（`PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY`）: win32k のシステムコールの禁止、拡張ポイントの禁止、
   実行時のコード生成の禁止、フォントの読み込みの禁止、ネットワーク上と低い整合性の場所からの DLL の読み込みの禁止、
   ヒープの破損での終了、ASLR、ハンドルの厳密な検査。
5. 引き継ぐハンドルは標準入出力と標準エラーのパイプだけ。コンソールは持たせない（`DETACHED_PROCESS`。コンソールを
   作ると OS が conhost を起動しようとして、子プロセスの禁止に当たる）。作業フォルダはシステムのフォルダ。
6. 通信プロセスの実行ファイルに、この AppContainer の読み取りと実行の許可が無ければ足す（利用者のフォルダに置いた
   アプリの実行ファイルは、AppContainer から読めない。Program Files の下ではすでに読めるので何もしない）。
   通信プロセスは C ランタイムを静的にリンクし（`.cargo/config.toml`）、OS の DLL だけで起動する。

通信プロセスが確かめるもの（`mechanisms` の名前）: トークンが AppContainer（`appcontainer`）、ジョブに入っていて
ハンドルが閉じたら終わる（`job_kill_on_close`）、プロセスの上限が1（`job_single_process`）、子プロセスの禁止
（`no_child_process`）、エラーの報告の画面を出さない（`no_error_dialog`）。どれかが無ければ終了コード 3 で終わる。

続けて、起動の条件で決まるものを読み、掛かっているものだけを名前で知らせる（通信プロセスはこれでは終わらない。
起動した側が、自分の起動の条件と照らす。起動の処理が退行しても気づけるように。監査 2026-11-21 I-13）:

| 名前 | 意味（読み方） |
|---|---|
| `no_private_network` | トークンに `privateNetworkClientServer` の能力が無い（`CheckTokenCapability`。確かめられなければ入れない）。ローカルのネットワークを使わない起動で入る |
| `no_dynamic_code` | 実行時のコード生成の禁止（`GetProcessMitigationPolicy` の `ProcessDynamicCodePolicy` の ProhibitDynamicCode） |
| `no_win32k` | win32k のシステムコールの禁止（`ProcessSystemCallDisablePolicy` の DisallowWin32kSystemCalls） |
| `no_untrusted_images` | ネットワーク上と低い整合性の場所からの DLL の読み込みの禁止（`ProcessImageLoadPolicy` の NoRemoteImages と NoLowMandatoryLabelImages の両方） |
| `prefer_system32_images` | OS の DLL を、アプリのフォルダより先に System32 から読む（`ProcessImageLoadPolicy` の PreferSystem32Images） |
| `no_extension_points` | 拡張ポイントの禁止（`ProcessExtensionPointDisablePolicy` の DisableExtensionPoints） |
| `no_non_system_fonts` | システムのフォント以外の読み込みの禁止（`ProcessFontDisablePolicy` の DisableNonSystemFonts） |

ヒープの破損での終了・ASLR・ハンドルの厳密な検査は知らせない。

制限を確かめられなかったときに標準エラーへ書く文には、OS のエラーの文（利用者の言語に訳される）を使わず、
`<関数> failed (os error <番号>)` の形で番号だけを書く（対局・UI側は、診断の行を印字できる ASCII に絞る）。

Windows の `--sandbox-self-test` は、起動する側が渡した相手（`file=<パス>`・`pipe=<名前付きパイプの名前>`・
`pid=<プロセス>`・`tid=<スレッド,…>`・`port=<loopback で待ち受けているポート>`）に次を試す: `read_file`・`write_file`
（`<パス>.created` を作る）、`exec`（cmd.exe を起動）、`connect_named_pipe`、`open_process`、`open_thread`（コンマ区切りの
スレッドのどれかを、文脈の書き換えと停止・再開の権利で開けたら allowed）、`connect_loopback_tcp`。Windows ではどれも
`denied` になる（loopback への接続も OS が止める。`tests/tst_p2p_sandbox.cpp` が、アプリと同じ起動のしかたで、起動した側と、
動いているほかの通信プロセスのプロセスとすべてのスレッドを相手に確かめる）。
同じ AppContainer の通信プロセスどうしは、同じ PC でもつながる（`tst_p2p_endtoend` の同じ PC の対局）。待受を始めても、
Windows のファイアウォールは利用者に許可を求めない。

止められないもの: システムのフォルダのファイル（Windows・Program Files の下など、すべてのアプリケーション パッケージに
読み取りを許しているもの）は読める。私設アドレスへの接続を OS が止めるのは、`privateNetworkClientServer` を渡していない
とき（ローカルのネットワーク以外）で、OS がそのネットワークをプライベートと扱っている場合。利用者や管理者が
Windows エラー報告の保存（LocalDumps）を設定していれば、異常終了のダンプは残りうる（通信プロセスは鍵を持たない）。

協力ノード（`--service`）は Windows では動かない（識別鍵のファイルを自分で読むので、起動の時点でファイルを読めなく
する AppContainer と合わない）。

### 6.3 macOS

Linux と同じく、通信プロセスが自分で掛ける。標準入力を読む前に、次の順で掛ける。どれか必須のものが失敗したら
終了コード 3 で終わる。サンドボックスを掛ける libSystem の関数の呼び出しは `peer/macos` のクレートだけに置く
（unsafe を書くのはそこだけ。規則の本文は `peer/src/sandbox.rs` が決める）。

1. 親の終了を見張る（kqueue の `EVFILT_PROC`・`NOTE_EXIT`）。登録の後に親がすでに替わっていれば終わる。
   サンドボックスを掛けた後に見張りのスレッドを作り、親が終わったら自分に SIGKILL を送る（`parent_death_watch`）。
2. コアダンプを残さない（`RLIMIT_CORE` を0。`no_core_dump`）。
3. デバッガなどの接続を断る（`ptrace` の `PT_DENY_ATTACH`。`deny_attach`）。
4. サンドボックス（Seatbelt。`sandbox_init` にプロファイルの本文を渡す。プロセス全体に効き、外せない。`seatbelt`）。
   既定ですべて拒否し（`(deny default)`）、次だけを許す。
   - TCP と UDP の待受・着信・発信（IPv4・IPv6）
   - 自分へのシグナル
   - 次の名前の sysctl の読み取りだけ（許可の一覧。名前の一部で書くものは、その名前で始まるもの）:
     `net.routetable`（インターフェースのアドレスの一覧。getifaddrs が読む）、`hw.`（CPU の数など。Rust の標準
     ライブラリと tokio が読む）、`kern.os`（`kern.ostype`・`kern.osrelease`・`kern.osversion`・`kern.osproductversion`・
     `kern.osvariant_status` など OS の版）、`kern.version`、`kern.maxfilesperproc`、`kern.maxfiles`、`kern.usrstack64`、
     `sysctl.proc_cputype`
   - アドレスの変化の知らせを受ける configd（Mach のサービス `com.apple.SystemConfiguration.configd`）

   許していないので、ファイルの読み書き（開いてある標準入出力は使える）、プログラムの起動、プロセスの複製、
   ほかのプロセスへのシグナル、Unix ドメインのソケットへの接続（自動化 API など）、ほかの Mach のサービス
   （キーチェーン・クリップボードなど）、一覧にない sysctl（ほかのプロセスの引数と環境変数を読む `kern.procargs2`・
   `kern.procargs`、プロセスの一覧 `kern.proc`、コンピュータ名 `kern.hostname` など）は拒否される。インターフェースの
   物理アドレス（MAC アドレス）の読み取りも拒否する（一覧を読むたびに OS が確かめるので、この拒否だけはログに残さない）。
5. `--no-loopback` を付けて起動されたときは、規則の `localhost` を宛先とする発信も拒否する（`seatbelt_no_loopback`）。
   Seatbelt の宛先の規則は「すべて」か `localhost` でしか書けず、OS が止めることを自己試験で確かめているのは
   127.0.0.1 と ::1 への発信だけである。この Mac 自身を指すほかの宛先（127.0.0.0/8 のほかのアドレス・0.0.0.0・::・
   IPv4 射影の ::ffff:127.0.0.1・この Mac の LAN や VPN のアドレス）を OS が止めるかは確かめていない（自己試験が
   結果を書く。下）。止まらなくても、`internet` の profile では、通信プロセスの宛先の規則（5章。loopback・未指定・
   IPv4 射影へはつながず、私設アドレスへは利用者が許可した招待の宛先にだけつなぐ。Transport の層で、どの経路で
   始まった接続にも掛かる）が止める。
   対局・UI側の `PeerConfinement` は、ローカルのネットワーク（`sbq-local-1`）を使わないときに付ける。待受と着信には
   掛けない（loopback での待受の拒否は、すべてのアドレスでの待受まで止めてしまう）。

macOS の `--sandbox-self-test` は、上の制限を掛けた後に次を試す。試す相手は、渡されれば `file=<パス>`・
`pipe=<Unix ドメインのソケットのパス>`・`connect=<アドレス:ポート>` を使う。`read_file`（渡されなければ /etc/hosts）、`write_file`（`<パス>.created`。
渡されなければ /tmp に作る）、`exec`（/usr/bin/true を起動）、`fork`、`connect_unix_socket`（渡されたときだけ）、
`signal_parent`（親へ signal 0）、`listen_tcp`（すべてのアドレスでの待受。`allowed` が出る）、`connect_tcp`（渡された宛先へ接続。渡されたときだけ。
ほかの PC の宛先なら `allowed`）、`connect_loopback_tcp`
（127.0.0.1 と ::1 の閉じたポートへ接続。`--no-loopback` を付けたときは `denied`、付けないときは `allowed`）、
`list_interfaces`（getifaddrs。`allowed` が出る）、`sysctl_hw_ncpu`（`hw.ncpu`。`allowed` が出る）、`sysctl_procargs_parent`
（起動した側の `kern.procargs2`。`denied` が出る）、`sysctl_proc_all`（`kern.proc.all`。`denied` が出る）、`sysctl_hostname`
（`kern.hostname`。`denied` が出る）。sysctl は、サンドボックスが拒否した（EPERM・EACCES）ときだけ `denied`。
続けて、`--no-loopback` の規則がどこまで止めるかを書く（試験はこれらを確かめない。実機で動かしたときに、OS が止めるかが
分かる）: `connect_unspecified_v4`（0.0.0.0:9）、`connect_unspecified_v6`（[::]:9）、`connect_mapped_loopback`
（[::ffff:127.0.0.1]:9）、`connect_loopback_other`（127.0.0.2:9）、`connect_own_address`（動いているインターフェースの
loopback でない IPv4 アドレスの 9 番。そういうアドレスがあるときだけ）。どれも、OS が拒否した（PermissionDenied）ときだけ
`denied`。

止められないもの: 私設アドレスへの接続（サンドボックスの規則は、宛先を「すべて」か `localhost` でしか絞れない）と、
`--no-loopback` を付けないとき（ローカルのネットワーク）の loopback への接続。`--no-loopback` を付けたときも、
127.0.0.1・::1 でない、この Mac 自身を指す宛先（5. の上）は、OS が止めると確かめていない。どれも通信プロセスの
宛先の規則（5章）が守る。
macOS 15 以降では、同じ LAN の相手と初めて通信するときに、OS が利用者に「ローカルネットワーク上のデバイスの検索」の
許可を求めることがある（OS の設定で、通信プロセスの規則とは別）。

協力ノード（`--service`）は、識別鍵のファイルを読んだ後に同じ制限を掛ける（親の終了は見張らない）。

### 6.4 そのほかの OS

制限の実装がないので、終了コード 3 で終わる（P2P対局を使えない）。

## 7. 協力ノード（`--service`）

Linux と macOS で動く。

```text
shogiboardq-peer --service --network <networkName> --identity <鍵ファイル> --listen <multiaddr> [--listen ...]
                 [--bootstrap <multiaddr> ...] [--relay] [--meet]
```

- 識別鍵のファイル（Ed25519 の秘密鍵の protobuf 形式）がなければ作り、所有者だけが読み書きできる権限で保存する。
  読んだ後に、ファイルの操作を拒否する（Linux は Landlock、macOS はサンドボックス）。
- 利用者のアプリが最初に接続する相手になり、到達性の確認（AutoNAT v2）に応える。`--relay` で Circuit Relay v2 の中継を
  有効にする。対局ストリームは受け付けない（指し手もプレイヤーID も読まない。中継する通信は当事者間で暗号化されている）。
- 接続の上限（利用者のアプリより大きくする。在席を知らせる利用者・中継・待ち合わせの利用者を数百受けるため。
  監査 2026-11-21 S2）: 確立した接続は合計 512、確立前は受け 64・掛け 32（掛けるのは、ほかの協力ノードと到達性の確認の
  折り返し）、1ピアあたり 4。接続元は2段で数え（5章）、1つの接続元（IPv4 のアドレス・IPv6 の /64）からの確立前の受けは
  4・受け付ける接続は 16、同じアドレス帯（IPv4 の /24・IPv6 の /48）からの確立前の受けは 16・受け付ける接続は 64
  （CGNAT・IPoE で同じ帯を分け合う利用者を締め出さず、数個のアドレス帯では埋められない）。
- 中継の上限: 予約 128、1ピアあたりの回路 4、1回路 30 分・8 MiB。
- `--meet` で、合言葉の待ち合わせ（`README.md` 2.2節）を受け持つ。利用者が預けた、中身の分からない小さなデータ
  （1 KiB まで）を、10 分までの間、メモリにだけ持つ。上限は、全体で 16384 件、1つの接続元から 512 件、同じアドレス帯から
  2048 件（頻度と同時に処理する要求の上限は `README.md` 2.2節）。
- ログは標準エラーに、起動・停止と、接続の数だけを書く（`shogiboardq-peer: peers=<つながっているピアの数>`。
  `--relay` のときは、続けて ` relays=<中継している回路の数>`、`--meet` のときは、続けて
  ` meets=<待ち合わせで預かっている件数>`）。数は、起動した直後と、変わったときに書く（1秒に1行まで。決まった
  間隔では書かない）。接続元の IP とピアの ID、預かったものの番号は書かない。`--listen` のうち開けなかったアドレス
  （使用中のポートなど）は、起動のときに `shogiboardq-peer: could not listen on <multiaddr>` と1件ずつ書き、開けた
  アドレスで動く（どれも開けなければ終わる）。
- 権限の制限は6章と同じだが、親の終了では終わらない（`mechanisms` に `parent_death_signal`・`parent_death_watch` が
  入らない）。
