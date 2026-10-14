# 対局開始から投了までのフロー

本ドキュメントでは、対局が開始された後の指し手の流れ（人間の着手、エンジンの着手、時計管理）から、投了などによる終局までの一連のフローを説明する。対局が始まるまでの流れは [対局ダイアログから対局開始までのフロー](game-start-flow.md) を参照。対象はローカル対局（人間同士・人間対エンジン・エンジン同士）で、CSA 通信対局は経路が分かれる所だけを記す。

先手・後手はすべて `Turn`（`Turn::Black`／`Turn::White`、`src/core/shogitypes.h`）で表す。部品の生成と配線は `MainWindowServiceRegistry`（`src/app/gamesubregistry*.cpp`、`src/app/mainwindow*registry.cpp`）が行う。対局の司令塔 `MatchCoordinator` は一度だけ作られ（`MatchCoordinatorWiring::createMatchCoordinator()`）、対局ごとに使い回される。

## 全体フロー図

```
対局開始（configureAndStart → 時計開始 → 初手がエンジンなら go）
    │
    ▼
┌────────────────────────────────────────────────────────────┐
│ 対局ループ                                                 │
│                                                            │
│  ┌────────────────────────────────────────────────┐        │
│  │ 人間の手番                                     │        │
│  │  1. 盤のクリック（駒選択→移動先）              │        │
│  │  2. validateAndMove()（合法手判定・盤面更新）  │        │
│  │  3. Strategy::onHumanMove()                    │        │
│  │     時計の確定・棋譜追記                       │        │
│  └────────────────────────┬───────────────────────┘        │
│                           │                                │
│                           ▼                                │
│  ┌────────────────────────────────────────────────┐        │
│  │ エンジンの手番                                 │        │
│  │  1. position + go を送る（非同期）             │        │
│  │  2. bestmove 受信 → Usi::matchMoveReady        │        │
│  │  3. applyMove()（合法手判定・盤面更新）        │        │
│  │  4. 時計の確定・棋譜追記・評価値               │        │
│  └────────────────────────┬───────────────────────┘        │
│                           │                                │
│                           ▼                                │
│  着手のたびに千日手 → 最大手数（持将棋）を判定             │
│  時計の tick ごとに残り時間を判定 → 時間切れ負け           │
│  手番を交代して対局ループに戻る                            │
└────────────────────────────────────────────────────────────┘
    │
    │ 投了 / 中断 / 時間切れ / 千日手 / 持将棋 / 入玉宣言 / 反則
    ▼
┌────────────────────────────────────────────────────────────┐
│ 終局処理（GameEndHandler）                                 │
│  1. エンジンへ gameover を送る（エンジンがいるとき）       │
│  2. setGameOver() で終局状態を確定                         │
│  3. 棋譜に終局行（「▲投了」など）を追記し、閲覧モードへ    │
│  4. 結果ダイアログを表示                                   │
│  5. 棋譜を自動保存（設定時）                               │
│  6. gameEndProcessed（エンジン同士の連続対局なら次局へ）   │
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
2. 人間の手番チェック: `m_isHumanTurnCb` コールバックで現在が人間の手番かを確認し、エンジンの手番なら無視する。コールバックは `MainWindowServiceRegistry`（`src/app/mainwindowboardregistry.cpp`）が `PlayModePolicyService::isHumanTurnNow()` を呼ぶものとして設定する。
3. 1回目のクリック（駒選択）: 持ち駒のない駒台のマスは無視する。`ShogiView::startDrag()` でドラッグを始め、`selectPieceAndHighlight()` で駒を選ぶ。選んだマスにオレンジ色のハイライトを付け、合法な移動先（`EngineMoveValidator` で判定）を青色で示す（メニューで表示を切り替えられる）。相手の駒台や空きマスは選ばない。`m_waitingSecondClick = true` に設定する。
4. 2回目のクリック（移動先指定）: 同じマスならキャンセルする。別のマスなら `moveRequested(from, to)` シグナルを発行する。成り・不成のダイアログを出している間もドラッグ状態を保つため、ここではドラッグを終えない（`ShogiGameController::endDragSignal` → `ShogiView::endDrag()` で終える）。

右クリック（`onRightClick()`）は、2回目のクリック待ちなら選択を取り消す。

### 1.2 指し手のルーティング

```
BoardInteractionController::moveRequested(from, to)
    ↓  （MainWindowSignalRouter::connectMoveRequested() が接続）
MainWindow::onMoveRequested(from, to)
    ↓
MainWindowServiceRegistry::handleMoveRequested(from, to)   （src/app/mainwindowboardregistry.cpp）
    ↓
BoardSetupController::onMoveRequested(from, to)            （src/ui/controllers/boardsetupcontroller.cpp）
```

`BoardSetupController::onMoveRequested()` の処理:
- 局面編集モード（`BoardInteractionController::Mode::Edit`）なら `PositionEditController::applyEditMove()` に渡して終わる。
- 対局していない待機中なら、指した手を棋譜の変化として記録して終わる（`MainWindowServiceRegistry::canRecordVariationMove()`／`recordVariationMove()`）。
- 対局モードは `MatchCoordinator::playMode()` を使い、`NotStarted` なら自分が持つ `m_playMode` を使う。
- 次の手数を SFEN 履歴の件数（1未満なら1）とし、`ShogiGameController::validateAndMove()` を呼ぶ。
- 結果を `BoardInteractionController::onMoveApplied()` に渡してハイライトを更新する（1.4）。不合法ならここで終わる。
- 合法なら対局モードごとに後処理をする。人間同士と人間対エンジンは `MatchCoordinator::onHumanMove(hFrom, hTo, m_lastMove)`（2章。`m_lastMove` は `validateAndMove()` が作った棋譜文字列）、CSA 通信対局は `csaMoveRequested(hFrom, hTo, promote)` シグナルを発行する。

### 1.3 指し手の検証と盤面更新

**ファイル:** `src/game/shogigamecontroller.cpp`

```cpp
bool ShogiGameController::validateAndMove(QPoint& outFrom, QPoint& outTo, QString& record, int moveNumber,
                                          QStringList* sfenHistory, QList<ShogiMove>& gameMoves);
```

盤のクリック（表示の座標）による着手を検証し、合法手であれば指す。合法手の判定（`legalStatus()`）と盤への反映（`commitMove()`）は、エンジンの手を指す `applyMove()`（2章）と共通である。

#### 処理ステップ

| ステップ | 処理内容 |
|---|---|
| 入力検証 | 盤と手番があるか、移動元が盤上か手番の側の駒台か、移動元に駒があるかを確認する。座標は `BoardCoordinates::usiMoveFromSquares()` で `UsiMove` にする |
| 合法手判定 | `legalStatus()` が `EngineMoveValidator::isLegalMove()` で、成り・不成それぞれが合法かを調べる |
| 成り判定 | 下の「成り判定の詳細」のとおり。どちらも不合法なら `false` を返し、盤は変えない |
| ドラッグ終了 | `emit endDragSignal()`（→ `ShogiView::endDrag()`） |
| 盤面更新 | `commitMove()` → `ShogiBoard::applyUsiMove(move, mover)` で駒の移動・取得を反映 |
| 指し手リスト | `gameMoves` に `ShogiMove` を追加 |
| SFEN記録 | `board()->addSfenRecord(nextPlayerTurn, moveNumber, sfenHistory)` で新しい局面の SFEN を追加 |
| 棋譜文字列生成 | `convertMoveToKanjiStr()`（`KifMoveText::format()`）で日本語表記にする（例: `"▲７六歩(77)"`。直前の手と同じマスなら「同」） |
| 直前の手 | `m_lastMove` を更新する（`lastMove()` で読める。人間対エンジンではこれをエンジンに送る） |
| moveCommitted発行 | `emit moveCommitted(moverBefore, confirmedPly)`（手番変更の前、4.4） |
| 手番変更 | `setCurrentPlayer(nextPlayerTurn)` で相手に手番を移す（→ `currentPlayerChanged`、3.5） |

#### 成り判定の詳細

```
validateAndMove() の成り・不成
    ├─ 成りだけ合法（行き所のない駒など）→ 成る
    ├─ 不成だけ合法 → 成らない
    ├─ 両方合法
    │    ├─ 強制成りモード（setForcedPromotion()、定跡ウィンドウの手）→ 指定どおり
    │    └─ それ以外 → setPromotionChooser() で設定した関数で対局者に選ばせる
    │         DialogLaunchWiring::choosePromotion()
    │           → DialogCoordinator::showPromotionDialog() → PromotionFlow::askPromote()
    └─ どちらも不合法 → false
```

成りの選び方は `MainWindowSignalRouter::connectCoreSignals()` が設定する。強制成りの指定は1手かぎりで、`validateAndMove()` の始めに取り出して元に戻す。エンジンの手は成り・不成が決まった `UsiMove` で届くので、`applyMove()` は指し手どおりの成り・不成が合法かだけを確かめる。

### 1.4 着手後のハイライト更新

**ファイル:** `src/board/boardinteractioncontroller.cpp`

```
BoardInteractionController::onMoveApplied(from, to, success)
```

- 成功時: 移動元に薄い赤色、移動先に黄色のハイライトを表示し、選択中のオレンジ色のハイライトを消す。
- 失敗時（不合法手）: 選択とドラッグ状態を取り消す。

エンジンの手のハイライトは、Strategy がフック `MatchCoordinatorHooks::UI::showMoveHighlights` を呼び、`MainWindowMatchAdapter::showMoveHighlights()` → `BoardInteractionController::showMoveHighlights()` で表示する。

---

## 2. 対局モード別の着手後処理

人間の着手の後処理とエンジンの着手は、対局モードごとの Strategy（`src/game/`）が行う。

```
MatchCoordinator::onHumanMove(from, to, prettyMove)
    ↓
