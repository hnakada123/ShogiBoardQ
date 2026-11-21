# コード監査：P2P対局で組み替える価値のある箇所（2026-11-21）

「P2P対局のソースコードで、本来はもっと綺麗に実装できるのにコードが汚く、抜本的に書き直した方が良い箇所はあるか」という依頼で、P2P対局（`ENABLE_P2P_PLAY`）のコードを監査した。前回の監査（`p2p-code-audit-2026-11-12.md`）とセキュリティ監査（`security-audit-p2p-2026-11-13.md`）の修正の後に、エンジンで指す対局・合言葉（同じ LAN・協力ノード）・友だち一覧・ウィンドウの組み替えが入った状態が対象である。

- 対象のコミットは 63a480c0。本文の行番号はこの時点のもの。
- 対象のコード（試験を除く）:
  - C++（約15,100行）: `src/game/p2p/`、`src/network/p2p/`、`src/dialogs/p2p*`、`src/ui/controllers/p2pgameuicontroller`
  - Rust（約11,100行。試験を含む）: `peer/`（通信プロセス `shogiboardq-peer`）、`protocol/rust/`（`sbq-protocol`）
- `src/network/p2p`・`src/dialogs/p2p*`・`p2pgameuicontroller` は全部を読んだ。`src/game/p2p` と Rust は別のエージェントに全部を読ませ、下の R2・R5 と疑いの U1 は、挙げられた行をコードで確かめた。
- 探したのは構造の問題である。書式・命名の好み・コメントの量は対象にしていない。
- 監査ではソースを変更していない。不具合の疑い（4章）は読んだだけで、動かして確かめていない。

> **修正の進み具合**: 5章の順に直し、各項目に「状態」を書く。途中で止まったら、状態のない項目から再開する。

## 全体の傾向

**捨てて書き直すべき箇所はない。** 骨組み（ソケットも GUI も使わない状態機械、`protocol/` の1つの正本、通信プロセスの分離、対局の窓口 `GameChannel`）は、前回の監査の時点から崩れていない。

組み替える価値があるのは、機能を足すたびに継ぎ足された次の3種類である。

1. **状態を bool の組で持っている**: 対局の状態機械（R2）、招待で接続する途中の画面の状態（R1）、通信プロセスのつなぐ途中の接続（R5）。どれも「今どの段階か」を1つの値で持たず、複数の旗・複数の表から毎回組み立てている。
2. **同じ形を2回書いている**: IPC のメッセージの検査と読み取り（R4）、折り返すラベルの作り方と高さの直し方（R3）。
3. **1つだけ切り出されていない**: 「招待」タブ（R1）。ほかの5つのタブはパネルのクラスになっている。

どれも、プロトコルのバイト列・IPC の形・保存形式を変えずに直せる。

---

## 1. 組み替える価値のある箇所（効果の大きい順）

### R1 `P2pWindow` の「招待」タブと、接続の失敗の扱い（dialogs・ui・network）

**今の形**

- 「接続」「対局中」「対局記録」「相手」「設定」はパネルのクラス（`P2pConnectionPanel` など）だが、「招待」だけがウィンドウの中にある。部品を作る部分（`p2pwindow.cpp:174-341`）と操作の処理（`:622-860`）、表示名・対局者の欄の読み書きで約450行、メンバは20個あまり（`p2pwindow.h:164-194`）。`p2pwindow.cpp` は867行で、上限（1000行）に近い。
- 「今つなごうとしているのは何か」を4つの bool で持つ（`p2pwindow.h:188-200`）。
  - `m_connectingByInvite`（欄の下に「接続しています…」を出している）
  - `m_inviteInIsCode`（確かめた・使った文字列が合言葉だった）
  - `m_inviteInHasAddress`（その合言葉に接続先が入っていた）
  - `m_friendApplying`（最後に始めた接続が友だちへの申し込みだった。`P2pGameUiController` が `noteFriendApplyStarted()` で立てる）
- つながらなかった知らせが4本あり、2つのクラスが受け、上の bool で文を選ぶ。
  - `P2pLobbyService::inviteRejected`（通信プロセスが要求を断った）→ `P2pWindow::onInviteRejected`（`p2pwindow.cpp:757`）
  - `P2pLobbyService::friendApplyRejected`（同じ。友だちの申し込み）→ `P2pGameUiController::onFriendApplyRejected`（`p2pgameuicontroller.cpp:304`）。理由を見ずに、いつも「期限が過ぎています」と出す
  - `P2pLobbyService::connectFailed`（接続が開かなかった）→ `P2pWindow::onConnectFailed`（`:828`）
  - `P2pGameCoordinator::inviteRefused`（開いたが、相手が何も返さずに閉じた）→ `P2pWindow::onInviteRefused`（`:803`）
