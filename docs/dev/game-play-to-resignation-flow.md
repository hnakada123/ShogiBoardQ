# 対局開始から投了までのフロー

本ドキュメントでは、対局が開始された後の指し手の流れ（人間の着手、エンジンの着手、時計管理）から、投了などによる終局までの一連のフローを説明する。対局が始まるまでの流れは [対局ダイアログから対局開始までのフロー](game-start-flow.md) を参照。対象はローカル対局（人間同士・人間対エンジン・エンジン同士）で、CSA 通信対局は経路が分かれる所だけを記す。

先手・後手はすべて `Turn`（`Turn::Black`／`Turn::White`、`src/core/shogitypes.h`）で表す。誰が指すかは `MatchPlayers`（`src/core/matchplayers.h`）で表す。手番は盤（`ShogiBoard`）だけが持ち、`ShogiGameController::currentPlayer()` は盤の手番を返す。部品は起動時に `MainWindowComposer`（`src/app/mainwindowcomposer*.cpp`）が全部作って配線する（[composition-root.md](composition-root.md)）。対局の司令塔 `MatchCoordinator`（`src/game/matchcoordinator.cpp`）は `MainWindowComposer::createMatchAndAnalysis()` が一度だけ作り、対局ごとに使い回す。司令塔は子ハンドラもフック（コールバックの構造体）も持たない。一手の確定・終局の判定と記録・待った・中断と再開を自分で行い、アプリの動作状態（`AppActivityState` の対局中・待機）を書き、時計・評価値・終局は表示にシグナルで知らせ、棋譜の行は `MatchRecordSink`（`src/game/matchrecordsink.h`）へ書き込む。シグナルの受け先は `MainWindowComposer::connectGame()`（`src/app/mainwindowcomposer_wire.cpp`）がつなぎ、主な受け先は `GameSession`（`src/app/gamesession.cpp`）である。`GameSession` は `MatchRecordSink` の実装でもある。

盤・棋譜欄・指し手のハイライトは、司令塔が直接は動かさない。棋譜に行を書くと表示中の手（`KifuNavigationState`）が最新の手に移り、その変化を受けた部品（`BoardSyncPresenter`・`KifuDisplayPresenter` など）が自分で表示を合わせる（4.2）。

## 全体フロー図

```
対局開始（MatchCoordinator::start → beginPlay: 時計開始 → 初手がエンジンなら思考を依頼）
    │
    ▼
┌────────────────────────────────────────────────────────────┐
│ 対局ループ                                                 │
│                                                            │
│  ┌────────────────────────────────────────────────┐        │
│  │ 人間の手番                                     │        │
│  │  1. 盤のクリック（駒選択→移動先）              │        │
│  │  2. prepareMove() → applyMove()（盤面更新）    │        │
│  │  3. MatchCoordinator::onHumanMove()            │        │
│  │     → Strategy → completeMove()                │        │
│  └────────────────────────┬───────────────────────┘        │
│                           │                                │
│                           ▼                                │
│  ┌────────────────────────────────────────────────┐        │
│  │ エンジンの手番                                 │        │
│  │  1. position + go を送る（非同期）             │        │
│  │  2. bestmove 受信 → Usi::matchMoveReady        │        │
│  │  3. applyEngineMove()（合法手判定・盤面更新）  │        │
│  │     → completeMove()                           │        │
│  └────────────────────────┬───────────────────────┘        │
│                           │                                │
│                           ▼                                │
│  盤に指すと盤の手番が相手に移る（ShogiBoard::turnChanged） │
│  completeMove(): 時計の確定・棋譜追記（表示が最新の手へ）  │
│                  → 千日手 → 最大手数（持将棋）を判定       │
│  時計の tick ごとに残り時間を判定 → 時間切れ負け           │
│  対局ループに戻る                                          │
└────────────────────────────────────────────────────────────┘
    │
    │ 投了 / 中断 / 時間切れ / 千日手 / 持将棋 / 入玉宣言 / 反則
    ▼
┌────────────────────────────────────────────────────────────┐
│ 終局処理（MatchCoordinator::endGame）                      │
│  1. 時計を止め、エンジンへ gameover を送る（finish）       │
│  2. 棋譜に終局行（「▲投了」など）を書き、動作状態を待機に  │
│     戻して gameEnded を出す                                │
│  3. 知らせ方（Notice）に応じて、結果の知らせ               │
│     （gameOverMessage）、棋譜の自動保存の依頼（設定時）、  │
│     gameEndProcessed（エンジン同士の連続対局なら次局へ）、 │
│     中断なら gameInterrupted                               │
└────────────────────────────────────────────────────────────┘
```

---

## 1. 人間の着手フロー

### 1.1 盤面クリック → 駒選択

**ファイル:** `src/board/boardinteractioncontroller.cpp`

```
ユーザーが盤面をクリック
    ↓
ShogiView::clicked(QPoint)          （MainWindowComposer::connectBoard() が接続）
    ↓
BoardInteractionController::onLeftClick(pt)
```

`onLeftClick()` の処理:

1. 受け付けの確認: 盤で指した手の行き先（`BoardInputRoute`、`src/board/boardinputroute.h`）の `acceptsInput()` が false なら無視する。主画面の行き先は `BoardMoveController` で、`BoardMoveController` のコンストラクタが `setInputRoute(this)` で登録する。局面編集中は常に受け付け、それ以外は `HumanTurnPolicy::isHumanTurnNow()`（`src/ui/controllers/humanturnpolicy.cpp`）で決める。対局中は `MatchCoordinator::isHumanSide(盤の手番)`、通信対局中は人間が指していて自分の手番か、対局していないときは変化として記録できるか（`BranchTreeEditController::canRecordBoardMove()`）。解析中・詰み探索中・検討中は受け付けない。
2. 1回目のクリック（駒選択）: 持ち駒のない駒台のマスは無視する。`ShogiView::startDrag()` でドラッグを始め、`selectPieceAndHighlight()` で駒を選ぶ。選んだマスを `BoardHighlightLayer::Selection` の層に、合法な移動先（`EngineMoveValidator` で判定）を `LegalDestinations` の層に出す（メニューで表示を切り替えられる）。相手の駒台や空きマスは選ばない。`m_waitingSecondClick = true` に設定する。
3. 2回目のクリック（移動先指定）: 同じマスならキャンセルする。別のマスなら行き先の `BoardInputRoute::submit(from, to)` に渡す。成り・不成のダイアログを出している間もドラッグ状態を保つため、ここではドラッグを終えない（`ShogiGameController::endDragSignal` → `ShogiView::endDrag()` で終える。`connectBoard()` が接続）。

右クリック（`onRightClick()`）は、2回目のクリック待ちなら選択を取り消す。盤の手番が替わったとき（`ShogiBoard::turnChanged`）と動作状態が変わったとき（`AppActivityState::changed`。`BoardMoveController` が接続）も、つまんでいた駒の選択を取り消す（`cancelPendingClick()`）。

### 1.2 指し手のルーティング

```
BoardInputRoute::submit(from, to)
    ↓  （主画面の行き先は BoardMoveController）
BoardMoveController::playMove(from, to)        （src/ui/controllers/boardmovecontroller.cpp）
    ↓  対局・通信対局では ShogiGameController::prepareMove() で UsiMove にする（成り・不成もここで決まる）
BoardMoveController::playUsiMove(move)
```

`BoardMoveController::playMove()` は、アプリの動作状態（`AppActivityState::Activity`）で処理を分ける。クリックは1回だけ `UsiMove` にし、その後は `playUsiMove()` が扱う。定跡ウィンドウで選んだ手は成り・不成まで決まっているので、`JosekiWindowController` が `playUsiMove()` を直接呼ぶ（盤の座標に戻さない）。

- 局面編集中（`Activity::DuringPositionEdit`）なら `PositionEditSession::applyEditMove()` に渡して終わる。
- 対局していない待機中（`Activity::Idle`）なら、指した手を表示中の手の次の手（変化）として記録して終わる（盤の手は `BranchTreeEditController::recordBoardMove()`、定跡の手は `recordMove()`）。
- 通信対局中（`Activity::DuringCsaGame`）なら、`ShogiGameController::isLegal()` で合法手か確かめて `csaMoveRequested(move)` シグナルを発行する（→ `CsaGameCoordinator::onHumanMove(const UsiMove&)`。`CsaGameSession` が接続）。盤はサーバーの確認を受けてから変えるので、ここでは指さない。
- 対局中（`Activity::DuringGame`）で、司令塔が手を受け付け（`MatchCoordinator::canAcceptMove()`：終局しておらず棋譜に書ける）、盤の手番が人間（`isHumanSide()`）のときだけ指す。それ以外は `onMoveApplied(from, to, false)` で選択を取り消して終わる。
- `ShogiGameController::applyMove(move, MatchCoordinator::lastMoveDestination())` で指し、結果を `BoardInteractionController::onMoveApplied()` に渡して選択のハイライトを更新する（1.4）。不合法ならここで終わる。
- 合法なら、指した手（`RecordEntry`）を `MatchCoordinator::onHumanMove(played)` に渡す（2章）。考慮時間の確定・棋譜への追記・千日手と最大手数の判定・エンジンへの手番渡しは司令塔が行う。

### 1.3 指し手の検証と盤面更新

**ファイル:** `src/game/shogigamecontroller.cpp`

```cpp
std::optional<UsiMove> ShogiGameController::prepareMove(const QPoint& from, const QPoint& to);
std::optional<RecordEntry> ShogiGameController::applyMove(const UsiMove& move, BoardSquare previousTo);
```

盤のクリック（表示の座標）は `prepareMove()` が盤を変えずに `UsiMove` にする（合法手の判定と成り・不成の決定）。指すのは `applyMove()` で、指し手どおりの成り・不成が合法か（`isLegal()`）を確かめてから `commitMove()` で盤に指す。エンジンの手（2.2）・定跡の手も同じ `applyMove()` を通る。指した手は棋譜の1行（`RecordEntry`）として返し、対局の手順への追加は司令塔が行う。

#### 処理ステップ

