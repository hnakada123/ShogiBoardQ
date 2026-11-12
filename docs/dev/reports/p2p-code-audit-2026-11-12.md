# コード監査：P2P対局の責務の整理（2026-11-12）

「P2P対局のコードが責務ごとに整理されているか」という依頼で、P2P対局（`ENABLE_P2P_PLAY`）のコードを監査した。2026-11-11 に実験版リポジトリから通常版へ取り込んだ後の状態が対象である。

- 対象のコミットは 4421e0a6。本文の行番号はこの時点のもの。
- 対象のコード（試験を除く）:
  - C++（約1万行）: `src/game/p2p/`（4,315行）、`src/network/p2p/`（3,556行）、`src/dialogs/p2p*`・`src/ui/controllers/p2pgameuicontroller`（2,314行）、`src/headless/matchverificationservice`・`src/cli/clicommands_p2p`、合成ルートの P2P の部分
  - Rust（約5千行。試験を含む）: `peer/`（通信プロセス `shogiboardq-peer`）、`protocol/rust/`（`sbq-protocol`）
  - 取り決めの文書: `protocol/README.md`・`ipc.md`・`game-frames.md`、`docs/dev/p2p-game-spec.md`
- C++ は全部を読み、呼び出し元と重複を grep で確かめた。Rust は別のエージェントに読ませ、主な主張（`ipc.md` の版、`frameReceived` の2つの経路、`status_dirty`、使用期限の起点、`IDENTIFY_PROTOCOL`・`LAN_PORT`、使われない関数、レートの上限）はコードで確かめた。
- 観点:
  - 1つのクラスが複数の責務を持っていないか
  - 同じ規則・状態・語を複数の場所で持っていないか（C++ と Rust の間も）
  - 層・ディレクトリの置き場所は合っているか
  - 画面の部品が操作を直接実行していないか、同じ操作の入口が複数ないか
  - 名前・注釈・仕様書が実態と合っているか
- 不具合はコードの経路を読んで確かめた。アプリと試験では再現していない。
- 監査ではソースを変更していない。

> **修正の進み具合**: 7章の順に直し、各項目に「状態: 済（コミット）」を書く。途中で止まったら、状態のない項目から再開する。

## 全体の傾向

骨組みの責務の分け方はできている。直したほうがよい点は、部品のつなぎ目に集まっている。

良くできている点:

- 交渉 `P2p::Negotiation` と対局 `P2p::Match` は、ソケット・タイマー・UI を持たない。時刻は関数で受け取り、送るフレームと知らせを返すだけである。状態機械だけで試験できている（`tst_p2p_match`）。
- 純粋な部分を `src/game/p2p` に置いた理由が正しい。CLI の `verify-match` が使うには headless から参照できる必要があり、headless は network を参照できない（仕様 10.2節）。
- 司令塔 `P2pGameCoordinator` は通信プロセスを直接知らず、窓口 `P2pTransport` だけを使う。盤の操作は `P2pGameChannel` で、ローカル対局・CSA通信対局と同じ `GameChannels` の口を通る。
- `sbq-protocol` は sha2 と base64 だけに依存し、libp2p・tokio を含まない。通信プロセスは CBOR を自分で読まず、`game.rs` も外枠・頻度・受信量だけを見て、対局の規則を知らない。宛先の規則は Transport の層の1か所（`AddrPolicy`）で掛けている。
- 決定的CBOR・フレームの外枠・networkId・表示名は、C++（`tst_p2p_protocol`）と Rust（`protocol/rust/tests/vectors.rs`）が同じ試験入力（`protocol/testvectors/`）を読む。

残っている問題は次の4つに分けられる。

