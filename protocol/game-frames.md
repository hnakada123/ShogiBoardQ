# 対局フレームの payload

実装仕様 5章・6章の詳細。フレームの外枠は `README.md` 2章。payload は対局・UI側（C++）だけが読み書きし、
通信プロセスは読まない。値の型は `README.md` 1章の決定的CBOR（`uint`・`bstr`・`tstr`・配列）で、
`bstr(32)` は長さ32の byte string。真偽は uint の 0/1。

## 1. 条件と状態（署名・ハッシュの対象）

```text
Rules          = ["SBQ-RULES", 1, initialSfen, 512, "fourfold-draw", "perpetual-check-loss",
                  "no-declaration", "human", "human"]
TimeControl    = ["SBQ-TIME", 1, mainMs, byoyomiMs, 0, 0]
NetworkPolicy  = ["SBQ-NET", 1, 10000, 30000, 120000, 5000, 86400000, 10000, 600000]
State          = ["SBQ-STATE", 1, gameId, ply, previousStateHash, moveUsi, sfen, spentMs,
                  blackMainMs, whiteMainMs]
Transcript     = ["SBQ-TRANSCRIPT", 1, gameId, [State, ...]]
StartAgreement = ["SBQ-START", 1, networkId, blackNonce, whiteNonce, blackPlayerKey, whitePlayerKey,
                  blackSessionKey, whiteSessionKey, blackPeerIdHash, whitePeerIdHash,
                  rulesHash, timeControlHash, networkPolicyHash, startBefore]
StateApproval  = ["SBQ-APPROVE", 1, gameId, ply, stateHash]
Control        = ["SBQ-CONTROL", 1, gameId, connectionId, sequence, kind, bodyHash]
FinalResult    = ["SBQ-RESULT", 1, gameId, finalStateHash, transcriptHash, ply, outcome, cause]
```

- v1 で受け付ける Rules と NetworkPolicy は上の1つずつだけ（initialSfen は平手）。TimeControl は
  mainMs 0〜86,400,000、byoyomiMs 0〜600,000、両方0は拒否。
- `moveUsi`・`sfen`・ラベルは ASCII の tstr。sfen は持駒の順 `RBGSNLPrbgsnlp`、手数の欄は `ply + 1`。
  State0 の moveUsi は空文字、previousStateHash はゼロ32バイト、spentMs は0、残り時間は mainMs。
- `blackMainMs`・`whiteMainMs` はその手を指した後の基本持ち時間の残り（実装仕様 7.1節）。
- `*Hash` は `H(CBOR(値))`。`gameId = H(CBOR(StartAgreement))`。PeerIdHash は PeerId のバイナリの H。
- `startBefore` は UNIX秒。申込側が作るときに今から 180 秒後にする。
- `outcome`: 1=先手勝ち、2=後手勝ち、3=引き分け、4=合意中断。`cause`: 1=投了、2=詰み、3=時間切れ、
  4=千日手、5=連続王手の千日手、6=合意引き分け、7=512手到達、8=合意中断（実装仕様 8章の表）。

署名（Ed25519、libsodium）の対象は、上の値の決定的CBORのバイト列そのもの。

| 値 | 署名する鍵 |
|---|---|
| StartAgreement・FinalResult | プレイヤー鍵 |
| StateApproval・Control | 一局用鍵 |

## 2. フレームの種類と payload

| type | 名前 | gameId | payload |
|---|---|---|---|
| 1 | HELLO（開始前） | ゼロ | `["SBQ-HELLO", 1, role, networkId, [1], peerId, connectionRandom, playerKey, sessionKey, displayName, sidePreference, offerId, coin, conditions]` |
| 1 | HELLO（再接続） | gameId | `["SBQ-RECONNECT", 1, side, connectionRandom]` |
| 2 | START_AGREEMENT | ゼロ | `[StartAgreement, Rules, TimeControl, NetworkPolicy]` |
| 3 | START_SIGNATURE | gameId | `[side, signature]` |
| 4 | STATE_PROPOSE | gameId | `[State, signature]`（着手側の StateApproval への署名） |
| 5 | STATE_ACCEPT | gameId | `[ply, stateHash, signature]`（受信側の StateApproval への署名） |
| 6 | CONTROL | gameId | `[Control, body, signature]` |
| 7 | SYNC_REQUEST | gameId | `[fromPly, previousStateHash]` |
| 8 | SYNC_CHUNK | gameId | `[[[State, blackApproval, whiteApproval], ...], final]` |
| 9 | RESULT_SIGNATURE | gameId | `[FinalResult, side, signature]` |
| 10 | REJECT | ゼロか gameId | `[errorCode]` |