| ステップ | 処理内容 |
|---|---|
| 入力検証 | 移動元が盤上か手番の側の駒台か、移動元に駒があるかを確認する。座標は `BoardCoordinates::usiMoveFromSquares()` で `UsiMove` にする |
| 合法手判定 | `legalStatus()` が `EngineMoveValidator::legalStatus()` で、成り・不成それぞれが合法かを調べる |
| 成り判定 | 下の「成り判定の詳細」のとおり。どちらも不合法なら `nullopt` を返し、盤は変えない |
| ドラッグ終了 | `emit endDragSignal()`（→ `ShogiView::endDrag()`） |
| 盤面更新 | `commitMove()` → `ShogiBoard::applyUsiMove(move, mover)` で駒の移動・取得を反映し、手番を相手に移す（SFEN の手数も1つ進む。手番が替わると `ShogiBoard::turnChanged` が出る。3.5） |
| 棋譜の行 | `RecordEntry::played(ShogiMove(move, moving, captured), 棋譜文字列, board()->toSfen())`。棋譜文字列は `KifMoveText::format(mover, moving, move, previousTo)` で日本語表記にする（例: `"▲７六歩(77)"`。呼び出し側が渡した直前の手の移動先 `previousTo` と同じマスなら「同」。対局では `MatchCoordinator::lastMoveDestination()`） |
| 直前の手 | `m_lastMove` を更新する（`lastMove()` で読める） |

着手確定の知らせ `moveCommitted` はここでは出さない。司令塔が一手を確定し終えてから `notifyMoveCommitted()` で出す（4.4）。

#### 成り判定の詳細

```
prepareMove() の成り・不成
    ├─ 成りだけ合法（行き所のない駒など）→ 成る
    ├─ 不成だけ合法 → 成らない
    ├─ 両方合法
    │    └─ setPromotionChooser() で設定した関数で対局者に選ばせる
    │         DialogLauncher::choosePromotion() → PromoteDialog::ask()
    └─ どちらも不合法 → nullopt
```

成りの選び方は `MainWindowComposer::createControllers()` が設定する。エンジンの手と定跡の手は成り・不成が決まった `UsiMove` で届くので、`prepareMove()` を通らず、`applyMove()` が指し手どおりの成り・不成が合法かだけを確かめる。

### 1.4 着手後のハイライト更新

**ファイル:** `src/board/boardinteractioncontroller.cpp`

```
BoardInteractionController::onMoveApplied(from, to, success)
```

- 成功時: 直前の手の移動元（`BoardHighlightLayer::LastMoveFrom`）と移動先（`LastMoveTo`）を出し、選択（`Selection`）を消す。色は層ごとに `ShogiViewHighlighting` が決める。
- 失敗時（不合法手）: 選択とドラッグ状態を取り消す。

一手を確定して棋譜に書くと（人間の手もエンジンの手も）、表示中の手がその手に移り、`BoardSyncPresenter::syncToCurrentNode()`（`src/ui/presenters/boardsyncpresenter.cpp`）が `BoardInteractionController::showMoveHighlights()` でその手の移動元・移動先を示す（4.2）。

---

## 2. 対局モード別の着手後処理

人間の着手の後処理とエンジンの着手は、対局モードごとの Strategy（`src/game/`）が手番を進め、一手の確定は司令塔が行う。

```
MatchCoordinator::onHumanMove(played)
    ↓
GameModeStrategy::onHumanMove(played)          （対局モードの Strategy）
    ↓
MatchStrategyHost::completeHumanMove(played)   （司令塔 → completeMove()、2.1）
```

Strategy は対局開始時（と再開時）に `MatchCoordinator::createStrategy()` が `MatchStartOptions::players`（`MatchPlayers`）から作り、すぐに `start()` を呼ぶ。

| 対局者 | Strategy | エンジンの枠 |
|---|---|---|
| 先手・後手ともエンジン（`engineVsEngine()`） | `EngineVsEngineStrategy` | 先手 E1、後手 E2 |
| 先手がエンジン（`blackIsEngine`） | `HumanVsEngineStrategy`（`m_engineSide = Turn::Black`、`enginePath1`） | E1 |
| 後手がエンジン（`whiteIsEngine`） | `HumanVsEngineStrategy`（`m_engineSide = Turn::White`、`enginePath2`） | E1 |
| どちらも人間 | `HumanVsHumanStrategy` | なし |

Strategy は `MatchStrategyHost`（`src/game/gamemodestrategy.h`。`MatchCoordinator` が private に実装する）を通して、エンジンの枠（`engines()`）・持ち時間の出どころ（`timeSource()`。対局時計の `ClockTimeSource`）・盤の手番（`sideToMove()`）・手数（`currentPly()`）・終局したか（`isGameOver()`）を読み、一手の確定（`completeHumanMove()`・`applyEngineMove()`）、エンジンへの思考の依頼（`requestEngineMove(side)`）、エンジンの初期化を待つ間の時計の停止（`setWaitingForEngines()`）、人間の考慮時間の記録（`recordConsiderationTime()`）を司令塔に頼む。Strategy は手番ごとに誰が指すかだけを決める。

対局で指すエンジンは、Strategy が持つ `MatchEngineSeat`（`src/game/matchengineseat.cpp`）1台ずつで扱う（`GameModeStrategy::engineSeat(side)`。司令塔の `engineFor(side)` もここから引く）。`MatchEngineSeat::start(Params)` は `EngineLifecycleManager`（`src/game/enginelifecyclemanager.cpp`）の枠（E1・E2。思考情報と通信ログの表示先）からエンジンを借り（`acquire(owner, slot, name)` が返す move-only の `EngineLease`）、指す手番と持ち時間の出どころを渡して起動する。エンジンの初期化・指し手・投了・入玉宣言勝ち・エラーは `MatchEngineSeat` のシグナル（`initialized`・`moveReady`・`resigned`・`declaredWin`・`errorOccurred`）で届く。ほかの持ち主が同じ枠を取った・`destroyEngines()` で全部の枠を破棄したとき（`engineRevoked(owner)`）も `errorOccurred` で知らせる。`EngineLifecycleManager` は合成ルートが作り、司令塔（`MatchCoordinator::Deps::engines`）・検討・詰み探索（`AnalysisSession`）・棋譜解析・通信対局が共有する。枠の部品はリース・取り上げの知らせ・表示の消去だけを受け持ち、手番や時計は知らない。人間対エンジンでは、エンジンは手番によらず E1 に置く。

エンジンへの思考依頼は非同期である。依頼した関数はすぐに戻り、指し手は `Usi::matchMoveReady(const UsiMatchMove&)` → `MatchEngineSeat::moveReady` で Strategy の `onEngineMoveReady()` に届く。人間が先に指した後でもエンジンが初期化中なら、Strategy は初期化を待つ間時計を止め（`setWaitingForEngines(true)`）、`initialized` を受けてから思考を頼む。`UsiMatchMove`（`src/engine/usimatchhandler.h`）は、指した側・指し手・考えた時間・最善の読み筋の評価値を持つ。先読みの局面は `UsiMatchHandler` だけが持ち、`UsiMatchMove` には載せない。

### 2.1 一手の確定: MatchCoordinator::completeMove()

人間の手（`completeHumanMove()`）もエンジンの手（`applyEngineMove()`）も、盤に指した後はこの関数で一手を確定する。

```
MatchCoordinator::completeMove(played, engineScore)
    ├─ 終局済みなら false
    ├─ 時計（通知は QSignalBlocker で止め、最後にまとめて知らせる）
    │      clock->commitMove(mover)                 … 考慮時間を合計に足し、秒読み・加算を適用（3.3）
    │      played.time = clock->recordTime(mover)   … 棋譜に書く消費時間
    │      clock->setConsiderationTimeMs(mover, 0)
    │      clock->setCurrentPlayer(gc->currentPlayer())   … 時計の手番を盤の手番に合わせる（3.5）
    ├─ 対局の手順に足す: m_record.entries.append(played)
    ├─ 棋譜に書く: m_sink->appendRecord(played)    … GameSession → LiveGameSession（4.2）
    │      表示中の手が最新の手に移り、盤・ハイライト・棋譜欄が合う
    ├─ clock->notifyTimeUpdated()                 → ShogiClock::timeUpdated → TimeDisplayPresenter（3.6）
    ├─ エンジンの手なら emit engineMovePlayed(mover, ply, score) → EvaluationGraphPresenter::plotEngineScore()
    ├─ gc->notifyMoveCommitted(mover, ply)        → moveCommitted（4.4）
    ├─ 千日手チェック: checkAndHandleSennichite() → 成立なら false（8.4）
    └─ 最大手数チェック: maxMoves > 0 && 手数 >= maxMoves → handleMaxMovesJishogi()、false（8.3）
```

戻り値は対局が続くか（`true`）である。手数（`currentPly()`）は対局の手順（`RecordLine`）の手の数で、途中局面から始めた対局ではそれまでの手も数える。

### 2.2 人間 vs エンジン（HvE）: HumanVsEngineStrategy

**ファイル:** `src/game/humanvsenginestrategy.cpp`

人間が指した後、エンジンに思考を依頼し、届いた指し手を司令塔に指させる。

`start()` は前の対局のエンジンを破棄し（`destroyEngines()`）、`acquire(this, 1, m_engineSide, name)` でエンジンを E1 に作ってリース（`m_engine`）を持ち、`Usi::engineInitialized` と `Usi::matchMoveReady` を自分のスロットにつないでから `startEngine()` で起動する。起動の失敗は `start()` から戻った後に `Usi::errorOccurred` で届く。

#### 人間の着手後: onHumanMove()

```
HumanVsEngineStrategy::onHumanMove(played)
    ├─ (1) 一手を確定: host.completeHumanMove(played)（2.1）→ false（終局）なら return
    └─ (2) 盤の手番がエンジンなら思考を依頼（非同期）
           m_waitingForMove = true
           host.requestEngineMove(m_engineSide)
```

`MatchCoordinator::requestEngineMove(side)` は、`engineFor(side)->requestMatchMove(m_record.usiPosition(currentPly()))` を呼ぶ。探索させる局面（`UsiPosition`）は確定済みの対局の手順から作る。局面に至った直前の手（`UsiPosition::lastMove()`）がエンジン側に届き、読み筋の「同」表記と盤の強調に使われる（エンジンに直前の手を別に知らせる呼び出しは無い）。先読みの当たり・外れは `UsiMatchHandler` が自分の持つ先読みの局面と比べて判断する。思考時間はエンジンにつないだ時間の出どころ（`MatchTimeSource`。ローカル対局は対局時計の `ClockTimeSource`）から、思考を始める時点で求める（3.4）。

#### USI通信の詳細

思考の依頼と bestmove の受け取りは `src/engine/usi.cpp`、`src/engine/usimatchhandler.cpp`、`src/engine/usiprotocolhandler.cpp` にある。