1. **1つの状態・規則を2つの部品で持っている**: 動作状態をロビーと司令塔で受け渡す順序、終局の行の対応表、ジャーナルの書き手と読み手、終わり方の語。
2. **つなぎ目の部品が抱えすぎている**: `P2pLobbyService`（C++）と `node.rs`（Rust）。
3. **画面の部品が操作を直接実行している**: `P2pWindow`・既知の相手の表。申し出の確認は司令塔の知らせの中でモーダルに開く。
4. **C++ と Rust の取り決めのずれ**: IPC の文書の版、使用期限の起点、共通の試験入力が本番の関数を通らないこと。

---

## 1. 見つかった不具合

| # | 不具合 | 原因 | 確認 |
|---|---|---|---|
| B1 | 対局が始まった直後に同じ呼び出しの中で終わると、ロビーが遅れて届いた「取り上げ」の知らせを受けて `end()` し、通信プロセスまで止まる（P2P の接続が切れ、招待も無効になる） | `P2pLobbyService::onActivityRevoked`（`p2plobbyservice.cpp:141`）が、自分のリースが今も有効か（`m_lease.isCurrent()`）を見ず、司令塔が対局中か（`isBusy()`）だけで無視するかを決めている。`AppActivityState::begin` の `revoked` は次のイベントループで届く（`appactivitystate.cpp:52`） | コードで確認<br>**状態: 済（ee00f039）** |
| B2 | 相手の合意引き分け・合意中断の申し出を尋ねている間に、通信プロセスからの知らせと毎秒の確認が入れ子で処理され、知らせの処理の順が前後する。自分の手番に申し出が来ると、答えるまで盤を操作できず、その間も自分の時計は進む | `P2pGameUiController::onOfferReceived`（`p2pgameuicontroller.cpp:247`）が `DialogUtils::confirmAction`（`exec`）を開く。これは司令塔の `flushMatch` の知らせのループ（`p2pgamecoordinator.cpp:370`）の途中で、さらにその外側は `PeerProcess::onReadyReadStdout`（`peerprocess.cpp:183`）のループである | コードで確認<br>**状態: 済（567d4526）** |

B1 の経路は2つある。

1. 開始のジャーナルを書けない（ディスクがいっぱいなど）。`startMatch` は「P2P対局中」を始めた（ロビーへの `revoked` が予約される）後に `!journaled` で `Match::abort` し、`Finished` の知らせで「P2P対局中」を手放す。`gameFinished` を受けたロビーは `onGameReleased` で「P2P接続中」を取り直す。次のイベントループで予約されていた `revoked(lobby)` が届き、司令塔はもう対局中でないので `end()` が動く。
2. 相手が、開始署名（START_SIGNATURE）と ERROR の CONTROL や壊れたフレームを続けて送る。`onReadyReadStdout` は1回の読み取りでたまった知らせをすべて同期して処理するので、開始と終局が同じイベントループの中で起き、1 と同じ順になる。相手の操作で、こちらの P2P の接続を切らせられる。

直し方: B1 は `onActivityRevoked` の先頭に `if (m_lease.isCurrent()) return;` を足す（取り直した後の古い知らせを無視する）。根本の原因は、1つの動作状態をロビーと司令塔が暗黙の順序に頼って受け渡していることなので、3.1 の切り出しのときに受け渡しを1か所にまとめる。B2 は申し出の確認をモードレスのダイアログにし、答えをスロットで受けて `answerOffer` に渡す（司令塔の知らせの中でイベントループを回さない）。

## 2. 同じ規則・状態を2か所で持っているもの

### R1 終局の行の対応表が `MatchEnding` の外にある

