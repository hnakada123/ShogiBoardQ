# 対局ダイアログから対局開始までのフロー

対局の開始処理は、開始位置を値として確定してから検証し、その後に棋譜・盤・時計・表示へ適用する。部品の生成と配線は [MainWindowComposer](composition-root.md) が起動時に行い、対局開始の各シグナルは `MainWindowComposer::connectMatchSignals()`（`src/app/mainwindowcomposer_game.cpp`）がつなぐ。開始後の指し手と終局の流れは [対局開始から投了までのフロー](game-play-to-resignation-flow.md) を参照。

## 開始要求

メニュー「対局」→「対局」は `GameSession::startGameFromDialog()`（`src/app/gamesession.cpp`）を呼ぶ。棋譜または直前の対局の開始局面が平手でない（局面編集した・棋譜の局面図から始めた）なら、ダイアログで「現在の局面」を選ばせる。現在の局面以外から始めるときは、未保存の棋譜を捨ててよいかを `KifuFileController::confirmDiscardUnsaved()` で確かめる。ダイアログの結果と `KifuNavigationState::currentRecordLine()` を `GameStartCoordinator::initializeGame()` に渡す。この時点では棋譜も対局の状態も書き換えない。開始できなければ、理由を `ErrorBus` で知らせる。

`GameStartCoordinator::Ctx` は次を渡す。

| 値 | 用途 |
|---|---|
| `dialogData` | 対局者、手合割、時間設定、最大手数、自動保存、連続対局など |
| `recordLine` | 選択した分岐の開始局面・指し手・現在手数（`RecordLine`） |
| `startNodeId` | 分岐ツリーで選択したノードのID |
| `currentSfen`, `startSfen` | 表示中の局面と棋譜の開始局面（`AppState`）。手順が無い単独局面から始めるときに使う |
| `bottomSide` | 盤の手前に表示している側（`ShogiView::flipMode()` から求める）。人間を手前に表示するときの判定に使う |

`recordLine` の手順は、同じ局面へ到達する別の手順と区別する。過去の対局をSFENで検索したり、SFEN末尾の手数から棋譜を切り詰めたりしない。棋譜を持たない単独局面は、局面1件・指し手0件の手順になる。

## 検証と適用の順序

1. `GameStartCoordinator::initializeGame()` が現在局面またはプリセットから開始する手順を決定する。現在局面では `recordLine.currentPly` までを取り（`RecordLine::upTo()`）、後続の手を除く。プリセットでは `GameStartOptionsBuilder::startingPositionSfen()` の局面だけの手順にする。
2. 開始局面の玉を検証する（`validateKingsForGame()`：先手・後手の玉が盤上に1枚ずつ）。不正なら、クリーンアップ・時計設定・エンジン起動を行わずエラーを返す。
3. `GameStartOptionsBuilder` が `MatchStartOptions`（対局者 `MatchPlayers`・エンジンのパス・最大手数・自動保存）と時間設定を作り、`opt.recordLine` に決定した手順を入れる。
4. `requestPreStartCleanup(GameStartPreparation)` を通知する。`GameStartPreparation::Mode` は `NewGame`、`CurrentPosition`、`Reset` のいずれかであり、表示用SFENから推測しない。
5. `GameSession::prepareStart()` が `PreStartCleanupHandler::performCleanup()` を呼ぶ。新規ならツリーを開始局面で作り直し、現在局面なら指定ノードまでの棋譜表示を作る。終局ノードが選択されていれば親を起点とし、末尾の中断ノードは取り除く。指定ノードが無ければ、同じ局面を探し直さずに何も始めない。
6. クリーンアップが返した起点から、`GameSession::prepareStart()` がライブ対局のセッションを開始する（`LiveGameSession::startFromNode()`）。`Reset` では開始しない。クリーンアップハンドラ自体は対局を開始しない。
7. 「人を手前に表示する」なら盤の反転を頼み（`boardFlipRequested` → `MainWindowAppearanceController::flipBoard()`）、連続対局の設定を通知する（`consecutiveGamesConfigured` → `ConsecutiveGamesController::configure()`）。
8. `GameStartCoordinator::start()` が時計設定を一度適用し（`TimeControlUtil::applyToClock()`：時間の決まり・時間切れの扱い・開始局面の手番）、`requestApplyTimeControl` で表示層へ通知する。`GameSession::applyTimeControl()` は連続対局用に設定を控え、`TimeControlController::presentTimeControl()` で設定と開始時刻の記録・表示を行い、対局情報に持ち時間を書く。時計は再設定しない。
9. 対局者を通知する（`playerNamesResolved` → `PlayerInfoPresenter::onMatchPlayersResolved()`。盤・対局情報・通信ログの見出しの名前）。
10. `MatchCoordinator::start()` に確定した `MatchStartOptions` を渡す。司令塔は手順を `recordLine.currentPly` までに切り、`MatchSessionState::begin()` で設定を持ち、対局の手順（`MatchCoordinator::record()`）を作り、動作状態を対局中にし、盤を開始局面にする（`ShogiGameController::newGame()`）。`gamePrepared` を受けた `GameSession::onGamePrepared()` が `AppState` の開始局面・表示中の局面・対局者を更新し、盤・対局者名・手番の表示を合わせる。最後に対局者（`MatchPlayers`）から Strategy を作って開始する（エンジンの生成と起動。初期化は非同期）。
11. `finishStart()` が開始通知（`started` → `UiStatePolicyManager::transitionToDuringGame()`、`GameSession::onGameStarted()`）、棋譜の開始行の選択（`requestSelectKifuRow`）、`MatchCoordinator::beginPlay()`（時計の始動とエンジン初手の要求）を行う。初期化待ちのエンジンは準備完了後に思考を始める。