1. `Usi::requestMatchMove(position)`: エンジンが初期化中なら、終わるまで依頼を保留する（`UsiEngineSession::whenReady()`）。`UsiMatchHandler::requestMove()` を呼ぶ。
2. `UsiMatchHandler::requestMove()`: 探索する手番は `position.sideToMove()` で求める。時間の出どころが時間切れを返せば（`MatchSearchTiming::expired`）何もしない。先読み（`go ponder`）中なら、頼まれた局面が先読みの局面（`m_ponder`）と値として等しければ `ponderhit` を送り、外れなら `stop` を送って先読みの bestmove を受けてから探索し直す（どちらの場合も先読みの局面は捨てる）。そうでなければ `startAsyncSearch()` を呼ぶ。
3. `UsiMatchHandler::startAsyncSearch()`: `position.command()` と `go`（`UsiGoTimeCalculator` が求めた時間、3.4）を送り、応答待ちのタイマー（時間切れのある対局では今の手に使える残り時間）を始める。
4. bestmove 受信: `UsiProtocolHandler::handleBestMove()` が行を `UsiBestMove` にし、考慮時間（`go` または `ponderhit` からの経過）と最善の読み筋の評価値を添えて `bestMoveReceived` を出す。`UsiMatchHandler::onBestMoveReceived()` は、`resign`・`win` なら探索した手番を添えて `resigned`・`declaredWin` を出す（5.4、8.5）。通常の手なら `MatchTimeSource::finishTurn()`（ローカル対局は `ShogiClock::finishTurn()`）で手番の時間を精算し（時間切れならここで止める）、指し手・考慮時間・評価値を `UsiMatchMove` にまとめる。指し手を局面に指せれば（`UsiPosition::withMove()`）、予想手があれば予想手まで指した局面を先読みの局面にして先読み（`go ponder`）を始める。指せない手（反則）なら先読みはしない。どちらも `moveReady` を出し（`Usi::matchMoveReady` に中継）、指せない手は対局の側が反則手として扱う。

#### エンジンの指し手: onEngineMoveReady()

```
Usi::matchMoveReady(move)
    ↓
HumanVsEngineStrategy::onEngineMoveReady(move)
    ├─ (1) 待っていた手か確かめる（m_waitingForMove、move.side がエンジンの手番、終局していない）
    └─ (2) host.applyEngineMove(move)

MatchCoordinator::applyEngineMove(move)
    ├─ 終局済み・棋譜に書けないなら false
    ├─ gc->applyMove(move.move, lastMoveDestination())（合法手か確かめて指す。「同」の基準は手順の最後の手の移動先）
    │      └─ 反則手: handleEngineIllegalMove(move.side) → false（8.6）
    ├─ エンジンの考慮時間を時計に渡す: clock->setMeasuredConsiderationTime(move.side, move.elapsedMs)
    └─ completeMove(played, move.score)（2.1。評価値グラフへの追記を含む）
```

#### エンジンが先に指す場合

エンジンが先に指すとき（エンジンが先手のときと、駒落ちでエンジンが上手のとき）は、対局開始の最後に `GameSession::startGame()` が呼ぶ `MatchCoordinator::beginPlay()` が時計を動かし、Strategy の `startInitialMoveIfNeeded()` を呼ぶ。

```
MatchCoordinator::beginPlay()
    ├─ エンジンの初期化を待っていなければ clock->startClock()
    └─ HumanVsEngineStrategy::startInitialMoveIfNeeded()
           ├─ 終局済み・思考待ち・エンジンの手番でないなら return
           ├─ エンジンが初期化中なら host.setWaitingForEngines(true) で時計を止めて待つ
           │      → Usi::engineInitialized → onEngineInitialized() が
           │        setWaitingForEngines(false) で時計を再開して、もう一度呼ぶ
           └─ requestEngineMove() → host.requestEngineMove(m_engineSide)
```

指し手は同じく `onEngineMoveReady()` に届く。人間の初手は、相手エンジンの初期化中も計時を続ける。

### 2.3 人間 vs 人間（HvH）: HumanVsHumanStrategy::onHumanMove()

**ファイル:** `src/game/humanvshumanstrategy.cpp`

```cpp
void HumanVsHumanStrategy::onHumanMove(const RecordEntry& played)
{
    finishTurnTimer(played.mover);
    if (m_host.completeHumanMove(played)) armTurnTimerIfNeeded();
}
```

HvHはエンジン通信がないため、処理はシンプル:
1. 着手者の考慮時間を確定（`finishTurnTimer()`、3.2）
2. 一手を確定（2.1。秒読み/フィッシャー加算の適用、棋譜への追記、千日手・最大手数の判定）
3. 対局が続くなら、次の手番のタイマーを開始

`start()` はログを出すだけである。

### 2.4 エンジン vs エンジン（EvE）: EngineVsEngineStrategy

**ファイル:** `src/game/enginevsenginestrategy.cpp`

EvEの対局は、エンジンの指し手を受けるたびに `QTimer::singleShot(0, ...)` で次の手番を予約するループで進行する。

```
EngineVsEngineStrategy::start()
    ├─ 前のエンジンを破棄し、acquire(this, 1, Black) と acquire(this, 2, White) で作る（リースは m_black・m_white）
    ├─ 両エンジンの engineInitialized → kickNextTurn、matchMoveReady → onEngineMoveReady を接続
    ├─ startEngine() で両エンジンを起動
    └─ kickNextTurn()（エンジンはまだ初期化中なので、待つ状態にして戻る）

kickNextTurn()            ← beginPlay() の startInitialMoveIfNeeded() からも呼ばれる
    ├─ (1) 思考待ち・終局済み・エンジンがそろっていないなら何もしない
    ├─ (2) どちらかのエンジンが初期化中なら setWaitingForEngines(true) で時計を止めて待つ
    │      （engineInitialized で再び呼ばれ、setWaitingForEngines(false) で時計を再開する）
    └─ (3) 手番のエンジンに思考を依頼（非同期）
           m_waitingForMove = true
           host.requestEngineMove(host.sideToMove())

onEngineMoveReady(move)   ← Usi::matchMoveReady
    ├─ (1) 思考待ちで、終局しておらず、move.side が盤の手番か確かめる
    ├─ (2) host.applyEngineMove(move)（2.2）
    │      └─ false（反則手・千日手・最大手数・終局）なら return
    └─ (3) 次のターンをスケジュール
           QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextTurn)
```

`QTimer::singleShot(0, ...)` により、Qtのイベントループに制御を返してから次のターンを始める。思考の依頼も非同期なので、エンジンが考えている間も UI は止まらない。探索させる局面（`UsiPosition`）は人間対エンジンと同じく、司令塔が対局の手順から作る。相手エンジンの読み筋の「同」の基準も、その局面の直前の手（`lastMove()`）から決まる。時計は `setWaitingForEngines()` が止め・動かすが、`beginPlay()` より前（対局の準備中）は動かさない。

---

## 3. 時計管理

**ファイル:** `src/core/shogiclock.cpp`

`ShogiClock` は先手・後手の状態（残り時間・考慮時間・総考慮時間・秒読みに入っているか）を `SideState` にまとめて持ち、API はすべて `Turn` で受ける。50ms ごとの tick（`updateClock()`）で、時計の手番の側の残り時間と考慮時間を進める。時計は合成ルート（`MainWindowComposer::createBoard()`）が作り、司令塔（`MatchCoordinator::Deps::clock`）・`GameSession`・`TimeDisplayPresenter` に渡す。司令塔はこの時計を読む時間の出どころ `ClockTimeSource`（`src/engine/matchtimesource.h`）を持ち（`MatchStrategyHost::timeSource()`）、`MatchEngineSeat::start()` が対局のエンジンに `Usi::setMatchTimeSource()` でつなぐ。対局を始めるときは `GameSession::startGame()` が `TimeControlUtil::applyToClock()` で持ち時間の決まりと開始局面の手番を時計に入れ、棋譜の文書（`KifuDocument::beginMatch()`）が `MatchTimeRecord`（`src/kifu/matchtimerecord.h`。対局情報と棋譜に書く持ち時間と開始・終了の日時）に記録する。持ち時間の決まりと表示の仕様は [時間設定の仕様](game-time-control-spec.md) を参照。

### 3.1 時間計測の仕組み

対局モードごとに異なる方法で考慮時間を計測する。

| モード | 計測方法 | 計測開始 | 計測終了 |
|---|---|---|---|
| HvH | `QElapsedTimer`（`HumanVsHumanStrategy::m_turnTimer`） | `armTurnTimerIfNeeded()` | `finishTurnTimer()` → `MatchStrategyHost::recordConsiderationTime()` → `ShogiClock::setMeasuredConsiderationTime()` |
| HvE（人間） | 時計自身の計時（tick で時計の手番の側の考慮時間に足す） | 時計の手番が人間に移ったとき（`ShogiClock::setCurrentPlayer()`） | 時計の手番がエンジンに移ったとき |
| HvE（エンジン） | `UsiMatchMove::elapsedMs`（`UsiProtocolHandler` が測る） | go（または ponderhit）送信時 | bestmove受信時 → `MatchCoordinator::applyEngineMove()` が `setMeasuredConsiderationTime()` |
| EvE | `UsiMatchMove::elapsedMs`（`UsiProtocolHandler` が測る） | go（または ponderhit）送信時 | bestmove受信時 → `MatchCoordinator::applyEngineMove()` が `setMeasuredConsiderationTime()` |

人間同士でも、対局開始時には `m_turnTimer` を動かさないので、初手は時計自身が数えた考慮時間を使う。`setMeasuredConsiderationTime()` は、中断した対局を再開した手番では、中断前に使っていた考慮時間を足して確定する。

### 3.2 タイマー管理メソッド

`GameModeStrategy` の `armTurnTimerIfNeeded()`・`disarmTurnTimer()` は既定では何もせず、人間同士の Strategy だけが実装する。人間対エンジンの人間の考慮時間は時計自身が数えるので、Strategy は計測しない。

司令塔は終局処理（`endGame()`）で private の `MatchCoordinator::disarmTurnTimer()` を呼んで計測を打ち切る。待った（`undoTwoPlies()`）では打ち切ってから測り直し、中断した対局の再開（`resumeInterruptedGame()`）では計測を始める。

#### HvH用