- `p2pgamecoordinator.cpp:34` の `endingEntry` が、P2P の終局の原因（`P2p::Cause`）から棋譜の終局の行を自前で作る。CLAUDE.md は、原因から終局の行への対応を `MatchEnding`（`src/game/matchending.h`）の「ここだけ」に置くと定めている。ローカル対局と CSA通信対局はそれを通る（CSA はサーバーの結果を `MatchCause` に直す）。
- 連続王手の千日手だけ `RecordEntry::forSideToMove` を自分で呼んでいて、ほかの原因とそろえ方が違う。
- 名前もぶつかる。`P2p::MatchEnding`（`p2pmatch.h:56`、署名がそろったか・中断したかのローカルの終わり方の enum）と `::MatchEnding`（終局の対応表の名前空間）は、同じ名前で意味が違う。`P2pWindowText::endingText(P2p::MatchEnding, ...)` と `MatchEnding::endingOf` が並ぶと読み違えやすい。
- 直し方: `MatchCause` に P2P だけにある原因（詰み・合意の引き分け）を足し、司令塔は `P2p::Cause` を `MatchCause` に直して `MatchEnding::endingOf` を通す。`P2p::MatchEnding` は `P2p::Settlement` のような別の名前にする。

### R2 ジャーナルの書き手と読み手が別の層にある

- 書くのは `P2p::RecordKinds`（`src/game/p2p/p2precordkinds.cpp`）、読み戻すのは `P2pRecordStore::evidenceFromJournal`（`src/network/p2p/p2precordstore.cpp:220`）である。件のラベル（`"J-START"`・`"J-STATE"` など）と各件の要素の並びが、両方に文字列と添字で重複している。読み手は `CborReader` を使わず、ほかの読み取りと書き方が違う。
- 書き手も割れている。開始の3件（`J-START`・`J-STARTSIG`）は司令塔（`p2pgamecoordinator.cpp:195`）が書き、残りは `Match` が書く。`p2precordkinds.h` の説明は「対局の状態機械（Match）が書き」としている。
- `P2pGameCoordinator::saveEvidence`（`p2pgamecoordinator.cpp:477`）は、相手の矛盾した署名の証明があったか（ジャーナルを残すか）を知るためだけに、ジャーナル全体をディスクから読み直して `evidenceFromJournal` に通している（`:488`）。証明を書いたのは `Match` なので、`Match` は既にそれを知っている。
- 直し方: 読み戻しを `RecordKinds` の隣（game/p2p）に移し、ラベルを1か所で持つ（headless の試験で書いて読み戻せる）。開始の件も `Match` が書く（相手の表示名は `MatchSetup` に入れる）。`Match` に `hasConflictProof()` を持たせ、`saveEvidence` は読み直さない。

### R3 ローカルの終わり方の語が文字列で3か所にある

- `Match::endingName`（`p2pmatch.cpp:110`）が enum を `"mutually_signed"` などの語にし、ジャーナルと履歴の情報に書く。
- `P2pRecordStore::recoverJournals`（`p2precordstore.cpp:285`）は、enum に無い `"interrupted"` を自分で書く。
- `P2pWindowText::localEndingText`（`p2pwindowtext.cpp:109`）が語を文字列で比べて表示の文にする。同じ概念の文が `endingText(P2p::MatchEnding, ...)` にもあり、表が2つある。
- 直し方: game/p2p に「アプリの終了で中断」を含む enum と、語との相互の変換を1つ置き、記録の保存先と画面はそれを使う。

### R4 所有者だけが読み書きできるファイルの原子的な書き込みが3つある

`p2pknownpeers.cpp:67`・`p2precordstore.cpp:84`（`writeAtomically`）・`p2pidentitystore.cpp:160`（`writeFileAtomically`）が、同じ手順（`QSaveFile`・`ReadOwner | WriteOwner`・`commit`）を別々に書いている。フォルダの権限を付けるのは2つだけである。安全に関わる規則なので、1つの関数にまとめる。

## 3. 抱えすぎている部品

### R5 `P2pLobbyService` が7つの責務を持つ

`p2plobbyservice.h/.cpp` が次のものを持つ。

