# 対局開始から投了までのフロー

本ドキュメントでは、対局が開始された後の指し手の流れ（人間の着手、エンジンの着手、時計管理）から、投了などによる終局までの一連のフローを説明する。対局が始まるまでの流れは [対局ダイアログから対局開始までのフロー](game-start-flow.md) を参照。対象はローカル対局（人間同士・人間対エンジン・エンジン同士）で、CSA 通信対局は経路が分かれる所だけを記す。

先手・後手はすべて `Turn`（`Turn::Black`／`Turn::White`、`src/core/shogitypes.h`）で表す。誰が指すかは `MatchPlayers`（`src/core/matchplayers.h`）で表す。部品は起動時に `MainWindowComposer`（`src/app/mainwindowcomposer*.cpp`）が全部作って配線する（[composition-root.md](composition-root.md)）。対局の司令塔 `MatchCoordinator`（`src/game/matchcoordinator.cpp`）は `MainWindowComposer::createServices()` が一度だけ作り、対局ごとに使い回す。司令塔は子ハンドラもフック（コールバックの構造体）も持たない。一手の確定・終局の判定と記録・待った・中断と再開を自分で行い、表示にはシグナルで知らせ、棋譜の行は `MatchRecordSink`（`src/game/matchrecordsink.h`）へ書き込む。シグナルの受け先は `MainWindowComposer::connectMatchSignals()`（`src/app/mainwindowcomposer_game.cpp`）がつなぎ、主な受け先は `GameSession`（`src/app/gamesession.cpp`）である。`GameSession` は `MatchRecordSink` の実装でもある。

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
│  │  2. validateAndMove()（合法手判定・盤面更新）  │        │
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
│  completeMove(): 時計の確定・棋譜追記・表示の知らせ        │
│                  → 千日手 → 最大手数（持将棋）を判定       │
│  時計の tick ごとに残り時間を判定 → 時間切れ負け           │
│  手番を交代して対局ループに戻る                            │
└────────────────────────────────────────────────────────────┘
    │
    │ 投了 / 中断 / 時間切れ / 千日手 / 持将棋 / 入玉宣言 / 反則
    ▼
┌────────────────────────────────────────────────────────────┐
│ 終局処理（MatchCoordinator）                               │
│  1. エンジンへ gameover を送る（notifyGameOver）           │
│  2. finishGame(): 終局状態を確定し、棋譜に終局行           │
│     （「▲投了」など）を書いて gameEnded を出す             │
│  3. announceResult(): 結果の知らせ（gameOverMessage）、    │
│     棋譜の自動保存の依頼（設定時）、gameEndProcessed       │
│     （エンジン同士の連続対局なら次局へ）                   │
└────────────────────────────────────────────────────────────┘
```

---

## 1. 人間の着手フロー

### 1.1 盤面クリック → 駒選択

**ファイル:** `src/board/boardinteractioncontroller.cpp`

```
ユーザーが盤面をクリック
    ↓
ShogiView::clicked(QPoint)          （BoardSetupController::connectBoardClicks() が接続）
    ↓
BoardInteractionController::onLeftClick(pt)
```

`onLeftClick()` の処理:

1. 指し手入力の確認: 指し手入力が無効（`setMoveInputEnabled(false)`。`UiStatePolicyManager` が状態に応じて設定する）なら無視する。局面編集モードは除く。
2. 人間の手番チェック: `m_isHumanTurnCb` コールバックで現在が人間の手番かを確認し、エンジンの手番なら無視する。コールバックは `MainWindowComposer::wireGame()`（`src/app/mainwindowcomposer_game.cpp`）が `HumanTurnPolicy::isHumanTurnNow()`（`src/app/humanturnpolicy.cpp`）を呼ぶものとして設定する。対局中は `MatchCoordinator::isHumanSide(手番)`、通信対局中は人間が指していて自分の手番か、対局していないときは変化として記録できるか（`MainWindowActions::canRecordVariationMove()`）で決める。
3. 1回目のクリック（駒選択）: 持ち駒のない駒台のマスは無視する。`ShogiView::startDrag()` でドラッグを始め、`selectPieceAndHighlight()` で駒を選ぶ。選んだマスにオレンジ色のハイライトを付け、合法な移動先（`EngineMoveValidator` で判定）を青色で示す（メニューで表示を切り替えられる）。相手の駒台や空きマスは選ばない。`m_waitingSecondClick = true` に設定する。
4. 2回目のクリック（移動先指定）: 同じマスならキャンセルする。別のマスなら `moveRequested(from, to)` シグナルを発行する。成り・不成のダイアログを出している間もドラッグ状態を保つため、ここではドラッグを終えない（`ShogiGameController::endDragSignal` → `ShogiView::endDrag()` で終える）。

右クリック（`onRightClick()`）は、2回目のクリック待ちなら選択を取り消す。

### 1.2 指し手のルーティング

```
BoardInteractionController::moveRequested(from, to)
    ↓  （MainWindowComposer::connectSignals() が接続）
MainWindowActions::handleMoveRequested(from, to)           （src/app/mainwindowactions.cpp）
    ↓
BoardSetupController::onMoveRequested(from, to)            （src/ui/controllers/boardsetupcontroller.cpp）
```

`BoardSetupController::onMoveRequested()` の処理:
- 局面編集モード（`BoardInteractionController::Mode::Edit`）なら `PositionEditController::applyEditMove()` に渡して終わる。
- 対局していない待機中なら、指した手を棋譜の変化として記録して終わる（`MainWindowActions::canRecordVariationMove()`／`recordVariationMove()`）。
- 通信対局中（`AppActivityState::Activity::DuringCsaGame`）なら `csaMoveRequested(from, to)` シグナルを発行する（→ `CsaGameCoordinator::onHumanMove()`）。盤はサーバーの確認を受けてから変えるので、ここでは指さない。
- 対局中（`Activity::DuringGame`）で、司令塔が手を受け付け（`MatchCoordinator::canAcceptMove()`：終局しておらず棋譜に書ける）、手番が人間（`isHumanSide()`）のときだけ指す。それ以外は `onMoveApplied(from, to, false)` で選択を取り消して終わる。
- `ShogiGameController::positionUpdate()` で手番・着手確定の通知を保留してから、`ShogiGameController::validateAndMove()` を呼ぶ。
- 結果を `BoardInteractionController::onMoveApplied()` に渡してハイライトを更新する（1.4）。不合法ならここで終わる。
- 合法なら、指した手（`RecordEntry`）を `MatchCoordinator::onHumanMove(played)` に渡す（2章）。考慮時間の確定・棋譜への追記・千日手と最大手数の判定・エンジンへの手番渡しは司令塔が行う。関数を抜けるときに保留した通知（`currentPlayerChanged` → `moveCommitted`）が出る。

### 1.3 指し手の検証と盤面更新

**ファイル:** `src/game/shogigamecontroller.cpp`

```cpp
std::optional<RecordEntry> ShogiGameController::validateAndMove(QPoint& outFrom, QPoint& outTo);
```

盤のクリック（表示の座標）による着手を検証し、合法手であれば指す。`prepareMove()` が合法手の判定と成り・不成の決定を行い、`applyRecordedMove()` → `commitMove()` が盤に指す。盤への反映（`commitMove()`）は、エンジンの手を指す `applyMove()`（2.2）と共通である。指した手は棋譜の1行（`RecordEntry`）として返し、対局の手順への追加は司令塔が行う。

#### 処理ステップ

| ステップ | 処理内容 |
|---|---|
| 入力検証 | 盤があるか、移動元が盤上か手番の側の駒台か、移動元に駒があるかを確認する。座標は `BoardCoordinates::usiMoveFromSquares()` で `UsiMove` にする |
| 合法手判定 | `legalStatus()` が `EngineMoveValidator::legalStatus()` で、成り・不成それぞれが合法かを調べる |
| 成り判定 | 下の「成り判定の詳細」のとおり。どちらも不合法なら `nullopt` を返し、盤は変えない |
| ドラッグ終了 | `emit endDragSignal()`（→ `ShogiView::endDrag()`） |
| 盤面更新 | `commitMove()` → `ShogiBoard::applyUsiMove(move, mover)` で駒の移動・取得を反映する（SFEN の手数も1つ進む） |
| 棋譜の行 | `RecordEntry::played(ShogiMove(move, moving, captured), 棋譜文字列, board()->toSfen())`。棋譜文字列は `convertMoveToKanjiStr()`（`KifMoveText::format()`）で日本語表記にする（例: `"▲７六歩(77)"`。直前の手と同じマスなら「同」） |
| 直前の手 | `m_lastMove` を更新する（`lastMove()` で読める） |
| 手番変更 | `setCurrentPlayer(board()->currentPlayer())` で相手に手番を移す。`positionUpdate()` の間は `currentPlayerChanged` を保留し、更新を終えたときに出す（3.5） |

着手確定の知らせ `moveCommitted` はここでは出さない。司令塔が一手を確定し終えてから `notifyMoveCommitted()` で出す（4.4）。

#### 成り判定の詳細

```
prepareMove() の成り・不成
    ├─ 成りだけ合法（行き所のない駒など）→ 成る
    ├─ 不成だけ合法 → 成らない
    ├─ 両方合法
    │    ├─ 強制成りモード（setForcedPromotion()、定跡ウィンドウの手）→ 指定どおり
    │    └─ それ以外 → setPromotionChooser() で設定した関数で対局者に選ばせる
    │         DialogLauncher::choosePromotion()
    │           → DialogCoordinator::showPromotionDialog() → PromotionFlow::askPromote()
    └─ どちらも不合法 → nullopt