```cpp
void HumanVsHumanStrategy::armTurnTimerIfNeeded()
{
    if (m_turnTimerArmed) return;
    m_turnTimer.start();
    m_turnTimerArmed = true;
}

void HumanVsHumanStrategy::disarmTurnTimer()
{
    // 「待った」や終局で計測を打ち切る。次の armTurnTimerIfNeeded() で測り直す。
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}

void HumanVsHumanStrategy::finishTurnTimer(Turn mover)
{
    if (!m_turnTimerArmed) return;
    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;
    m_host.recordConsiderationTime(mover, ms);
    disarmTurnTimer();
}
```

### 3.3 秒読み/フィッシャー加算の適用

各着手後に、司令塔の `completeMove()` が指した側の `Turn` で `ShogiClock::commitMove()` を呼ぶ:

```cpp
void ShogiClock::commitMove(Turn player)
{
    SideState& s = side(player);
    // 累積へ加算した現在手の時間を、時計表示で二重に数えない。
    if (m_currentPlayer == player) m_currentConsiderationCommitted = true;
    s.totalConsiderationMs += s.considerationMs;
    updateShownConsideration(player);

    // 終局後は秒読み/加算は行わず、表示更新のみ
    if (m_gameOver) {
        emit timeUpdated();
        return;
    }

    const TimeControlSide& r = rules(player);
    if (r.byoyomiMs > 0) {
        // 残り0以下 or 既に秒読み中なら秒読み時間に戻す
        if (s.remainingMs <= 0 || s.byoyomiApplied) {
            s.remainingMs = r.byoyomiMs;
            s.byoyomiApplied = true;
        }
    } else if (r.incrementMs > 0 && s.remainingMs > 0) {
        // 次の手番の分の加算（使い切った後は加算しない）
        s.remainingMs += r.incrementMs;
    }

    // 着手を確定した状態を、次の手番の開始状態として「待った」用に残す。
    pushHistory();
    emit timeUpdated();
}
```

棋譜の行に書く消費時間は、確定した後に `recordTime(player)`（`RecordTime`）で読む。

### 3.4 USI goコマンドの時間パラメータ計算

**ファイル:** `src/engine/usigotime.cpp` - `UsiGoTimeCalculator::compute()`

対局の種類（人間対エンジン、エンジン同士、CSA通信対局）によらず、この計算だけで `go` の時間を求める。入力（`UsiGoTimeInput`）は時間の決まり（`TimeControl`）と先後の残り時間である。エンジンとの対局のやり取り（`UsiMatchHandler`）は、エンジンにつないだ時間の出どころ `MatchTimeSource`（`src/engine/matchtimesource.h`）だけから時間を読む。思考を始める時点で `snapshot(side, pondering)` が `MatchSearchTiming`（`input`・応答を待つ期限 `timeoutMs`・時間切れ `expired`）を返す。人間対エンジン・エンジン同士では `ClockTimeSource` が対局時計（3章の冒頭）の残り時間を読む。CSA通信対局では `CsaGameCoordinator` 自身が `MatchTimeSource` を実装し、`CsaGameProgress::engineTiming(margin)` のサーバーの残り時間（`UsiTimingParams`）から返す（表示用の時計は読まない）。時計のない場合とテストは `FixedTimeSource` が決まった値を返す。

```
UsiGoTime compute() の返り値:
    btime      = 先手の残り時間（ms）
    wtime      = 後手の残り時間（ms）
    byoyomi    = 秒読み（ms）
    binc       = 先手フィッシャー加算（ms）
    winc       = 後手フィッシャー加算（ms）
    useByoyomi = byoyomi を送るか（false なら binc/winc を送る）
```

**秒読みモード**の場合:
- 秒読み期間に入っている側の `btime`/`wtime` は0になる（時計の持ち時間の残りが0のため）
- `byoyomi` には、手番側の秒読みの残り（stop 待ちや局面の準備で使った分を引いた値）を使う

**フィッシャーモード**の場合:
- `btime`/`wtime` には、時計の残り時間から次に使う加算分を引いた値を設定する（将棋所の USI 仕様どおり、`btime + binc` が今回の手に使える時間）。先読み（`go ponder`）では自分の加算がまだ適用されていないので、自分の残り時間はそのまま送る
- `binc`/`winc` に加算時間を設定

どちらの方式でも、手番側の予算から余裕（最大250ms）を引く。秒読みから先に引き、加算方式で `btime` が足りなければ手番側の `binc` を減らす（詳細は [時間設定の仕様](game-time-control-spec.md) の「エンジンへの時間指定」）。

### 3.5 盤の手番と時計の手番

手番の正本は盤（`ShogiBoard`）である。盤に指すと（`ShogiBoard::applyUsiMove()`）手番が相手に移り、`ShogiBoard::turnChanged` が出る。手番の表示は盤の手番をそのまま示す。

```
ShogiBoard::applyUsiMove(move, mover)        （ShogiGameController::commitMove() の中）
    ↓ advanceTurn(): 手番を相手に移す
ShogiBoard::turnChanged(Turn)
    ├─ BoardChromeWidget::setActiveSide(Turn)             … 対局者名の手番の印（ShogiView::setBoard() が接続）
    └─ BoardInteractionController::cancelPendingClick()  … つまんでいた駒の選択を取り消す
```

時計は、どちらの側の時間を減らすかを自分で持つ（`ShogiClock` の手番）。司令塔の `completeMove()` は一手を確定するときに `ShogiClock::setCurrentPlayer(gc->currentPlayer())` で時計の手番を盤の手番に合わせる（2.1）。対局の開始時は `TimeControlUtil::applyToClock()` が開始局面の手番を、中断した対局の再開時は `ShogiClock::restoreSnapshot()` が中断したときの手番を時計に入れる。`ShogiClock::setCurrentPlayer()` は、前回の tick からの経過時間を前の手番の側に精算してから手番を切り替え、新しい手番の側の考慮時間を0から数え直す（同じ手番なら何もしない）。

エンジンの bestmove を受け取ったときは、`UsiMatchHandler::onBestMoveReceived()` が `MatchTimeSource::finishTurn()`（`ClockTimeSource` は `ShogiClock::finishTurn()`）で時計を精算し、手番が切り替わるまでエンジンの側の時間を減らさない。

### 3.6 時計のUI更新

```
ShogiClock::timeUpdated()
    ↓  （TimeDisplayPresenter のコンストラクタが接続。司令塔は中継しない）
TimeDisplayPresenter::showClock()            … 時計の今の残り時間と手番を読む
    ↓
TimeDisplayPresenter::showTimes(blackMs, whiteMs, sideToMove)
```

`ShogiClock::timeUpdated()` は、tick で表示する秒が変わったときと、`commitMove()`・`startClock()`・`stopClock()` などで状態が変わったときに出る。司令塔の `completeMove()` は時計の通知を止めてまとめて値を変えた後、`ShogiClock::notifyTimeUpdated()` で今の値の `timeUpdated` を1回出す（2.1）。CSA通信対局でも、サーバーの残り時間を写した同じ時計を同じ経路で表示する。

`TimeDisplayPresenter::showTimes()`（`src/ui/presenters/timedisplaypresenter.cpp`）の処理:
- 先手・後手の時計の文字列を盤の対局者欄に設定する（`ShogiView::chrome()->card(Turn)` で取る側ごとの `PlayerCard::setClockText()`）。制限ありは残り時間 `HH:MM:SS`（秒は切り上げ）、無制限は「消費 HH:MM:SS」
- 手番の側の残り時間と秒読みから警告の段階を決め、`BoardChromeWidget::setUrgency()` に設定する。警告の段階は時計から決める。手番の印は盤の手番が決める（3.5）ので、ここでは変えない

---

## 4. 棋譜記録

### 4.1 対局の手順（RecordLine）

司令塔は対局の手順を `RecordLine m_record`（`src/kifu/recordline.h`。`MatchCoordinator::record()` で読める）として自分で持つ。開始局面（`startSfen`）と、指した手の行（`RecordEntry`）の並びで、各行はその手の後の局面の SFEN を持つ。終局の行は含めない。

```
startSfen  "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"  (開始局面)
entries[0] "▲７六歩(77)"  sfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"
entries[1] ...
```

- `MatchCoordinator::start()` が `MatchStartOptions::recordLine` の `currentPly` までを手順にする（後続の手は捨てる）。
- `completeMove()` が確定した手を末尾に足す。行の SFEN は `ShogiGameController::commitMove()` が指した後の盤から作る（`ShogiBoard::applyUsiMove()` が SFEN の手数を1つ進める）。
- エンジンに探索させる局面は `m_record.usiPosition(currentPly())`、次の手の「同」表記の基準は `lastMoveDestination()`（`m_record.destinationOf(currentPly())`）、千日手の判定は `m_record.sfens()` から作る。待った（`undoTwoPlies()`）は末尾の2手を取り除き、両方のエンジンに `Usi::forgetPonder()` で先読みの局面を捨てさせる。

対局の設定・対局者・最大手数・終局状態は `MatchSessionState`（`src/game/matchsessionstate.h`）が持つ。先読みの局面はエンジンの `UsiMatchHandler` だけが持つ。

### 4.2 棋譜行の追記

司令塔は確定した行を `MatchRecordSink` に書き込む。受け手は `GameSession`（`MainWindowComposer::createSessions()` が `MatchCoordinator::setRecordSink()` で設定）で、ライブ対局のセッション（`LiveGameSession`、`src/kifu/livegamesession.cpp`）を通して分岐ツリーに追記する。追記すると表示中の手（`KifuNavigationState`）が足した行に移り、`KifuNavigationState::changed` を受けた部品がそれぞれ表示を合わせる。

```
MatchCoordinator::completeMove() / appendTerminalRecord()
    ↓ m_sink->appendRecord(entry)
GameSession::appendRecord(entry)                 （src/app/gamesession.cpp）
    ├─ 棋譜の途中から続けた対局では、消費時間の累計を続ける（LiveGameSession::continuedTime()）
    │   （未保存の印は付けない。ツリーにノードを足すと revision が進み、KifuDocument::isModified() が true になる）
    └─ LiveGameSession::appendRecord(entry)
           ├─ 分岐ツリーに子ノードを足す（同じ手の子があれば使い回す）
           └─ 表示中の手を足したノードに移す（KifuNavigationState::setCurrentNode()）
                 ↓ KifuNavigationState::changed
                 ├─ KifuDisplayPresenter::refresh()          … 棋譜欄・分岐候補欄・分岐ツリーに足した行を出し、カーソルを合わせる
                 ├─ BoardSyncPresenter::syncToCurrentNode()  … 盤は対局の盤がもう指しているので入れ替えず、指し手をハイライトする
                 ├─ EvaluationGraphPresenter::showCurrentPly()
                 └─ JosekiWindowController::updateJosekiWindow() など
```