MatchTurnHandler::onHumanMove()          （src/game/matchturnhandler.cpp）
    ↓
GameModeStrategy::onHumanMove()          （対局モードの Strategy）
```

Strategy は対局開始時に `MatchTurnHandler::createAndStartModeStrategy()` が `PlayMode` から作る。

| PlayMode | Strategy | エンジン |
|---|---|---|
| `EvenHumanVsEngine`／`HandicapHumanVsEngine` | `HumanVsEngineStrategy`（`m_engineSide = Turn::White`） | `usi1` |
| `EvenEngineVsHuman`／`HandicapEngineVsHuman` | `HumanVsEngineStrategy`（`m_engineSide = Turn::Black`） | `usi1` |
| `HumanVsHuman` | `HumanVsHumanStrategy` | なし |
| `EvenEngineVsEngine`／`HandicapEngineVsEngine` | `EngineVsEngineStrategy` | 先手 `usi1`、後手 `usi2` |

Strategy は `MatchCoordinator::StrategyContext`（`src/game/strategycontext.h`）を通して、MatchCoordinator の状態（GC・時計・エンジン・position 文字列・手数）と、千日手・最大手数・反則の終局処理を使う。

人間対エンジンでは、エンジンは手番によらず `usi1`（`MatchCoordinator::primaryEngine()`）に入り、`usi2` は空のままになる。Strategy はエンジンの手番を `Turn m_engineSide` で持つ。

エンジンへの思考依頼は非同期である。依頼した関数はすぐに戻り、指し手は `Usi::matchMoveReady(move, position, ponder)` シグナルで Strategy の `onEngineMoveReady()` に届く。エンジンの手は `ShogiGameController::applyMove()` で合法手か確かめてから指す。

### 2.1 人間 vs エンジン（HvE）: HumanVsEngineStrategy

**ファイル:** `src/game/humanvsenginestrategy.cpp`

人間が指した後、エンジンに思考を依頼し、届いた指し手を盤に指す。対局開始時に `start()` がエンジンを `usi1` として作り、投了・勝ち宣言をエンジンの手番で配線し（5.4）、`Usi::engineInitialized` と `Usi::matchMoveReady` を自分のスロットにつなぐ。

#### 人間の着手後: onHumanMove()

```
人間の着手が確定（validateAndMove 成功）
    ↓
HumanVsEngineStrategy::onHumanMove(humanFrom, humanTo, prettyMove)
    │
    ├─ (1) 人間の手のハイライト
    │      hooks.ui.showMoveHighlights(humanFrom, humanTo)
    │
    ├─ (2) 人間の手を時計で確定し、棋譜に追記
    │      clock->commitMove(humanSide)
    │      hooks.game.appendKifuLine(prettyMove, clock->considerationAndTotalTime(humanSide))
    │      clock->setConsiderationTimeMs(humanSide, 0)
    │      pokeTimeUpdateNow()
    │
    ├─ (3) 手番表示をエンジン側にする
    │      updateTurnDisplay(m_engineSide)
    │
    └─ (4) onHumanMoveEngineReply(humanFrom, humanTo)
           ├─ 手数（currentMoveIndex）を SFEN 履歴の件数 - 1 に合わせる
           ├─ 人間の計測タイマーを止める（finishHumanTimerInternal()）
           ├─ エンジンに直前の手の移動先を知らせる（読み筋の「同」表記用）
           │      eng->setPreviousFileTo() / setPreviousRankTo()
           ├─ 千日手チェック: checkAndHandleSennichite() → 成立なら return（8.4）
           ├─ 最大手数チェック: currentMoveIndex >= maxMoves
           │      └─ 到達: handleMaxMovesJishogi() → return（8.3）
           ├─ エンジンの手番でなければ、人間のタイマーを動かして return
           └─ エンジンに思考を依頼（非同期）
                  m_waitingForMove = true
                  eng->requestHumanReply(positionStr1, positionPonder1, gc->lastMove(),
                                         timing, positionStrHistory)
```

`timing` は既定値の `UsiTimingParams` で、思考時間はエンジンにつないだ対局時計から思考を始める時点で求める（3.4）。

#### USI通信の詳細

思考の依頼と bestmove の受け取りは `src/engine/usi_async.cpp`、`src/engine/usimatchhandler.cpp`、`src/engine/usiprotocolhandler.cpp` にある。

1. `Usi::requestHumanReply()`: position 文字列に人間の手を足し（例: `position startpos moves 7g7f`）、履歴に積んで `requestMatchMove()` を呼ぶ。
2. `Usi::requestMatchMove()`: エンジンが初期化中なら、終わるまで依頼を保留する。対局時計（`Usi::setMatchClock()` でつないだ `ShogiClock`）を `UsiMatchHandler` に渡す。
3. `UsiMatchHandler::requestMove()`: 時計を精算し、終局していれば何もしない。先読み（`go ponder`）中なら、予想が当たれば `ponderhit` を送り、外れなら `stop` を送って先読みの bestmove を受けてから探索し直す。そうでなければ `startAsyncSearch()` を呼ぶ。
4. `UsiMatchHandler::startAsyncSearch()`: `position` と `go`（`UsiGoTimeCalculator` が求めた時間、3.4）を送り、応答待ちのタイマー（時間切れのある対局では今の手に使える残り時間）を始める。
5. bestmove 受信: `UsiProtocolHandler::handleBestMoveLine()` が考慮時間（`go` または `ponderhit` からの経過）を記録し、`resign`・`win` なら専用のシグナルを出す（5.4、8.5）。`UsiMatchHandler::onBestMoveReceived()` は `ShogiClock::finishTurn()` で時計を精算し、通常の手なら position に bestmove を加え、予想手があれば先読み（`go ponder`）を始めてから、`Usi::matchMoveReady(move, position, ponder)` を出す。

#### エンジンの指し手: onEngineMoveReady()

```
Usi::matchMoveReady(move, position, ponder)
    ↓
HumanVsEngineStrategy::onEngineMoveReady(move, position, ponder)
    │
    ├─ (1) 待っていた手か確かめる（m_waitingForMove、送り元が usi1、終局していない）
    │
    ├─ (2) position 文字列を更新（positionStr1 / positionPonder1）
    │
    ├─ (3) エンジンの指し手を盤面に適用（合法手か確かめる）
    │      gc->applyMove(move, rec, nextIdx, sfenHistory, gameMoves)
    │      └─ 反則手: handleEngineIllegalMove(m_engineSide) → return（8.6）
    │
    ├─ (4) currentMoveIndex を進め、ハイライトを表示
    │      hooks.ui.showMoveHighlights(eFrom, eTo)
    │
    ├─ (5) エンジンの考慮時間を時計に確定
    │      thinkMs = eng->lastBestmoveElapsedMs()
    │      clock->setMeasuredConsiderationTime(engineSide, thinkMs)
    │      clock->commitMove(engineSide)
    │
    ├─ (6) 棋譜にエンジンの指し手を追記
    │      hooks.game.appendKifuLine(rec, clock->considerationAndTotalTime(engineSide))
    │
    ├─ (7) 盤面描画・手番表示を更新
    │      hooks.ui.renderBoardFromGc()
    │      updateTurnDisplay(gc->currentPlayer())
    │
    ├─ (8) 評価値グラフに追記
    │      hooks.game.appendEval(m_engineSide)
    │
    ├─ (9) 千日手チェック → 最大手数チェック
    │
    └─ (10) 人間タイマー再開
           armTurnTimerIfNeeded()
```

#### エンジンが先に指す場合

エンジンが先に指すとき（エンジンが先手のときと、駒落ちでエンジンが上手のとき）は、対局開始の処理（`GameStartCoordinator`）が `MatchCoordinator::startInitialEngineMoveIfNeeded()` を呼ぶ。

```
MatchCoordinator::startInitialEngineMoveIfNeeded()
    ↓
MatchTurnHandler::startInitialEngineMoveIfNeeded()
    ↓
HumanVsEngineStrategy::startInitialMoveIfNeeded()
    ├─ 終局済み・思考待ち・エンジンの手番でないなら return
    ├─ エンジンが初期化中なら時計を止めて待つ
    │      → Usi::engineInitialized → onEngineInitialized() が時計を再開して、もう一度呼ぶ
    └─ startInitialEngineMove()
           eng->requestMatchMove(positionStr1, positionPonder1, timing)
