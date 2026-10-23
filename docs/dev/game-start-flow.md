# 対局ダイアログから対局開始までのフロー

対局の開始処理は、開始の計画を値として作って検証してから、棋譜・時計・対局者の表示・対局の順に適用する。部品の生成と配線は [MainWindowComposer](composition-root.md) が起動時に行う。メニュー項目は `MenuActionsWiring`（`src/app/menuactionswiring.cpp`）が `GameSession` のスロットへつなぎ、司令塔の終局・中断と連続対局のシグナルは `MainWindowComposer::connectGame()`（`src/app/mainwindowcomposer_wire.cpp`）がつなぐ。開始後の指し手と終局の流れは [対局開始から投了までのフロー](game-play-to-resignation-flow.md) を参照。

## 開始要求

メニュー「対局」→「対局」は `GameSession::startGameFromDialog()`（`src/app/gamesession.cpp`）を呼ぶ。棋譜または直前の対局の開始局面が平手でない（局面編集した・棋譜の局面図から始めた）なら、ダイアログで「現在の局面」を選ばせる（`StartGameDialog::forceCurrentPositionSelection()`）。現在の局面以外から始めるときは、未保存の棋譜を捨ててよいかを `KifuFileController::confirmDiscardUnsaved()` で確かめる。ダイアログの値と表示中の手を `GameStartRequest` にまとめ、`GameStartOptionsBuilder::plan()`（`src/game/gamestartoptionsbuilder.cpp`）で開始の計画 `GameStartPlan` を作る。計画を作る間は棋譜も対局の状態も書き換えない。開始できなければ、理由を `ErrorBus` で知らせる。

`GameStartRequest` は次を渡す。

| 値 | 用途 |
|---|---|
| `dialogData` | 対局者2人（`PlayerChoice`。人間かエンジンか・名前・エンジン・時間）、開始局面の手合割（`startingHandicap`。`nullptr` なら現在の局面）、最大手数、自動保存、連続対局など（`StartGameDialogData`、`src/game/startgamedatabridge.h`） |
| `recordLine` | 表示中の手順の開始局面・指し手・現在手数（`KifuNavigationState::currentRecordLine()` の `RecordLine`） |
| `startNodeId` | 分岐ツリーで表示中のノードのID |
| `bottomSide` | 盤の手前に表示している側（`ShogiView::flipMode()` から求める）。人間を手前に表示するときの判定に使う |

`recordLine` の手順は、同じ局面へ到達する別の手順と区別する。過去の対局をSFENで検索したり、SFEN末尾の手数から棋譜を切り詰めたりしない。棋譜を持たない単独局面は、開始局面だけで指し手0件の手順になる。

`GameStartPlan` は次を持つ。

| 値 | 内容 |
|---|---|
| `options` | `MatchStartOptions`（対局者 `MatchPlayers`・開始局面・手順 `recordLine`・エンジンのパス・最大手数・自動保存） |
| `time` | 時間の決まり（`MatchTimeSettings`） |
| `preparation` | 開始前に棋譜をどう整えるか（`GameStartPreparation`） |
| `flipBoard` | 「人を手前に表示する」で盤を反転するか |
| `consecutiveGames`, `switchTurnEachGame` | 連続対局の局数と、1局ごとに手番を入れ替えるか |

## 検証と適用の順序