- `role`: 1=申込側（招待を受けた側）、2=募集側（招待した側）。`side`: 1=先手、2=後手。
- `peerId`: 自分の PeerId のバイナリ（通信プロセスが知らせた接続の相手と一致することを相手が確かめる）。
- `connectionRandom`: bstr(32)。`connectionId = H(CBOR(["SBQ-CONN", 1, gameId, blackRandom, whiteRandom]))`。
  開始前の HELLO の乱数は、開始合意の後の最初の接続の connectionId に使う。再接続では再接続の HELLO で交換し直す。
- `playerKey`・`sessionKey`: bstr(32) の公開鍵。`displayName`: `displayname-v1.txt` の規則の tstr。
- `sidePreference`: 0=指定なし、1=先手希望、2=後手希望。
- `offerId`: 予約の項目。常にゼロ32バイトを入れ、ゼロ以外は断る（第4版までは公開募集への申込みで使った。実装仕様の「第5版の変更点」）。
- `coin`: 申込側は `commitA = H(CBOR(["SBQ-COMMIT", 1, nonceA]))`、募集側は `nonceB`。どちらも bstr(32)。
- `conditions`: 申込側は空の配列、募集側は `[Rules, TimeControl, NetworkPolicy]`。
- STATE_ACCEPT の ply・stateHash は承認する State のもの。
- SYNC_CHUNK は fromPly から連続した State を、両方の承認の署名とともに運ぶ。`final` は最後の塊なら1。1回に64件まで。
- `errorCode`: 1=UNSUPPORTED_VERSION、2=WRONG_NETWORK、3=BUSY、4=EXPIRED、5=CONDITION_MISMATCH、
  6=INVALID_SIGNATURE、7=INVALID_MOVE、8=STATE_CONFLICT、9=CLOCK_MISMATCH、10=LIMIT_EXCEEDED、
  11=STORAGE_ERROR、12=CANCELLED（利用者が取りやめた・アプリを終了した）、13=DISCONNECT_LIMIT（切断時間の合計が上限を超えた）、
  14=GAME_TIME_LIMIT（1局の実時間の上限）、15=CLOCK_DISCONTINUITY（スリープ・時計の変更で計時の連続性を確かめられない。実装仕様 7.3節）。
  12〜15 は第4版の表に足したもの。

## 3. CONTROL

`kind` と body:

| kind | 名前 | body |
|---|---|---|
| 1 | READY | `[state0Hash]` |
| 2 | RESUME | `[latestPly, latestStateHash]`（両方の承認がそろった最新の State） |
| 3 | RESULT_PROPOSE | `FinalResult` そのもの |
| 4 | ABORT | `[latestStateHash, reasonCode]`（reasonCode は2章の errorCode） |
| 5 | ERROR | `[errorCode, relatedPly]` |
| 6 | RESULT_DECLINE | `[bodyHash]`（断る RESULT_PROPOSE の bodyHash） |

`bodyHash = H(CBOR(body))`。sequence は接続ごと・向きごとに 1 から増やす。同じ接続での再送は同じバイト列。

## 4. 手順

開始前（申込側を A、募集側を B とする）:

1. A → B: HELLO（role 1、commitA）。
2. B: ブロック・先後の希望・表示名を確かめ、断るなら REJECT。受けるなら B → A: HELLO（role 2、nonceB、条件）。
3. A: B の HELLO で条件を初めて知る。先後を決め、
   StartAgreement を作って A → B: START_AGREEMENT（nonceA は StartAgreement の A の側の nonce に入る）。
4. B: StartAgreement が双方の HELLO と一致すること、commitA の開示、先後の決定、期限を確かめる。
5. 双方の利用者が条件確認画面で開始を押す。A は押したときに START_SIGNATURE を送る。B は押していて、かつ
   A の START_SIGNATURE を確かめたときに START_SIGNATURE を送る。
6. 両方の開始署名がそろったら、先手が STATE_PROPOSE（State0）、後手が STATE_ACCEPT（State0）と CONTROL READY を送る。
   仮予約は B が HELLO を返してから 60 秒、開始署名がそろってから READY までは 30 秒。

再接続:

1. 申込側の通信プロセスが相手へ接続し直す（`connectPeer`）。相手は `setGamePeer` で受け付ける。
2. 双方が HELLO（再接続）を送り、新しい connectionId を決める。
3. 双方が CONTROL RESUME を送る。署名を一局用鍵で確かめるまで、ほかのフレームを受け付けない。
4. 自分の最新の両署名付き State が相手より古い側が SYNC_REQUEST を送り、相手が SYNC_CHUNK で返す。
5. 相手の承認のない自分の State があれば、同じバイト列の STATE_PROPOSE を送り直す。