```

指し手は同じく `onEngineMoveReady()` に届く。

### 2.2 人間 vs 人間（HvH）: HumanVsHumanStrategy::onHumanMove()

**ファイル:** `src/game/humanvshumanstrategy.cpp`

```cpp
void HumanVsHumanStrategy::onHumanMove(const QPoint& /*from*/,
                                        const QPoint& /*to*/,
                                        const QString& prettyMove)
{
    // 着手後の現在手番は「次の手番」なので、着手者はその逆
    const Turn curAfterMove = m_ctx.gc() ? m_ctx.gc()->currentPlayer() : Turn::White;
    const Turn mover = (curAfterMove == Turn::Black) ? Turn::White : Turn::Black;

    // 直前手の消費時間（consideration）を確定
    finishTurnTimerAndSetConsideration(mover);

    // HvH でも秒読み/インクリメントを適用し、総考慮へ加算して表示値を確定
    ShogiClock* clock = m_ctx.clock();
    if (clock) clock->commitMove(mover);

    // 指し手＋考慮時間を棋譜欄に追記する。千日手・持将棋の終局行より先に記録する
    if (m_ctx.hooks().game.appendKifuLine) {
        QString elapsed = QStringLiteral("00:00/00:00:00");
        if (clock) {
            elapsed = clock->considerationAndTotalTime(mover);
        }
        m_ctx.hooks().game.appendKifuLine(prettyMove, elapsed);
    }

    // 表示更新（時計ラベル等）
    if (clock) m_ctx.pokeTimeUpdateNow();

    // 千日手チェック（SFENは呼び出し前に追加済み）
    if (m_ctx.checkAndHandleSennichite()) return;

    // 最大手数チェック
    const QStringList* rec = m_ctx.sfenHistory();
    const int movesPlayed = rec ? static_cast<int>(rec->size() - 1) : m_ctx.currentMoveIndex();
    if (m_ctx.maxMoves() > 0 && movesPlayed >= m_ctx.maxMoves()) {
        m_ctx.handleMaxMovesJishogi();
        return;
    }

    // 次手番の計測と UI 準備
    armTurnTimerIfNeeded();
}
```

HvHはエンジン通信がないため、処理はシンプル:
1. 着手者の考慮時間を確定（3.2）
2. 秒読み/フィッシャー加算を適用（3.3）
3. 棋譜に追記し、時計表示を更新
4. 千日手・最大手数を判定（8.3、8.4）
5. 次の手番のタイマーを開始

### 2.3 エンジン vs エンジン（EvE）: EngineVsEngineStrategy

**ファイル:** `src/game/enginevsenginestrategy.cpp`

EvEの対局は、エンジンの指し手を受けるたびに `QTimer::singleShot(0, ...)` で次の手番を予約するループで進行する。エンジンは `MatchTurnHandler::createAndStartModeStrategy()` が `initEnginesForEvE()` で作り（`usi1` が先手、`usi2` が後手）、`initializeAndStartEngineFor()` で起動する。

```
EngineVsEngineStrategy::start()
    ├─ 時計を開始し、手番を GC に合わせる
    ├─ usi1/usi2 の engineInitialized → kickNextEvETurn、matchMoveReady → onEngineMoveReady を接続
    └─ kickNextEvETurn()

kickNextEvETurn()
    │
    ├─ (1) 思考待ち・終局済み・モード違い・エンジンなしなら何もしない
    │
    ├─ (2) どちらかのエンジンが初期化中なら時計を止めて待つ
    │      （engineInitialized で再び呼ばれ、時計を再開する）
    │
    ├─ (3) 現在の手番を判定
    │      p1ToMove = (gc->currentPlayer() == Turn::Black)
    │      mover    = p1ToMove ? usi1 : usi2
    │
    └─ (4) エンジンに思考を依頼（非同期）
           m_waitingForMove = true
           mover->requestMatchMove(pos, ponder, timing)

onEngineMoveReady(move, position, ponder)   ← Usi::matchMoveReady
    │
    ├─ (1) 送り元が手番のエンジンか確かめ、その側の position 文字列を更新
    │
    ├─ (2) 指し手を盤面に適用
    │      gc->applyMove(move, rec, m_eveMoveIndex + 1, sfenRecordForEvE(), gameMovesForEvE())
    │      └─ 反則手: handleEngineIllegalMove(moverTurn) → return（8.6）
    │      m_eveMoveIndex++
    │
    ├─ (3) 相手側のposition文字列を同期
    │      p1ToMove なら positionStr2 = positionStr1
    │      そうでなければ positionStr1 = positionStr2
    │
    ├─ (4) 考慮時間を時計に確定
    │      clock->setMeasuredConsiderationTime(moverTurn, mover->lastBestmoveElapsedMs())
    │      clock->commitMove(moverTurn)
    │
    ├─ (5) 棋譜追記
    │      hooks.game.appendKifuLine(rec, clock->considerationAndTotalTime(moverTurn))
    │
    ├─ (6) 相手エンジンに前回の指し手座標を通知
    │      receiver->setPreviousFileTo() / setPreviousRankTo()
    │
    ├─ (7) 盤面描画・ハイライト・手番表示
    │      hooks.ui.renderBoardFromGc()
    │      hooks.ui.showMoveHighlights(from, to)
    │      updateTurnDisplay(gc->currentPlayer())
    │
    ├─ (8) 千日手チェック → 最大手数チェック（総手数 = SFEN 履歴の件数 - 1）
    │
    └─ (9) 次のターンをスケジュール
           QTimer::singleShot(0, this, &EngineVsEngineStrategy::kickNextEvETurn)
```

`QTimer::singleShot(0, ...)` により、Qtのイベントループに制御を返してから次のターンを始める。思考の依頼も非同期なので、エンジンが考えている間も UI は止まらない。

エンジン同士の評価値グラフは Strategy では更新せず、着手確定の通知（4.4）を受けた `BoardSetupController::onMoveCommitted()` が描き直す。

---

## 3. 時計管理

**ファイル:** `src/core/shogiclock.cpp`

`ShogiClock` は先手・後手の状態（残り時間・考慮時間・総考慮時間・秒読みに入っているか）を `SideState` にまとめて持ち、API はすべて `Turn` で受ける。50ms ごとの tick（`updateClock()`）で、手番の側の残り時間と考慮時間を進める。時計は `TimeControlController`（`src/ui/controllers/timecontrolcontroller.cpp`）が作って持ち、`MatchCoordinator` は `MatchTimekeeper`（`src/game/matchtimekeeper.cpp`）を通して参照する。持ち時間の決まりと表示の仕様は [時間設定の仕様](game-time-control-spec.md) を参照。

### 3.1 時間計測の仕組み

対局モードごとに異なる方法で考慮時間を計測する。

| モード | 計測方法 | 計測開始 | 計測終了 |
|---|---|---|---|
| HvH | `QElapsedTimer`（`HumanVsHumanStrategy::m_turnTimer`） | `armTurnTimerIfNeeded()` | `finishTurnTimerAndSetConsideration()` → `ShogiClock::setMeasuredConsiderationTime()` |
| HvE（人間） | 時計自身の計時（tick で手番の側の考慮時間に足す） | 時計の手番が人間に移ったとき（`ShogiClock::setCurrentPlayer()`） | 時計の手番がエンジンに移ったとき |
| HvE（エンジン） | `Usi::lastBestmoveElapsedMs()` | go（または ponderhit）送信時 | bestmove受信時 → `setMeasuredConsiderationTime()` |
| EvE | `Usi::lastBestmoveElapsedMs()` | go（または ponderhit）送信時 | bestmove受信時 → `setMeasuredConsiderationTime()` |

人間同士でも、対局開始時には `m_turnTimer` を動かさないので、初手は時計自身が数えた考慮時間を使う。`setMeasuredConsiderationTime()` は、中断した対局を再開した手番では、中断前に使っていた考慮時間を足して確定する。

### 3.2 タイマー管理メソッド

MatchCoordinator の次のメソッドは、MatchTurnHandler を通して Strategy に渡す。エンジン同士の Strategy では何もしない（`GameModeStrategy` の既定の空の実装）。

| MatchCoordinator | Strategy |
|---|---|
| `armTurnTimerIfNeeded()` | `armTurnTimerIfNeeded()` |
| `finishTurnTimerAndSetConsiderationFor(Turn mover)` | `finishTurnTimerAndSetConsideration(Turn mover)` |
| `disarmHumanTimerIfNeeded()` | `disarmTurnTimer()` |

`disarmHumanTimerIfNeeded()` は、終局処理（`GameEndHandler`、`GameStateController::onMatchGameEnded()`）で計測を打ち切るのに使う。

#### HvH用

```cpp
// タイマー開始
void HumanVsHumanStrategy::armTurnTimerIfNeeded()
{
    if (!m_turnTimerArmed) {
        m_turnTimer.start();
        m_turnTimerArmed = true;
    }
}

// タイマー停止・考慮時間確定
void HumanVsHumanStrategy::finishTurnTimerAndSetConsideration(Turn mover)
{
    if (!m_turnTimerArmed) return;

    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;

    ShogiClock* clock = m_ctx.clock();
    if (clock) clock->setMeasuredConsiderationTime(mover, ms);
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}
```

`disarmTurnTimer()` は「待った」や終局で計測を打ち切り、次の `armTurnTimerIfNeeded()` で測り直す。

#### HvE用

```cpp
// 人間タイマー開始
void HumanVsEngineStrategy::armTurnTimerIfNeeded()
{
    if (!m_humanTimerArmed) {
        m_humanTurnTimer.start();
        m_humanTimerArmed = true;
    }
}