- プレイヤーの保存先の持ち主: 何人目のプレイヤーか（`acquire`）、ロック（`P2pRecordStore::lock`）、ジャーナルの復旧、既知の相手、プレイヤー鍵
- 通信プロセスの寿命（`PeerProcess`）
- IPC の JSON の組み立て（メソッド名と引数の欄の名前）と、検査済みの結果・通知の欄を文字列で読み直すこと（`onPeerReplied`・`handleConnectionOpened`、`:168` 以降）
- 招待（作る・読む・接続する）と、招待した側の条件
- 開いた接続の振り分け（`P2pTransport` の実装）
- REJECT フレームの組み立て（`rejectConnection`、`:344`。`requestId = 1` と空の対局ID もここで決める）
- 実験版のビルドの使用期限（`kBuildLifetimeDays`・`isBuildExpired`）

IPC の欄の名前を知っている場所は、検査（`peeripc.cpp` の `checkResult`・`checkNotification`）と読み取り（ロビー）の2つになっている。

直し方:

- 「プレイヤーの保存先」（何人目か・ロック・記録・既知の相手・鍵の場所）を別の部品にする。`P2pRecordStore` が `p2p/` 全体のロックと `identityDirectory()` を持つのもそちらへ移す（記録の保存先がフォルダ全体の持ち主になっている）。
- IPC は `PeerIpc` に型付きの要求・結果・通知（`StartResult`・`ConnectionOpened` など）を持たせ、検査と読み取りを同じ場所にする。
- REJECT は `P2p::rejectFrame(code)` のような関数を p2pframes に置く。
- 使用期限は 5.2 で Rust とまとめる。

### R6 `Match` を責務ではなく行数で2ファイルに分けている

`P2p::Match` は1つのクラス（約955行）を `p2pmatch.cpp`（State0 と着手）と `p2pmatch_control.cpp`（制御・結果・再接続）に分けている。CLAUDE.md の「行数で機械的に分けず、責務を別のクラスに切り出す」に合わない。

切り出す候補:

- 時計の観測: `m_myTurnStart`・`m_observeStart`・`m_reconnectAtObserveStart`・`m_reconnectTotal`・`m_reconnectStart` と、それを使う計算（`currentTurnElapsedMs`・`reconnectTotalMs`・`tick` の時間切れの判定・`handleStatePropose` の観測時間）。値のクラスにすれば単体で試験できる。
- 結果の交換: 自分の提案・相手の申し出・同じ手数での申し出の回数・結果への署名（`m_myProposal` から `m_resultAgreedAt` まで）。

### R7 Rust の `node.rs`（851行）が抱えすぎている

`peer/src/node.rs` に次のものが同居している。

- 待受の候補と 48210 番の確保（41-102, 352-370）
- 使用期限の時計（104-121）
- IPC の入口: `stdin_task`（223-254）、hello と start の受け取り（257-296）、start の処理（299-331）。ほかの要求の処理は `node/requests.rs` にある
- Swarm の作成
- 対局ストリームの開閉と受け付け（476-702, 763-788）
- 招待トークンの照合（657-673, 754）
- 毎秒の処理（704-728）と networkStatus を決める処理（730-750）

招待の状態は2つのファイルに割れている。照合と期限切れの削除は `node.rs:715`、発行・上限・消去は `requests.rs:21,45,55,118` にある。

直し方: `node/reservations.rs` と同じ「状態だけの型」の形で、`node/invites.rs`（発行・照合・期限切れ・消去と定数時間の比較。単体で試験できる）、`listen.rs`（待受の候補と開く処理）、`node/streams.rs`（`Conn`・`Internal`・`accept_task`・`on_inbound`・`update_routes`）に切り出す。IPC の握手と start は ipc 側か `requests.rs` へ移す。

### R8 Rust の IPC の出力の形が散らばっている

- 要求は `ipc.rs` の型で受けるが、応答と通知は `json!` で直接組んでいる。組む場所が `node.rs:268,321,566,584,614,697,739`、`events.rs:134`、`sandbox.rs:19` に分かれる。
- `frameReceived` は2か所で組んでいて、扱いが違う。`node.rs:698`（ストリームの最初のフレーム）は `notify` で送り、受信キューの計上（`notify_with_release`）を通らない。`node.rs:615` は通る。
- 切断の理由・経路・接続の由来は `&'static str` の文字列があちこちに書かれている。C++ 側は `peeripc.cpp:207-275` の1つの表で検査している。
- 直し方: `ipc.rs` に結果と通知の Serialize できる型と、理由の enum を置き、`ipc.md` の3章・4章と1対1にする。

