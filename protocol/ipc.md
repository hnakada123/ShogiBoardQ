# 対局・UI側と通信プロセスの IPC

実装仕様 10.5節の詳細。対局・UI側（C++ の `PeerProcess`）が通信プロセス `shogiboardq-peer` を QProcess で
起動し、標準入出力でやり取りする。

## 1. 起動

| 起動のしかた | 用途 |
|---|---|
| `shogiboardq-peer`（引数なし） | 対局・UI側から使う。標準入出力で IPC を行う |
| `shogiboardq-peer --service ...` | 協力ノード（7章）。IPC を使わない |
| `shogiboardq-peer --sandbox-self-test` | 権限の制限を掛けてから、禁止した操作を試し、結果を JSON 1行で標準出力に書いて終わる（試験用） |
| `shogiboardq-peer --version` | 版を1行で書いて終わる |

- 対局・UI側は、実行ファイルをアプリと同じフォルダ（`QCoreApplication::applicationDirPath()`）から絶対パスで起動する。
  環境変数は渡さない（空の環境）。標準入出力と標準エラー以外のファイル記述子を引き継がない
  （Unix は `QProcess::UnixProcessFlag::CloseFileDescriptors`）。
- 通信プロセスは、標準入力を読む前に権限の制限（6章）を掛ける。掛けられなければ、標準エラーに理由を書いて
  終了コード 3 で終わる。
- 標準入力が終端に達したら、直ちに終了する（親の終了）。Linux では親の終了で SIGKILL を受ける設定もする。

| 終了コード | 意味 |
|---|---|
| 0 | `shutdown` または標準入力の終端による終了 |
| 1 | そのほかの失敗 |
| 2 | IPC の違反（上限超過・形式不正・知らないメソッド）。対局・UI側が送ったものが正しくない |
| 3 | 権限の制限を掛けられなかった |

標準エラーは診断の文だけに使う（秘密・招待文字列・IP アドレスを書かない）。対局・UI側は先頭から 64 KiB までを
読み、保存しない。

## 2. メッセージの形

どちらの向きも、4バイトのネットワークバイト順の長さに、UTF-8 の JSON オブジェクト1つを続ける。
長さは 2〜131072（128 KiB）。超えたら、受けた側は相手を IPC の違反として扱う（通信プロセスは終了コード 2 で終わり、
対局・UI側は通信プロセスを終了させる）。

```json
{"protocolVersion":2,"processGeneration":"3","requestId":"17","method":"sendFrame","params":{...}}
{"protocolVersion":2,"processGeneration":"3","requestId":"17","result":{...}}
{"protocolVersion":2,"processGeneration":"3","requestId":"17","error":{"code":"unknown_connection","message":"..."}}
{"protocolVersion":2,"processGeneration":"3","notification":"frameReceived","params":{...}}
```

- `protocolVersion` は 2（`hello` の応答の `ipcVersion` と同じ）。違えば IPC の違反。
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
| `invite_invalid`・`invite_expired`・`invite_wrong_network`・`invite_own` | 招待文字列が読めない・期限切れ・別のネットワーク・自分の招待 |
| `unknown_connection` | 知らない接続 |
| `invalid_frame` | 送るフレームの外枠が正しくない（`README.md` 2章） |
| `rate_limited`・`limit` | 頻度・件数の上限 |
| `internal` | そのほかの失敗 |

## 3. 要求（対局・UI側 → 通信プロセス）