// 人間タイマー停止（onHumanMoveEngineReply()・finishTurnTimerAndSetConsideration() から呼ばれる）
void HumanVsEngineStrategy::finishHumanTimerInternal()
{
    // 考慮時間は ShogiClock が計測済みなので、ここでは人間用の計測タイマーを止めるだけ
    if (m_humanTurnTimer.isValid()) m_humanTurnTimer.invalidate();
    m_humanTimerArmed = false;
}
```

HvE では人間の考慮時間を時計が数えるので、人間タイマーの停止は考慮時間を設定しない。`disarmTurnTimer()` は終局時等に人間タイマーを無効化する。

### 3.3 秒読み/フィッシャー加算の適用

各着手後に、指した側の `Turn` で `ShogiClock::commitMove()` を呼ぶ:

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

棋譜欄に書く考慮時間は、確定した後に `considerationAndTotalTime(player)`（`"MM:SS/HH:MM:SS"`）で読む。

### 3.4 USI goコマンドの時間パラメータ計算

**ファイル:** `src/engine/usigotime.cpp` - `UsiGoTimeCalculator::compute()`

対局の種類（人間対エンジン、エンジン同士、CSA通信対局）によらず、この計算だけで `go` の時間を求める。入力（`UsiGoTimeInput`）は時間の決まり（`TimeControl`）と先後の残り時間で、`UsiMatchHandler::goTimeInput()` が作る。人間対エンジン・エンジン同士では、エンジンにつないだ時計（`MatchCoordinator::initializeAndStartEngineFor()` が `Usi::setMatchClock()` で設定）から、思考を始める時点の残り時間を読む。CSA通信対局ではエンジンに時計をつながず、`CsaMoveProgressHandler` がサーバーの残り時間を入れた `UsiTimingParams` を使う。

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

着手で GC の手番が変わると、次の経路で時計の手番も変わる。

```
ShogiGameController::setCurrentPlayer(next)      （commitMove() の最後）
    ↓ currentPlayerChanged(Turn)
TurnManager::set(Turn)                           （TurnSyncBridge::wire() が接続）
    ↓ changed(Turn)
MainWindow::onTurnManagerChanged(Turn)
    ↓
MainWindowServiceRegistry::onTurnManagerChanged(Turn)
    ↓
TurnStateSyncService::onTurnManagerChanged(Turn)  （src/app/turnstatesyncservice.cpp）
    ├─ MainWindowServiceRegistry::updateTurnStatus(now) → ShogiClock::setCurrentPlayer(now)
    ├─ ShogiGameController::setCurrentPlayer(now)   （同じ手番なら何もしない）
    └─ ShogiView::updateTurnIndicator(now)
```

`ShogiClock::setCurrentPlayer()` は、前回の tick からの経過時間を前の手番の側に精算してから手番を切り替え、新しい手番の側の考慮時間を0から数え直す。Strategy が呼ぶ `MatchTurnHandler::updateTurnDisplay()`（フック `MatchCoordinatorHooks::UI::updateTurnDisplay`）も `MainWindow::onTurnManagerChanged()` を呼ぶので、同じ処理になる。

エンジンの bestmove を受け取ったときは、`UsiMatchHandler::onBestMoveReceived()` が `ShogiClock::finishTurn()` で時計を精算し、手番が切り替わるまでエンジンの側の時間を減らさない。

### 3.6 時計のUI更新

```
ShogiClock::timeUpdated()
    ↓
MatchTimekeeper::onClockTick() → emitTimeUpdateFromClock()
    ↓
MatchCoordinator::timeUpdated(blackMs, whiteMs, sideToMove, urgencyMs)
    ↓  （MatchCoordinatorWiring::bindRuntimeDependencies() が接続）
TimeDisplayPresenter::onMatchTimeUpdated(blackMs, whiteMs, sideToMove, urgencyMs)
```

`ShogiClock::timeUpdated()` は、tick で表示する秒が変わったときと、`commitMove()`・`startClock()`・`stopClock()` などで状態が変わったときに出る。着手の直後などすぐに表示を合わせたいときは、`MatchCoordinator::pokeTimeUpdateNow()` が今の値で `timeUpdated` を出す。

```cpp
void MatchTimekeeper::emitTimeUpdateFromClock()
{
    if (!m_clock || !m_refs.gc) return;

    const qint64 blackMs = m_clock->remainingMs(Turn::Black);
    const qint64 whiteMs = m_clock->remainingMs(Turn::White);
    const Turn side = m_refs.gc->currentPlayer();

    const bool enableUrgency = !m_clock->hasByoyomi(side) || m_clock->byoyomiApplied(side);

    const qint64 activeMs = (side == Turn::Black) ? blackMs : whiteMs;
    const qint64 urgencyMs = enableUrgency ? activeMs
                                           : std::numeric_limits<qint64>::max();

    emit timeUpdated(blackMs, whiteMs, side, urgencyMs);
}
```

`TimeDisplayPresenter::onMatchTimeUpdated()` の処理:
- 先手・後手の時計の文字列を盤に設定する（`ShogiView::setBlackClockText()`／`setWhiteClockText()`）。制限ありは残り時間 `HH:MM:SS`（秒は切り上げ）、無制限は「消費 HH:MM:SS」
- 手番の側（`ShogiView::setActiveIsBlack()`）と、残り時間・秒読みから決めた警告の段階（`ShogiView::setUrgencyVisuals()`）を設定する。引数の `urgencyMs` は使わず、警告の段階は時計から決める

---

## 4. 棋譜記録

### 4.1 SFEN記録の更新

各着手時に `ShogiGameController::commitMove()`（`validateAndMove()`・`applyMove()` の共通部分）で更新される:

```cpp
board()->addSfenRecord(nextPlayerTurn, moveNumber, sfenHistory);
```

SFEN の手数フィールドは `moveNumber + 1` になる。SFENリストには各手番後の局面が順に追加される:
```
[0] "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"  (初期局面)
[1] "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"  (1手目 ▲７六歩 の後)
[2] ...
```

SFEN 履歴は `MainWindow` の `m_kifu.sfenRecord` で、`MatchCoordinator::Deps::sfenRecord` として共有される。

### 4.2 棋譜行の追記

Strategy は MatchCoordinator のフック経由で棋譜欄に1行追記する（エンジン同士の例）:

```cpp
m_ctx.hooks().game.appendKifuLine(rec, m_ctx.clock()->considerationAndTotalTime(moverTurn));
```

- `rec`: 指し手の日本語表記（例: `"▲７六歩(77)"`）
- 第2引数: 考慮時間と累計時間（例: `"00:03/00:00:06"`）

フックは `MainWindowServiceRegistry::buildMatchHooks()`（`src/app/gamesubregistry_wiring.cpp`）が設定し、`GameRecordUpdateService::appendKifuLine()` を呼ぶ。`GameRecordUpdateService` は棋譜欄（`GameRecordPresenter::appendMoveLine()`）とライブ対局セッション（`LiveGameSessionUpdater::appendMove()`）に追記する。終局行の追記が済んだ後は何も追記しない。

### 4.3 指し手リストの管理

`MatchCoordinator`（`src/game/matchcoordinator.h`）のメンバー:

```cpp
QList<ShogiMove> m_gameMoves;          ///< 対局中の指し手リスト
QList<ShogiMove>* m_externalGameMoves = nullptr; ///< 外部共有の指し手リスト（非所有、Deps経由）
int m_currentMoveIndex = 0;              ///< 現在の手数インデックス
```

指し手リストは `gameMovesRef()` で読み、外部共有のリスト（`Deps::gameMoves`、`MainWindow` の `m_kifu.gameMoves`）があればそちらを使う。`configureAndStart()` は、指し手リストを SFEN 履歴の手数までに切り詰める（`alignGameMovesWithSfenHistory()`）。

手数の数え方は Strategy ごとに違う。人間対エンジンは `m_currentMoveIndex` を SFEN 履歴の件数 - 1 に合わせて進め、人間同士は SFEN 履歴の件数 - 1 を使う。エンジン同士は `EngineVsEngineStrategy::m_eveMoveIndex` を `applyMove()` に渡す手数に使い、最大手数は SFEN 履歴の件数 - 1 で判定する。

### 4.4 着手確定の通知（moveCommitted）

`ShogiGameController::moveCommitted(Turn mover, int ply)` は手番を交代する前に出る。

```
ShogiGameController::moveCommitted(mover, ply)
    ├─ MainWindow::onMoveCommitted()   （MainWindowSignalRouter::connectCoreSignals() が接続）
    │    ↓
    │  MainWindowServiceRegistry::handleMoveCommitted()
    │    ├─ BoardSetupController::onMoveCommitted()   … エンジン同士なら評価値グラフを描き直す
    │    ├─ GameRecordUpdateService::updateCurrentSfenAfterMove()   … 現在局面の SFEN を更新
    │    └─ 定跡ウィンドウを更新
    └─ PieceSoundPlayer::playMoveSound()   … 駒音