```

成りの選び方は `MainWindowComposer::connectSignals()` が設定する。強制成りの指定は1手かぎりで、`prepareMove()` の始めに取り出して元に戻す。エンジンの手は成り・不成が決まった `UsiMove` で届くので、`applyMove()` は指し手どおりの成り・不成が合法かだけを確かめる。

### 1.4 着手後のハイライト更新

**ファイル:** `src/board/boardinteractioncontroller.cpp`

```
BoardInteractionController::onMoveApplied(from, to, success)
```

- 成功時: 移動元に薄い赤色、移動先に黄色のハイライトを表示し、選択中のオレンジ色のハイライトを消す。
- 失敗時（不合法手）: 選択とドラッグ状態を取り消す。

司令塔は一手を確定するたびに（人間の手もエンジンの手も）`MatchCoordinator::moveHighlightRequested(from, to)` を出し、`BoardInteractionController::showMoveHighlights()` が表示する（`connectMatchSignals()` が接続）。

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

Strategy は対局開始時（と再開時）に `MatchCoordinator::createStrategy()` が `MatchStartOptions::players`（`MatchPlayers`）から作る。

| 対局者 | Strategy | エンジンの枠 |
|---|---|---|
| 先手・後手ともエンジン（`engineVsEngine()`） | `EngineVsEngineStrategy` | 先手 E1、後手 E2 |
| 先手がエンジン（`blackIsEngine`） | `HumanVsEngineStrategy`（`m_engineSide = Turn::Black`、`enginePath1`） | E1 |
| 後手がエンジン（`whiteIsEngine`） | `HumanVsEngineStrategy`（`m_engineSide = Turn::White`、`enginePath2`） | E1 |
| どちらも人間 | `HumanVsHumanStrategy` | なし |

Strategy は `MatchStrategyHost`（`src/game/gamemodestrategy.h`。`MatchCoordinator` が private に実装する）を通して、盤（`gameController()`）・時計（`clock()`）・エンジンの枠（`engines()`）・手数（`currentPly()`）を読み、一手の確定（`completeHumanMove()`・`applyEngineMove()`）とエンジンへの思考の依頼（`requestEngineMove(side)`）を司令塔に頼む。Strategy は手番ごとに誰が指すかだけを決める。

エンジンは `EngineLifecycleManager`（`src/game/enginelifecyclemanager.cpp`）の枠（E1・E2。思考情報と通信ログの表示先）に置き、枠ごとにどちらの手番を指すかを覚える（`createEngine(slot, side, name)`、`engineFor(side)`）。`EngineLifecycleManager` は合成ルートが作り、司令塔（`MatchCoordinator::Deps::engines`）と検討・詰み探索（`AnalysisSession`）が共有する。人間対エンジンでは、エンジンは手番によらず E1 に置く。

エンジンへの思考依頼は非同期である。依頼した関数はすぐに戻り、指し手は `Usi::matchMoveReady(move, position, ponder)` シグナルで Strategy の `onEngineMoveReady()` に届く。

### 2.1 一手の確定: MatchCoordinator::completeMove()

人間の手（`completeHumanMove()`）もエンジンの手（`applyEngineMove()`）も、盤に指した後はこの関数で一手を確定する。

```
MatchCoordinator::completeMove(played, engineMove)
    ├─ 終局済みなら false
    ├─ 時計（通知は QSignalBlocker で止め、最後にまとめて知らせる）
    │      clock->commitMove(mover)                 … 考慮時間を合計に足し、秒読み・加算を適用（3.3）
    │      played.time = clock->recordTime(mover)   … 棋譜に書く消費時間
    │      clock->setConsiderationTimeMs(mover, 0)
    │      clock->setCurrentPlayer(gc->currentPlayer())
    ├─ 対局の手順に足す: m_record.entries.append(played)
    ├─ 棋譜に書く: m_sink->appendRecord(played)    … GameSession → LiveGameSession（4.2）
    ├─ emit boardChanged()                        → GameSession::renderBoard()
    ├─ emit moveHighlightRequested(from, to)      → BoardInteractionController::showMoveHighlights()
    ├─ emit turnChanged(gc->currentPlayer())      → TurnStateSyncService::onTurnManagerChanged()
    ├─ pokeTimeUpdateNow()                        → timeUpdated（3.6）
    ├─ エンジンの手なら emit engineMovePlayed(mover, ply) → EvaluationGraphController::plotEngineScore()
    ├─ gc->notifyMoveCommitted(mover, ply)        → moveCommitted（4.4）
    ├─ 千日手チェック: checkAndHandleSennichite() → 成立なら false（8.4）
    └─ 最大手数チェック: maxMoves > 0 && 手数 >= maxMoves → handleMaxMovesJishogi()、false（8.3）
```

戻り値は対局が続くか（`true`）である。手数（`currentPly()`）は対局の手順（`RecordLine`）の手の数で、途中局面から始めた対局ではそれまでの手も数える。

### 2.2 人間 vs エンジン（HvE）: HumanVsEngineStrategy

**ファイル:** `src/game/humanvsenginestrategy.cpp`

人間が指した後、エンジンに思考を依頼し、届いた指し手を司令塔に指させる。

`start()` は前の対局のエンジンを破棄し（`destroyEngines()`）、`createEngine(1, m_engineSide, name)` でエンジンを E1 に作り、`Usi::engineInitialized` と `Usi::matchMoveReady` を自分のスロットにつないでから `startEngine()` で起動する。最後に `refreshBoardAndTurn()`（`boardChanged`・`turnChanged`）で盤と手番の表示を合わせる。

#### 人間の着手後: onHumanMove()

```
HumanVsEngineStrategy::onHumanMove(played)
    ├─ (1) 一手を確定: host.completeHumanMove(played)（2.1）→ false（終局）なら return
    ├─ (2) エンジンに直前の手の移動先を知らせる（読み筋の「同」表記用）
    │      engine->setPreviousMove(played.move.move.to)
    └─ (3) エンジンの手番なら思考を依頼（非同期）
           m_waitingForMove = true
           host.requestEngineMove(m_engineSide)