- ロビーは、断られた要求が友だちの申し込みかをメソッド名の文字列で見分けている（`p2plobbyservice.cpp:195`）。接続が開かなかった・閉じられたときは見分けず、ウィンドウの `m_friendApplying` に任せている。

**直し方**

- 「招待」タブを `P2pInvitePanel` に切り出す。ウィンドウに残るのは、帯・タブ・設定の読み書き・パネルどうしのつなぎだけにする。
- 試みの種類（招待文字列・同じ LAN の合言葉・協力ノードを使う合言葉・友だち）を enum にし、ロビーが接続の要求と一緒に持つ。失敗は「種類＋理由」の1つのシグナルで知らせ、受け手は1つにする。画面の4つの bool は要らなくなる。

**大きさと危険**: 中。部品の名前（`objectName`）を保てば、`tst_p2p_ui`（1,686行）と自動化 API の手順がそのまま使える。`tr()` の文脈が `P2pWindow` から変わるので、4言語の訳を移す。

### R2 対局の状態機械 `P2p::Match`（game/p2p）

前回の監査の R6 で、計時を `P2p::MatchTimer` に切り出した。結果の交換は「別のクラスにすると状態機械への参照が要る」として残した。その後の機能の追加で、残した部分の旗が増えている。

**今の形**

- `Phase`（5値）のほかに、bool 9個・optional 5個・「空なら未設定」の値で状態を表している（`p2pmatch.h:195-240`）。
  - `m_readyDone` は「Opening でない」と同じ意味である。初めて Playing にする2か所（`p2pmatch.cpp:273-274`、`p2pmatch_control.cpp:104-105`）でだけ立ち、戻らない。それでも `p2pmatch_control.cpp:146`・`223`・`302` は `m_phase` と両方を確かめている。
  - 再接続の進み具合は `m_myRandom.isEmpty()`・`m_resumeSent`・`m_resumeReceived`・`m_syncing` の組み合わせである（`p2pmatch_control.cpp:366-368`・`390`・`405-419`・`427-428`・`445`）。`m_syncing` は「相手の要求を待つ」と「相手のチャンクを待つ」の両方を指す。
  - 確認待ちの自分の手は、`m_pending`・`m_pendingFrame`・`m_pendingSent`・`m_pendingSentAt` の4つに分かれ、消すときは3つを並べて消す（`p2pmatch.cpp:346-348`、`p2pmatch_control.cpp:459-461`）。
  - `m_myProposalHash`・`m_pendingOfferHash` は、結果の値から求められるものを別に持ち、代入のたびに手で合わせている。
- 2ファイル（491行と492行。合わせて983行）は話題で分けてあるが、同じクラスの全メンバを両方が触る。制御メッセージの上限は、定数（`p2pmatch.cpp:17`）とリテラル 4096（`p2pmatch_control.cpp:40`・`489`）に分かれている。`:489` の検査（`m_controlOut + m_controlIn > 4096`）は、2つの番号が再接続のたびに 0 に戻るので成り立たない（上限そのものは `:40` と `p2pmatch.cpp:454` で効いている）。
- 「相手の提案を返して一致させる」4行が3回ある（`p2pmatch_control.cpp:205-210`・`249-252`・`281-284`）。

**直し方**

`MatchTimer` と同じく、状態機械への参照を持たない値の部品に切り出す。送信・ジャーナル・終局は `Match` に残す。

1. 制御の通し番号・connectionId・件数の上限を持つ部品（署名して作る・受けて「重複・飛び・署名不正・上限・受理」を返す・再接続で作り直す）。
2. 確認待ちの手を、1つの構造体（状態・フレーム・送ったか・送った時刻）の optional にする。
3. 再接続の段階を enum にする。`m_readyDone` は消す。
4. 結果の交換の値（自分の提案・相手の申し出・答えを待つ申し出・一致した結果・双方の署名）を1つの部品にまとめ、ハッシュは値から求める。判断（だれの手番か・時計・棋譜から決まる終局）は `Match` に残す。

**大きさと危険**: 中〜大。バイト列・共通の試験入力・ジャーナルの形は変えない。`tst_p2p_match`（1,290行）が公開の関数だけで順序・重複・再接続を確かめている。セキュリティ監査の P7・P17・P18 の挙動を保つ。