### R9 Rust の協力ノードが IPC の偽の経路を作っている

`service.rs:121-123` は知らせを捨てるだけの `Out` を作り、`:141` は使わない要求の通路を作っている。`service` フラグによる分岐も `create`・`on_tick`・`on_identify`・swarm に散っている。`Node` が IPC に直結しているためで、`Node` は型付きのイベントを返し、IPC への橋渡しを別の部品にすれば、協力ノードは知らせを受けなくて済む。

## 4. 画面の部品

### R10 `P2pWindow` が操作を直接実行している

- `P2pWindow`（dialogs の層）は、接続と切断・鍵の作り直しは意図のシグナル（`startRequested` など）で知らせ、`P2pGameUiController` が受ける。一方、招待（`p2pwindow.cpp:537,571,596`）と投了・申し出・対局の中止（`:643-661`）は、ロビーと司令塔を直接呼ぶ。定跡ウィンドウ（`JosekiWindow`）は意図のシグナルだけで知らせる形にそろえてある。
- そのため投了の入口が2つある。メニューの投了は `GameChannels::active()` の `P2pGameChannel::resign` を通るが、ウィンドウの投了は通らず、確認のダイアログも別に書いている。
- `p2pwindow.cpp:37` は、LAN の待受のポートの文（`:627`）のためだけに IPC の層の `peeripc.h` を include している。
- 直し方: 招待と対局の操作も意図のシグナルにして `P2pGameUiController` が受ける。投了・申し出は `GameChannel` を通す。ポートはロビー（または 5.3 の protocol の定数）から読む。

### R11 既知の相手の記録を3か所が直接書き換える

- 司令塔（`p2pgamecoordinator.cpp` の `startMatch` と `Finished` の処理）、相手の表（`p2ppeerspanel.cpp:120-170`）が、`P2pKnownPeers::upsert` で返る参照を書き換えてから `save()` を呼ぶ。保存を呼び忘れても気づけない。
- 相手の表は表示したときの一覧を持つだけで、対局で数が変わっても作り直さない。
- 直し方: `P2pKnownPeers` に意図ごとの関数（対局を始めた・結果が確定しなかった・名前を付ける・確認済みにする・ブロックする・消す）を置き、保存もその中で行う。変わったら知らせる。

### R12 文の表が2か所にある

`P2pWindowText` は「共通の文」だが、通信プロセスの失敗（`PeerProcess::Failure`）と使い始められない理由（`P2pLobbyService::StartFailure`）の文は `p2pgameuicontroller.cpp` に直接書いている。`P2pWindowText` に集める。

## 5. C++ と Rust の取り決め

### 5.1 IPC の文書の版が実装と違う

**状態: 済（aed37b09）**。`shutdown` は通信プロセスから出ないので、文書と対局・UI側の検査から外した。

`protocol/ipc.md:39-45` は `protocolVersion` を 1 と書いているが、実装は C++（`peeripc.h:17`）も Rust（`ipc.rs:18`）も 2 である。同じ文書の75行目は `ipcVersion` を 2 としている。切断の理由 `shutdown` は `ipc.md:135` と `peeripc.cpp:270` にあるが、Rust のどこからも出ない。

### 5.2 実験版の使用期限を2つの言語が別々に決めている

- 日数の 180 を、Rust（`protocol/rust/src/network.rs:48` の `BUILD_LIFETIME_DAYS`）と C++（`p2plobbyservice.h` の `kBuildLifetimeDays`）が別々に持つ。
- 起点の時刻も別々に決める。Rust は `peer/build.rs` が最後に動いた時刻、C++ は CMake で構成した時刻（`CMakeLists.txt:1075`）。どちらも `SOURCE_DATE_EPOCH` があればそれを使うが、無いと起点がずれ、期限の近くで両者の判定が食い違いうる。共通の試験入力にもない。
- 直し方: CMake が cargo に起点を環境変数で渡す。または、通信プロセスが hello の応答で期限を返し、C++ はそれを使う。