```

`MatchCoordinator::requestEngineMove(side)` は、`engineFor(side)->requestMatchMove(m_record.positionCommand(currentPly()), ponder, {})` を呼ぶ。`position` は確定済みの対局の手順から作り、ponder は前の bestmove で先読みを始めた局面（`MatchSessionState::ponder(side)`）である。思考時間はエンジンにつないだ対局時計から、思考を始める時点で求める（3.4）。

#### USI通信の詳細

思考の依頼と bestmove の受け取りは `src/engine/usi.cpp`、`src/engine/usimatchhandler.cpp`、`src/engine/usiprotocolhandler.cpp` にある。

1. `Usi::requestMatchMove(position, ponder, timing)`: エンジンが初期化中なら、終わるまで依頼を保留する。`UsiMatchHandler::requestMove()` を呼ぶ。
2. `UsiMatchHandler::requestMove()`: 時計が終局していれば何もしない。先読み（`go ponder`）中なら、予想が当たれば `ponderhit` を送り、外れなら `stop` を送って先読みの bestmove を受けてから探索し直す。そうでなければ `startAsyncSearch()` を呼ぶ。
3. `UsiMatchHandler::startAsyncSearch()`: `position` と `go`（`UsiGoTimeCalculator` が求めた時間、3.4）を送り、応答待ちのタイマー（時間切れのある対局では今の手に使える残り時間）を始める。
4. bestmove 受信: `UsiProtocolHandler::handleBestMoveLine()` が考慮時間（`go` または `ponderhit` からの経過）を記録し、`resign`・`win` なら専用のシグナルを出す（5.4、8.5）。`UsiMatchHandler::onBestMoveReceived()` は `ShogiClock::finishTurn()` で時計を精算し（時計が終局していればここで止める）、通常の手なら position に bestmove を加え、予想手があれば先読み（`go ponder`）を始めてから、`Usi::matchMoveReady(move, position, ponder)` を出す。

#### エンジンの指し手: onEngineMoveReady()

```
Usi::matchMoveReady(move, position, ponder)
    ↓
HumanVsEngineStrategy::onEngineMoveReady(move, position, ponder)
    ├─ (1) 待っていた手か確かめる（m_waitingForMove、送り元が手番のエンジン、終局していない）
    ├─ (2) 先読みの局面を覚える: host.setEnginePonder(m_engineSide, ponder)
    └─ (3) host.applyEngineMove(move, m_engineSide, engine->lastBestmoveElapsedMs())

MatchCoordinator::applyEngineMove(move, mover, elapsedMs)
    ├─ 終局済み・棋譜に書けないなら false
    ├─ positionUpdate() で通知を保留
    ├─ gc->applyMove(move)（合法手か確かめて指す）
    │      └─ 反則手: handleEngineIllegalMove(mover) → false（8.6）
    ├─ エンジンの考慮時間を時計に渡す: clock->setMeasuredConsiderationTime(mover, elapsedMs)
    └─ completeMove(played, true)（2.1。評価値グラフへの追記を含む）
```

#### エンジンが先に指す場合

エンジンが先に指すとき（エンジンが先手のときと、駒落ちでエンジンが上手のとき）は、対局開始の最後に `GameStartCoordinator` が呼ぶ `MatchCoordinator::beginPlay()` が時計を動かし、Strategy の `startInitialMoveIfNeeded()` を呼ぶ。

```
MatchCoordinator::beginPlay()
    ├─ clock->startClock()
    └─ HumanVsEngineStrategy::startInitialMoveIfNeeded()
           ├─ 終局済み・思考待ち・エンジンの手番でないなら return
           ├─ エンジンが初期化中なら時計を止めて待つ
           │      → Usi::engineInitialized → onEngineInitialized() が時計を再開して、もう一度呼ぶ
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

`start()` は盤と手番の表示を合わせるだけである。

### 2.4 エンジン vs エンジン（EvE）: EngineVsEngineStrategy

**ファイル:** `src/game/enginevsenginestrategy.cpp`

EvEの対局は、エンジンの指し手を受けるたびに `QTimer::singleShot(0, ...)` で次の手番を予約するループで進行する。

```
EngineVsEngineStrategy::start()
    ├─ 前のエンジンを破棄し、createEngine(1, Black) と createEngine(2, White) で作る
    ├─ 両エンジンの engineInitialized → kickNextTurn、matchMoveReady → onEngineMoveReady を接続
    ├─ startEngine() で両エンジンを起動
    ├─ 時計を開始し、盤と手番の表示を合わせる
    └─ kickNextTurn()

kickNextTurn()            ← beginPlay() の startInitialMoveIfNeeded() からも呼ばれる
    ├─ (1) 思考待ち・終局済み・エンジンがそろっていないなら何もしない
    ├─ (2) どちらかのエンジンが初期化中なら時計を止めて待つ
    │      （engineInitialized で再び呼ばれ、時計を再開する）
    └─ (3) 手番のエンジンに思考を依頼（非同期）
           m_waitingForMove = true
           host.requestEngineMove(gc->currentPlayer())

onEngineMoveReady(move, position, ponder)   ← Usi::matchMoveReady
    ├─ (1) 送り元が手番のエンジンか確かめる
    ├─ (2) 先読みの局面を覚える: host.setEnginePonder(mover, ponder)
    ├─ (3) 相手エンジンに直前の手の移動先を知らせる
    │      receiver->setPreviousMove(move.to)
    ├─ (4) host.applyEngineMove(move, mover, engine->lastBestmoveElapsedMs())（2.2）
    │      └─ false（反則手・千日手・最大手数・終局）なら return
    └─ (5) 次のターンをスケジュール
           QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextTurn)
```

`QTimer::singleShot(0, ...)` により、Qtのイベントループに制御を返してから次のターンを始める。思考の依頼も非同期なので、エンジンが考えている間も UI は止まらない。`position` は人間対エンジンと同じく、司令塔が対局の手順から作る。

---

## 3. 時計管理

**ファイル:** `src/core/shogiclock.cpp`

`ShogiClock` は先手・後手の状態（残り時間・考慮時間・総考慮時間・秒読みに入っているか）を `SideState` にまとめて持ち、API はすべて `Turn` で受ける。50ms ごとの tick（`updateClock()`）で、手番の側の残り時間と考慮時間を進める。時計は `TimeControlController`（`src/ui/controllers/timecontrolcontroller.cpp`）が作って持ち、合成ルートが司令塔（`MatchCoordinator::Deps::clock`）と `EngineLifecycleManager` に渡す。`EngineLifecycleManager::createEngine()` は作ったエンジンに `Usi::setMatchClock()` で同じ時計をつなぐ。持ち時間の決まりと表示の仕様は [時間設定の仕様](game-time-control-spec.md) を参照。

### 3.1 時間計測の仕組み

対局モードごとに異なる方法で考慮時間を計測する。

| モード | 計測方法 | 計測開始 | 計測終了 |
|---|---|---|---|
| HvH | `QElapsedTimer`（`HumanVsHumanStrategy::m_turnTimer`） | `armTurnTimerIfNeeded()` | `finishTurnTimer()` → `ShogiClock::setMeasuredConsiderationTime()` |
| HvE（人間） | 時計自身の計時（tick で手番の側の考慮時間に足す） | 時計の手番が人間に移ったとき（`ShogiClock::setCurrentPlayer()`） | 時計の手番がエンジンに移ったとき |
| HvE（エンジン） | `Usi::lastBestmoveElapsedMs()` | go（または ponderhit）送信時 | bestmove受信時 → `MatchCoordinator::applyEngineMove()` が `setMeasuredConsiderationTime()` |
| EvE | `Usi::lastBestmoveElapsedMs()` | go（または ponderhit）送信時 | bestmove受信時 → `MatchCoordinator::applyEngineMove()` が `setMeasuredConsiderationTime()` |