| method | params | result |
|---|---|---|
| `hello` | `{"uiVersion": 文字列}` | `{"peerVersion": 文字列, "ipcVersion": 2, "sandbox": Sandbox}` |
| `start` | `{"networkName", "bootstrap": [文字列], "relayOnly": 真偽}` | `{"peerId", "peerIdBytes", "networkId", "profile"}` |
| `createInvite` | `{}` | `{"invite", "expiresAt", "addressCount"}` |
| `cancelInvites` | `{}` | `{}` |
| `inspectInvite` | `{"invite"}` | `{"peerId", "expiresAt", "addressCount", "usableAddressCount", "privateAddresses": [文字列]}` |
| `connectInvite` | `{"invite", "allowPrivate": 真偽}` | `{"connection"}` |
| `setGamePeer` | `{"peerId"}`（空文字で解除） | `{}` |
| `connectPeer` | `{"peerId"}` | `{"connection"}` |
| `sendFrame` | `{"connection", "frame"}` | `{}` |
| `closeConnection` | `{"connection"}` | `{}` |
| `shutdown` | `{}` | `{}` |

- `hello` は最初に1回だけ。`Sandbox` は `{"applied": 真偽, "os": "linux", "mechanisms": [文字列], "landlockAbi": 整数}`。
  `mechanisms` には掛けたものを入れる（`no_new_privs`・`landlock_fs`・`seccomp`・`parent_death_signal`・`no_core_dump`、
  掛けられた場合だけ `landlock_scope`）。`applied` は必須のもの（`landlock_scope` 以外のすべて）を掛けたとき true。
  通信プロセスは掛けられなければ終了しているので、`applied: false` を返すことはない。対局・UI側は値を確かめ直す。
- `start` は `hello` の後に1回だけ。`networkName` は `README.md` 1.1節の一覧にあるもの。
  `bootstrap` は利用者が指定した協力ノード（`/p2p/<PeerId>` で終わる multiaddr。16件まで）。今の profile で使えない
  宛先（5章）は捨てる。通信プロセスが自分から接続を始める相手は、この協力ノードと、招待・対局の相手だけである。
  `relayOnly` は「直接接続を使わない」（5.1節）。結果の `peerId` は文字列表記（base58btc）、
  `peerIdBytes` はバイナリの base64、`networkId` は16進、`profile` は `local` か `internet`。
- `createInvite` は 10 分有効の招待を作る（同時に4件まで。超えたら古いものから無効にする）。`expiresAt` は UNIX秒の
  文字列、`addressCount` は文字列に入れた到達候補の数。招待文字列は対局・UI側でも記録しない。
- `inspectInvite` は招待文字列を読んだ結果を返し、接続はしない。`privateAddresses` は今の profile が `internet` のときの、
  私設アドレスの到達候補（利用者に見せて許可を求める）。
- `connectInvite`・`connectPeer` は接続を始めて、すぐに番号 `connection`（10進の文字列）を返す。
  ストリームが開けたら `connectionOpened`、開けなければ `connectionClosed`（`reason` は `unreachable`・`timeout`、
  `relayOnly` で招待に使える中継経由の宛先が無いときは `no_relay_route`。全体で 30 秒）が届く。
  `connectPeer` は `setGamePeer` の相手にだけ使える（再接続。つなぐのに使った宛先を覚えておいて使う）。
- 受け付けた側（inbound）の `connectionOpened` は、相手の最初のフレームが届いてから出す（最初のフレームを 5 秒待つ）。
  `origin` が `game` なら、続けてそのフレームを `frameReceived` で渡す（`invite` の最初のフレームは
  INVITE_TOKEN なので渡さない）。したがって接続を開いた側（outbound）は、`connectionOpened` を受けたら先にフレームを
  送る（申込みの HELLO、再接続の CONTROL など）。
- 対局ストリームを受け付けるのは、有効な招待の INVITE_TOKEN を最初に送ってきた相手と、`setGamePeer` の相手だけ。
  ほかのストリームは閉じる。
- `setGamePeer` を設定している間は、その PeerId からの対局ストリームだけを受け付け（招待のトークンは要らない。
  `origin` は `game`）、ほかのピアからの対局ストリームは閉じる。
- `sendFrame` の `frame` はフレーム全体（長さの前置きを含めない）の base64。通信プロセスは外枠を確かめてから送る。
  接続ごとの送信の列は 64 件までで、あふれたら `rate_limited` を返す。
  INVITE_TOKEN（type 11）は送れない。