### R3 折り返すラベルの高さ合わせ（dialogs・widgets）

**今の形**

3段の回避策が7ファイルに散っている。

1. `DialogUtils::fitWrappedLabels`（`src/widgets/dialogutils.cpp:111-123`）が、表示・文字の大きさの変更のときに、折り返すラベルの最小の高さを今の文の高さに固定する。
2. 文を替える側は、固定された高さを毎回 `setMinimumHeight(0)` で外す（`p2pwindow.cpp:573`・`644`、`p2pconnectionpanel.cpp:54`、`p2pchoicecard.cpp:70`、`p2pstatusstrip.cpp:101`、`p2psettingspanel.cpp:128`）。外し忘れると、短い文に替えても高さが残る。
3. `P2pWindow::growToFit`（`p2pwindow.cpp:659-670`）が、アプリ全体の `LayoutRequest` を8回流してから、足りない分だけウィンドウを広げる。呼ぶ所は6か所あり、文を替えた後に呼び忘れると文が欠ける。

ラベルを作る補助（`plainLabel`・`headingLabel`・`wrappedLabel`）も、6ファイルに9回定義されている。

**直し方**

- 折り返すラベルの部品を1つ作り、作り方（PlainText・折り返し）と、文を替えたときに前の高さを残さないことを、その部品に持たせる。見出しのラベルも同じ場所に置く。
- ウィンドウは、自分に届いた `LayoutRequest` で、収まらなくなっていれば広げる。文を替える側は、ウィンドウに知らせない。

**大きさと危険**: 小〜中。ウィンドウの高さは4言語で測り直す（`tst_p2p_ui` の `inviteCheckResultIsNotClipped` など）。

### R4 `PeerIpc` の検査と読み取りの二重記述（network/p2p）

**状態: 済**。`readReply(Method, result)`・`readNotification(name, params)` が、フィールドを1つずつ形を確かめながら読む（読み残したフィールドがあれば断る）。`PeerProcess` は読めた型（`Reply`・`Notification`）だけを知らせ、JSON は外へ出さない。hello の応答は `Hello { Sandbox }`、メソッドは enum `Method`、到達性は enum `Reachability`、接続の出どころは enum `Origin` にした。要求の外枠と長さの前置き（`encodeRequest`・`takeMessage`）も `peeripc` に移した。検査が少し厳しくなった所が2つある: `meetGet` の `blobs` と `createPresence` の `presence` は base64 として読めること（以前は印字できる ASCII かだけを見て、読むときに読めない文字を読み飛ばした）。通信プロセスは正しい base64 しか送らないので、動きは変わらない。

**今の形**

- 受けたメッセージの形の検査（`checkResult`・`checkNotification`。`peeripc.cpp:280`・`346`）と、値の取り出し（`readReply`・`readNotification`。`:481`・`:547`）が、同じメソッド名・通知名の if の列で2回書かれている。フィールド名は両方に現れ、取り出す側は検査済みの値を `value_or(0)` で読み直す。語から値への対応（`kind`・`origin`・`direction`・`route`）も、検査の `oneOf` と取り出しの比較に分かれている。
- ヘッダーは「メソッド・通知・フィールドの名前と値の語は、このファイルだけが知る」としているが、漏れが2つある。
  - hello の応答に型がなく、`sandbox` の JSON を `peerprocess.cpp:34-47`（必須の仕組みの検査）と `p2pwindow.cpp:552-559`（仕組みの名前の表示）が直接読む。
  - メソッド名の文字列が `p2plobbyservice.cpp:195-198` にある。

**直し方**

- メッセージごとに「検査しながら読んで `optional<T>` を返す」関数1つにする。フィールドの読み取りの部品は、読んだ名前を数えて、知らないフィールド・足りないフィールドを最後に確かめる。
- hello の応答も型にする（`Hello { peerVersion, sandbox { applied, os, mechanisms } }`）。
- 失敗した要求の種類は、文字列でなく型で渡す。

**大きさと危険**: 小。`tst_p2p_endtoend`・`tst_p2p_sandbox` と、IPC の形の試験が守る。IPC の形（`protocol/ipc.md`）は変えない。

### R5 Rust: つなぐ途中の接続が5つの表に分かれている（peer）

**今の形**