人間同士でも、対局開始時には `m_turnTimer` を動かさないので、初手は時計自身が数えた考慮時間を使う。`setMeasuredConsiderationTime()` は、中断した対局を再開した手番では、中断前に使っていた考慮時間を足して確定する。

### 3.2 タイマー管理メソッド

`GameModeStrategy` の `armTurnTimerIfNeeded()`・`disarmTurnTimer()` は既定では何もせず、人間同士の Strategy だけが実装する。人間対エンジンの人間の考慮時間は時計自身が数えるので、Strategy は計測しない。

司令塔は終局処理（時間切れ・中断・千日手・持将棋・入玉宣言・終局行の記録）で private の `MatchCoordinator::disarmTurnTimer()` を呼んで計測を打ち切る。待った（`undoTwoPlies()`）では打ち切ってから測り直し、中断した対局の再開（`resumeInterruptedGame()`）では計測を始める。

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
    if (ShogiClock* clock = m_host.clock()) clock->setMeasuredConsiderationTime(mover, ms);
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

対局の種類（人間対エンジン、エンジン同士、CSA通信対局）によらず、この計算だけで `go` の時間を求める。入力（`UsiGoTimeInput`）は時間の決まり（`TimeControl`）と先後の残り時間である。人間対エンジン・エンジン同士では、エンジンにつないだ時計（3章の冒頭）から、思考を始める時点の残り時間を `matchSearchSnapshot()`（`src/engine/matchsearchsnapshot.h`）が読む。CSA通信対局ではエンジンに時計をつながず、`CsaGameProgress::engineTiming()` がサーバーの残り時間を入れた `UsiTimingParams` を使う。

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

### 3.5 時計の手番の切り替え

司令塔の `completeMove()` は、一手を確定するときに時計の手番を直接切り替え（`ShogiClock::setCurrentPlayer()`）、`turnChanged` で手番の表示を知らせる。盤（GC）の手番の変更も、次の経路で時計と表示に届く（同じ手番なら何もしない）。

```
ShogiGameController::setCurrentPlayer(next)      （commitMove() の中。positionUpdate() の間は保留）
    ↓ currentPlayerChanged(Turn)
TurnManager::set(Turn)                           （TurnStateSyncService::updateDeps() が接続。TurnManager は TurnStateSyncService が持つ）
    ↓ changed(Turn)
TurnStateSyncService::onTurnManagerChanged(Turn)  （src/app/turnstatesyncservice.cpp）
    ├─ ShogiClock::setCurrentPlayer(now)
    ├─ ShogiGameController::setCurrentPlayer(now)   （同じ手番なら何もしない）
    └─ ShogiView::updateTurnIndicator(now)
```

`MatchCoordinator::turnChanged` も `TurnStateSyncService::onTurnManagerChanged()` に直接つながる（`connectMatchSignals()`）ので、同じ処理になる。`ShogiClock::setCurrentPlayer()` は、前回の tick からの経過時間を前の手番の側に精算してから手番を切り替え、新しい手番の側の考慮時間を0から数え直す。

エンジンの bestmove を受け取ったときは、`UsiMatchHandler::onBestMoveReceived()` が `ShogiClock::finishTurn()` で時計を精算し、手番が切り替わるまでエンジンの側の時間を減らさない。

### 3.6 時計のUI更新

```
ShogiClock::timeUpdated()
    ↓  （MatchCoordinator のコンストラクタが接続）
MatchCoordinator::emitTimeUpdate()
    ↓
MatchCoordinator::timeUpdated(blackMs, whiteMs, sideToMove, urgencyMs)
    ↓  （MainWindowComposer::connectMatchSignals() が接続）
TimeDisplayPresenter::onMatchTimeUpdated(blackMs, whiteMs, sideToMove, urgencyMs)
```

`ShogiClock::timeUpdated()` は、tick で表示する秒が変わったときと、`commitMove()`・`startClock()`・`stopClock()` などで状態が変わったときに出る。着手の直後などすぐに表示を合わせたいときは、`MatchCoordinator::pokeTimeUpdateNow()` が今の値で `timeUpdated` を出す。

```cpp
void MatchCoordinator::emitTimeUpdate()
{
    const qint64 blackMs = m_clock->remainingMs(Turn::Black);
    const qint64 whiteMs = m_clock->remainingMs(Turn::White);
    const Turn side = m_gc->currentPlayer();
    const bool enableUrgency = !m_clock->hasByoyomi(side) || m_clock->byoyomiApplied(side);
    const qint64 activeMs = (side == Turn::Black) ? blackMs : whiteMs;
    emit timeUpdated(blackMs, whiteMs, side, enableUrgency ? activeMs : std::numeric_limits<qint64>::max());
}
```

`TimeDisplayPresenter::onMatchTimeUpdated()` の処理:
- 先手・後手の時計の文字列を盤に設定する（`ShogiView::setBlackClockText()`／`setWhiteClockText()`）。制限ありは残り時間 `HH:MM:SS`（秒は切り上げ）、無制限は「消費 HH:MM:SS」
- 手番の側（`ShogiView::setActiveIsBlack()`）と、残り時間・秒読みから決めた警告の段階（`ShogiView::setUrgencyVisuals()`）を設定する。引数の `urgencyMs` は使わず、警告の段階は時計から決める

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
- エンジンへの `position` は `m_record.positionCommand(currentPly())`、千日手の判定は `m_record.sfens()` から作る。待った（`undoTwoPlies()`）は末尾の2手を取り除く。

対局の設定・対局者・最大手数・終局状態・先読みの局面は `MatchSessionState`（`src/game/matchsessionstate.h`）が持つ。

### 4.2 棋譜行の追記

司令塔は確定した行を `MatchRecordSink` に書き込む。受け手は `GameSession`（`MainWindowComposer::wireGame()` が `MatchCoordinator::setRecordSink()` で設定）で、ライブ対局のセッション（`LiveGameSession`、`src/kifu/livegamesession.cpp`）を通して分岐ツリーに追記する。

```
MatchCoordinator::completeMove() / appendTerminalRecord()
    ↓ m_sink->appendRecord(entry)
GameSession::appendRecord(entry)                 （src/app/gamesession.cpp）
    ├─ AppState::currentSfenStr を指した後の局面にする（定跡・検討の局面）
    ├─ 棋譜の途中から続けた対局では、消費時間の累計を続ける（LiveGameSession::continuedTime()）
    ├─ GameRecordModel::markDirty()
    └─ LiveGameSession::appendRecord(entry)
           ├─ 分岐ツリーに子ノードを足す（同じ手の子があれば使い回す）
           └─ emit moveAdded → KifuDisplayCoordinator::onLiveGameMoveAdded()
                 → 棋譜欄に1行足し（KifuDisplayPresenter::presentLiveNode()）、カーソルを最新手に合わせる
```

終局行を書いた後は `LiveGameSession::canAddMove()` が false になり、`GameSession::canAppendRecord()` も false を返す。司令塔は棋譜に書けないときは手を指さない（`canAcceptMove()`・`applyEngineMove()`）。通信対局の行も、`CsaGameWiring` が同じ `MatchRecordSink`（`GameSession`）に書く。

### 4.3 対局の状態の持ち主

| データ | 持ち主 |
|---|---|
| 対局の手順（開始局面・指した手・各手の後の局面） | `MatchCoordinator::m_record`（`RecordLine`） |
| 対局の設定・対局者・最大手数・終局状態・先読みの局面 | `MatchSessionState` |
| 盤と手番 | `ShogiGameController` |
| 中断した対局（再開用） | `MatchCoordinator::m_interruptedGame`（`InterruptedGameState`） |
| 棋譜の正本（分岐ツリー） | `KifuBranchTree`（`LiveGameSession` が対局の手を足す） |

