# MatchCoordinator 設計書（ISSUE-010）

> **2026-10-15 追記（統合）:** この文書は以前、司令塔を子ハンドラ（`GameEndHandler`・`GameStartOrchestrator`・`MatchTimekeeper`・`MatchUndoHandler`・`MatchTurnHandler`・`EngineGameOverNotifier`）とフック（`MatchCoordinatorHooks`・`StrategyContext`）に分ける設計を記していた。これらは `MatchCoordinator` 本体と `MatchSessionState`・`MatchStrategyHost` に統合し、上位層へのフックはシグナルと `MatchRecordSink` に置き換えた。司令塔を作って配線していた `MatchCoordinatorWiring` も削除した。分割当時の設計と変更履歴は git の履歴を参照。以下は現在の構成である。

## 1. 現状サマリ

`MatchCoordinator`（`src/game/matchcoordinator.cpp`）はローカル対局（人間・エンジン）の司令塔である。対局の手順（`RecordLine`）を持ち、一手の確定（時計・棋譜・千日手・最大手数）、終局の判定と記録、待った、中断と再開を受け持つ。手番ごとに誰が指すかは対局モード別の Strategy が決める。CSA 通信対局はこのクラスを使わない（`CsaGameCoordinator`）。

| ファイル | 行数 | 内容 |
|---------|------|------|
| `src/game/matchcoordinator.h` | 202 | 司令塔のクラス・`Deps`・シグナル |
| `src/game/matchcoordinator.cpp` | 559 | 開始・着手・時計の通知・終局・待った・中断と再開 |
| `src/game/matchsessionstate.h` | 53 | 対局の設定と終了状態の持ち主 |
| `src/game/gamemodestrategy.h` | 77 | `MatchStrategyHost`（Strategy が司令塔に頼む操作）と `GameModeStrategy` |
| `src/game/matchrecordsink.h` | 28 | 棋譜の受け手のインターフェース |
| `src/game/matchtypes.h` | 45 | 終局の型（`MatchCause`・`MatchGameEndInfo`・`MatchGameOverState`） |
| `src/game/startoptions.h` | 43 | `MatchStartOptions`・`GameStartPreparation` |
| `src/game/interruptedgamestate.h` | 19 | 中断した対局の状態 |

行数は 2026-10-15 時点。

---

## 2. 依存と生成

```cpp
struct Deps {
    ShogiGameController* gc = nullptr;          ///< 対局の盤（必須、非所有）
    ShogiClock* clock = nullptr;                ///< 対局の時計（必須、非所有）
    AppActivityState* activity = nullptr;       ///< アプリの動作状態（必須、非所有）
    EngineLifecycleManager* engines = nullptr;  ///< エンジンの枠 E1・E2（必須、非所有。検討と共有する）
};
```

- 司令塔は `MainWindowComposer::createServices()`（`src/app/mainwindowcomposer_create.cpp`）が起動時に一度だけ作り、作り直さない。時計は `TimeControlController` が持ち、エンジンの枠 `EngineLifecycleManager` は合成ルートが作って検討・詰み探索（`AnalysisSession`）と共有する。
- コンストラクタは、エンジンの投了・入玉宣言勝ち・エラー（`EngineLifecycleManager::engineResigned`・`engineDeclaredWin`・`engineError`。エラーは `QueuedConnection`）と時計の `ShogiClock::timeUpdated` を自分のスロットにつなぐ。
- 棋譜の受け手は `MainWindowComposer::wireGame()` が `setRecordSink(GameSession)` で設定する。司令塔のシグナルの受け先は `MainWindowComposer::connectMatchSignals()`（`src/app/mainwindowcomposer_game.cpp`）がつなぐ（5章）。
- 司令塔は Hooks（コールバックの構造体）を持たない。手番のコピーも持たず、手番は `ShogiGameController::currentPlayer()` だけを使う。

---

## 3. 責務

| 区分 | メソッド |
|------|---------|
| 開始・再開 | `start()`（盤・手順・対局者・Strategy を準備。時計はまだ動かさない）、`beginPlay()`（時計を動かし、初手がエンジンなら思考を始める）、`resumeInterruptedGame()`、`discardInterruptedGame()`、private の `createStrategy()`・`captureInterruptedGame()` |
| 着手 | `canAcceptMove()`、`onHumanMove()`、`forceImmediateMove()`、private の `completeMove()`（一手の確定） |
| 待った | `undoTwoPlies()` |
| 終局 | `handleResign()`、`handleBreakOff()`、`handlePlayerTimeOut()`、`handleNyugyokuDeclaration()`、`clearGameOverState()`。private の `handleEngineResign()`・`handleEngineWin()`・`handleEngineIllegalMove()`・`handleEngineError()`・`checkAndHandleSennichite()`・`handleMaxMovesJishogi()`、記録の `finishGame()`・`appendTerminalRecord()`、知らせの `announceResult()`・`resultMessage()` |
| 時計 | `clock()`、`pokeTimeUpdateNow()`、private の `emitTimeUpdate()` |
| 読み取り | `record()`、`currentPly()`、`options()`、`players()`、`hasStarted()`、`isHumanSide()`、`gameOverState()`、`isGameOver()`、`interruptedGame()`、`usesHandicapNames()` |