- `closeConnection` で閉じた接続には `connectionClosed` を出さない（相手には閉じたことが届く）。
- `shutdown` は応答を返した後、招待を無効にして終わる。

## 4. 通知（通信プロセス → 対局・UI側）

| notification | params |
|---|---|
| `networkStatus` | `{"state": "connecting"\|"online"\|"offline", "connectedPeers": 整数, "reachability": "unknown"\|"public"\|"private", "relayReservations": 整数}` |
| `connectionOpened` | `{"connection", "peerId", "peerIdBytes", "route": "direct"\|"relay", "direction": "inbound"\|"outbound", "origin": "invite"\|"game"}` |
| `connectionRoute` | `{"connection", "route"}`（中継から直接接続に移ったときなど） |
| `frameReceived` | `{"connection", "frame"}` |
| `connectionClosed` | `{"connection", "reason"}` |

- 自分や相手のアドレス（IP）は、通知に入れない（対局・UI側は IP を受け取らない。例外は `inspectInvite` の
  `privateAddresses` だけ）。
- `state` は、接続しているピアがある（または協力ノードを指定せずに待受を始めた）とき `online`、協力ノードへ接続中のとき
  `connecting`、それ以外は `offline`。値が変わったときにだけ、1秒に1回まで出す。
- `reachability` は到達性の確認（AutoNAT v2）の結果（`relayOnly` では確かめないので `unknown` のまま）。`route` は、その相手との接続が
  すべて中継経由なら `relay`、1つでも直接の接続があれば `direct`。
- `frameReceived` は外枠を確かめたフレームだけを運ぶ。INVITE_TOKEN は運ばない。
- `connectionClosed` の `reason` は `closed`・`reset`・`timeout`・`unreachable`・`no_relay_route`・`invalid_frame`・`rate_limited`・`limit`。
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
- `local` は、同じ PC と同じ LAN の相手と、協力ノードなしでつなぐ。待受のアドレスのうち loopback と私設アドレスを、
  自分のアドレスとして招待に載せる（Identify には載せない。Identify は招待を持たない相手にも届くので、待受のポートに
  つないだだけの LAN の機器に、この PC の私設アドレスの一覧を配らない。つなぎ直すのは招待で接続した側で、宛先は
  招待のものを使う）。招待には、同じ LAN の相手が使う私設アドレスを先に、同じ PC でしか使えない loopback を最後に
  入れる。届いた接続は、接続元が loopback・私設アドレスのものだけを受け付け、ほかは
  暗号の手続きを始める前に閉じる（Transport の層。中継経由の接続は接続元の IP を持たないので、ここでは断らない）。
  私設アドレスへの接続に、招待ごとの許可（`allowPrivate`）は求めない（このネットワークを選んだ利用者は、LAN 内の
  宛先へつなぐことを選んでいる）。
- `local` の待受のポートは TCP・UDP とも 48210 に決める。起動のたびに変わると、招待する側の PC のファイアウォールで
  許可するポートを決められないためである。ほかのプロセス（同じ PC の2つ目のアプリなど）が使っていれば、空いている
  ポートで待ち受ける。TCP は、使用中かを先に確かめる（libp2p の TCP の待受は同じポートを複数のプロセスで共有できる
  設定で開くので、確かめないと、届いた接続が2つのプロセスに振り分けられる）。確かめてから開くまでの間に、同時に
  起動したプロセスも同じことをすると両方が開けてしまうので、UDP（QUIC）を先に開け、48210 の UDP を開けたプロセス
  だけが TCP でも 48210 を試す（UDP の待受は共有できないので、48210 の持ち主が1つに決まる）。
  `internet` では、到達性の確認（AutoNAT v2）で確かめたインターネット上のアドレスと中継のアドレスだけを載せる。