対局の手順は分岐ツリーの写しではない。対局の開始時に、表示中の手順（`KifuNavigationState::currentRecordLine()`）から作る（[対局開始フロー](game-start-flow.md)）。

### 4.4 着手確定の通知（moveCommitted）

`completeMove()` は表示の知らせの後に `ShogiGameController::notifyMoveCommitted(mover, ply)` を呼ぶ。`positionUpdate()` の間（盤のクリックによる着手と `applyEngineMove()`）は保留し、更新を終えたときに `currentPlayerChanged` の後で出す。

```
ShogiGameController::moveCommitted(mover, ply)
    ├─ MainWindowActions::handleMoveCommitted()   （MainWindowComposer::connectSignals() が接続）
    │    └─ JosekiWindowController::updateJosekiWindow()   … 定跡ウィンドウを更新
    └─ PieceSoundPlayer::playMoveSound()   … 駒音
```

通信対局では `CsaGameCoordinator` がサーバーの確認を受けた手で `notifyMoveCommitted()` を呼ぶ。評価値グラフは、エンジンの手のたびに司令塔が出す `engineMovePlayed(side, ply)` を `EvaluationGraphController::plotEngineScore()` が受けて、その側のエンジンの評価値を足す（人間対エンジン・エンジン同士で同じ）。

---

## 5. 投了フロー

### 5.1 投了の発動

**ファイル:** `src/app/gameactionswiring.cpp`

```cpp
QObject::connect(ui->actionResign,       &QAction::triggered, session, &GameSession::resign,              Qt::UniqueConnection);
```

ユーザーがメニュー「対局」→「投了」をクリックすると発動する。確認ダイアログは表示されない（即座に投了処理が実行される）。投了のメニューが有効になるのは、人間が参加する対局（人間同士・人間対エンジン）の対局中と、CSA 通信対局中だけである（`UiStatePolicyManager::effectivePolicy()`。ローカル対局で人間が参加するかは `localGameHasHuman`）。

### 5.2 投了のルーティング

```
GameSession::resign()                           （src/app/gamesession.cpp）
    ├─ CSA通信対局: CsaGameCoordinator::onResign() → return
    ↓
MatchCoordinator::handleResign()                （src/game/matchcoordinator.cpp）
```

### 5.3 MatchCoordinator::handleResign()

**ファイル:** `src/game/matchcoordinator.cpp`

投了処理の中核メソッド。

```cpp
void MatchCoordinator::handleResign()
{
    if (isGameOver()) return;
    const GameEndInfo info{Cause::Resignation, m_gc->currentPlayer()};
    m_engines->notifyGameOver(info.loser);
    finishGame(info);
    announceResult(info);
}
```

#### 処理ステップ

1. 終局済みチェック: 終局済みなら何もしない。
2. GameEndInfoの構築: 投了の敗者は現在の手番の側（`gc->currentPlayer()`）。
3. エンジンへの終了通知: `EngineLifecycleManager::notifyGameOver(loser)`（下表）。
4. 終局の記録: `finishGame(info)`（6.1）。終局状態の確定、終局行の追記、`gameEnded` まで行う。
5. 結果の知らせ: `announceResult(info)`（6.2）。

#### エンジンへの終了通知

**ファイル:** `src/game/enginelifecyclemanager.cpp`

`notifyGameOver(std::optional<Turn> loser)` は、すべての終局処理で共通に使う。手番を持つ枠のエンジン（対局のエンジン）ごとに、指している手番で結果を決める。

| 引数 | 通知内容 |
|---|---|
| 負けた側（`Turn`） | 負けた側を指すエンジンに `"gameover lose"`、相手を指すエンジンに `"gameover win"` |
| `nullopt`（引き分け・中断） | すべてのエンジンに `"gameover draw"` |

人間同士ではエンジンが無いので何も送らない。人間対エンジンではエンジンは E1 にあり、その手番で勝ち負けを決める。どの通知でも、投了の知らせを止め（`setSquelchResignLogging(true)`）、エンジンの今の処理を取り消して `stop` を送り、`gameover` を送ったあと `Usi::cleanupEngineProcessAndThread(false)` で `quit` を送ってプロセスを終了させる（思考情報の表示は残す）。

### 5.4 エンジン投了: handleEngineResign(Turn side)

**ファイル:** `src/game/enginelifecyclemanager.cpp`、`src/game/matchcoordinator.cpp`

エンジンが `bestmove resign` を返した場合の処理。

```
USIエンジンが "bestmove resign" を送信
    ↓
UsiProtocolHandler::handleBestMoveLine() が bestMoveResignReceived シグナルを発行
    （先読みの結果、setSquelchResignLogging(true) の後、2回目以降は発行しない）
    ↓
Usi::bestMoveResignReceived
    ↓  （EngineLifecycleManager::createEngine() が接続）
EngineLifecycleManager::onEngineResign()
    ↓  送り元の枠が指している手番で emit engineResigned(side)
    ↓  （MatchCoordinator のコンストラクタが接続）
MatchCoordinator::handleEngineResign(side)   ← side はエンジンの手番（Turn）
    ├─ 時計を停止（ShogiClock::stopClock()）
    ├─ info = {Resignation, loser = side}
    ├─ notifyGameOver(side)
    │      投了したエンジンに "gameover lose"（エンジン同士なら相手に "gameover win"）、どちらも quit
    ├─ finishGame(info)
    └─ announceResult(info)
```

- エンジンの手番は `createEngine(slot, side, name)` で枠に覚えさせる。人間対エンジンではエンジンが後手でも E1 に入るが、知らせはエンジンの手番で出る。
- 入玉宣言勝ちの `bestmove win` も同じく `onEngineWin()` → `engineDeclaredWin(side)` → `MatchCoordinator::handleEngineWin(side)` に届く（8.5）。
- 中断のときは `EngineLifecycleManager::disconnectArbiterSignals()` で両方の接続を外す（8.2）。

`resign`・`win` の bestmove は `Usi::matchMoveReady` では知らせない（`UsiMatchHandler::onBestMoveReceived()` が特殊な指し手を除く）。

---

## 6. 終局状態管理

### 6.1 finishGame()

**ファイル:** `src/game/matchcoordinator.cpp`

終局を記録する入口で、すべての終局処理（中断を含む）が呼ぶ。

```cpp
void MatchCoordinator::finishGame(const GameEndInfo& info)
{
    if (!m_session.finish(info)) {
        qCDebug(lcGame) << "finishGame() ignored: already over";
        return;
    }
    emit gameOverStateChanged(m_session.gameOver());
    appendTerminalRecord(info);
    // 終局の行を棋譜に書いた後で、時刻と UI を更新させる
    emit gameEnded(info);
}
```

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

受信先は `MainWindowComposer::connectMatchSignals()` がつなぐ。発行の順は表の上から下である（`gameOverMessage` 以降は `announceResult()` が出す）。

| シグナル | 受信先 | 処理内容 |
|---|---|---|
| `gameOverStateChanged`（終局した） | `GameSession::onGameOverStateChanged()` | 名前・時計の手番ハイライトを保護。終局行の追記前はここで戻る（7.1） |
| `gameOverStateChanged`（終局行を書いた） | `GameSession::onGameOverStateChanged()` | 最終行へのカーソル移動・閲覧モードへの遷移（7.1） |
| `gameEnded` | `GameSession::onGameEnded()` | ライブ対局セッションの確定、盤の操作・時計の停止、終局時刻の記録、UI状態を待機に戻す（7.2） |
| `gameOverMessage` | `DialogCoordinator::showInformation()` | 結果のダイアログ（`QMessageBox::information`） |
| `autoSaveRequested` | `KifuFileController::autoSaveKifuToFile()` | 棋譜の自動保存（設定時） |
| `gameEndProcessed` | `GameSession::onGameEndProcessed()` | エンジン同士の連続対局なら次局へ（7.3） |

