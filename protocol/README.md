# P2P対局の共通の取り決め（protocol/）

通信プロセス `shogiboardq-peer`（Rust、`peer/`）と対局・UI側（C++、`src/game/p2p/`・`src/network/p2p/`）が
同じ結果を出さなければならない取り決めを、ここに1つだけ置く。仕様の本文は
[実装仕様](../docs/dev/p2p-game-spec.md)、ここはその実装の詳細（バイト列と JSON の形）である。

別のソフトウェアが互換の実装を作るための文書は
[互換実装のための仕様書](../docs/dev/p2p-interop-spec.html)にある。ここの取り決めと、対局・UI側の手順（交渉・対局・
再接続・証跡の検証）を、その実装者向けに1つにまとめたもので、ここの取り決めを変えたら、そちらも同時に直す。

| ファイル | 内容 | 使う側 |
|---|---|---|
| `README.md`（本書） | 決定的CBOR・ハッシュ・フレームの外枠・招待文字列・合言葉 | 両方 |
| `ipc.md` | 対局・UI側と通信プロセスの間の IPC（4バイト長＋JSON） | 両方 |
| `game-frames.md` | 対局フレームの payload（HELLO など）。通信プロセスは読まない | C++ |
| `displayname-v1.txt` | 表示名に使える文字の表 v1 | C++ |
| `testvectors/` | 両方の実装で同じ結果になることを確かめる入力の一覧 | 両方の試験 |
| `rust/` | Rust の crate `sbq-protocol`（上の取り決めの Rust 実装。`peer/` が使う） | Rust |

## 1. 決定的CBOR

署名・ハッシュ・フレームに使う CBOR は、RFC 8949 の core deterministic encoding のうち、次の部分だけを使う。
読む側は、ここにないものをすべて拒否する。

- 使える型は 4つだけ: 符号なし整数（major 0）、byte string（major 2）、text string（major 3）、配列（major 4）。
  負の整数・map・タグ・浮動小数・単純値（true/false/null/undefined）は使わない。真偽は整数の 0/1 で表す。
- 長さと整数は最短の形（0〜23 は先頭バイトに、24〜255 は 1 バイト、256〜65535 は 2 バイト、
  65536〜2^32−1 は 4 バイト、それ以上は 8 バイト）。最短でない形は拒否する。
- 不定長（additional information 31）と 28〜30 は拒否する。
- text string は正しい UTF-8 であること（過長な形・サロゲート・U+10FFFF を超える値を拒否）。
- 入れ子は 16 段まで（最上位の値を1段目と数える）。
- 入力は値1つだけで、後ろに余りのバイトがあれば拒否する。
- 長さを読んだら、残りの入力に収まるかを確かめてから領域を確保する。
- 読み終えた値を符号化し直したバイト列が、元のバイト列と一致すること（上の規則を守れば必ず一致する）。

ハッシュ関数 `H` は SHA-256 で、`H(CBOR(x))` は x の決定的CBORのバイト列の SHA-256 である。
16進表記は小文字。

### 1.1 ネットワーク

```text
networkId = H(CBOR(["SBQ-NETWORK", 1, networkName]))     ; networkName は text（ASCII、1〜32バイト）
```

このビルドが参加できる networkName は次の2つだけである（実装仕様 3.1節）。両方の実装がこの一覧を持ち、
ほかの名前を断る。

| networkName | profile | 使用期限 | 宛先 |
|---|---|---|---|
| `sbq-local-1` | `local`（ローカル。同じ PC・同じ LAN。画面の表示は「ローカル（同じPC・同じLAN）」） | なし | loopback と私設アドレスを使える。待受は IPv4 の全インターフェース（ポート 48210。`ipc.md` 5章）で、接続元が loopback・私設アドレスの接続だけを受け付ける |
| `sbq-limited-1` | `internet`（インターネット。画面の表示は「インターネット」。名前は実験版のときのまま変えない） | ビルドから180日 | インターネット上のアドレスと中継だけ（`ipc.md` 5章） |

両方の実装が同じ値を使う、ネットワークの定数（Rust は `sbq-protocol` の `network`。対局・UI側の C++ の値は、
CMake が `network.rs` の行から読む）:

| 定数 | 値 | 使うところ |
|---|---|---|
| `IDENTIFY_PROTOCOL` | `/shogiboardq/1.0.0` | libp2p の Identify で示すプロトコルの版。相手のこの値で接続を断らない |
| `LAN_PORT` | 48210 | ローカルのネットワークの待受のポート（TCP と UDP。使用中なら空いているポート。`ipc.md` 5章） |
| `BUILD_LIFETIME_DAYS` | 180 | ビルドの使用期限の日数（期限のあるネットワークだけ。実装仕様 3.1節） |

`rulesHash` は v1 では1つの値（平手・512手・人間対人間の `Rules`。`game-frames.md`）だけを使う。
対局・UI側だけが扱い、通信プロセスは使わない。

公開募集（GossipSub・Kademlia・一覧取得）は第5版でやめた（実装仕様の「第5版の変更点」）。対局の相手は
招待文字列（3章）で決める。

## 2. 対局フレームの外枠

libp2p のプロトコル `/shogiboardq/game/1.0.0` のストリームに、4バイトのネットワークバイト順の長さと
決定的CBORのフレームを続けて流す。長さは 1〜65536（64 KiB）。長さを読んでから領域を確保する。

```text
Frame = [version, type, requestId, gameId, payload]
```

| 項目 | 型 | 規則 |
|---|---|---|
| version | uint | 1（外枠の形の版。HELLO の対応する版の一覧の版 2（対局者の種別。`game-frames.md` 2章）でも 1 のまま） |
| type | uint | 1〜11（下の表） |
| requestId | uint | 0〜2^64−1 |
| gameId | bstr | 32バイト。対局ID が決まる前はゼロ32バイト |
| payload | array | 中身は種類ごと（`game-frames.md`）。通信プロセスは読まない |

| type | 名前 | type | 名前 |
|---|---|---|---|
| 1 | HELLO | 7 | SYNC_REQUEST |
| 2 | START_AGREEMENT | 8 | SYNC_CHUNK |
| 3 | START_SIGNATURE | 9 | RESULT_SIGNATURE |
| 4 | STATE_PROPOSE | 10 | REJECT |
| 5 | STATE_ACCEPT | 11 | INVITE_TOKEN |
| 6 | CONTROL | | |

通信プロセスは、受けたフレームと送るフレームのどちらにも、この外枠の規則（長さ・決定的CBOR・5要素の配列・
version・type の範囲・gameId の長さ・payload が配列）を確かめ、合わなければストリームを閉じる（送る側なら
要求をエラーにする）。payload の意味は読まない。例外は INVITE_TOKEN だけで、通信プロセスどうしで処理し、
対局・UI側へ渡さない。対局・UI側は外枠の検査からすべてやり直す。

### 2.1 INVITE_TOKEN

```text
[1, 11, 0, zero32, [token]]   ; token は bstr(32)
```

招待を受けた側の通信プロセスが、招待した側への対局ストリームを開いた直後、最初のフレームとして送る。
招待した側の通信プロセスは、まだ使われていない有効期限内の招待のトークンと、処理時間が一致した長さで
変わらない比較で照合する。一致すればその招待を使用済みにし、ストリームを対局・UI側へ知らせる
（`connectionOpened`、`origin` は `invite`）。一致しなければストリームを閉じ、対局・UI側には知らせない。
トークンをログ・IPC へ出さない。

1つの招待が受け付けるトークンは、招待文字列（3章）のトークンと、合言葉を付けた招待なら合言葉から作るトークン
（3.1節）の2通りで、どちらか一方で1回だけ使える。

## 3. 招待文字列

```text
invite = "sbqinv1." + base64url(body) + "." + hex(H(body)[0..4])
body   = CBOR(["SBQ-INVITE", 1, networkId, peerId, [addr, ...], token, expiresAt])
```

| 項目 | 型 | 規則 |
|---|---|---|
| networkId | bstr(32) | 1.1節 |
| peerId | bstr | 招待した側の PeerId のバイナリ表現（Ed25519 の identity multihash。38バイト） |
| addr | bstr | 到達候補の multiaddr のバイナリ表現（`/p2p/` を含めない）。0〜8件、1件 100 バイト以下 |
| token | bstr(32) | CSPRNG。通信プロセスが作り、照合する |
| expiresAt | uint | UNIX秒。作成から最長 600 秒 |