終局行を書いた後は `LiveGameSession::canAddMove()` が false になり、`GameSession::canAppendRecord()` も false を返す。司令塔は棋譜に書けないときは手を指さない（`canAcceptMove()`・`applyEngineMove()`）。通信対局の行も、`CsaGameSession`（`src/ui/coordinators/csagamesession.cpp`）が同じ `MatchRecordSink`（`GameSession`）に書く。

### 4.3 対局の状態の持ち主

| データ | 持ち主 |
|---|---|
| 対局の手順（開始局面・指した手・各手の後の局面） | `MatchCoordinator::m_record`（`RecordLine`） |
| 対局の設定・対局者・最大手数・終局状態・先読みの局面 | `MatchSessionState` |
| 盤と手番 | `ShogiBoard`（`ShogiGameController` が持つ） |
| 残り時間・考慮時間・時計の手番 | `ShogiClock` |
| 対局者・持ち時間と対局の開始・終了日時（対局情報・棋譜用） | `KifuDocument`（`players()`・`timeRecord()`。`MatchTimeRecord`） |
| 中断した対局（再開用） | `MatchCoordinator::m_interruptedGame`（`InterruptedGameState`） |
| 棋譜の正本（分岐ツリー） | `KifuBranchTree`（`LiveGameSession` が対局の手を足す） |
| 表示中の手 | `KifuNavigationState` |

対局の手順は分岐ツリーの写しではない。対局の開始時に、表示中の手順（`KifuNavigationState::currentRecordLine()`）から作る（[対局開始フロー](game-start-flow.md)）。

### 4.4 着手確定の通知（moveCommitted）

`completeMove()` は棋譜に書いて表示を合わせた後に `ShogiGameController::notifyMoveCommitted(mover, ply)` を呼ぶ。

```
ShogiGameController::moveCommitted(mover, ply)
    └─ PieceSoundPlayer::playMoveSound()   … 駒音（MainWindowComposer::connectBoard() が接続）
```

定跡ウィンドウは、表示中の手の変化（`KifuNavigationState::changed`）で `JosekiWindowController::updateJosekiWindow()` が更新する（4.2）。通信対局では `CsaGameCoordinator` がサーバーの確認を受けた手で `notifyMoveCommitted()` を呼ぶ。評価値グラフは、エンジンの手のたびに司令塔が出す `engineMovePlayed(side, ply, score)` を `EvaluationGraphPresenter::plotEngineScore()` が受けて、その手の評価値（`UsiMatchMove::score`。指した側から見た最善の読み筋の評価値）を足す（人間対エンジン・エンジン同士で同じ）。

---

## 5. 投了フロー

### 5.1 投了の発動

**ファイル:** `src/app/menuactionswiring.cpp`

```cpp
QObject::connect(ui->actionResign, &QAction::triggered, session, &GameSession::resign);
```

ユーザーがメニュー「対局」→「投了」をクリックすると発動する。確認ダイアログは表示されない（即座に投了処理が実行される）。投了のメニューが有効になるのは、人間が参加する対局（人間同士・人間対エンジン）の対局中と、CSA 通信対局中だけである（`UiStatePolicyManager::effectivePolicy()`。ローカル対局で人間が参加するかは `MainWindowComposer::createSessions()` が渡す `localGameHasHuman`）。

### 5.2 投了のルーティング

```
GameSession::resign()                           （src/app/gamesession.cpp）
    ↓ activeChannel()->resign()                  … 対局の種類で分岐しない（GameChannel、src/game/gamechannel.h）
    ├─ CSA通信対局中（DuringCsaGame）: CsaGameChannel::resign() → CsaGameCoordinator::onResign()
    └─ ローカル対局中（DuringGame）:  LocalGameChannel::resign()
           ↓
       MatchCoordinator::handleResign()          （src/game/matchcoordinator.cpp）
```

対局していなければ `activeChannel()` は nullptr で、何もしない。中断（`breakOff()` → `GameChannel::breakOff()`）・すぐ指させる（`makeEngineMoveNow()` → `GameChannel::moveNow()`）・入玉宣言（8.5）も同じ窓口を通る。

### 5.3 MatchCoordinator::handleResign()

**ファイル:** `src/game/matchcoordinator.cpp`

投了処理の中核メソッド。

```cpp
void MatchCoordinator::handleResign()
{
    if (isGameOver()) return;
    const Turn loser = m_gc->currentPlayer();
    endGame({Cause::Resignation, loser}, loser, Notice::Announce);
}
```

#### 処理ステップ

1. 終局済みチェック: 終局済みなら何もしない。
2. 敗者の決定: 投了の敗者は盤の手番の側（`gc->currentPlayer()`）。
3. 終局: `endGame(info, loser, Notice::Announce)`（6.1）。時計の停止、エンジンへの終了通知（下表）、終局行の追記、待機への切り替え、`gameEnded`、結果の知らせまで行う。

#### エンジンへの終了通知

**ファイル:** `src/game/matchcoordinator.cpp`、`src/game/matchengineseat.cpp`

すべての終局処理（`endGame()`）は、Strategy の `engineSeat(side)` ごとに `MatchEngineSeat::finish(GameOverResult)` を呼ぶ。結果は枠ではなく、エンジンが指している手番で決める。

| 引数 | 通知内容 |
|---|---|
| 負けた側（`Turn`） | 負けた側を指すエンジンに `"gameover lose"`、相手を指すエンジンに `"gameover win"` |
| `nullopt`（引き分け・中断） | すべてのエンジンに `"gameover draw"` |

人間同士ではエンジンが無いので何も送らない。人間対エンジンではエンジンは E1 にあり、その手番で勝ち負けを決める。`finish()` はまずエンジンの知らせの接続を外し（終局の後に届く投了の bestmove などを受けない）、エンジンの今の処理を取り消し（`Usi::cancelCurrentOperation()`。遅れて届いた bestmove で着手・先読みを再開させない）、`stop` と `gameover` を送ったあと `Usi::quit(false)` で `quit` を送ってプロセスを終了させる（応答しなければ強制終了する。思考情報の表示は残す）。通信対局（`CsaGameCoordinator`）の終局も同じ `finish()` を使う。

### 5.4 エンジン投了: handleEngineResign(Turn side)

**ファイル:** `src/engine/usimatchhandler.cpp`、`src/game/matchengineseat.cpp`、`src/game/matchcoordinator.cpp`

エンジンが `bestmove resign` を返した場合の処理。

```
USIエンジンが "bestmove resign" を送信
    ↓
UsiProtocolHandler::handleBestMove() が UsiBestMove（special = Resign）にして bestMoveReceived を出す
    （取り消した探索に遅れて届いた結果は知らせない）
    ↓
UsiMatchHandler::onBestMoveReceived()
    （手番の探索の結果だけを扱う。外れた先読みの結果は使わない）
    ↓  emit resigned(探索した手番)   … 時計を精算する前に知らせる
Usi::resigned(side)
    ↓  （MatchEngineSeat::start() が接続）
MatchEngineSeat::resigned(side)
    ↓  （MatchCoordinator が Strategy を作ったときに engineSeat(side) ごとに接続）
MatchCoordinator::handleEngineResign(side)   ← side はエンジンの手番（Turn）
    └─ endGame({Resignation, side}, side, Notice::Announce)
           投了したエンジンに "gameover lose"（エンジン同士なら相手に "gameover win"）、どちらも quit
```

- 手番は `UsiMatchHandler` が探索した局面（`position`）から求める。人間対エンジンではエンジンが後手でも E1 に入るが、知らせはエンジンの手番で出る。
- 入玉宣言勝ちの `bestmove win` も同じく `UsiMatchHandler` の `declaredWin(side)` → `Usi::declaredWin` → `MatchEngineSeat::declaredWin(side)` → `MatchCoordinator::handleEngineWin(side)` に届く（8.5）。
- 中断の後に届く `bestmove resign` は、`endGame()` の `MatchEngineSeat::finish()` が接続を外した後なので受けない（8.2）。

`resign`・`win` の bestmove は `Usi::matchMoveReady` では知らせない（`UsiMatchHandler::onBestMoveReceived()` が特殊な指し手を先に処理する）。

---

## 6. 終局状態管理

### 6.1 endGame()

**ファイル:** `src/game/matchcoordinator.cpp`

終局の入口で、すべての終局処理（中断を含む）が呼ぶ。

```cpp
void MatchCoordinator::endGame(const GameEndInfo& info, std::optional<Turn> engineLoser, Notice notice)
{
    if (!m_session.finish(info)) {
        qCDebug(lcGame) << "endGame() ignored: already over";
        return;
    }
    // 時計を止める。終局の行の時間を確定しても、秒読み・加算はしない
    m_clock->markGameOver();
    m_clock->stopClock();
    disarmTurnTimer();
    // 対局のエンジンに終局を知らせて終える（負けた側・勝った側・引き分け）
    for (const Turn side : {Turn::Black, Turn::White}) {
        MatchEngineSeat* seat = m_strategy ? m_strategy->engineSeat(side) : nullptr;
        if (!seat) continue;
        seat->finish(!engineLoser ? GameOverResult::Draw
                                  : (*engineLoser == side ? GameOverResult::Lose : GameOverResult::Win));
    }
    appendTerminalRecord(info);
    m_activityLease.reset();
    // 終局の行を棋譜に書いた後で、時刻と UI を更新させる
    emit gameEnded(info);

    switch (notice) {
    case Notice::Announce:
        emit gameOverMessage(tr("対局終了"), resultMessage(info));
        [[fallthrough]];
    case Notice::Declaration:
        // 入玉宣言は宣言の結果ダイアログ（人間）か handleEngineWin（エンジン）で知らせる。
        // 棋譜の自動保存は終局の受け手（gameEnded）が行う
        emit gameEndProcessed(info);
        break;
    case Notice::Interrupt:
        if (m_interruptedGame) emit gameInterrupted();
        break;
    }
}
```

`engineLoser` はエンジンに負けと知らせる側で、引き分け・中断は `nullopt` にする（5.3 の表）。`notice` は終局の後の知らせ方である。

| `Notice` | 使う終局 | 終局後に出すシグナル |
|---|---|---|
| `Announce` | 投了・時間切れ・反則手・千日手・連続王手の千日手・最大手数 | `gameOverMessage` → `gameEndProcessed` |
| `Declaration` | 入玉宣言（人間・エンジン） | `gameEndProcessed` |
| `Interrupt` | 中断（利用者の操作・エンジンのエラー） | 再開用の状態があれば `gameInterrupted` |