### 6.2 announceResult()

**ファイル:** `src/game/matchcoordinator.cpp`

結果の知らせと棋譜の自動保存の依頼。

```cpp
void MatchCoordinator::announceResult(const GameEndInfo& info)
{
    emit gameOverMessage(tr("対局終了"), resultMessage(info));
    qCDebug(lcGame) << "Game ended";
    if (m_session.options().autoSaveKifu && !m_session.options().kifuSaveDir.isEmpty()) {
        qCInfo(lcGame) << "auto save: dir=" << m_session.options().kifuSaveDir;
        emit autoSaveRequested(m_session.options().kifuSaveDir);
    }
    emit gameEndProcessed(info);
}
```

中断（8.2）は `announceResult()` を呼ばない。入玉宣言（8.5）は自動保存の依頼と `gameEndProcessed` だけを出し、結果は宣言の判定結果のダイアログで知らせる。

結果メッセージは `resultMessage()` が作る。駒落ちで対局情報の見出しが下手・上手のとき（`setHandicapNames(true)`。`GameSession::onGameStarted()` が設定する）は、先手・後手の代わりに下手・上手と呼ぶ。

| 終局原因 | メッセージ |
|---|---|
| `Resignation` | 「%1の投了。%2の勝ちです。」（%1 は敗者、%2 は勝者） |
| `Timeout` | 「%1の時間切れ。%2の勝ちです。」 |
| `Jishogi` | 「最大手数に達しました。持将棋です。」 |
| `IllegalMove` | 「%1の反則負け。%2の勝ちです。」 |
| `Sennichite` | 「千日手が成立しました。」 |
| `OuteSennichite` | 「%1の連続王手の千日手。%2の勝ちです。」 |
| `NyugyokuWin`・`BreakOff` | （`announceResult()` を通らない。既定の文は「対局が終了しました。」） |

### 6.3 棋譜への終局手追記: appendTerminalRecord()

棋譜欄に終局手（「投了」「時間切れ」等）を1行だけ追記する。`finishGame()` が呼ぶ。

#### 終局手の棋譜表記

終局行は `RecordEntry::ending(terminal, mover, time)` で作り、表記は印（▲/△）と終局語の表（`TerminalTypes::label()`、`src/core/terminaltype.cpp`）から作る。

| 終局原因 | `TerminalType` | 棋譜表記例 | 印（▲/△）の側 |
|---|---|---|---|
| 投了 (Resignation) | `Resign` | `"▲投了"` / `"△投了"` | 敗者 |
| 時間切れ (Timeout) | `Timeout` | `"▲時間切れ"` / `"△時間切れ"` | 敗者 |
| 反則負け (IllegalMove) | `IllegalLoss` | `"▲反則負け"` / `"△反則負け"` | 敗者 |
| 中断 (BreakOff) | `Interrupt` | `"▲中断"` / `"△中断"` | 中断したときの手番の側 |
| 入玉勝ち (NyugyokuWin) | `DeclarationWin` | `"▲入玉勝ち"` / `"△入玉勝ち"` | 勝者（宣言した側） |
| 持将棋 (Jishogi) | `Impasse` | `"▲持将棋"` / `"△持将棋"` | 終局した局面の手番の側 |
| 千日手 (Sennichite) | `Repetition` | `"▲千日手"` / `"△千日手"` | 終局した局面の手番の側 |
| 連続王手の千日手 (OuteSennichite) | `IllegalLoss`／`IllegalWin` | `"▲反則負け"` / `"△反則勝ち"` など | 終局した局面の手番の側。王手を続けた側が手番なら「反則負け」、相手が手番なら「反則勝ち」 |

`▲` = 先手、`△` = 後手。

#### 二重追記防止と消費時間

```cpp
void MatchCoordinator::appendTerminalRecord(const GameEndInfo& info)
{
    m_clock->stopClock();

    // ... 終局行の側（lineOwner）と TerminalType を決める ...

    // 終局行の消費時間は、その行（投了・宣言など）を行った手番側のもの。
    // 入玉宣言勝ちでは宣言した勝者が手番側になる。
    const Turn mover = (info.cause == Cause::NyugyokuWin) ? opponent(info.loser) : lineOwner;
    m_clock->commitMove(mover);
    const RecordEntry entry = RecordEntry::ending(terminal, mover, m_clock->recordTime(mover));
    if (m_sink && m_sink->canAppendRecord()) m_sink->appendRecord(entry);
    disarmTurnTimer();

    if (m_session.markTerminalRecorded()) emit gameOverStateChanged(m_session.gameOver());
}
```

`finishGame()` は終局済みなら何もしないので、終局行は1対局で1回だけ書く。`MatchSessionState::markTerminalRecorded()` が `GameOverState::moveAppended` を `true` にし、`gameOverStateChanged` が再発行される。これにより `GameSession::onGameOverStateChanged()` が閲覧（リプレイ）モードへ移る（7.1）。

終局行の考慮時間は、その行を行った側（入玉宣言勝ちでは宣言した勝者、千日手・持将棋・連続王手の千日手では手番の側、それ以外は敗者）のものである。時計を止めた時点で手番側の経過が考慮時間に精算されているので、その考慮時間を `commitMove()` で確定し、`recordTime()` を棋譜の行に書く。

---

## 7. 終局後のUI状態遷移

**ファイル:** `src/app/gamesession.cpp`

`finishGame()`・`announceResult()` のシグナルは次の順に処理される。

### 7.1 GameSession::onGameOverStateChanged()

`gameOverStateChanged` は終局したとき（`moveAppended == false`）と、終局行を書いた後（`moveAppended == true`）の2回届く。

```
gameOverStateChanged シグナル受信
    ↓
onGameOverStateChanged(st)
    ├─ (1) 終局していなければ return（clearGameOverState() で解いたとき）
    │
    ├─ (2) 名前・時計の手番ハイライトを保護
    │      ShogiView::setGameOverStyleLock(true)
    │
    ├─ (3) 終局行の追記前（moveAppended == false）なら return
    │
    ├─ (4) 表示中の手数を最終行（終局行）に合わせる
    │      KifuNavigationCoordinator::syncNavStateToPly(rows - 1)
    │
    ├─ (5) UI遷移
    │      UiStatePolicyManager::transitionToIdle()
    │      ReplayController::setReplayMode(true)   → リプレイモードに設定（時計を止め、手番の強調を消す）
    │
    └─ (6) ハイライト消去
           ShogiView::removeHighlightAllData()
```

2回目は `appendTerminalRecord()` の中で届くので、終局行はすでに棋譜欄にある（4.2）。

### 7.2 GameSession::onGameEnded()

```
GameSession::onGameEnded(info)
    ├─ (1) 中断以外なら、ライブ対局のセッションを確定する（LiveGameSession::commit()）
    │      中断は再開できるよう確定しない（再開で中断の行を除く）
    │
    ├─ (2) 時計を止め、盤の操作を止める
    │      clock->stopClock() / ShogiView::setMouseClickMode(false) / ShogiView::update()
    │
    ├─ (3) 棋譜ビューを単一選択モードにし、リプレイモードにする
    │
    ├─ (4) 終局時刻を記録し、対局情報に反映
    │      recordGameEndTime() → TimeControlController::recordGameEndTime()
    │                          → PlayerInfoPresenter::updateGameInfoWithEndTime()
    │
    ├─ (5) 中断なら連続対局の設定を戻す（ConsecutiveGamesController::reset()）
    │
    └─ (6) UiStatePolicyManager::transitionToIdle()
```