- 1つの接続番号が「協力ノードから引き取り中 → 合言葉の宛先へダイヤル中 → どの接続で開くか待ち → 開いた」と進む間、段階ごとに別の表に入る: `conns`・`opening`・`code_connects`・`code_dials`（`peer/src/node.rs:95-109`）、`meets.connects`（`peer/src/node/meets.rs:63`）。
- 同じ問いに、毎回いくつもの表を見て答えている。
  - 数える: `game_connection_count` が3つを足す（`node/streams.rs:173-178`）。
  - 閉じる: `close_conn` が4つから消す（`node/streams.rs:437-448`）。
  - あるか: `close_connection` が3つを順に調べる（`node/requests.rs:558-561`）。
  - `connectionClosed` を出す所が3か所ある（`streams.rs:443`、`codes.rs:266`、`meets.rs:309`）。
- IPC の要求の処理は、応答の返し方が3通りあり（すぐ返す・後で返す・`Option`）、`on_request` に入れ子の match が並ぶ（`node/requests.rs:82-135`）。エラーは `(&'static str, String)` の組で、語（`invite_not_found` など）が文字列のまま4ファイルにある。通知の語は `words!` で enum にしてある（`ipc.rs:253-309`）のに、エラーだけ揃っていない。

**直し方**

- 番号1つに段階1つを持つ `Conns` 型にまとめる。番号の発行・数・閉じる・`connectionClosed` の通知はこの型だけが行う。`Reservations`・`Invites` と同じく Swarm に触れない型にして、段階の進み方を単体で試す。
- エラーの語を enum にし、`IpcError { code, message }` と `Reply { Now(Value), Later }` で `on_request` を1行ずつの表にする。

**大きさと危険**: `streams.rs`・`codes.rs`・`meets.rs`・`requests.rs` の計300行前後。通知の有無・理由の細かい違い（`closeConnection` は通知しない、引き取りの失敗は `unreachable` など）を保つ。`peer/tests/ipc.rs` が守る。効くのは、次のつなぎ方を足すときである。

---

## 2. 小さな整理で足りる箇所

| # | 箇所 | 今の形 | 直し方 |
|---|---|---|---|
| S1 | `P2pGameCoordinator`（810行） | 自分のエンジンの起動・指し手の依頼・持ち時間・異常の処理（約170行。`p2pgamecoordinator.cpp:637-804`）が司令塔の中にある | エンジンの部分を別のクラスにする |
| S2 | 招待する側の条件 | 表示名・持ち時間・先後・対局者の4つ組が、`createInvite` の引数・ロビーのメンバ4つ・`P2pFriendPresence::HostSetup`・`NegotiationRequest`・ウィンドウの getter の5通りで表されている | 値の型1つにする |
| S3 | 友だちの変更の知らせ | `P2pKnownPeers` に変更の知らせがなく、「相手」タブが `reloadFriends()` を3か所で呼ぶ（`p2ppeerspanel.cpp` の `onToggleFriend`・`onToggleBlocked`・`onRemove`） | 記録の側から知らせる |
| S4 | 先後の表し方 | 純粋層に bool（`iAmBlack`）・`quint8`（`kSideBlack`）・`Turn` の3通りがあり、`iAmBlack ?` の三項が `Match` と `Negotiation` で32行ある | `SignedState::approval(Turn)`・`GameState::mainMs(Turn)` を足し、1/2 への変換を `p2pframes` だけにする |
| S5 | 交渉から対局への引き継ぎ | 同じ10項目を司令塔（`p2pgamecoordinator.cpp:184-194`）と試験が写している | `Negotiation` に `MatchSetup` を作る関数を置く |
| S6 | 状態と両署名の組の読み書き | `p2pframes.cpp`・`p2pevidence.cpp`・`p2precordkinds.cpp` の3か所にあり、署名の長さの厳しさが3通りある | `SignedState` に読み書きを置く |
| S7 | `P2pGameUiController` | 知らせの箱の `QPointer` が5つある。`#include` がファイルの途中にある（`p2pgameuicontroller.cpp:41-42`） | 申し出の箱の組を小さなクラスにする |
| S8 | ネットワークの名前 | `sbq-local-1`・`sbq-limited-1` が `p2pprotocol.cpp:263`・`p2pconnectionpanel.cpp:29`・`networksettings.h:120` にある | 純粋層の表から引く |
| S9 | Rust: 役割の bool | `NodeConfig` の `service`・`relay_only`・`relay_server`・`meet_server` から `!relay_only && !service` のように導いている | `enum Role` にする |
| S10 | Rust: `sandbox.rs`（787行） | OS ごとの `apply` と自己試験が1ファイルに離れて並ぶ | `sandbox/{linux,macos,windows}.rs` に分ける |
| S11 | Rust: 小部品の重複 | 相手ごとの枠の数え方（`streams.rs`・`meet.rs`）、トークンバケット（`game.rs`・`meetstore.rs`）、長さ付きの読み書き（`game.rs`・`meet.rs`） | 1つにする |