### 5.3 protocol/ に置かれていない通信の定数

互換仕様書（`p2p-interop-spec.html:395, 1123-1124`）に載っている次の2つが、`protocol/README.md` と `sbq-protocol` にない。

- Identify の `/shogiboardq/1.0.0`（`peer/src/swarm.rs:18`）
- LAN の待受のポート 48210（`peer/src/node.rs:41` の `LAN_PORT`）。C++ の `peeripc.h:19` の `kLanPort` と二重になっている。

`sbq-protocol` の network に置き、README に足す。

### 5.4 共通の試験入力が本番の関数を通らない

`tst_p2p_protocol` が `protocol/testvectors/frames.txt` で確かめているのは `Frame::isWellFormedEnvelope`（`p2pframes.cpp:33`）で、これは試験からしか使われない。本番で使う `Frame::decode`（`p2pframes.cpp:46`）は、同じ外枠の検査を別に書いている（INVITE_TOKEN を断るところだけが違う）。`decode` が外枠の検査を `isWellFormedEnvelope` と共有するか、試験入力を `decode` にも通す。

IPC の形には共通の試験入力が無い。C++ の `PeerIpc::parse`・`checkResult`・`checkNotification`（通信プロセスからの入力の信頼の境界）は、直接の単体試験も無く、本物の通信プロセスを使う `tst_p2p_endtoend` でしか通らない。

### 5.5 Rust の「対局中か」の判定が3か所にある

START_SIGNATURE を見て受信の頻度の上限を変える規則がある。その記録が `game.rs:140`、`node.rs:694`、`requests.rs:248` に散っている。`game.rs:124` の `RateLimiter::new(10)` は、`:132` の `(5, 10)` とは別に書いた数である。`ConnShared` に送信・受信の観測と上限の決定をまとめる。

再接続の手順（`game-frames.md` 4章）では START_SIGNATURE を送り直さないので、再接続の新しいストリームは毎秒5件の上限のままになる。SYNC_CHUNK がこの上限に掛かるかは確かめていない。

## 6. 小さな重複・名残・名前のずれ