### 7.3 GameSession::onGameEndProcessed()

結果の知らせ・自動保存の後に呼ばれる。中断では何もしない。エンジン同士の対局（`MatchCoordinator::players().engineVsEngine()`）で、連続対局の残りがあれば `ConsecutiveGamesController::startNextGame()` で次局を予約する（[対局開始フロー](game-start-flow.md) の連続対局）。

---

## 8. その他の終局条件

### 8.1 時間切れ: handlePlayerTimeOut()

**ファイル:** `src/game/matchcoordinator.cpp`

```cpp
void MatchCoordinator::handlePlayerTimeOut(Turn player)
{
    if (isGameOver()) return;
    const GameEndInfo info{Cause::Timeout, player};
    m_clock->markGameOver();
    m_clock->stopClock();
    disarmTurnTimer();
    m_engines->notifyGameOver(info.loser);
    finishGame(info);
    announceResult(info);
}
```

時計の残り時間が0になると、`ShogiClock::playerTimeOut(Turn)`（引数は時間が切れた側）から `TimeControlController::onClockPlayerTimeOut()` を経て呼ばれる。

```
ShogiClock::updateClock()（50ms ごとの tick）
    ↓ chargeElapsed(): 手番の側の残り時間を減らし、秒読みがあれば秒読みに入る
    ↓ 残りが0で、時間切れを負けにする設定のとき
ShogiClock::declareTimeout(player)
    ├─ 時計を終局状態にし、tick を止める
    └─ emit playerTimeOut(player)
    ↓  （TimeControlController::ensureClock() が接続）
TimeControlController::onClockPlayerTimeOut(Turn player)
    ↓
MatchCoordinator::handlePlayerTimeOut(Turn player)
    ├─ 時計を終局状態にして止める（markGameOver() / stopClock()）
    ├─ disarmTurnTimer()
    ├─ notifyGameOver(player)（時間が切れた側に "gameover lose"、相手に "gameover win" → quit）
    ├─ finishGame({Timeout, player})
    └─ announceResult(info) → 「%1の時間切れ。%2の勝ちです。」
```

時間切れの判定は、持ち時間に制限があり、時間切れを負けにする設定（`ShogiClock::setLoseOnTimeout(true)`）のときだけ行う（`ShogiClock::enforcesTimeout()`）。

### 8.2 中断: handleBreakOff()

**ファイル:** `src/game/matchcoordinator.cpp`

ユーザーがメニュー「対局」→「中断」をクリックした場合（`GameSession::breakOff()`）と、エンジンのエラー（8.7）で呼ばれる。CSA通信対局では `GameSession::breakOff()` が `CsaGameCoordinator::requestChudan()` に渡す。

```cpp
void MatchCoordinator::handleBreakOff()
{
    if (isGameOver()) return;
    captureInterruptedGame();
    // 時計の精算時に時間切れが確定した場合、中断に上書きしない。
    if (isGameOver()) return;
    // 中断の後に届く bestmove resign を終局にしない
    m_engines->disconnectArbiterSignals();

    const GameEndInfo info{Cause::BreakOff, m_gc->currentPlayer()};
    m_clock->markGameOver();
    m_clock->stopClock();
    disarmTurnTimer();
    // quit に応答しない場合も terminate/kill まで進める。中断後の思考表示は保持する。
    m_engines->notifyGameOver(std::nullopt);
    finishGame(info);
    if (m_interruptedGame) emit gameInterrupted();
}
```

```
MatchCoordinator::handleBreakOff()
    ├─ (1) 終局済みチェック
    │
    ├─ (2) 再開用に対局の状態を保存（captureInterruptedGame()）
    │      ShogiClock::pauseAndSnapshot() で時計を止めて状態を写し、開始の設定・対局の手順・
    │      「同」表記の基準を InterruptedGameState に入れる
    │
    ├─ (3) 時計を止めたときの精算で時間切れが確定していれば return
    │
    ├─ (4) エンジンの投了・勝ち宣言の知らせを切断
    │      EngineLifecycleManager::disconnectArbiterSignals()
    │
    ├─ (5) 時計を終局状態にして止め、計測を打ち切る
    │
    ├─ (6) エンジンへ "gameover draw" と quit（notifyGameOver(nullopt)）
    │
    ├─ (7) finishGame({BreakOff, 手番の側})
    │      → 終局行 "▲中断" / "△中断" を追記し、gameEnded
    │
    └─ (8) 再開用の状態があれば emit gameInterrupted()
           → GameSession::onGameInterrupted() が「対局再開」を使えるようにする
```

中断は結果のダイアログを出さず、自動保存もしない（`announceResult()` を呼ばない）。`GameSession::onGameEnded()` は中断のライブ対局セッションを確定しない。再開（`GameSession::resumeInterruptedGame()`）は中断の行を除いてから、保存した手順と時計で続ける（[対局開始フロー](game-start-flow.md) の「中断と再開」）。

### 8.3 最大手数到達（持将棋）: handleMaxMovesJishogi()

**ファイル:** `src/game/matchcoordinator.cpp`

`completeMove()` が一手を確定するたびに、最大手数（`MatchSessionState::maxMoves()`、0 なら無制限）を対局の手順の手数（`currentPly()`）と比べ、達していれば呼ぶ。対局モードによらず同じ判定である。

```
MatchCoordinator::handleMaxMovesJishogi()
    ├─ 終局済みチェック
    ├─ 計測を打ち切る（disarmTurnTimer()）
    ├─ エンジンへ "gameover draw" + quit（notifyGameOver(nullopt)）
    ├─ GameEndInfo構築（cause = Jishogi、loser = Turn::Black）
    ├─ finishGame(info) → 手番の側で "▲持将棋" / "△持将棋"
    └─ announceResult(info) → 「最大手数に達しました。持将棋です。」
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
           ├─ Draw                   → {Sennichite, Turn::Black}、引き分け
           ├─ ContinuousCheckByBlack → {OuteSennichite, Turn::Black}
           └─ ContinuousCheckByWhite → {OuteSennichite, Turn::White}
       成立したら disarmTurnTimer() → notifyGameOver() → finishGame() → announceResult() → true
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
DialogLauncher::handleNyugyokuDeclaration()        （src/ui/coordinators/dialoglauncher.cpp）
    ├─ 通信対局中: 自分の手番なら確認してから CsaGameCoordinator::declareWin() → return
    ↓
NyugyokuDeclarationHandler::handleDeclaration()   （src/ui/controllers/nyugyokudeclarationhandler.cpp）
    ├─ 宣言できるのは対局中の手番の人間だけ。持将棋ルール（24点法・27点法）の設定が必要
    ├─ 確認ダイアログ
    ├─ NyugyokuJudgement::judge() で宣言条件と点数を判定
    ├─ MatchCoordinator::handleNyugyokuDeclaration(declarer, success, isDraw)
    └─ 結果ダイアログを表示

エンジンの宣言: "bestmove win"
    ↓
EngineLifecycleManager::onEngineWin() → engineDeclaredWin(side)
    ↓
MatchCoordinator::handleEngineWin(side)
    ├─ NyugyokuJudgement::judgeEngineDeclaration() で盤面から判定
    ├─ handleNyugyokuDeclaration(side, success, isDraw)
    └─ 判定結果を gameOverMessage で知らせる
```

入玉宣言には3つの結果がある:

| 結果 | Cause | 敗者 |
|---|---|---|
| 引き分け（持将棋） | `Jishogi` | 宣言者（引き分け） |
| 宣言成功（勝ち） | `NyugyokuWin` | 宣言者の相手 |
| 宣言失敗（反則負け） | `IllegalMove` | 宣言者 |