終局処理はどれも、エンジンへの通知（`EngineLifecycleManager::notifyGameOver()`）→ `finishGame()`（終局状態の確定・終局行の追記・`gameEnded`）→ `announceResult()`（結果の知らせ・自動保存の依頼・`gameEndProcessed`）の順に進む。中断は `announceResult()` を呼ばず、入玉宣言は結果を呼び出し側のダイアログで知らせる。流れの詳細は [対局開始から投了までのフロー](game-play-to-resignation-flow.md) を参照。

---

## 4. 状態の持ち主

| 状態 | 持ち主 |
|------|-------|
| 対局の手順（開始局面と指した手の行。終局の行は含めない） | `MatchCoordinator::m_record`（`RecordLine`） |
| 開始の設定・対局者・最大手数・終局状態・エンジンの先読み局面 | `MatchSessionState m_session` |
| 対局モードの Strategy | `std::unique_ptr<GameModeStrategy> m_strategy` |
| 中断した対局（再開用） | `std::unique_ptr<InterruptedGameState> m_interruptedGame` |
| 盤と手番 | `ShogiGameController`（Deps） |
| 時計 | `ShogiClock`（Deps。`TimeControlController` が所有） |
| エンジン | `EngineLifecycleManager`（Deps。合成ルートが所有） |

`MatchSessionState` は対局上の操作だけを公開し、フィールドへの可変ポインタを渡さない。

| 操作 | 内容 |
|------|------|
| `begin(options)` | 開始の設定を持ち、終了状態と先読み局面を消す |
| `finish(info)` | 終局を記録する（終局済みなら `false`） |
| `markTerminalRecorded()` | 終局の行を書いたことを記録する（未終局・記録済みなら `false`） |
| `clearGameOver()` | 終了状態を解く |
| `ponder(side)`・`setPonder()`・`clearPonder()` | 手番ごとの先読みの局面 |

開始手順は `MatchStartOptions::recordLine` で受け取り、`currentPly` までを対局の手順にする。過去対局の検索、USI position 文字列の履歴、独立した現在手数カウンターは無い。エンジンへの `position` は `RecordLine::positionCommand()` で対局の手順から作り、待ったと中断再開も同じ手順を使う。

---

## 5. Strategy と MatchStrategyHost

対局モードごとの Strategy（`GameModeStrategy`）は `createStrategy()` が `MatchStartOptions::players`（`MatchPlayers`）から作る。

| 対局者 | Strategy |
|--------|----------|
| 先手・後手ともエンジン | `EngineVsEngineStrategy`（先手 E1、後手 E2） |
| 一方がエンジン | `HumanVsEngineStrategy`（エンジンは手番によらず E1） |
| どちらも人間 | `HumanVsHumanStrategy` |

Strategy は手番ごとに誰が指すかだけを決め、一手の確定とエンジンへの思考の依頼は `MatchStrategyHost`（`src/game/gamemodestrategy.h`）を通して司令塔に頼む。司令塔はこのインターフェースを private に継承して実装し、`strategyHost()` で渡す（テストからも一手を確定するときに使う）。

| `MatchStrategyHost` | 内容 |
|------|------|
| `gameController()`・`clock()`・`engines()`・`isGameOver()`・`currentPly()` | 盤・時計・エンジンの枠・終局状態・手数を読む |
| `refreshBoardAndTurn()` | 盤と手番の表示を知らせる（`boardChanged`・`turnChanged`） |
| `requestEngineMove(side)` | 手番のエンジンに、対局の手順の `position` と先読み局面で思考させる |
| `setEnginePonder(side, ponder)` | エンジンが返した先読みの局面を覚える |
| `applyEngineMove(move, mover, elapsedMs)` | エンジンの手を合法手か確かめて指し、一手を確定する（反則手は反則負け） |
| `completeHumanMove(played)` | 盤で指した人間の手で一手を確定する |

`GameModeStrategy` の `armTurnTimerIfNeeded()`・`disarmTurnTimer()` は人間同士の Strategy だけが実装する（人間の考慮時間の計測）。

---

## 6. 上位層への知らせ（シグナルと MatchRecordSink）