1. `GameStartOptionsBuilder::plan()` が現在局面またはプリセットから開始する手順を決定する。現在局面では `recordLine.currentPly` までを取り（`RecordLine::upTo()`）、後続の手を除く。プリセットでは、ダイアログで選んだ手合割（`StartGameDialogData::startingHandicap`。`Handicaps::kAll` の1つ）の局面だけの手順にする。現在局面から始めるかは `startsFromCurrentPosition()`（`startingHandicap` が `nullptr`）で決まる。
2. 開始局面を検証する（`GameStartOptionsBuilder::validatePosition()`：先手・後手の玉が盤上に1枚ずつで、`PositionValidator` の検査（駒の数・駒台の玉・手番でない側への王手）を通る）。不正なら計画を作らず、棋譜・時計・エンジンに触れずに理由を返す。
3. 同じ `plan()` が `buildStartOptions()` で `MatchStartOptions` を、`buildTimeSettings()` で時間設定を作り、`options.recordLine` に決定した手順を入れる。`preparation.mode` は `GameStartPreparation::Mode::NewGame` か `CurrentPosition` のいずれかで、表示用のSFENから推測しない。`flipBoard` は `needsHumanAtBottomFlip()` が決める。
4. 計画ができたら `GameSession::startGame()` が次の順に1つの関数で適用する。まず `SessionResetCoordinator::prepareGameStart()`（`src/app/sessionresetcoordinator.cpp`）で棋譜を開始局面に整え、ライブ対局のセッションを始める（`LiveGameSession::startFromNode()`。表示中の手が開始局面になる）。新規ならツリーを開始局面で作り直し、現在局面なら指定ノードまでの手順を残す。終局ノードが選択されていれば親を起点とし、末尾の中断ノードは取り除く。指定ノードが無ければ、同じ局面を探し直さずに何も始めない。ツリーから作られない表示（通信ログ・思考情報・検討・解析結果）はエンジンの表示の持ち主 `EngineDisplayPresenter::clearAll()` で消し、評価値グラフは現在局面からならその手数までに切り、新規なら消す。
5. 盤の反転が要るなら `MainWindowAppearanceController::flipBoard()` を呼び、ダイアログからの開始なら連続対局の1局目として `ConsecutiveGameSeries::beginSeries()` に計画を渡す。
6. 時計を一度だけ設定する（`TimeControlUtil::applyToClock()`：時間の決まり・時間切れの扱い・開始局面の手番）。`TimeDisplayPresenter::showClock()` で時計の表示を合わせる（対局中は `TimeDisplayPresenter` が `ShogiClock::timeUpdated` を直接受ける）。
7. `GameSession::beginMatchRecord(players, time, continuesRecord)` で棋譜の文書に対局を記録する（`KifuDocument::beginMatch()`。通信対局の開始 `GameSession::onCsaGameStarted()` も、サーバーが知らせた対局者名と持ち時間で同じ関数を通る）。文書は対局者と、持ち時間・開始日時（`MatchTimeRecord::begin()`）を記録し、対局情報を対局日・開始日時・対局者・手合割・持ち時間で作り直す（棋戦・場所・備考など利用者が入力した項目は残す）。作り直した対局情報は `headerReset` で対局情報欄と盤の対局者名に届く。読み込んだ棋譜の途中から続ける対局（`continuesRecord`。ライブ対局が棋譜の途中のノードから始まる）では、棋譜の対局情報を残して対局者名だけを書き換え、対局者と日時を分岐の最初の手のメモにする（`GameSession::noteContinuedMatch()`）。盤の対局者名は、文書の知らせ（`playersChanged`・`headerReset`）と動作状態の変化を受けた `PlayerInfoPresenter` が出す。思考タブの2つ目のエンジン欄と評価値グラフの系列名は、`KifuDocument::beginMatch()` が出す `playersChanged` を受けた `EngineDisplayPresenter` が合わせる（通信ログの見出しは枠に入ったエンジンの名前）。最後に、終局したときの自動保存の指定（`MatchStartOptions::autoSave`。`MatchAutoSave`。ローカル対局は拡張子 `kifu`）を覚えておく。終局の `GameSession::finishMatchRecord()` が `MatchAutoSave::isActive()` なら `KifuFileController::autoSaveKifu(dir, extension)` で保存する（中断は保存しない）。
8. `MatchCoordinator::start()` に確定した `MatchStartOptions` を渡す。司令塔は手順を `recordLine.currentPly` までに切り、`MatchSessionState::begin()` で設定を持ち、対局の手順（`MatchCoordinator::record()`）を作り、動作状態を対局中にし（`AppActivityState::begin()` で受け取ったリースを終局まで持つ。メニューとボタンは `UiStatePolicyManager` がこの変化を受けて切り替える）、盤を開始局面にする（`ShogiGameController::newGame()`）。最後に対局者（`MatchPlayers`）から Strategy を作って開始する（エンジンの生成と起動。初期化は非同期）。時計はまだ動かさない。
9. 中断した対局を捨てる（`GameSession::discardInterruptedGame()`）。終局の知らせで対局者を下手・上手と呼ぶかは、司令塔がそのつど棋譜の文書から読む（`MatchCoordinator::usesHandicapNames()` → `KifuDocument::usesHandicapNames()`。`MatchCoordinator::Deps::document`）。
10. `MatchCoordinator::beginPlay()` が時計を始動し、初手がエンジンの手番なら思考を始める（`GameModeStrategy::startInitialMoveIfNeeded()`）。初手のエンジンが初期化中なら、準備完了後に思考を始め、それまで時計を止めておく（`MatchStrategyHost::setWaitingForEngines()`）。初手が人間なら、相手エンジンの初期化を待たずに計時する。

## 対局中と再開

`MatchSessionState`（`src/game/matchsessionstate.h`）は対局設定（対局者・最大手数・自動保存）と終了状態を所有する（エンジンの先読みの局面は `UsiMatchHandler` だけが持つ）。終了状態の変更は `finish()`、`markTerminalRecorded()`、`clearGameOver()` を通し、フィールドへのポインタを他の部品へ渡さない。