```

人間対エンジンの評価値グラフは、エンジンの手の後に Strategy がフック `MatchCoordinatorHooks::Game::appendEval(Turn side)` を呼んで追記する（先手のエンジンなら `EvaluationGraphController::redrawEngine1Graph()`、後手なら `redrawEngine2Graph()`）。

---

## 5. 投了フロー

### 5.1 投了の発動

**ファイル:** `src/ui/wiring/gameactionswiring.cpp`

```cpp
QObject::connect(ui->actionResign,       &QAction::triggered, gso, &GameSessionOrchestrator::handleResignation, Qt::UniqueConnection);
```

ユーザーがメニュー「対局」→「投了」をクリックすると発動する。確認ダイアログは表示されない（即座に投了処理が実行される）。投了のメニューが有効になるのは、人間が参加する対局（人間同士・人間対エンジン）の対局中と、CSA 通信対局中だけである（`UiStatePolicyManager::effectivePolicy()`）。

### 5.2 投了のルーティング

```
GameSessionOrchestrator::handleResignation()    （src/app/gamesessionorchestrator.cpp）
    ├─ CSA通信対局: CsaGameCoordinator::onResign() → return
    ↓
GameStateController::handleResignation()        （src/game/gamestatecontroller.cpp）
    ↓
MatchCoordinator::handleResign()                （src/game/matchcoordinator.cpp）
    ↓
GameEndHandler::handleResign()                  （src/game/gameendhandler.cpp）
```

`MatchCoordinator` の終局処理のメソッドは、遅延生成した `GameEndHandler`（`ensureGameEndHandler()`）にそのまま渡す。`GameEndHandler` のシグナル（`gameOverStateChanged`・`requestAppendGameOverMove`・`gameEnded`・`gameEndProcessed`）は、同じ名前の `MatchCoordinator` のシグナルとして出し直される。

### 5.3 GameEndHandler::handleResign()

**ファイル:** `src/game/gameendhandler.cpp`

投了処理の中核メソッド。

```cpp
void GameEndHandler::handleResign()
{
    if (m_refs.gameOver->isOver) {
        qCDebug(lcGame) << "handleResign: already game over, ignoring";
        return;
    }

    GameEndInfo info;
    info.cause = Cause::Resignation;
    info.loser = (m_refs.gc && m_refs.gc->currentPlayer() == Turn::Black)
                     ? Turn::Black : Turn::White;

    auto rawSender = [this](Usi* which, const QString& cmd) {
        sendRawToEngineHelper(which, cmd);
    };
    Usi* u1 = m_refs.usi1Provider ? m_refs.usi1Provider() : nullptr;
    Usi* u2 = m_refs.usi2Provider ? m_refs.usi2Provider() : nullptr;
    EngineGameOverNotifier::notifyResignation(
        *m_refs.playMode, info.loser, u1, u2, rawSender);

    setGameOver(info, true);
    displayResultsAndUpdateGui(info);
}
```

#### 処理ステップ

1. 終局済みチェック: 終局済みなら何もしない。
2. GameEndInfoの構築: 投了の敗者は現在の手番の側（`gc->currentPlayer()`）。
3. エンジンへの終了通知: `EngineGameOverNotifier::notifyResignation()`（下表）。
4. 終局状態の確定: `setGameOver(info, true)`（6.1）。
5. 結果ダイアログの表示: `displayResultsAndUpdateGui(info)`（6.2）。

#### エンジンへの終了通知

**ファイル:** `src/game/enginegameovernotifier.cpp`

`notifyResignation(playMode, Turn loser, usi1, usi2, sendRaw)` は、投了・エンジンの投了・エンジンの反則手で共通に使う。

| モード | 通知内容 |
|---|---|
| HvE | `usi1` に、エンジンが負けなら `"gameover lose"`、勝ちなら `"gameover win"` |
| EvE | 負けたエンジン（先手なら `usi1`、後手なら `usi2`）に `"gameover lose"`、勝ったエンジンに `"gameover win"` |
| HvH | エンジン通知なし |

どの通知でも、エンジンの今の処理を取り消して `stop` を送り、`gameover` を送ったあと `Usi::cleanupEngineProcessAndThread(false)` で `quit` を送ってプロセスを終了させる（思考情報の表示は残す）。人間対エンジンのエンジンの手番は対局モードから決める（`EvenEngineVsHuman`／`HandicapEngineVsHuman` なら先手、それ以外は後手）。

### 5.4 エンジン投了: handleEngineResign(Turn side)

**ファイル:** `src/game/enginelifecyclemanager.cpp`、`src/game/gameendhandler.cpp`

エンジンが `bestmove resign` を返した場合の処理。

```
USIエンジンが "bestmove resign" を送信
    ↓
UsiProtocolHandler::handleBestMoveLine() が bestMoveResignReceived シグナルを発行
    （先読みの結果、setSquelchResignLogging(true) の後、2回目以降は発行しない）
    ↓
Usi::bestMoveResignReceived
    ↓
wireResignToArbiter(engine, side) で接続されたスロットが呼ばれる
    ├─ 先手のエンジンの場合: EngineLifecycleManager::onBlackEngineResign()
    └─ 後手のエンジンの場合: EngineLifecycleManager::onWhiteEngineResign()
    ↓  EngineLifecycleManager::Hooks::onEngineResign(side)
MatchCoordinator::handleEngineResign(side)
    ↓
GameEndHandler::handleEngineResign(side)   ← side はエンジンの手番（Turn）
    ├─ 時計を停止（ShogiClock::stopClock()）
    ├─ info = {Resignation, loser = side}
    ├─ EngineGameOverNotifier::notifyResignation()
    │      投了したエンジンに "gameover lose"（エンジン同士なら相手に "gameover win"）、どちらも quit
    ├─ 両エンジンの投了通知を抑止（setSquelchResignLogging(true)）
    ├─ setGameOver(info, true)
    └─ displayResultsAndUpdateGui(info)