盤・手番・対局者名・時計・終局の表示はシグナルで知らせる。受け先は `MainWindowComposer::connectMatchSignals()` がつなぐ。

| シグナル | 受け先 |
|---------|-------|
| `gamePrepared(opt)` | `GameSession::onGamePrepared()` |
| `boardChanged()` | `GameSession::renderBoard()` |
| `turnChanged(side)` | `TurnStateSyncService::onTurnManagerChanged()` |
| `moveHighlightRequested(from, to)` | `BoardInteractionController::showMoveHighlights()` |
| `movesUndone(ply)` | `GameSession::onMovesUndone()` |
| `engineMovePlayed(side, ply)` | `EvaluationGraphController::plotEngineScore()` |
| `timeUpdated(...)` | `TimeDisplayPresenter::onMatchTimeUpdated()` |
| `gameOverStateChanged(st)` | `GameSession::onGameOverStateChanged()` |
| `gameEnded(info)` | `GameSession::onGameEnded()` |
| `gameOverMessage(title, message)` | `DialogCoordinator::showInformation()` |
| `autoSaveRequested(dir)` | `KifuFileController::autoSaveKifuToFile()` |
| `gameEndProcessed(info)` | `GameSession::onGameEndProcessed()` |
| `gameInterrupted()` | `GameSession::onGameInterrupted()` |

棋譜の行は `MatchRecordSink`（`src/game/matchrecordsink.h`）に書き込む。

| `MatchRecordSink` | 内容 |
|------|------|
| `canAppendRecord()` | 行を追記できるか（対局のセッションが終わっていれば `false`。司令塔はその手を指さない） |
| `appendRecord(entry)` | 確定した手・終局の行を追記する |
| `canUndoRecords(count)`・`undoRecords(count)` | 待ったで末尾の行を取り消す |

実装は `GameSession`（`src/app/gamesession.cpp`）で、ライブ対局のセッション（`LiveGameSession`）を通して分岐ツリーに書く。通信対局の行も `CsaGameWiring` が同じ受け手に書く。

シグナル以外に司令塔を直接呼ぶ部品は、`GameSession`（利用者の操作と開始の設定）、`GameStartCoordinator`（`start()`・`beginPlay()`・`resumeInterruptedGame()`）、`BoardSetupController`（人間の着手）、`HumanTurnPolicy`（人間の手番か）、`TimeControlController`（時間切れ）、`NyugyokuDeclarationHandler`（人間の入玉宣言）、`ReplayController`（時計の表示の更新）、`PositionEditCoordinator`・`CandidateArrowController`（終局状態の読み取り）である。

---

## 7. 以前の分割との対応

| 以前 | 現在 |
|------|------|
| `GameEndHandler` | 司令塔の終局処理（`finishGame()`・`appendTerminalRecord()`・`announceResult()` など） |
| `GameStartOrchestrator` | `MatchCoordinator::start()`・`createStrategy()`。開始オプションの組み立ては `GameStartOptionsBuilder`、開始の順序は `GameStartCoordinator` |
| `MatchTimekeeper` | 時間の決まりと残り時間は `ShogiClock`、表示への通知は `MatchCoordinator::emitTimeUpdate()`、`go` の時間は `UsiGoTimeCalculator` |
| `MatchUndoHandler` | `MatchCoordinator::undoTwoPlies()` と `MatchRecordSink::undoRecords()`、表示は `movesUndone` |
| `MatchTurnHandler` | `MatchCoordinator::createStrategy()`・`onHumanMove()`・`beginPlay()`・`handleEngineError()` |
| `EngineGameOverNotifier` | `EngineLifecycleManager::notifyGameOver()` |
| `StrategyContext`（`MatchStrategyContext`） | `MatchStrategyHost` |
| `MatchCoordinatorHooks`（UI・Time・Engine・Game）・`UndoHooks` | 6章のシグナルと `MatchRecordSink` |
| `MatchCoordinatorWiring` | `MainWindowComposer::createServices()`・`wireGame()`・`connectMatchSignals()` |
| `AnalysisSessionHandler` | `AnalysisSession`（`src/analysis/analysissession.cpp`。合成ルートが作り、司令塔とはエンジンの枠だけを共有する） |
| `PlayMode` | 誰が指すかは `MatchPlayers`、何をしているかは `AppActivityState::Activity` |

---

## 8. 関連文書

- [対局ダイアログから対局開始までのフロー](game-start-flow.md): 開始の順序、中断と再開、連続対局、回帰テスト
- [対局開始から投了までのフロー](game-play-to-resignation-flow.md): 一手の確定、時計、終局処理
- [composition-root.md](composition-root.md): 合成ルートでの生成と配線
- [時間設定の仕様](game-time-control-spec.md): 時間の決まりの持ち方と `go` の時間