- 到達性の確認は AutoNAT v2（`/libp2p/autonat/2/dial-request`）で行う。確かめてもらうのは、相手（協力ノード）が
  観測した自分のアドレスの候補のうちインターネット上の直接のアドレスだけで、1件ずつ頼む（v1 は待受のアドレス
  （私設・loopback・link-local を含む）を全部送るので使わない）。確認に応えるのは協力ノード（`--service`）だけで、
  ふつうのプレイヤーは応えない（ほかのピアの頼みで、そのピアへ接続を始めない）。協力ノードへは新しいポートで接続し、
  観測されたアドレスを待受のポートに直して候補にする。
- 中継の hop に対応する協力ノードには、自分の到達性によらず予約を取る（2件まで）。中継経由のアドレスは、利用者が
  指定した協力ノードの宛先から作り、招待に入れる（`relayOnly` の相手は、中継経由のアドレスでしかつなげない）。
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

上限（実装仕様4章）: 接続は合計32（確立前は受け・掛けそれぞれ8、同じ接続元の IP からの確立前の受けは3、1ピアあたり4）、
接続の確立（暗号の手続きと多重化の取り決め）は20秒まで、同じアドレス帯（IPv4 は /24、IPv6 は /48。`local` では
LAN 全体が同じ帯に入るので接続元の IP ごと）から受け付ける接続は8、1接続の yamux のストリームは64まで。
対局ストリームは合わせて8（`setGamePeer` の間は、その相手からのものだけを受け付け、ほかは最初のフレームを読まずに
閉じる）。届いた対局ストリームの最初のフレームは5秒まで待ち、待つのは全体で16本・相手ごとに1本まで。
接続ごとの受信キュー 256 KiB（対局・UI側へ書き終えていない受信。超えたら `limit` で閉じる）。
対局ストリームのフレームは、START_SIGNATURE（type 3）を双方が送るまでは 5件/秒、その後は 20件/秒（どちらも
バーストは2倍）。超えたらストリームを閉じる（`rate_limited`）。

## 6. 権限の制限（Linux）

標準入力を読む前に、次の順で掛ける。どれか必須のものが失敗したら終了コード 3 で終わる。

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

Linux 以外では、制限の実装がないので終了コード 3 で終わる（P2P対局を使えない）。

`landlockAbi` は、カーネルと Landlock の crate の両方が知っている ABI のうち最も新しい版（crate が知らない新しい版の
カーネルでも、crate の知っている版で報告する）。

`--sandbox-self-test` は、上の制限を掛けた後に次を試し、`{"applied":..., "results":{"<名前>":"denied"|"allowed"}}` を書く。
`read_file`（/etc/hostname を開く）、`write_file`（/tmp に作る）、`exec`（/bin/true を起動）、`fork`（スレッドでない clone）、
`unix_socket`（AF_UNIX の socket）、`unix_connect_abstract`、`signal_parent`（親へ signal 0）、`connect_loopback_tcp`（127.0.0.1 の
閉じたポートへ接続。OS の側では止めないので `allowed` が出る。実装仕様 10.4節の「OS の側で止められなかった操作」）。

## 7. 協力ノード（`--service`）

```text
shogiboardq-peer --service --network <networkName> --identity <鍵ファイル> --listen <multiaddr> [--listen ...]
                 [--bootstrap <multiaddr> ...] [--relay]
```

- 識別鍵のファイル（Ed25519 の秘密鍵の protobuf 形式）がなければ作り、所有者だけが読み書きできる権限で保存する。
  読んだ後に Landlock でファイルの操作を拒否する。
- 利用者のアプリが最初に接続する相手になり、到達性の確認（AutoNAT v2）に応える。`--relay` で Circuit Relay v2 の中継を
  有効にする。対局ストリームは受け付けない（指し手もプレイヤーID も読まない。中継する通信は当事者間で暗号化されている）。
- 中継の上限: 予約 128、1ピアあたりの回路 4、1回路 30 分・8 MiB。
- ログは標準エラーに、起動・停止・接続数だけを書く（接続元の IP は書かない）。
- 権限の制限は6章と同じだが、親の終了では終わらない（`mechanisms` に `parent_death_signal` が入らない）。