`MatchSessionState::finish()` は終局済みなら `false` を返し、そうでなければ `isOver`・`hasLast`・`lastInfo`・`when` を設定する。終局状態はこのクラスの操作（`finish()`・`markTerminalRecorded()`・`clearGameOver()`）でだけ変える。

終局の型は `src/game/matchtypes.h` にある（`MatchCoordinator::Cause`・`GameEndInfo`・`GameOverState` はその using エイリアス）:

```cpp
struct MatchGameEndInfo {
    MatchCause  cause = MatchCause::Resignation;  ///< 終局原因
    Turn        loser = Turn::Black;              ///< 敗者
};

struct MatchGameOverState {
    bool             isOver        = false;  ///< 終局済みか
    bool             moveAppended  = false;  ///< 棋譜に終局手を追記済みか
    bool             hasLast       = false;  ///< 直近の終局情報があるか
    MatchGameEndInfo lastInfo;               ///< 直近の終局情報
    QDateTime        when;                   ///< 終局日時
};
```

敗者は `lastInfo.loser`（`Turn`）で持つ。

#### 発行されるシグナル

受信先は `MainWindowComposer::connectGame()` がつなぐ。発行の順は表の上から下である。動作状態を待機にしたこと（`AppActivityState::changed`）は、`gameEnded` より前に `UiStatePolicyManager` に届く（7.2）。

| シグナル | 受信先 | 処理内容 |
|---|---|---|
| `gameEnded` | `GameSession::onGameEnded()` | `finishMatchRecord()`: ライブ対局セッションの確定・終局時刻の記録・棋譜の自動保存（設定時）。中断も確定する（再開は中断の行を除いて記録し直す）が保存はせず、連続対局の設定を戻す（7.3） |
| `gameOverMessage` | `NotificationPresenter::showInformation()` | 結果のダイアログ（`QMessageBox::information`） |
| `gameEndProcessed` | `GameSession::onGameEndProcessed()` | エンジン同士の連続対局なら次局へ（7.4） |
| `gameInterrupted` | `GameSession::onGameInterrupted()` | 中断の行を `attachInterruptedRecord()` で中断した対局に結びつけ、「対局再開」を使えるようにする（8.2） |

### 6.2 結果の知らせ: resultMessage()

**ファイル:** `src/game/matchcoordinator.cpp`

`Notice::Announce` の終局では、`endGame()` が `resultMessage()` の文で `gameOverMessage(tr("対局終了"), …)` を出す。中断（8.2）は結果を知らせず、自動保存もしない。入玉宣言（8.5）は `gameEndProcessed` だけを出し、結果は宣言の判定結果のダイアログで知らせる。自動保存はどの終局でも `gameEnded` を受けた `GameSession::finishMatchRecord()` が行う（中断を除く）。

駒落ちで対局情報の見出しが下手・上手のとき（`usesHandicapNames()`。司令塔が `MatchCoordinator::Deps::document` の `KifuDocument::usesHandicapNames()` を読む）は、先手・後手の代わりに下手・上手と呼ぶ。

| 終局原因 | メッセージ |
|---|---|
| `Resignation` | 「%1の投了。%2の勝ちです。」（%1 は敗者、%2 は勝者） |
| `Timeout` | 「%1の時間切れ。%2の勝ちです。」 |
| `Jishogi` | 「最大手数に達しました。持将棋です。」 |
| `IllegalMove` | 「%1の反則負け。%2の勝ちです。」 |
| `Sennichite` | 「千日手が成立しました。」 |
| `OuteSennichite` | 「%1の連続王手の千日手。%2の勝ちです。」 |
| `NyugyokuWin`・`BreakOff` | （`Notice::Announce` では使わない。既定の文は「対局が終了しました。」） |

### 6.3 棋譜への終局手追記: appendTerminalRecord()

棋譜欄に終局手（「投了」「時間切れ」等）を1行だけ追記する。`endGame()` が呼ぶ。

#### 終局手の棋譜表記

終局行は `RecordEntry::ending(terminal, mover, time)` で作り、表記は印（▲/△）と終局語の表（`TerminalTypes::label()`、`src/core/terminaltype.cpp`）から作る。印の側（`mover`）は `RecordEntry::endingSide()` が決める。

| 終局原因 | `TerminalType` | 棋譜表記例 | 印（▲/△）の側 |
|---|---|---|---|
| 投了 (Resignation) | `Resign` | `"▲投了"` / `"△投了"` | 敗者 |
| 時間切れ (Timeout) | `Timeout` | `"▲時間切れ"` / `"△時間切れ"` | 敗者 |
| 反則負け (IllegalMove) | `IllegalLoss` | `"▲反則負け"` / `"△反則負け"` | 敗者 |
| 中断 (BreakOff) | `Interrupt` | `"▲中断"` / `"△中断"` | 中断したときの手番の側 |
| 入玉勝ち (NyugyokuWin) | `DeclarationWin` | `"▲入玉勝ち"` / `"△入玉勝ち"` | 勝者（宣言した側） |
| 持将棋 (Jishogi) | `Impasse` | `"▲持将棋"` / `"△持将棋"` | 終局した局面の手番の側 |
| 千日手 (Sennichite) | `Repetition` | `"▲千日手"` / `"△千日手"` | 終局した局面の手番の側 |
| 連続王手の千日手 (OuteSennichite) | `IllegalLoss`／`IllegalWin` | `"▲反則負け"` / `"△反則勝ち"` など | 終局した局面の手番の側。王手を続けた側が手番なら「反則負け」、相手が手番なら「反則勝ち」（`RecordEntry::perpetualCheckTerminal(loser, sideToMove)`。CSA通信対局の `CsaGameProgress::endRecord()` と同じ規則） |

`▲` = 先手、`△` = 後手。

#### 二重追記防止と消費時間

```cpp
void MatchCoordinator::appendTerminalRecord(const GameEndInfo& info)
{
    const Turn sideToMove = m_gc->currentPlayer();
    TerminalType terminal = TerminalType::None;
    // ... info.cause から TerminalType を決める ...

    // 終局行の印と消費時間は、その行（投了・宣言など）を行った側のもの
    const Turn mover = RecordEntry::endingSide(terminal, sideToMove, info.loser);
    m_clock->commitMove(mover);
    const RecordEntry entry = RecordEntry::ending(terminal, mover, m_clock->recordTime(mover));
    if (m_sink && m_sink->canAppendRecord()) m_sink->appendRecord(entry);
    m_session.markTerminalRecorded();
}
```

`endGame()` は終局済みなら何もしないので、終局行は1対局で1回だけ書く。`MatchSessionState::markTerminalRecorded()` が `GameOverState::moveAppended` を `true` にする。

終局行の考慮時間は、その行を行った側（入玉宣言勝ちでは宣言した勝者、千日手・持将棋・中断・連続王手の千日手では手番の側、それ以外は敗者）のものである。時計は `endGame()` が終局状態にして止めてあるので、`commitMove()` は秒読み・加算をせずにその側の考慮時間を確定し、`recordTime()` を棋譜の行に書く。

---

## 7. 終局後のUI状態遷移

`endGame()` の処理と、そのシグナルは次の順に表示へ届く。

### 7.1 表示中の手（終局行）

`appendTerminalRecord()` が終局行を書くと、表示中の手が終局行に移る（4.2）。`KifuNavigationState::changed` を受けて、`KifuDisplayPresenter` が棋譜欄のカーソルを最終行（終局行）に合わせ、`BoardSyncPresenter` が盤のハイライトを消す（終局行は盤を変える手ではないので `BoardInteractionController::clearAllHighlights()`）。盤の局面は終局した局面のまま変わらない。

### 7.2 動作状態を待機に戻す

`endGame()` は終局行を書いた後で `AppActivityState` を `Idle` にする。`UiStatePolicyManager::refreshActivity()` が `AppActivityState::changed` を受けて、メニュー・ツールバー・棋譜欄の移動ボタン・分岐ツリーのクリック・盤のクリックを待機の状態にする。待機中に盤で指した手は棋譜の変化として記録する（1.2）。時計は `endGame()` が止めている。

### 7.3 GameSession::onGameEnded()

**ファイル:** `src/app/gamesession.cpp`

```
GameSession::onGameEnded(info)
    ├─ 中断なら連続対局の設定を戻す（ConsecutiveGameSeries::reset()）
    └─ finishMatchRecord(autoSave = 中断でない)   … 通信対局の終局（onCsaGameEnded）も autoSave = true で同じ関数を通る
           ├─ (1) commit なら、ライブ対局のセッションを確定する（LiveGameSession::commit()）
           │      中断は再開できるよう確定しない（再開で中断の行を除く）
           ├─ (2) 終局時刻を記録し、対局情報に反映（終局行を書いた後で行う）
           │      KifuDocument::recordMatchEnd()
           │        ├─ MatchTimeRecord::recordEnd()
           │        └─ 対局情報の終了日時を書く（未保存に数えない。棋譜の途中から続けた対局では書かない）
           │             → emit headerValueSet → GameInfoPane::updateGameInfoValue()
           └─ (3) commit で自動保存の指定が有効なら（MatchAutoSave::isActive()）
                  KifuFileController::autoSaveKifu(dir, extension)   … 連続対局の次局・通信対局の次の対局より前に書き出す
```

自動保存の指定（`MatchAutoSave`: `enabled`・`dir`・`extension`）は、対局の開始時に `GameSession` が覚える。ローカル対局は `MatchStartOptions::autoSave`（対局ダイアログの設定。拡張子は `kifu`）、通信対局は `CsaGameCoordinator::options().autoSave`（接続ダイアログで選んだ保存先と形式）である。

### 7.4 GameSession::onGameEndProcessed()

結果の知らせの後に呼ばれる（中断では出ない。自動保存は 7.3 で済んでいる）。エンジン同士の対局（`MatchCoordinator::players().engineVsEngine()`）で、連続対局の残りがあれば（`ConsecutiveGameSeries::shouldStartNextGame()`）`ConsecutiveGameSeries::startNextGame()` で次局を予約する。次局は今の終局処理を抜けてから `ConsecutiveGameSeries::nextGameReady` → `GameSession::startNextGame()` で始まる（[対局開始フロー](game-start-flow.md) の連続対局）。

---

## 8. その他の終局条件