- base64url は RFC 4648 の URL 用の文字で、`=` を付けない。検査の値は body の SHA-256 の先頭4バイトの小文字16進。
- 文字列全体は 1024 バイト以下。前後の空白（ASCII の空白・タブ・改行）だけは取り除いてから読む。
- 読む側は、接頭辞・3つに分けた形・base64url・検査の値・決定的CBOR・各項目の型と長さ・networkId が
  今のネットワークと一致すること・有効期限内であること・peerId が自分でないことを確かめる。
- 到達候補のうち、今の profile で使えない宛先（`ipc.md` 5章）は捨てる。私設アドレスは、`local` 以外の
  profile では利用者が確認画面で許可したとき（`connectInvite` の `allowPrivate`）だけ使う。

### 3.1 合言葉（同じ LAN の招待の短い形）

`local` の profile で、直接つなぐ招待（`relayOnly` でない）には、招待文字列と一緒に合言葉を付ける。合言葉は、
招待した側の LAN の中のアドレスと短い秘密だけを 12 文字ほどにしたもので、口頭で伝えられる。招待した側の
PeerId・networkId・期限は入らない（期限は招待した側が確かめる）。招待文字列と同じ招待を指し、どちらか一方で
1回だけ使える。

```text
code = 4文字ごとに "-" で区切った base32c(data) + check
data = version(1) | hasPort(1) | addr{1..4} | [port(16)] | secret(34..38)      ; ビットの列。先頭のビットから詰める
addr = kind(2) | host(n) | more(1)
```

| 項目 | ビット | 規則 |
|---|---|---|
| version | 1 | 0 |
| hasPort | 1 | 0 なら、宛先は決まったポート（1.1節の 48210）の QUIC と TCP。1 なら、`port` の QUIC だけ |
| kind・host | 2 + n | 0: `192.168.x.y`（n = 16、host は x.y）、1: `172.16.0.0/12`（n = 20、host は下位 20 ビット）、2: `10.x.y.z`（n = 24）、3: `127.0.0.1`（n = 0） |
| more | 1 | 1 なら、次の addr が続く。addr は 1〜4 件で、同じアドレスを2回入れない |
| port | 16 | hasPort が 1 のときだけ。1〜65535 |
| secret | 34〜38 | CSPRNG。data の長さに検査の 5 ビットを足した全体が 5 の倍数になるように、34 に 0〜4 ビットを足した長さ |

- base32c は Crockford の Base32（`0123456789ABCDEFGHJKMNPQRSTVWXYZ`）で、data を先頭から 5 ビットずつ1文字にする。
- check は1文字で、それより前の文字（区切りを除いた大文字。text）から
  `H(CBOR(["SBQ-CODE-CHECK", 1, text]))` の先頭のバイトの上位 5 ビットを同じ文字の表で表す。
- 文字は 9〜33 文字（区切りを除く。192.168 のアドレス1件・決まったポートなら 12 文字）。
- 読む側は、空白（ASCII の空白・タブ・改行）と `-` を読み飛ばし、大文字と小文字を区別せず、`O` を `0`、`I` と `L` を
  `1` として読む。ほかの文字・文字数・check・version・件数・同じアドレスの重複・port が 0・secret の長さが
  合わないものは読まない。区切りと空白を含めて 96 バイトを超える文字列は読まない。
- 入れるアドレスは、待受のアドレスのうち私設アドレス（最大4件）で、私設アドレスが1つもないときだけ `127.0.0.1` を
  入れる。QUIC も TCP も決まったポートで待ち受けていれば hasPort を 0 にし、そうでなければ QUIC の待受のポートを
  書く。QUIC の待受がなければ、合言葉を付けない。
- 読む側は、`local` 以外の profile では合言葉を使わない（`invite_wrong_network`）。合言葉の宛先から自分の待受の
  アドレスを除き、残らなければ自分の招待として扱う（`invite_own`）。