`handleNyugyokuDeclaration()` の処理:
1. 計測の打ち切り・時計停止
2. GameEndInfo構築
3. `notifyGameOver()` でエンジンへ gameover 通知（引き分けは `"gameover draw"`、それ以外は勝ち負け）
4. `finishGame()` で終局を記録
5. 棋譜の自動保存の依頼（設定時）と `gameEndProcessed` の発行

結果ダイアログは呼び出し側が出すので、`announceResult()` は呼ばない。

### 8.6 エンジンの反則手: handleEngineIllegalMove()

エンジンの指し手が `ShogiGameController::applyMove()` で不合法と判定されると、`MatchCoordinator::applyEngineMove()` は盤に指さずに `handleEngineIllegalMove(エンジンの手番)` を呼ぶ（2.2）。

```
MatchCoordinator::handleEngineIllegalMove(Turn side)
    ├─ 時計を停止
    ├─ info = {IllegalMove, loser = side}
    ├─ notifyGameOver(side)            … 投了と同じく勝ち負けを知らせる
    ├─ finishGame(info)                → "▲反則負け" / "△反則負け"
    └─ announceResult(info)            → 「%1の反則負け。%2の勝ちです。」
```

### 8.7 エンジンのエラー（中断として扱う）

エンジンの異常終了・起動の失敗・bestmove の応答待ちの期限切れは、対局を中断として終える。

```
Usi::errorOccurred(message)
    （UsiMatchHandler::onSearchTimeout() の "Timeout waiting for bestmove." もここに出る。
      時計の時間切れで終局していれば出さない）
    ↓  （EngineLifecycleManager::createEngine() が、手番を持つエンジンだけ接続）
EngineLifecycleManager::engineError
    ↓  （MatchCoordinator のコンストラクタが QueuedConnection で接続。開始処理を終えてから中断する）
MatchCoordinator::handleEngineError(msg)
    ├─ 終局済み・Strategy が無いなら何もしない
    ├─ 両枠のエンジンの今の処理を取り消す
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
    ↓
MatchCoordinator::handleResign()
    ├─ info = {Resignation, loser = 手番の側}
    ├─ EngineLifecycleManager::notifyGameOver(loser)   // HvE: E1 に "gameover win/lose" + quit
    ├─ finishGame(info)
    │   ├─ MatchSessionState::finish(info)
    │   ├─ emit gameOverStateChanged(isOver=true)          ──→ (A)
    │   ├─ appendTerminalRecord(info)
    │   │   ├─ 時計を止め、投了した側の考慮時間を確定
    │   │   ├─ MatchRecordSink::appendRecord("▲投了")      ──→ (B)
    │   │   └─ markTerminalRecorded()
    │   │       └─ emit gameOverStateChanged(moveAppended=true)  ──→ (A')
    │   └─ emit gameEnded(info)                            ──→ (C)
    └─ announceResult(info)
        ├─ emit gameOverMessage("対局終了", "先手の投了。後手の勝ちです。")
        │                                    ──→ DialogCoordinator::showInformation()
        ├─ emit autoSaveRequested(dir)  // 設定時 ──→ KifuFileController::autoSaveKifuToFile()
        └─ emit gameEndProcessed(info)                     ──→ (D)

(A) GameSession::onGameOverStateChanged(st)  // moveAppended=false
    └─ setGameOverStyleLock(true) だけ行って return

(B) GameSession::appendRecord(entry)
    └─ LiveGameSession::appendRecord() → 分岐ツリーに終局ノード
        └─ moveAdded → KifuDisplayCoordinator が棋譜欄に「▲投了」の行を足す

(A') GameSession::onGameOverStateChanged(st)  // moveAppended=true
    ├─ KifuNavigationCoordinator::syncNavStateToPly(lastRow)
    ├─ UiStatePolicyManager::transitionToIdle()
    ├─ ReplayController::setReplayMode(true)
    └─ shogiView->removeHighlightAllData()

(C) GameSession::onGameEnded(info)
    ├─ LiveGameSession::commit()
    ├─ clock->stopClock() / shogiView->setMouseClickMode(false)
    ├─ 棋譜ビューを単一選択に / setReplayMode(true)
    ├─ recordGameEndTime()  // 終局時刻を記録し、対局情報に反映
    └─ UiStatePolicyManager::transitionToIdle()

(D) GameSession::onGameEndProcessed(info)
    └─ エンジン同士の連続対局なら次局を予約（投了のメニューはエンジン同士では使えない）

対局終了 → リプレイ（棋譜閲覧）モードへ
```

---

## 11. 関連ファイル一覧

| コンポーネント | ヘッダ | 実装 |
|---|---|---|
| 盤面操作コントローラ | `src/board/boardinteractioncontroller.h` | `src/board/boardinteractioncontroller.cpp` |
| ボードセットアップコントローラ（着手の振り分け） | `src/ui/controllers/boardsetupcontroller.h` | `src/ui/controllers/boardsetupcontroller.cpp` |
| 着手要求・着手確定の受け口 | `src/app/mainwindowactions.h` | `src/app/mainwindowactions.cpp` |
| 部品の生成と配線（司令塔のシグナルの接続） | `src/app/mainwindowcomposer.h` | `src/app/mainwindowcomposer_create.cpp`, `src/app/mainwindowcomposer_game.cpp`, `src/app/mainwindowcomposer_finalize.cpp` |
| 盤を人間が操作できるかの判定 | `src/app/humanturnpolicy.h` | `src/app/humanturnpolicy.cpp` |
| ゲームコントローラ | `src/game/shogigamecontroller.h` | `src/game/shogigamecontroller.cpp` |
| 対局司令塔（一手の確定・終局・待った・中断と再開） | `src/game/matchcoordinator.h` | `src/game/matchcoordinator.cpp` |
| 対局の設定・終局状態 | `src/game/matchsessionstate.h` | - |
| 棋譜の受け手のインターフェース | `src/game/matchrecordsink.h` | - |
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
| 対局の流れの UI 側（投了・中断の受け口、棋譜の受け手、終局後の UI） | `src/app/gamesession.h` | `src/app/gamesession.cpp` |
| ライブ対局のセッション | `src/kifu/livegamesession.h` | `src/kifu/livegamesession.cpp` |
| 対局の手順・棋譜の1行 | `src/kifu/recordline.h`、`src/kifu/recordentry.h` | - |
| ターンマネージャ | `src/game/turnmanager.h` | `src/game/turnmanager.cpp` |
| 手番の同期（TurnManager を持つ） | `src/app/turnstatesyncservice.h` | `src/app/turnstatesyncservice.cpp` |
| 将棋時計 | `src/core/shogiclock.h` | `src/core/shogiclock.cpp` |
| 時間制御コントローラ | `src/ui/controllers/timecontrolcontroller.h` | `src/ui/controllers/timecontrolcontroller.cpp` |
| 時計の表示 | `src/ui/presenters/timedisplaypresenter.h` | `src/ui/presenters/timedisplaypresenter.cpp` |
| USIエンジン | `src/engine/usi.h` | `src/engine/usi.cpp` |
| 対局の思考依頼・bestmove の受け取り | `src/engine/usimatchhandler.h` | `src/engine/usimatchhandler.cpp` |
| 対局時計から go の入力を作る | `src/engine/matchsearchsnapshot.h` | - |
| USIプロトコルハンドラ | `src/engine/usiprotocolhandler.h` | `src/engine/usiprotocolhandler.cpp` |
| go の時間の計算 | `src/engine/usigotime.h` | `src/engine/usigotime.cpp` |
| 対局メニューの配線 | `src/app/gameactionswiring.h` | `src/app/gameactionswiring.cpp` |
| 入玉宣言の受け口 | `src/ui/coordinators/dialoglauncher.h`、`src/ui/controllers/nyugyokudeclarationhandler.h` | `src/ui/coordinators/dialoglauncher.cpp`、`src/ui/controllers/nyugyokudeclarationhandler.cpp` |