### 8.1 時間切れ: handlePlayerTimeOut()

**ファイル:** `src/game/matchcoordinator.cpp`

```cpp
void MatchCoordinator::handlePlayerTimeOut(Turn player)
{
    if (isGameOver()) return;
    endGame({Cause::Timeout, player}, player, Notice::Announce);
}
```

時計の残り時間が0になると、`ShogiClock::playerTimeOut(Turn)`（引数は時間が切れた側）から呼ばれる。

```
ShogiClock::updateClock()（50ms ごとの tick）
    ↓ chargeElapsed(): 時計の手番の側の残り時間を減らし、秒読みがあれば秒読みに入る
    ↓ 残りが0で、時間切れを負けにする設定のとき
ShogiClock::declareTimeout(player)
    ├─ 時計を終局状態にし、tick を止める
    └─ emit playerTimeOut(player)
    ↓  （MatchCoordinator のコンストラクタが接続）
MatchCoordinator::handlePlayerTimeOut(Turn player)
    └─ endGame({Timeout, player}, player, Notice::Announce)
           ├─ 時計を止め、計測を打ち切る
           ├─ 時間が切れた側に "gameover lose"、相手に "gameover win" → quit
           ├─ 終局行 "▲時間切れ" / "△時間切れ"
           └─ 「%1の時間切れ。%2の勝ちです。」
```

時間切れの判定は、持ち時間に制限があり、時間切れを負けにする設定（`ShogiClock::setLoseOnTimeout(true)`）のときだけ行う（`ShogiClock::enforcesTimeout()`）。

### 8.2 中断: handleBreakOff()

**ファイル:** `src/game/matchcoordinator.cpp`

ユーザーがメニュー「対局」→「中断」をクリックした場合（`GameSession::breakOff()`）と、エンジンのエラー（8.7）で呼ばれる。CSA通信対局では `GameSession::breakOff()` が窓口（`CsaGameChannel::breakOff()`）経由で `CsaGameCoordinator::requestChudan()` に渡す。

```cpp
void MatchCoordinator::handleBreakOff()
{
    if (isGameOver()) return;
    captureInterruptedGame();
    // 時計の精算時に時間切れが確定した場合、中断に上書きしない。
    if (isGameOver()) return;
    // 中断の後に届く bestmove resign は、終局の知らせ（MatchEngineSeat::finish）の後なので受けない。
    // quit に応答しない場合も terminate/kill まで進める。中断後の思考表示は保持する。
    endGame({Cause::BreakOff, m_gc->currentPlayer()}, std::nullopt, Notice::Interrupt);
}
```

```
MatchCoordinator::handleBreakOff()
    ├─ (1) 終局済みチェック
    │
    ├─ (2) 再開用に対局の状態を保存（captureInterruptedGame()）
    │      ShogiClock::pauseAndSnapshot() で時計を止めて状態を写し、開始の設定・対局の手順を
    │      InterruptedGameState に入れる（「同」表記の基準は再開後も手順の最後の手から導く）
    │
    ├─ (3) 時計を止めたときの精算で時間切れが確定していれば return
    │
    └─ (4) endGame({BreakOff, 手番の側}, nullopt, Notice::Interrupt)
           ├─ 時計を終局状態にして止め、計測を打ち切る
           ├─ MatchEngineSeat::finish(Draw): 知らせの接続を外し、エンジンへ "gameover draw" と quit
           ├─ 終局行 "▲中断" / "△中断" を追記し、待機に戻して gameEnded
           └─ 再開用の状態があれば emit gameInterrupted()
                  → GameSession::onGameInterrupted() が中断の行を MatchCoordinator::attachInterruptedRecord() で結びつけ、
                    resumeAvailabilityChanged → UiStatePolicyManager::refreshResumeAvailability() で「対局再開」を使えるようにする
```

中断は結果のダイアログを出さず、自動保存もしない。`GameSession::onGameEnded()` は中断のライブ対局セッションを確定しない。再開（`GameSession::resumeInterruptedGame()`）は中断の行を除いてから、保存した手順と時計で続ける（[対局開始フロー](game-start-flow.md) の「中断と再開」）。

### 8.3 最大手数到達（持将棋）: handleMaxMovesJishogi()

**ファイル:** `src/game/matchcoordinator.cpp`

`completeMove()` が一手を確定するたびに、最大手数（`MatchSessionState::maxMoves()`、0 なら無制限）を対局の手順の手数（`currentPly()`）と比べ、達していれば呼ぶ。対局モードによらず同じ判定である。

```
MatchCoordinator::handleMaxMovesJishogi()
    ├─ 終局済みチェック
    └─ endGame({Jishogi, Turn::Black}, nullopt, Notice::Announce)
           ├─ エンジンへ "gameover draw" + quit
           ├─ 手番の側で "▲持将棋" / "△持将棋"
           └─ 「最大手数に達しました。持将棋です。」
```

引き分けなので、`loser` の値は結果メッセージにも終局行の印にも使わない。

### 8.4 千日手: checkAndHandleSennichite()

**ファイル:** `src/game/matchcoordinator.cpp`、`src/game/sennichitedetector.cpp`

`completeMove()` が一手を棋譜に追記してから呼ぶ。判定の詳細は [千日手検出機能](sennichite-detection.md) を参照。

```
MatchCoordinator::checkAndHandleSennichite()
    ├─ 終局済み、または対局の手順が3手未満（局面が4つ未満）なら false
    └─ SennichiteDetector::check(m_record.sfens())
           ├─ None                   → false（対局を続ける）
           ├─ Draw                   → endGame({Sennichite, Turn::Black}, nullopt, Announce)、引き分け
           ├─ ContinuousCheckByBlack → endGame({OuteSennichite, Turn::Black}, Turn::Black, Announce)
           └─ ContinuousCheckByWhite → endGame({OuteSennichite, Turn::White}, Turn::White, Announce)
       成立したら true
```

| 原因 | GameEndInfo | エンジンへの通知 | 終局行 |
|---|---|---|---|
| 千日手 | `{Sennichite, Turn::Black}` | `"gameover draw"` + quit | 手番の側で `"▲千日手"` |
| 連続王手の千日手 | `{OuteSennichite, 王手を続けた側}` | 王手を続けた側に `"gameover lose"`、相手に `"gameover win"`、どちらも quit | 手番の側で `"▲反則負け"`／`"▲反則勝ち"` |

### 8.5 入玉宣言: handleNyugyokuDeclaration()

**ファイル:** `src/game/matchcoordinator.cpp`

```cpp
void handleNyugyokuDeclaration(Turn declarer, bool success, bool isDraw);
```

宣言の入口は2つある。

```
人間の宣言: メニュー「入玉宣言」
    ↓
GameSession::declareNyugyoku()                    （src/app/gamesession.cpp）
    ↓
NyugyokuDeclarationHandler::handleDeclaration(activeChannel())   （src/ui/controllers/nyugyokudeclarationhandler.cpp）
    ├─ 対局中でなければ（窓口が nullptr）警告、手番の人間でなければ（GameChannel::isHumanToMove()）案内して終わる
    ├─ こちらで判定する対局（GameChannel::judgesDeclaration()。ローカル対局）なら持将棋ルール（24点法・27点法）の設定が必要
    ├─ 確認ダイアログ（宣言する側の呼び名は KifuDocument::usesHandicapNames() で下手・上手にもなる）
    ├─ GameChannel::declareNyugyoku()
    │     ├─ ローカル対局: LocalGameChannel → MatchCoordinator::declareNyugyoku(declarer, rule)
    │     │      ├─ NyugyokuJudgement::judge() で盤面から宣言条件と点数を判定
    │     │      ├─ handleNyugyokuDeclaration(declarer, success, isDraw) で終局
    │     │      └─ 判定結果を返す
    │     └─ CSA通信対局: CsaGameChannel → CsaGameCoordinator::declareWin()（成否はサーバーが判定。nullopt を返す）
    └─ 判定結果が返れば結果ダイアログを表示

エンジンの宣言: "bestmove win"
    ↓
UsiMatchHandler::declaredWin(side) → Usi::declaredWin → MatchEngineSeat::declaredWin(side)
    ↓
MatchCoordinator::handleEngineWin(side)
    ├─ NyugyokuJudgement::judgeEngineDeclaration() で盤面から判定
    ├─ handleNyugyokuDeclaration(side, success, isDraw)
    └─ 判定結果を gameOverMessage で知らせる
```

入玉宣言には3つの結果がある:

| 結果 | Cause | 敗者 | エンジンに負けと知らせる側 |
|---|---|---|---|
| 引き分け（持将棋） | `Jishogi` | 宣言者（引き分け） | なし（`"gameover draw"`） |
| 宣言成功（勝ち） | `NyugyokuWin` | 宣言者の相手 | 宣言者の相手 |
| 宣言失敗（反則負け） | `IllegalMove` | 宣言者 | 宣言者 |

`handleNyugyokuDeclaration()` は、判定結果から上の表の GameEndInfo とエンジンに負けと知らせる側を決めて、`endGame(info, …, Notice::Declaration)` を呼ぶ。`endGame()` が時計の停止・計測の打ち切り・gameover の通知・終局行の記録・待機への切り替え・`gameEnded`（受けた `GameSession` が棋譜を確定して自動保存する）を行い、`gameEndProcessed` を出す。結果ダイアログは呼び出し側が出すので、`resultMessage()` の知らせは出さない。

### 8.6 エンジンの反則手: handleEngineIllegalMove()

エンジンの指し手が `ShogiGameController::applyMove()` で不合法と判定されると、`MatchCoordinator::applyEngineMove()` は盤に指さずに `handleEngineIllegalMove(エンジンの手番)` を呼ぶ（2.2）。

```
MatchCoordinator::handleEngineIllegalMove(Turn side)
    ├─ 終局済みチェック
    └─ endGame({IllegalMove, side}, side, Notice::Announce)
           ├─ 投了と同じく勝ち負けを知らせる
           ├─ "▲反則負け" / "△反則負け"
           └─ 「%1の反則負け。%2の勝ちです。」
```

### 8.7 エンジンのエラー（中断として扱う）

エンジンの異常終了・起動の失敗・bestmove の応答待ちの期限切れは、対局を中断として終える。