合言葉でつなぐ側は、合言葉の宛先へ、相手の PeerId を決めずにつなぐ。最初につながった相手の PeerId と、合言葉から
作った鍵で、INVITE_TOKEN（2.1節）で送るトークンを作る。合言葉の秘密そのものは回線に流さない。

```text
salt  = H(CBOR(["SBQ-CODE-SALT", 1, networkId, port, [ip4, ...]]))[0..16]     ; port は hasPort が 0 なら 0。ip4 は bstr(4)
key   = Argon2id(password = secret の 8 バイトのビッグエンディアン, salt,
                 版 0x13, メモリ 65536 KiB, 回数 2, 並列 1, 出力 32 バイト)
token = H(CBOR(["SBQ-CODE-TOKEN", 1, key, 招待した側の PeerId, つなぐ側の PeerId]))
```

- Argon2id の条件は、libsodium の `crypto_pwhash`（`ALG_ARGON2ID13`、opslimit 2、memlimit 67108864）と同じ。
- PeerId はバイナリ表現（38 バイト）。招待した側は、合言葉を付けた招待ごとに key を持ち、届いた INVITE_TOKEN の
  トークンを、自分の PeerId と、ストリームを開いた相手の PeerId（接続の暗号の手続きで確かめたもの）から作った値と、
  処理時間が中身によらない比較で照合する。トークンは双方の PeerId に結び付くので、合言葉の宛先を乗っ取った機器が
  受け取ったトークンを、招待した側への接続に使い回せない。
- 秘密は短いので、招待した側は、合言葉のトークンの照合に続けて失敗できる回数を 8 回までにし、4 秒ごとに1回分
  戻す（10 分の招待に、合わせて 150 回ほど）。回数が残っていない間は、合言葉のトークンとして照合しない
  （招待文字列のトークンは、いつでも照合する）。
- 合言葉には招待した側の PeerId が入っていないので、つなぐ側は、つながった相手が招待した本人かどうかを接続の時点では
  確かめられない（合言葉の宛先を乗っ取った機器は、つなぐ側の相手として条件の確認まで進める。招待した側へは
  つなげない）。相手は、招待文字列のときと同じく、条件確認画面のプレイヤーID で確かめる。

## 4. 試験の入力（testvectors/）

| ファイル | 行の形 |
|---|---|
| `cbor.txt` | `valid <hex>` または `invalid <hex>`（決定的CBORとして読めるか） |
| `frames.txt` | `valid <hex>` または `invalid <hex>`（2章の外枠として正しいか） |
| `displayname.txt` | `accept <hex(UTF-8)>` または `reject <hex(UTF-8)>`（C++ だけが読む） |
| `network.txt` | `<networkName> <hex(networkId)> <hex(rulesHash)>`（rulesHash は双方が人間の Rules のもの。C++ だけが確かめる） |
| `rules.txt` | `<blackKind> <whiteKind> <hex(rulesHash)> <hex(Rules の決定的CBOR)>`（対局者の種別の4通り。C++ だけが読む） |
| `code.txt` | `code <合言葉> <ポート か -> <アドレス,...> <hex(秘密)>`・`invalid <文字列>`・`key <networkName> <合言葉> <hex(パスワード)> <hex(ソルト)> <hex(鍵)>`・`token <hex(鍵)> <hex(招待した側の PeerId)> <hex(つなぐ側の PeerId)> <hex(トークン)>`（3.1節。Rust だけが読む。対局・UI側は合言葉を読まない） |

`#` で始まる行と空行は読み飛ばす。C++（`tests/tst_p2p_protocol.cpp`）と Rust（`protocol/rust` の試験）が
同じファイルを読み、同じ結果になることを確かめる（開発方針 5.1節の「実装どうしの一致」）。`frames.txt` は、
対局・UI側が本番で使う読み取り（`P2p::Frame::decode`。正しい外枠でも INVITE_TOKEN だけは断る）にも通す。
IPC のメッセージは向きごとに読み手が違う（要求は通信プロセス、応答と通知は対局・UI側）ので、共通の試験入力を
持たず、それぞれの読み手の試験（`peer/src/ipc.rs`・`tests/tst_p2p_protocol.cpp`）で確かめる。