```

配線:
- 人間対エンジン: `HumanVsEngineStrategy::start()` が `wireResignToArbiter(usi1, m_engineSide)` で配線する。エンジンが後手でもエンジンは `usi1` に入るが、配線はエンジンの手番で行う（`initializeAndStartEngineFor(m_engineSide, ...)` も同じ手番で配線し直す）。
- エンジン同士: `EngineLifecycleManager::initEnginesForEvE()` が `usi1` を先手、`usi2` を後手として配線する。
- 入玉宣言勝ちの `bestmove win` も同じく `wireWinToArbiter()` で `onBlackEngineWin()`／`onWhiteEngineWin()` → `MatchCoordinator::handleEngineWin(side)` に届く（8.5）。
- 中断のときは `EngineLifecycleManager::disconnectArbiterSignals()` で両方の接続を外す（8.2）。

`resign`・`win` の bestmove は `Usi::matchMoveReady` では知らせない（`UsiMatchHandler::onBestMoveReceived()` が特殊な指し手を除く）。

---

## 6. 終局状態管理

### 6.1 setGameOver()

**ファイル:** `src/game/gameendhandler.cpp`

終局を確定させる入口で、`GameEndHandler` の各終局処理が呼ぶ（`MatchCoordinator::setGameOver()` はこれに渡すだけ）。中断（8.2）だけは `handleBreakOff()` が終局状態を直接設定する。

```cpp
void GameEndHandler::setGameOver(const GameEndInfo& info, bool appendMoveOnce)
{
    if (m_refs.gameOver->isOver) {
        qCDebug(lcGame) << "setGameOver() ignored: already over";
        return;
    }

    m_refs.gameOver->isOver        = true;
    m_refs.gameOver->hasLast       = true;
    m_refs.gameOver->lastInfo      = info;
    m_refs.gameOver->when          = QDateTime::currentDateTime();

    emit gameOverStateChanged(*m_refs.gameOver);

    if (appendMoveOnce && !m_refs.gameOver->moveAppended) {
        qCDebug(lcGame) << "emit requestAppendGameOverMove";
        emit requestAppendGameOverMove(info);
    }
    // 終局手の追記・ライブセッションの確定後に時刻と UI を更新する。
    emit gameEnded(info);
}
```

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

受信先は、司令塔を作るときに `MatchCoordinatorWiring::connectMatchSignals()` が `MatchCoordinator` のシグナルへ直接つなぐ。同じシグナルの受信先は、つないだ順（表の受信先の並び）に呼ばれる。発行の順は表の上から下である。

| シグナル | 受信先 | 処理内容 |
|---|---|---|
| `gameOverStateChanged` | `GameStateController::onGameOverStateChanged()` | 終局スタイルの保護。終局行の追記前はここで戻る（7.2） |
| `requestAppendGameOverMove` | `GameStateController::onRequestAppendGameOverMove()`、続いて `SessionLifecycleCoordinator::commitLiveGameSessionIfActive()` | 終局行の棋譜追記（1回限り、7.1）、ライブ対局セッションの確定 |
| `gameEnded` | `SessionLifecycleCoordinator::handleGameEnded()`、続いて `UiStatePolicyManager::transitionToIdle()` | 盤の操作・時計の停止、終局時刻の記録（7.3）、UI状態を待機に戻す |

### 6.2 displayResultsAndUpdateGui()

**ファイル:** `src/game/gameendhandler_record.cpp`

結果表示と棋譜の自動保存。

```cpp
void GameEndHandler::displayResultsAndUpdateGui(const GameEndInfo& info)
{
    const QString msg = resultMessage(info);
    if (m_hooks.showGameOverDialog) m_hooks.showGameOverDialog(tr("対局終了"), msg);
    qCDebug(lcGame) << "Game ended";

    if (m_hooks.autoSaveKifuIfEnabled) {
        m_hooks.autoSaveKifuIfEnabled();
    }
    emit gameEndProcessed(info);
}
```

- `showGameOverDialog` は `MainWindowMatchAdapter::showGameOverMessageBox()` → `DialogCoordinator::showGameOverMessage()`（`QMessageBox::information`）で表示する。
- `autoSaveKifuIfEnabled` は、自動保存が有効で保存先があるとき、フック `MatchCoordinatorHooks::Game::autoSaveKifu` → `KifuFileController::autoSaveKifuToFile()` で保存する。
- `gameEndProcessed` は `SessionLifecycleCoordinator::handleGameEndProcessed()` が受け、エンジン同士の連続対局なら次局を始める（中断では何もしない）。

結果メッセージは `resultMessage()` が作る。駒落ちで対局情報の見出しが下手・上手のときは、先手・後手の代わりに下手・上手と呼ぶ。

| 終局原因 | メッセージ |
|---|---|
| `Resignation` | 「%1の投了。%2の勝ちです。」（%1 は敗者、%2 は勝者） |
| `Timeout` | 「%1の時間切れ。%2の勝ちです。」 |
| `Jishogi` | 「最大手数に達しました。持将棋です。」 |
| `NyugyokuWin` | 「%1の入玉宣言。%2の勝ちです。」（どちらも勝者） |
| `IllegalMove` | 「%1の反則負け。%2の勝ちです。」 |
| `Sennichite` | 「千日手が成立しました。」 |
| `OuteSennichite` | 「%1の連続王手の千日手。%2の勝ちです。」 |
| `BreakOff` | 「対局が終了しました。」 |

### 6.3 棋譜への終局手追記: appendGameOverLineAndMark()

棋譜欄に終局手（「投了」「時間切れ」等）を1行だけ追記する。`GameStateController::setGameOverMove()`（7.1）が `MatchCoordinator::appendGameOverLineAndMark(cause, loser)` を通して呼ぶ。

#### 終局手の棋譜表記

| 終局原因 | 棋譜表記例 | 印（▲/△）の側 |
|---|---|---|
| 投了 (Resignation) | `"▲投了"` / `"△投了"` | 敗者 |
| 時間切れ (Timeout) | `"▲時間切れ"` / `"△時間切れ"` | 敗者 |
| 反則負け (IllegalMove) | `"▲反則負け"` / `"△反則負け"` | 敗者 |
| 中断 (BreakOff) | `"▲中断"` / `"△中断"` | 中断したときの手番の側 |
| 入玉勝ち (NyugyokuWin) | `"▲入玉勝ち"` / `"△入玉勝ち"` | 勝者（宣言した側） |
| 持将棋 (Jishogi) | `"▲持将棋"` / `"△持将棋"` | 終局した局面の手番の側 |
| 千日手 (Sennichite) | `"▲千日手"` / `"△千日手"` | 終局した局面の手番の側 |
| 連続王手の千日手 (OuteSennichite) | `"▲反則負け"` / `"△反則勝ち"` など | 終局した局面の手番の側。王手を続けた側が手番なら「反則負け」、相手が手番なら「反則勝ち」（`KifuParseCommon::foulTerminalMove()`） |

`▲` = 先手、`△` = 後手。

#### 二重追記防止

```cpp
void GameEndHandler::appendGameOverLineAndMark(Cause cause, Turn loser)
{
    if (!m_refs.gameOver->isOver) return;        // 終局していなければ無視
    if (m_refs.gameOver->moveAppended) return;   // 既に追記済みなら無視
    if (!m_refs.clock || !m_hooks.appendKifuLine) {
        markGameOverMoveAppended();
        return;
    }

    // ... 時計を止め、終局行を作り、終局行を行った側の考慮時間を確定する ...

    m_hooks.appendKifuLine(line, elapsed);
    if (m_hooks.disarmHumanTimerIfNeeded) m_hooks.disarmHumanTimerIfNeeded();
    markGameOverMoveAppended();                  // moveAppended = true にして二重追記を防止
}
```

終局行の考慮時間は、その行を行った側（入玉宣言勝ちでは宣言した勝者、千日手・持将棋・連続王手の千日手では手番の側、それ以外は敗者）のものである。時計を止めた時点で手番側の経過が考慮時間に精算されているので、その考慮時間を `commitMove()` で確定し、`considerationAndTotalTime()` を棋譜欄に書く。

`markGameOverMoveAppended()` が呼ばれると `GameOverState::moveAppended` が `true` になり、`gameOverStateChanged` シグナルが再発行される。これにより `GameStateController::onGameOverStateChanged()` が呼ばれ、リプレイモードへの遷移が行われる（7.2）。

---

## 7. 終局後のUI状態遷移

**ファイル:** `src/game/gamestatecontroller.cpp`

`setGameOver()` の3つのシグナルは次の順に処理される。`GameStateController` のフックは `MainWindowServiceRegistry::ensureGameStateController()`（`src/app/gamesubregistry.cpp`）が設定する。

### 7.1 終局行の追記: GameStateController::setGameOverMove()

`requestAppendGameOverMove` を受けた `onRequestAppendGameOverMove(info)` が `setGameOverMove(info.cause, info.loser)` を呼ぶ。

```cpp
void GameStateController::setGameOverMove(MatchCoordinator::Cause cause, Turn loser)
{
    if (!m_match || !m_match->gameOverState().isOver) return;

    m_match->appendGameOverLineAndMark(cause, loser);

    // UI 後処理
    if (m_hooks.updateBoardView) {
        m_hooks.updateBoardView();
    }
    if (m_hooks.setKifuViewSingleSelection) {
        m_hooks.setKifuViewSingleSelection();
    }
    if (m_hooks.setReplayMode) {
        m_hooks.setReplayMode(true);
    }
}
```

- `updateBoardView` → `ShogiView::update()`
- `setKifuViewSingleSelection` → 棋譜ビューを単一選択モードに設定
- `setReplayMode(true)` → `ReplayController::setReplayMode(true)`（時計を止め、手番の強調を消す）

`appendGameOverLineAndMark()` の中の `markGameOverMoveAppended()` が `gameOverStateChanged` を出すので、7.2 の処理はこの関数の途中で走る。`GameStateController` の後に、`SessionLifecycleCoordinator::commitLiveGameSessionIfActive()` がライブ対局セッションを確定する。

### 7.2 GameStateController::onGameOverStateChanged()

`moveAppended = true` になった後に、閲覧（リプレイ）モードへの最終遷移を行う。

```
gameOverStateChanged シグナル受信
    ↓
onGameOverStateChanged(st)
    ├─ (1) 終局していなければ return
    │
    ├─ (2) 名前・時計の手番ハイライトを保護
    │      setGameOverStyleLock(true)
    │
    ├─ (3) 終局行の追記前（moveAppended == false）なら return
    │
    ├─ (4) 表示中の手数を最終行（終局行）に合わせる
    │      syncCursorToPly(rows - 1) → KifuNavigationCoordinator::syncNavStateToPly()
    │
    ├─ (5) UI遷移
    │      enableArrowButtons()  → UiStatePolicyManager::transitionToIdle()
    │      setReplayMode(true)   → リプレイモードに設定
    │
    └─ (6) ハイライト消去
           removeHighlightAllData() → ShogiView::removeHighlightAllData()
```

### 7.3 GameStateController::onMatchGameEnded()

`gameEnded` は `SessionLifecycleCoordinator::handleGameEnded()`（`src/app/sessionlifecyclecoordinator.cpp`）に届き、そこから呼ばれる。

```
SessionLifecycleCoordinator::handleGameEnded(info)
    ├─ GameStateController::onMatchGameEnded(info)
    │   ├─ (1) 名前・時計の手番ハイライトを保護
    │   │      setGameOverStyleLock(true)
    │   │
    │   ├─ (2) タイマー停止・マウスクリック無効化
    │   │      match->disarmHumanTimerIfNeeded()
    │   │      clock->stopClock()
    │   │      setMouseClickMode(false)
    │   │
    │   ├─ (3) 棋譜追記と UI 後処理
    │   │      setGameOverMove(info.cause, info.loser)
    │   │      （終局行は 7.1 で追記済みなので追記しない。中断は 8.2 を参照）
    │   │
    │   └─ (4) UIの後処理
    │          enableArrowButtons()
    │          棋譜ビューを単一選択モードに設定
    │
    ├─ 終局時刻を記録し、対局情報に反映
    │      TimeControlController::recordGameEndTime() → updateEndTime(gameEndDateTime())
    │
    └─ 中断なら連続対局の設定を戻す（ConsecutiveGamesController::reset()）