Strategy は `MatchStrategyHost`（`src/game/gamemodestrategy.h`。司令塔が private に実装する）を通してエンジンの枠・盤の手番・手数を読み、一手の確定とエンジンへの思考の依頼を司令塔に頼む。司令塔のメンバーには直接触れない。エンジンに探索させる局面（`UsiPosition`）は、その時点の対局の手順（`MatchCoordinator::record()`）から `RecordLine::usiPosition()` で生成する。人間対エンジン、エンジン同士、待ったの後で同じ生成経路を使う。

### 中断と再開

中断（`MatchCoordinator::handleBreakOff()`）は、`captureInterruptedGame()` で開始の設定・対局の手順・時計のスナップショットを `InterruptedGameState` に保存してから終局を記録し、`gameInterrupted` を出す。`GameSession::onGameInterrupted()` は中断の行のノードを `MatchCoordinator::attachInterruptedRecord()` で `InterruptedGameState::terminalNodeId` に覚えさせ、「対局再開」を使えるようにする（司令塔の `resumeAvailabilityChanged` を受けた `UiStatePolicyManager::refreshResumeAvailability()` が `canResumeInterruptedGame()` に問い合わせる）。棋譜を変えたとき（`KifuBranchTree` の構造の変更の知らせ `nodeAppended`・`childrenReordered`・`subtreeRemoved`・`reset`）、別の対局を始めたとき、局面編集を始めたとき（`MainWindowActions::beginPositionEditing()`）、「新規」にしたときは、中断した対局を捨てる。

再開（`GameSession::resumeInterruptedGame()`）は、中断の行の親ノードへ表示中の手を戻し（`KifuNavigationState::resetToNode()`）、ライブ対局のセッションを破棄して中断の行を除いてから、そのノードでセッションを開始し直す。終了日時を消したあと、`MatchCoordinator::resumeInterruptedGame()` が手順・盤・時計を復元して Strategy を作り直し（「同」表記の基準は復元した手順の最後の手から導く）、続く `MatchCoordinator::beginPlay()` が時計の始動と初手の要求を行う。以前の `position` 文字列の履歴は保持しない。先読みの局面はエンジンの `UsiMatchHandler` だけが持ち、Strategy の `start()` がエンジンを作り直すので残らない。復元した確定手順から新しい探索を始める。時計は保存時の値（時間の決まりと時間切れの扱いを含む）を復元し、通常開始の設定適用は通さない。

### 連続対局

エンジン同士の連続対局の次局は、終局の後処理（`gameEndProcessed` → `GameSession::onGameEndProcessed()` → `ConsecutiveGameSeries::startNextGame()`）から少し遅らせて準備する。`ConsecutiveGameSeries`（`src/game/consecutivegameseries.cpp`）は1局目の計画を元に、開始手順を空にし（同じ開始局面の `GameStartPreparation::Mode::NewGame`）、盤の反転を外した計画を控えておく。手番を入れ替える設定ならエンジン名・エンジンのパス・持ち時間を入れ替え、`nextGameReady` で渡す。`GameSession::startNextGame()` は独立した棋譜として同じ開始局面で棋譜を作り直し（`SessionResetCoordinator::resetToNewRecord()`）、`startGame()` を連続対局の局数を数え直さずに呼ぶ。時間設定は通常の開始と同じく一度だけ適用する。

## 回帰テスト

- `tst_game_start_options`: 選択した分岐とSFEN手数の異なる開始局面を、実際のUSI通信経路で検証する。開始設定と対局者名、人を手前に表示するための反転も検証する。
- `tst_game_start_flow`: 不正な開始要求で計画が作られず状態が変わらないこと、時計設定が一度だけ実行されること、ダイアログからの対局者と時間設定の読み取りを検証する。
- `tst_preset_gamestart_cleanup`, `tst_positionedit_gamestart`: 新規・現在局面・局面編集・中断からの開始を検証する。
- `tst_app_game_session`, `tst_wiring_contracts`: 合成ルートで組み立てた `GameSession` と配線で、開始・終局時の棋譜、ライブセッション、時計、UI状態を検証する。
- `tst_matchcoordinator`, `tst_match_end`: 司令塔の開始・着手の記録・終局の記録と知らせ・待った・中断を検証する。
- `tst_gamestrategy`, `tst_undo_flow`, `tst_background_tasks`: 着手確定、待った、中断時のプロセス終了を検証する。

これらは本体の `shogiboardq_static` をリンクする。本体クラスのメソッドを空実装に差し替えるテスト用シンボル定義は使用しない。外部エンジンとの通信にはテスト用のUSIプロセスを用いる。