```
Usi::errorOccurred(message)
    （UsiMatchHandler::timedOut を受けた Usi::onMatchTimeout() の "Timeout waiting for bestmove." もここに出る。
      時計の時間切れで終局していれば timedOut は出さない）
    ↓  （MatchEngineSeat::start() が接続。ほかの持ち主が枠を取ったとき（engineRevoked）も同じ知らせを出す）
MatchEngineSeat::errorOccurred
    ↓  （MatchCoordinator が Strategy を作ったときに接続。起動の失敗は開始の処理から戻った後に届くので、そのまま中断してよい）
MatchCoordinator::handleEngineError(msg)
    ├─ 終局済み・Strategy が無いなら何もしない
    ├─ 両枠のエンジンの今の処理を取り消す（Usi::cancelCurrentOperation()）
    └─ エンジンを使う対局なら
           handleBreakOff()（8.2）
           emit gameOverMessage("対局中断", "エンジンエラー: <msg>")
```

検討・詰み探索のエンジンのエラーは、使う側（`AnalysisSession`）が直接受け取る。

---

## 9. 終局原因一覧

| 原因 | Cause列挙値 | 発動契機 | 棋譜表記 |
|---|---|---|---|
| 投了 | `Resignation` | ユーザーの投了操作 / エンジンの `bestmove resign` | `"▲投了"` |
| 時間切れ | `Timeout` | 時計の残り時間が0 | `"▲時間切れ"` |
| 中断 | `BreakOff` | ユーザーの中断操作 / エンジンのエラー | `"▲中断"` |
| 持将棋 | `Jishogi` | 最大手数到達 / 入玉宣言の引き分け | `"▲持将棋"` |
| 入玉宣言勝ち | `NyugyokuWin` | 入玉宣言の成功（人間の宣言 / エンジンの `bestmove win`） | `"▲入玉勝ち"` |
| 反則負け | `IllegalMove` | 入玉宣言の失敗 / エンジンの反則手 | `"▲反則負け"` |
| 千日手 | `Sennichite` | 同一局面の4回目の出現 | `"▲千日手"` |
| 連続王手の千日手 | `OuteSennichite` | 同一局面の4回目の出現で、一方の手がすべて王手 | `"▲反則負け"` / `"▲反則勝ち"` |

`Cause` は `MatchCoordinator::Cause`（`src/game/matchtypes.h` の `MatchCause`）。棋譜表記の印の側は 6.3 を参照。

---

## 10. シグナルチェーンまとめ

### 投了時の完全なシグナルフロー

```
ユーザー: メニュー「投了」クリック
    ↓
actionResign::triggered
    ↓
GameSession::resign()
    ↓ activeChannel()（ローカル対局なら LocalGameChannel）->resign()
MatchCoordinator::handleResign()
    └─ endGame({Resignation, loser = 手番の側}, loser, Notice::Announce)
        ├─ MatchSessionState::finish(info)
        ├─ 時計を終局状態にして止め、計測を打ち切る
        ├─ MatchEngineSeat::finish(win/lose)               // HvE: E1 に "gameover win/lose" + quit
        ├─ appendTerminalRecord(info)
        │   ├─ 投了した側の考慮時間を確定
        │   ├─ MatchRecordSink::appendRecord("▲投了")      ──→ (A)
        │   └─ MatchSessionState::markTerminalRecorded()
        ├─ m_activityLease.reset()（動作状態を Idle に） ──→ (B)
        ├─ emit gameEnded(info)                            ──→ (C)
        ├─ emit gameOverMessage("対局終了", "先手の投了。後手の勝ちです。")
        │                                    ──→ NotificationPresenter::showInformation()
        └─ emit gameEndProcessed(info)                     ──→ (D)

(A) GameSession::appendRecord(entry)
    └─ LiveGameSession::appendRecord() → 分岐ツリーに終局ノード、表示中の手を終局ノードへ
        └─ KifuNavigationState::changed
            ├─ KifuDisplayPresenter::refresh()          // 棋譜欄に「▲投了」の行を出してカーソルを合わせる
            └─ BoardSyncPresenter::syncToCurrentNode()  // 盤のハイライトを消す

(B) AppActivityState::changed
    └─ UiStatePolicyManager::refreshActivity()  // メニュー・ボタン・盤のクリックを待機の状態に

(C) GameSession::onGameEnded(info) → finishMatchRecord(true)
    ├─ LiveGameSession::commit()
    ├─ KifuDocument::recordMatchEnd()  // 終局時刻を記録し、対局情報に反映
    └─ KifuFileController::autoSaveKifu(dir, extension)  // 自動保存の指定が有効なとき

(D) GameSession::onGameEndProcessed(info)
    └─ エンジン同士の連続対局なら次局を予約（投了のメニューはエンジン同士では使えない）

対局終了 → 待機（棋譜閲覧）に戻る
```

---

## 11. 関連ファイル一覧

| コンポーネント | ヘッダ | 実装 |
|---|---|---|
| 盤面操作コントローラ | `src/board/boardinteractioncontroller.h` | `src/board/boardinteractioncontroller.cpp` |
| 盤で指した手の振り分け | `src/ui/controllers/boardmovecontroller.h` | `src/ui/controllers/boardmovecontroller.cpp` |
| 部品の生成と配線（司令塔のシグナルの接続） | `src/app/mainwindowcomposer.h` | `src/app/mainwindowcomposer_create.cpp`, `src/app/mainwindowcomposer_wire.cpp` |
| 盤で指した手の行き先（インターフェース） | `src/board/boardinputroute.h` | - |
| 盤を人間が操作できるかの判定 | `src/ui/controllers/humanturnpolicy.h` | `src/ui/controllers/humanturnpolicy.cpp` |
| 盤（局面と手番） | `src/core/shogiboard.h` | `src/core/shogiboard.cpp` |
| ゲームコントローラ | `src/game/shogigamecontroller.h` | `src/game/shogigamecontroller.cpp` |
| 対局司令塔（一手の確定・終局・待った・中断と再開） | `src/game/matchcoordinator.h` | `src/game/matchcoordinator.cpp` |
| 対局の設定・終局状態 | `src/game/matchsessionstate.h` | - |
| 棋譜の受け手のインターフェース | `src/game/matchrecordsink.h` | - |
| 対局相手の窓口（投了・中断・即指し・入玉宣言） | `src/game/gamechannel.h`、`src/network/csagamechannel.h` | `src/game/gamechannel.cpp`、`src/network/csagamechannel.cpp` |
| 終局した棋譜の自動保存の指定 | `src/game/matchautosave.h` | - |
| 終局の型 | `src/game/matchtypes.h` | - |
| 対局者 | `src/core/matchplayers.h` | - |
| Strategy の基底・司令塔への操作（MatchStrategyHost） | `src/game/gamemodestrategy.h` | - |
| 人間対エンジン | `src/game/humanvsenginestrategy.h` | `src/game/humanvsenginestrategy.cpp` |
| 人間同士 | `src/game/humanvshumanstrategy.h` | `src/game/humanvshumanstrategy.cpp` |
| エンジン同士 | `src/game/enginevsenginestrategy.h` | `src/game/enginevsenginestrategy.cpp` |
| エンジンの枠・終局の通知・投了の知らせ | `src/game/enginelifecyclemanager.h` | `src/game/enginelifecyclemanager.cpp` |
| 千日手の判定 | `src/game/sennichitedetector.h` | `src/game/sennichitedetector.cpp` |
| 入玉宣言の判定 | `src/game/nyugyokujudgement.h` | `src/game/nyugyokujudgement.cpp` |
| 中断した対局の状態 | `src/game/interruptedgamestate.h` | - |
| 連続対局 | `src/game/consecutivegameseries.h` | `src/game/consecutivegameseries.cpp` |
| 対局の利用者の操作（開始・投了・中断・再開・入玉宣言）、棋譜の受け手、終局後の処理（ローカル対局・通信対局で共通） | `src/app/gamesession.h` | `src/app/gamesession.cpp` |
| ライブ対局のセッション | `src/kifu/livegamesession.h` | `src/kifu/livegamesession.cpp` |
| 表示中の手 | `src/kifu/kifunavigationstate.h` | `src/kifu/kifunavigationstate.cpp` |
| 対局の手順・棋譜の1行 | `src/kifu/recordline.h`、`src/kifu/recordentry.h` | - |
| 盤を表示中の手に合わせる・指し手のハイライト | `src/ui/presenters/boardsyncpresenter.h` | `src/ui/presenters/boardsyncpresenter.cpp` |
| 棋譜欄・分岐の表示 | `src/ui/presenters/kifudisplaypresenter.h` | `src/ui/presenters/kifudisplaypresenter.cpp` |
| 将棋時計 | `src/core/shogiclock.h` | `src/core/shogiclock.cpp` |
| 棋譜の文書（対局情報・対局者・持ち時間・上書き保存先） | `src/kifu/kifudocument.h` | `src/kifu/kifudocument.cpp` |
| 持ち時間と開始・終了の日時 | `src/kifu/matchtimerecord.h` | `src/kifu/matchtimerecord.cpp` |
| 持ち時間を時計に入れる | `src/game/timecontrolutil.h` | `src/game/timecontrolutil.cpp` |
| 時計の表示 | `src/ui/presenters/timedisplaypresenter.h` | `src/ui/presenters/timedisplaypresenter.cpp` |
| 対局者名・時計・手番の印の表示 | `src/views/boardchromewidget.h` | `src/views/boardchromewidget.cpp` |
| 対局者1人のカード（手番の札・名前・時計） | `src/views/playercard.h` | `src/views/playercard.cpp` |
| USIエンジン | `src/engine/usi.h` | `src/engine/usi.cpp` |
| 対局の思考依頼・bestmove の受け取り（UsiMatchMove） | `src/engine/usimatchhandler.h` | `src/engine/usimatchhandler.cpp` |
| エンジンの思考に使う時間の出どころ（対局時計・固定の値） | `src/engine/matchtimesource.h` | `src/engine/matchtimesource.cpp` |
| USIプロトコルハンドラ | `src/engine/usiprotocolhandler.h` | `src/engine/usiprotocolhandler.cpp` |
| go の時間の計算 | `src/engine/usigotime.h` | `src/engine/usigotime.cpp` |
| メニューの配線 | `src/app/menuactionswiring.h` | `src/app/menuactionswiring.cpp` |
| 動作状態ごとのメニュー・盤のクリックの有効/無効 | `src/ui/coordinators/uistatepolicymanager.h` | `src/ui/coordinators/uistatepolicymanager.cpp` |
| 利用者の入玉宣言（確認・宣言・結果の表示） | `src/ui/controllers/nyugyokudeclarationhandler.h` | `src/ui/controllers/nyugyokudeclarationhandler.cpp` |