```

---

## 8. その他の終局条件

### 8.1 時間切れ: handlePlayerTimeOut()

**ファイル:** `src/game/matchcoordinator.cpp`

```cpp
void MatchCoordinator::handlePlayerTimeOut(Turn player) {
    if (m_gameOver.isOver) return;
    ensureMatchTurnHandler();
    m_turnHandler->handlePlayerTimeOut(player);   // gc->applyTimeoutLossFor(player) で勝敗を設定
    ensureGameEndHandler();
    m_gameEndHandler->handleTimeout(player);      // 終局状態・エンジンへの通知・結果表示
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
    ├─ MatchTurnHandler::handlePlayerTimeOut(player)
    │      gc->applyTimeoutLossFor(player) / gc->finalizeGameResult()
    └─ GameEndHandler::handleTimeout(player)
           ├─ setGameOver({Timeout, player}, true)
           ├─ 時計を終局状態にして止める（markGameOver() / stopClock()）
           ├─ disarmHumanTimerIfNeeded()
           ├─ エンジンへの通知（stop → 時間が切れた側に "gameover lose"、相手に "gameover win" → quit）
           │      EvE: usi1 を先手、usi2 を後手として / HvE: usi1 をエンジンの手番として
           └─ displayResultsAndUpdateGui(info) → 「%1の時間切れ。%2の勝ちです。」
```

時間切れの判定は、持ち時間に制限があり、時間切れを負けにする設定（`ShogiClock::setLoseOnTimeout(true)`）のときだけ行う（`ShogiClock::enforcesTimeout()`）。

### 8.2 中断: handleBreakOff()

**ファイル:** `src/game/matchcoordinator.cpp`、`src/game/gameendhandler.cpp`

ユーザーがメニュー「対局」→「中断」をクリックした場合（`GameSessionOrchestrator::handleBreakOffGame()` → `GameStateController::handleBreakOffGame()`）と、エンジンのエラー（8.7）で呼ばれる。CSA通信対局では `CsaGameCoordinator::requestChudan()` に渡す。

```cpp
void MatchCoordinator::handleBreakOff()
{
    if (m_gameOver.isOver) return;
    captureInterruptedGame();
    // 時計の精算時に時間切れが確定した場合、中断に上書きしない。
    if (m_gameOver.isOver) return;
    ensureEngineManager();
    m_engineManager->disconnectArbiterSignals();

    ensureGameEndHandler();
    m_gameEndHandler->handleBreakOff();
    if (m_interruptedGame) emit gameInterrupted();
}
```

```
MatchCoordinator::handleBreakOff()
    ├─ (1) 終局済みチェック
    │
    ├─ (2) 再開用に対局の状態を保存（captureInterruptedGame()、src/game/matchcoordinator_resume.cpp）
    │      ShogiClock::pauseAndSnapshot() で時計を止めて状態を写し、開始の設定・SFEN 履歴・
    │      指し手・position 文字列を InterruptedGameState に入れる（ローカル対局のみ）
    │
    ├─ (3) 時計を止めたときの精算で時間切れが確定していれば return
    │
    ├─ (4) エンジンの投了・勝ち宣言のシグナルを切断
    │      EngineLifecycleManager::disconnectArbiterSignals()
    │
    ├─ (5) GameEndHandler::handleBreakOff()
    │
    └─ (6) 再開用の状態があれば emit gameInterrupted()
           → GameSessionOrchestrator::onGameInterrupted() が「対局再開」を使えるようにする

GameEndHandler::handleBreakOff()
    ├─ (1) 終局状態を直接設定
    │      isOver = true / hasLast = true / when = 現在時刻
    │      lastInfo = {BreakOff, loser = 今の手番の側}
    │
    ├─ (2) 時計停止・タイマー停止
    │      clock->markGameOver() / clock->stopClock()
    │      disarmHumanTimerIfNeeded()
    │
    ├─ (3) エンジンへの通知
    │      EvE: 両エンジンに stop → "gameover draw"
    │      HvE: usi1 に stop → "gameover draw"
    │
    ├─ (4) UI終局通知
    │      emit gameEnded(info)
    │      → GameStateController::onMatchGameEnded() → setGameOverMove()
    │        → appendGameOverLineAndMark(BreakOff, 手番の側) が "▲中断" / "△中断" を追記
    │
    ├─ (5) appendBreakOffLineAndMark()（追記済みなら何もしない）
    │
    └─ (6) エンジンを終了させる
           cleanupEngineProcessAndThread(false)（quit を送る。思考情報の表示は残す）
```

中断は `setGameOver()` を通らないので、`requestAppendGameOverMove` は出ない。終局行は `gameEnded` を受けた 7.3 の処理で追記される。

### 8.3 最大手数到達（持将棋）: handleMaxMovesJishogi()

**ファイル:** `src/game/gameendhandler.cpp`

各 Strategy が着手の後に最大手数（`maxMoves()`、0 なら無制限）を判定し、`MatchCoordinator::handleMaxMovesJishogi()` を呼ぶ。人間同士とエンジン同士は総手数（SFEN 履歴の件数 - 1）、人間対エンジンは `currentMoveIndex` で比べる。

```
GameEndHandler::handleMaxMovesJishogi()
    ├─ 終局済みチェック
    ├─ タイマー停止（disarmHumanTimerIfNeeded()）
    ├─ エンジンへ "gameover draw" + quit 通知（EvE: 両エンジン、それ以外: usi1）
    ├─ GameEndInfo構築（cause = Jishogi、loser = Turn::Black）
    ├─ setGameOver(info, true)
    ├─ appendGameOverLineAndMark(Jishogi, Turn::Black) → 手番の側で "▲持将棋" / "△持将棋"
    │      （setGameOver() の requestAppendGameOverMove で追記済みなら何もしない）
    └─ displayResultsAndUpdateGui(info) → 「最大手数に達しました。持将棋です。」
```

引き分けなので、`loser` の値は結果メッセージにも終局行の印にも使わない。

### 8.4 千日手: checkAndHandleSennichite()

**ファイル:** `src/game/gameendhandler.cpp`、`src/game/sennichitedetector.cpp`

各 Strategy が着手の後（棋譜に追記してから）に `checkAndHandleSennichite()` を呼ぶ。判定の詳細は [千日手検出機能](sennichite-detection.md) を参照。

```
GameEndHandler::checkAndHandleSennichite()
    ├─ SFEN 履歴（エンジン同士は EngineVsEngineStrategy::sfenRecordForEvE()）が4件未満なら false
    └─ SennichiteDetector::check(sfenHistory)
           ├─ None                   → false（対局を続ける）
           ├─ Draw                   → handleSennichite()
           ├─ ContinuousCheckByBlack → handleOuteSennichite(Turn::Black)
           └─ ContinuousCheckByWhite → handleOuteSennichite(Turn::White)
```

| 処理 | GameEndInfo | エンジンへの通知 | 終局行 |
|---|---|---|---|
| `handleSennichite()` | `{Sennichite, Turn::Black}` | `"gameover draw"` + quit | 手番の側で `"▲千日手"` |
| `handleOuteSennichite(Turn loser)` | `{OuteSennichite, loser}` | 王手を続けた側に `"gameover lose"`、相手に `"gameover win"`、どちらも quit | 手番の側で `"▲反則負け"`／`"▲反則勝ち"` |

どちらも `setGameOver(info, true)` → `appendGameOverLineAndMark()` → `displayResultsAndUpdateGui(info)` の順に処理する。

### 8.5 入玉宣言: handleNyugyokuDeclaration()

**ファイル:** `src/game/gameendhandler.cpp`

```cpp
void handleNyugyokuDeclaration(Turn declarer, bool success, bool isDraw);
```

宣言の入口は2つある。

```
人間の宣言: メニュー「入玉宣言」
    ↓
DialogLaunchWiring::handleNyugyokuDeclaration()
    ↓
NyugyokuDeclarationHandler::handleDeclaration()   （src/ui/controllers/nyugyokudeclarationhandler.cpp）
    ├─ 宣言できるのは手番の人間だけ。持将棋ルール（24点法・27点法）の設定が必要
    ├─ 確認ダイアログ
    ├─ NyugyokuJudgement::judge() で宣言条件と点数を判定
    ├─ MatchCoordinator::handleNyugyokuDeclaration(declarer, success, isDraw)
    └─ 結果ダイアログを表示

エンジンの宣言: "bestmove win"
    ↓
EngineLifecycleManager::onBlackEngineWin() / onWhiteEngineWin()   （wireWinToArbiter() で配線）
    ↓
MatchCoordinator::handleEngineWin(side) → GameEndHandler::handleEngineWin(side)
    ├─ NyugyokuJudgement::judgeEngineDeclaration() で盤面から判定
    ├─ handleNyugyokuDeclaration(side, success, isDraw)
    └─ 判定結果のダイアログを表示（showGameOverDialog）
```

入玉宣言には3つの結果がある:

| 結果 | Cause | 敗者 |
|---|---|---|
| 引き分け（持将棋） | `Jishogi` | 宣言者（引き分け） |
| 宣言成功（勝ち） | `NyugyokuWin` | 宣言者の相手 |
| 宣言失敗（反則負け） | `IllegalMove` | 宣言者 |

`handleNyugyokuDeclaration()` の処理:
1. タイマー停止・時計停止
2. GameEndInfo構築
3. `EngineGameOverNotifier::notifyNyugyoku()` でエンジンへ gameover 通知（引き分けは `"gameover draw"`、それ以外は勝ち負け）
4. `setGameOver()` で終局確定
5. 棋譜の自動保存（設定時）と `gameEndProcessed` の発行

結果ダイアログは呼び出し側が出すので、`displayResultsAndUpdateGui()` は呼ばない。

### 8.6 エンジンの反則手: handleEngineIllegalMove()

エンジンの指し手が `ShogiGameController::applyMove()` で不合法と判定されると、Strategy は盤に指さずに `handleEngineIllegalMove(エンジンの手番)` を呼ぶ（2.1、2.3）。

```
GameEndHandler::handleEngineIllegalMove(Turn side)
    ├─ 時計を停止
    ├─ info = {IllegalMove, loser = side}
    ├─ EngineGameOverNotifier::notifyResignation()   … 投了と同じく勝ち負けを知らせる
    ├─ 両エンジンの投了通知を抑止
    ├─ setGameOver(info, true)          → "▲反則負け" / "△反則負け"
    └─ displayResultsAndUpdateGui(info) → 「%1の反則負け。%2の勝ちです。」
```

### 8.7 エンジンのエラー（中断として扱う）

エンジンの異常終了・起動の失敗・bestmove の応答待ちの期限切れは、対局を中断として終える。

```
Usi::errorOccurred(message)
    （UsiMatchHandler::onSearchTimeout() の "Timeout waiting for bestmove." もここに出る。
      時計の時間切れで終局していれば出さない）
    ↓  （EngineLifecycleManager::initializeAndStartEngineFor() が接続）
EngineLifecycleManager::engineError
    ↓  （QueuedConnection。開始処理を終えてから中断する）
MatchCoordinator::onUsiError(msg)
    ↓
MatchTurnHandler::handleUsiError(msg)
    ├─ 両エンジンの今の処理を取り消す
    ├─ 検討・詰み探索のエラーなら AnalysisSessionHandler::handleEngineError() が処理して return
    └─ エンジンを使う対局中なら
           MatchCoordinator::handleBreakOff()（8.2）
           showGameOverDialog("対局中断", "エンジンエラー: <msg>")
```

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
GameSessionOrchestrator::handleResignation()
    ↓
GameStateController::handleResignation()
    ↓
MatchCoordinator::handleResign()
    ↓
GameEndHandler::handleResign()
    ├─ info = {Resignation, loser = 手番の側}
    ├─ EngineGameOverNotifier::notifyResignation()   // HvE: usi1 に "gameover win/lose" + quit
    ├─ setGameOver(info, true)
    │   ├─ emit gameOverStateChanged(isOver=true)     ──→ (A)
    │   ├─ emit requestAppendGameOverMove(info)       ──→ (B)
    │   └─ emit gameEnded(info)                       ──→ (C)
    └─ displayResultsAndUpdateGui(info)
        ├─ hooks.showGameOverDialog("対局終了", "先手の投了。後手の勝ちです。")
        ├─ hooks.autoSaveKifuIfEnabled()  // 設定時
        └─ emit gameEndProcessed(info)                ──→ (D)

(A) GameStateController::onGameOverStateChanged(st)  // moveAppended=false
    └─ setGameOverStyleLock(true) だけ行って return

(B) GameStateController::onRequestAppendGameOverMove(info)
    └─ setGameOverMove(cause, loser)
        ├─ appendGameOverLineAndMark(Resignation, loser)
        │   ├─ 時計を止め、投了した側の考慮時間を確定
        │   ├─ "▲投了" を棋譜に追記
        │   └─ markGameOverMoveAppended()
        │       └─ emit gameOverStateChanged(moveAppended=true)  ──→ (A')
        ├─ shogiView->update() / 棋譜ビューを単一選択に
        └─ setReplayMode(true)
    続いて SessionLifecycleCoordinator::commitLiveGameSessionIfActive()

(A') GameStateController::onGameOverStateChanged(st)  // moveAppended=true
    ├─ syncCursorToPly(lastRow)
    ├─ enableArrowButtons()  → UiStatePolicyManager::transitionToIdle()
    ├─ setReplayMode(true)
    └─ shogiView->removeHighlightAllData()

(C) SessionLifecycleCoordinator::handleGameEnded(info)
    ├─ GameStateController::onMatchGameEnded(info)
    │   ├─ shogiView->setGameOverStyleLock(true)
    │   ├─ match->disarmHumanTimerIfNeeded()
    │   ├─ clock->stopClock()
    │   ├─ shogiView->setMouseClickMode(false)
    │   ├─ setGameOverMove(cause, loser)  // 追記済みなので UI の後処理だけ
    │   └─ enableArrowButtons() / 棋譜ビューを単一選択に
    └─ 終局時刻を記録し、対局情報に反映
    続いて UiStatePolicyManager::transitionToIdle()

(D) SessionLifecycleCoordinator::handleGameEndProcessed(info)
    └─ エンジン同士の連続対局なら次局を開始（投了では該当しない）

対局終了 → リプレイ（棋譜閲覧）モードへ
```

---

## 11. 関連ファイル一覧

| コンポーネント | ヘッダ | 実装 |
|---|---|---|
| 盤面操作コントローラ | `src/board/boardinteractioncontroller.h` | `src/board/boardinteractioncontroller.cpp` |
| ボードセットアップコントローラ | `src/ui/controllers/boardsetupcontroller.h` | `src/ui/controllers/boardsetupcontroller.cpp` |
| 着手要求・着手確定の受け口 | `src/app/mainwindowserviceregistry.h` | `src/app/mainwindowboardregistry.cpp` |
| ゲームコントローラ | `src/game/shogigamecontroller.h` | `src/game/shogigamecontroller.cpp` |
| 対局司令塔 | `src/game/matchcoordinator.h` | `src/game/matchcoordinator.cpp`、`src/game/matchcoordinator_engine.cpp`、`src/game/matchcoordinator_time.cpp`、`src/game/matchcoordinator_resume.cpp` |
| 司令塔のフック・終局の型 | `src/game/matchcoordinatorhooks.h`、`src/game/matchtypes.h` | - |
| ターン進行ハンドラ | `src/game/matchturnhandler.h` | `src/game/matchturnhandler.cpp` |
| Strategy の基底・コンテキスト | `src/game/gamemodestrategy.h`、`src/game/strategycontext.h` | - |
| 人間対エンジン | `src/game/humanvsenginestrategy.h` | `src/game/humanvsenginestrategy.cpp` |
| 人間同士 | `src/game/humanvshumanstrategy.h` | `src/game/humanvshumanstrategy.cpp` |
| エンジン同士 | `src/game/enginevsenginestrategy.h` | `src/game/enginevsenginestrategy.cpp` |
| 終局処理ハンドラ | `src/game/gameendhandler.h` | `src/game/gameendhandler.cpp`、`src/game/gameendhandler_record.cpp` |
| エンジンへの終局通知 | `src/game/enginegameovernotifier.h` | `src/game/enginegameovernotifier.cpp` |
| エンジンの生成・投了の配線 | `src/game/enginelifecyclemanager.h` | `src/game/enginelifecyclemanager.cpp` |
| 千日手の判定 | `src/game/sennichitedetector.h` | `src/game/sennichitedetector.cpp` |
| ゲーム状態コントローラ | `src/game/gamestatecontroller.h` | `src/game/gamestatecontroller.cpp` |
| ターンマネージャ | `src/game/turnmanager.h` | `src/game/turnmanager.cpp` |
| 手番の同期 | `src/game/turnsyncbridge.h`、`src/app/turnstatesyncservice.h` | `src/app/turnstatesyncservice.cpp` |
| 時計の配線・残り時間の通知 | `src/game/matchtimekeeper.h` | `src/game/matchtimekeeper.cpp` |
| 将棋時計 | `src/core/shogiclock.h` | `src/core/shogiclock.cpp` |
| 時間制御コントローラ | `src/ui/controllers/timecontrolcontroller.h` | `src/ui/controllers/timecontrolcontroller.cpp` |
| 時計の表示 | `src/ui/presenters/timedisplaypresenter.h` | `src/ui/presenters/timedisplaypresenter.cpp` |
| USIエンジン | `src/engine/usi.h` | `src/engine/usi.cpp`、`src/engine/usi_async.cpp` |
| 対局の思考依頼・bestmove の受け取り | `src/engine/usimatchhandler.h` | `src/engine/usimatchhandler.cpp` |
| USIプロトコルハンドラ | `src/engine/usiprotocolhandler.h` | `src/engine/usiprotocolhandler.cpp` |
| go の時間の計算 | `src/engine/usigotime.h` | `src/engine/usigotime.cpp` |
| 司令塔の配線 | `src/ui/wiring/matchcoordinatorwiring.h` | `src/ui/wiring/matchcoordinatorwiring.cpp` |
| 対局メニューの配線 | `src/ui/wiring/gameactionswiring.h` | `src/ui/wiring/gameactionswiring.cpp` |
| 投了・中断の受け口 | `src/app/gamesessionorchestrator.h` | `src/app/gamesessionorchestrator.cpp` |
| 終局後のセッション処理 | `src/app/sessionlifecyclecoordinator.h` | `src/app/sessionlifecyclecoordinator.cpp` |
| 棋譜追記 | `src/app/gamerecordupdateservice.h` | `src/app/gamerecordupdateservice.cpp` |