`GameSession::onGameStarted()` は、終局の知らせで対局者を下手・上手と呼ぶかを司令塔に設定し（`setHandicapNames()`）、中断した対局を捨て、定跡ウィンドウを更新し、連続対局用に開始設定を控える。

## 対局中と再開

`MatchSessionState`（`src/game/matchsessionstate.h`）は対局設定（対局者・最大手数・自動保存）、終了状態、エンジンの先読み局面を所有する。終了状態の変更は `finish()`、`markTerminalRecorded()`、`clearGameOver()` を通し、フィールドへのポインタを他の部品へ渡さない。

Strategy は `MatchStrategyHost`（`src/game/gamemodestrategy.h`。司令塔が private に実装する）を通して盤・時計・エンジンの枠を読み、一手の確定とエンジンへの思考の依頼を司令塔に頼む。司令塔のメンバーには直接触れない。エンジンへの `position` は、その時点の対局の手順（`MatchCoordinator::record()`）から `RecordLine::positionCommand()` で生成する。人間対エンジン、エンジン同士、待ったの後で同じ生成経路を使う。

### 中断と再開

中断（`MatchCoordinator::handleBreakOff()`）は、`captureInterruptedGame()` で開始の設定・対局の手順・時計のスナップショット・「同」表記の基準を `InterruptedGameState` に保存してから終局を記録し、`gameInterrupted` を出す。`GameSession::onGameInterrupted()` は中断の行のノードを覚えて「対局再開」を使えるようにする。棋譜を変えたとき（`KifuBranchTree::treeChanged`）、別の対局を始めたとき、局面編集を始めたときは、中断した対局を捨てる。

再開（`GameSession::resumeInterruptedGame()`）は、中断の行の親ノードへカーソルを戻し、ライブ対局のセッションを破棄して中断の行を除いてから、そのノードでセッションを開始し直す。続く `GameStartCoordinator::resumeInterruptedGame()` が `MatchCoordinator::resumeInterruptedGame()` で手順・盤・手番・時計を復元して Strategy を作り直し、`finishStart()` で開始通知・行の選択・時計の始動と初手の要求を行う。以前の `position` 文字列の履歴は保持しない。先読みは破棄し（`MatchSessionState::begin()`）、復元した確定手順から新しい探索を始める。時計は保存時の値（時間の決まりと時間切れの扱いを含む）を復元し、通常開始の設定適用は通さない。

### 連続対局

エンジン同士の連続対局の次局は、終局の後処理（`gameEndProcessed` → `GameSession::onGameEndProcessed()` → `ConsecutiveGamesController::startNextGame()`）から少し遅らせて準備する。`ConsecutiveGamesController` は控えた開始設定から、手番を入れ替える設定ならエンジンと持ち時間を入れ替え、開始手順を空にした `StartParams`（対局者名の通知あり）を `nextGameReady` で渡す。`GameSession::startNextGame()` は独立した棋譜として同じ開始局面で棋譜まわりを作り直し、`GameStartPreparation::Mode::NewGame` でクリーンアップしてから `GameStartCoordinator::start()` を呼ぶ。時間設定は通常の `start()` が一度適用する。

## 回帰テスト

- `tst_game_start_options`: 選択した分岐とSFEN手数の異なる開始局面を、実際のUSI通信経路で検証する。開始設定と対局者名、人を手前に表示するための反転も検証する。
- `tst_game_start_flow`: 不正な開始要求で状態が変わらないこと、クリーンアップと時計設定が一度だけ実行されること、ダイアログからの対局者と時間設定の読み取りを検証する。
- `tst_preset_gamestart_cleanup`, `tst_positionedit_gamestart`: 新規・現在局面・局面編集・中断からの開始を検証する。
- `tst_app_game_session`, `tst_wiring_contracts`: 合成ルートで組み立てた `GameSession` と配線で、開始・終局時の棋譜、ライブセッション、時計、UI状態を検証する。
- `tst_matchcoordinator`, `tst_match_end`: 司令塔の開始・着手の記録・終局の記録と知らせ・待った・中断を検証する。
- `tst_gamestrategy`, `tst_undo_flow`, `tst_background_tasks`: 着手確定、待った、中断時のプロセス終了を検証する。

これらは本体の `shogiboardq_static` をリンクする。本体クラスのメソッドを空実装に差し替えるテスト用シンボル定義は使用しない。外部エンジンとの通信にはテスト用のUSIプロセスを用いる。
