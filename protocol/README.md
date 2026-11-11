# P2P対局の共通の取り決め（protocol/）

通信プロセス `shogiboardq-peer`（Rust、`peer/`）と対局・UI側（C++、`src/game/p2p/`・`src/network/p2p/`）が
同じ結果を出さなければならない取り決めを、ここに1つだけ置く。仕様の本文は
[実装仕様](../docs/dev/p2p-game-spec.md)、ここはその実装の詳細（バイト列と JSON の形）である。

| ファイル | 内容 | 使う側 |
|---|---|---|
| `README.md`（本書） | 決定的CBOR・ハッシュ・フレームの外枠・招待文字列・募集・一覧取得 | 両方 |
| `ipc.md` | 対局・UI側と通信プロセスの間の IPC（4バイト長＋JSON） | 両方 |
| `game-frames.md` | 対局フレームの payload（HELLO など）。通信プロセスは読まない | C++ |
| `displayname-v1.txt` | 表示名に使える文字の表 v1 | 両方 |
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

### 1.1 ネットワークとロビー

```text
networkId = H(CBOR(["SBQ-NETWORK", 1, networkName]))     ; networkName は text（ASCII、1〜32バイト）
lobbyKey  = H(CBOR(["SBQ-LOBBY", 1, networkId, rulesHash])) ; networkId・rulesHash は byte string（32）
topic     = "/shogiboardq/lobby/1/" + hex(networkId) + "/" + hex(rulesHash)
```

このビルドが参加できる networkName は次の2つだけである（実装仕様 3.1節）。両方の実装がこの一覧を持ち、
ほかの名前を断る。

| networkName | profile | 使用期限 | 宛先 |
|---|---|---|---|
| `sbq-local-1` | `local`（ローカル検証用） | なし | loopback と私設アドレスを使える。待受は 127.0.0.1 |
| `sbq-limited-1` | `internet`（限定試験用） | ビルドから180日 | インターネット上のアドレスと中継だけ（`ipc.md` 5章） |

`rulesHash` は v1 では1つの値（平手・512手・人間対人間の `Rules`。`game-frames.md`）だけを使う。

## 2. 対局フレームの外枠

libp2p のプロトコル `/shogiboardq/game/1.0.0` のストリームに、4バイトのネットワークバイト順の長さと
決定的CBORのフレームを続けて流す。長さは 1〜65536（64 KiB）。長さを読んでから領域を確保する。

```text
Frame = [version, type, requestId, gameId, payload]
```

| 項目 | 型 | 規則 |
|---|---|---|
| version | uint | 1 |
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

## 4. 募集

公開募集は GossipSub の topic（1.1節）へ流し、一覧取得（5章）でも配る。どちらも同じ `SignedOffer` を運ぶ。

```text
OfferBody   = ["SBQ-OFFER", 1, networkId, offerId, sequence, peerId, displayName, rulesHash,
               [mainMs, byoyomiMs], sidePreference, createdAt, expiresAt, availability]
SignedOffer = [body, signature]   ; body は OfferBody の決定的CBORを入れた bstr。signature は bstr(64)
```

| 項目 | 型 | 規則 |
|---|---|---|
| offerId | bstr(32) | CSPRNG。通信プロセスが作る |
| sequence | uint | 同じ offerId の更新ごとに増える |
| peerId | bstr | 発信ピアの PeerId（バイナリ）。Ed25519 の identity multihash に限る |
| displayName | tstr | `displayname-v1.txt` の規則 |
| rulesHash | bstr(32) | topic の rulesHash と一致 |
| mainMs | uint | 0〜86,400,000 |
| byoyomiMs | uint | 0〜600,000。mainMs と両方0は拒否 |
| sidePreference | uint | 0=指定なし、1=先手希望、2=後手希望 |
| createdAt・expiresAt | uint | UNIX秒。`createdAt < expiresAt <= createdAt + 120` |
| availability | uint | 1=Open、2=Withdrawn |

- signature は、peerId から取り出した Ed25519 公開鍵（libp2p 識別鍵）による body のバイト列への署名。
- SignedOffer 全体は 1024 バイト以下。
- 受けた側は、型・範囲・署名・発信者（GossipSub のメッセージの source）と peerId の一致・期限・
  時刻のずれ（createdAt が自分の時計より 30 秒を超えて先なら拒否）・sequence（同じ offerId で前より大きいこと）・
  更新の頻度（同じ発信者で10秒に1回まで。保持している募集の取り下げ（Withdrawn）は頻度によらず受け付ける）を確かめ、
  すべて通ったものだけを保持し、転送する。形・署名・発信者の一致が誤っているものは GossipSub で Reject（送ってきた
  ピアを減点）、期限切れ・古い sequence・頻度・上限・時刻のずれは Ignore（減点しない）として扱う。
- 同じ発信者の Open の募集は1件だけ保持する（新しい offerId が来たら古いものを捨てる）。
- 再受信で有効期限を延ばさない。見た (offerId, sequence) は10分間、上限付きで覚える。
- 保持は1,000件まで。上限に達したら新しい発信者の募集は期限切れで空くまで受け付けない。

## 5. 一覧取得 `/shogiboardq/lobby/1.0.0`

途中から参加したピアが、ほかのピアが保持している募集を取り寄せる。1回の要求に1回の応答を返す
（libp2p の request-response。どちらも 4 バイトの長さ＋決定的CBOR、64 KiB まで）。

```text
ListRequest  = ["SBQ-LIST", 1, networkId, rulesHash, cursor]          ; cursor は uint（最初は 0）
ListResponse = ["SBQ-LIST-RESP", 1, [SignedOffer, ...], nextCursor]   ; 50件まで。続きがなければ nextCursor は 0
```

- 応答の各募集は、受けた側が4章と同じ規則で検査する。応答した相手を募集の発信者とはみなさない。
- 同じ相手からの要求は 10 秒に 1 回まで。超えた要求には空の一覧を返す。

## 6. 試験の入力（testvectors/）

| ファイル | 行の形 |
|---|---|
| `cbor.txt` | `valid <hex>` または `invalid <hex>`（決定的CBORとして読めるか） |
| `frames.txt` | `valid <hex>` または `invalid <hex>`（2章の外枠として正しいか） |
| `displayname.txt` | `accept <hex(UTF-8)>` または `reject <hex(UTF-8)>` |
| `network.txt` | `<networkName> <hex(networkId)> <hex(rulesHash)> <hex(lobbyKey)> <topic>` |

`#` で始まる行と空行は読み飛ばす。C++（`tests/tst_p2p_protocol.cpp`）と Rust（`protocol/rust` の試験）が
同じファイルを読み、同じ結果になることを確かめる（開発方針 5.1節の「実装どうしの一致」）。