---

## 3. 書き直し不要と判断した部分

- **純粋層の設計**: 交渉 `P2p::Negotiation` と対局 `P2p::Match` は時刻を関数で受ける状態機械で、検証（`p2pevidenceverifier`）とライブの対局が同じ `GameRules`（`initialState`・`checkNext`・`judge`）を使う。規則の二重実装はない。
- **`p2pcbor`・`p2pprotocol`・`p2pframes`**: 型ごとの手書きだが、書く側と読む側が隣り合い、検査は `CborReader` に寄っている。表にまとめると、型ごとの追加の検査でかえって読みにくくなる。
- **版の違い**: 分岐は HELLO の形、交渉の3か所、ジャーナルの開始の件だけで、回線の互換のためのものである。
- **形式の正本**: `protocol/` に1つで、C++ と Rust が共通の試験入力（`protocol/testvectors/`）で固定されている。
- **Rust の小さな型**: `invites`・`presences`・`reservations`・`circuits`・`outbound`・`addr.rs` は状態と規則だけの型で、Node に触れず、単体試験つきである。R5 の手本になる。主ループの select は5本で、各々が1つの関数へ渡すだけである。
- **本体との境界**: `#ifdef SBQ_ENABLE_P2P` は8ファイル16か所だけで、合成ルート・メニュー・動作状態の表に収まっている。
- **鍵の保管と記録**: `P2pIdentityStore`（秘密の消し方）、`P2pOwnerFile`（原子的な書き込み）、`P2pRecordStore`（ジャーナルの復旧）、`P2pPlayerProfile`（ロック）は、責務が1つで小さい。
- **`P2pFriendPresence`**: 預ける・調べる・申し込みを受けるの3つを、待ちの表と時計で素直に書いてある。

---

## 4. 不具合の疑い（読んだだけで、動かして確かめていない）

| # | 疑い | 根拠 |
|---|---|---|
| U1 | 千日手などで棋譜から終局が決まった直後に、次の手を指せる。その直後に回線が切れると、双方の記録が食い違う恐れがある | `Match::playMove`（`p2pmatch.cpp:380-392`）と司令塔の `isMyMoveExpected` は、棋譜から決まった終局を見ない。切断がなければ、相手は先に届いた結果の提案で一致し、後から届いた手を取り込まない（`p2pmatch.cpp:200`）ので無害である。切断があると、指した側は確認待ちの手があるために結果の提案を送り直せない（`p2pmatch_control.cpp:148`・`484-488`）。この先の経路は確かめていない |
| U2 | Rust: 確立の直後に閉じた接続の知らせで、同じ相手へ開く途中の接続が `unreachable` で失敗しうる | `peer/src/node/events.rs:51-62`・`77-98`。`peer_conns` に入れずに閉じた接続にも ConnectionClosed が届き、その相手のほかの接続が無ければ `fail_openings_of` が走る |
| U3 | Rust: 協力ノードが預けものを置き換えるとき、古いものを消してから件数の上限を調べる | `peer/src/meetstore.rs:133-143`。接続元が変わっていて新しい接続元が上限なら、古いものを失ったうえで断られる |
| U4 | Rust: 「協力ノードの中継だけを使う」で同じ LAN の合言葉につなぐと、理由が `no_relay_route` でなく `unreachable` になる | `peer/src/node/requests.rs:344-349` → `codes.rs:182-190`。画面は使える候補が0件なら接続ボタンを押せなくするので、通常は届かない |

---

## 5. 進め方

1. R4（IPC のメッセージを型で読む。R1 のロビーの直しの土台になる）
2. R3（ラベルの部品。R1 で切り出すパネルを新しい部品で書く）
3. R1（「招待」タブの切り出しと、接続の失敗の一本化）
4. R2（`Match` の値の部品への切り出し。U1 を試験で確かめ、起きるなら一緒に直す）
5. R5（Rust の `Conns` と IPC のエラーの型）
6. 2章の小さな整理（触った部品に関わるものから）