| # | 内容 | 場所 |
|---|---|---|
| S1 | 汎用の CBOR の読み取り補助 `CborReader` が、条件・状態の値のヘッダーにある。保存先の部品（既知の相手・記録・鍵）もこれのために `p2pprotocol.h` を読む | `p2pprotocol.h:34`。`p2pcbor.h` に移す |
| S2 | 「仮予約」（`kReservationMs`・`reservationRemainingMs`・`m_reservationDeadline`）は公開募集の時代の語で、仕様書に無い。中身は条件の確認と開始署名までの期限 | `p2pgamecoordinator.cpp:26`・`p2pgamecoordinator.h`・`p2pconfirmdialog.cpp:180` |
| S3 | 後ろに宣言のない注釈「申込側の開始署名を確かめた（募集側はここで募集を取り下げる）」 | `p2pnegotiation.h:86` |
| S4 | `P2pRecordStore` のファイルの説明が「既知の相手」を挙げるが、既知の相手は `P2pKnownPeers` | `p2precordstore.h:5` |
| S5 | ロビー・ウィンドウの説明は「アドレス（IP）は届かない・出さない」と書くが、`inspectInvite` の `privateAddresses` は届き、ウィンドウに出す（`ipc.md:128-129` は例外として書いている） | `p2plobbyservice.h:33`・`p2pwindow.h:28`・`p2pwindow.cpp:580` |
| S6 | 仕様 10.2節の部品の表に、実在しない `p2pgameprogress`・`p2pevidencestore`・`p2pgamedialog` が載り、`p2pnegotiation`・`p2pmatch`・`p2pframes`・`p2precordstore`・`p2pknownpeers` などが無い。司令塔を「状態機械」と書くが、状態機械は `Match`・`Negotiation` | `docs/dev/p2p-game-spec.md:758-779`<br>**状態: 済（aed37b09）** |
| S7 | IPC のフレームの base64 の上限が 64 KiB を自分で計算している（`P2p::kMaxFrameBytes` を使わない） | `peeripc.cpp:132` |
| S8 | `P2pHistoryInfo::mySide` が 1・2 の数で、読み戻しも `== 2 ? 2 : 1` と書く（`kSideBlack`・`kSideWhite` を使わない） | `p2precordstore.h`・`p2precordstore.cpp:237` |
| S9 | `TimeControlTerms` から `TimeControl` への換算を、棋譜と時計のために同じ関数の中で2回書いている | `p2pgamecoordinator.cpp:221-233` |
| S10 | CMake の `SRC_P2P` の並びが崩れている（`peeripc`・`peerprocess` が ui の後） | `CMakeLists.txt:1104-1111` |
| S11 | Rust: `status_dirty` は書くだけで読まれない | `node.rs:214`・`events.rs` の8か所 |
| S12 | Rust: 使われない関数・値 | `hash::from_hex32`（`hash.rs:50`）・`peerid::ed25519_public_key`（`peerid.rs:14`）・`ConnState::Open` の tasks の2つ目（`node.rs:147`） |
| S13 | Rust: `game::split` は `stream.split()` の言い換えだけ。tokio の実行器の組み立てが2か所で同じ。16件の上限が4か所に別々に書かれている。`relay_only` を `Node` と `AddrPolicy` の両方が持つ | `game.rs:180`・`main.rs:23-33`・`service.rs:108-118`・`node.rs:308`・`events.rs:31`・`reservations.rs:18`・`addr.rs:22` |
| S14 | Rust: `sbq-protocol` のモジュールの説明の一覧に `peerid` が無い | `protocol/rust/src/lib.rs:4-8` |
| S15 | Rust: 宛先を扱うコードが `addr.rs` の外にもある（IP とポートの読み直し、`prefix_of`、招待に入れる宛先の選択、`parse_peer_id`） | `node.rs:48-70`・`events.rs:20`・`requests.rs:73-100`・`swarm.rs:140` |
| S16 | 対局記録のタブを開くたびに、全部の証跡を GUI スレッドで読んで検証する（記録は合計 1 GiB まで残る） | `p2phistorypanel.cpp:34` |

## 7. 試験の置き場所

- ジャーナルの読み戻し（`evidenceFromJournal`）は、`tst_p2p_coordinator` の `recoverJournals` の1件でしか通らない。R2 で game/p2p に移せば、書いて読み戻す試験を `tst_p2p_protocol` に置ける。
- `PeerIpc` の検査は直接の試験が無い（5.4）。
- Rust の単体試験は純粋な部分だけにある（addr 6件、ipc 5件、node 3件、reservations 3件、game 2件）。`requests.rs`・`events.rs` と招待の照合は、本物のプロセスを起動する `peer/tests/ipc.rs` でしか通らない。R7 の `node/invites.rs` は単体で試験できる。
- `frame::invite_token`（`frame.rs:121`）を確かめる試験も試験入力も無い。

## 8. 直す順

1. B1（1行で直る）
2. B2（入れ子のイベントループを消す）
3. 5.1・S6（文書の誤り）
4. R1・R2・R3（同じ規則の二重化を解く）
5. 5.2・5.3・5.4（C++ と Rust の取り決めを protocol/ と共通の試験入力にそろえる）
6. R10・R11・R12（画面の部品）
7. R5・R6（C++ の部品の切り出し。1つずつ）
8. R7・R8・R9（Rust の部品の切り出し）
9. R4・S1〜S16・7章の試験
