# 手動テストシナリオ

作成日: 2026-02-25  
更新日: 2026-10-15
目的: MainWindow 責務移譲の各フェーズ完了後に、既存挙動の回帰がないことを確認する。

---

## ベースライン数値

| 指標 | 値 |
|---|---|
| `mainwindow.cpp` 行数 | 45 |
| `mainwindow.h` 行数 | 48 |
| `MainWindow::` メソッド数 | 5 |

---

## シナリオ 1: 新規対局開始

### 1-A: 人間 vs 人間

**操作手順:**
1. アプリを起動する
2. メニュー「対局」→「新規対局」を選択する
3. 対局設定ダイアログで「人間 vs 人間」を選択する
4. 対局者名を入力する（例: 先手「テスト先手」、後手「テスト後手」）
5. 「開始」をクリックする

**期待される結果:**
- 盤面が平手初期配置で表示される
- 先手側の対局者名が正しく表示される
- 後手側の対局者名が正しく表示される
- 手番表示が先手（下側）になっている
- 棋譜欄がクリアされ、初期状態になっている
- ナビゲーションボタンが無効化されている（対局中）

**重点確認ポイント:**
- 対局者の組 `MatchPlayers`（`MatchCoordinator::players()`）が人間同士（`blackIsEngine`・`whiteIsEngine` とも false）になっているか
- 対局者の記録（`GameSession::startGame()` → `beginMatchRecord()` → `KifuDocument::beginMatch()`。`KifuDocument::players()`）と盤への表示（文書と動作状態の知らせを受ける `PlayerInfoPresenter`）と、動作状態（`AppActivityState::activity()`） が `DuringGame` になるか（`MatchCoordinator::start()`）
- `UiStatePolicyManager` の状態遷移（`Idle` → `DuringGame`）
- 時計表示の初期化（持ち時間設定がある場合。`TimeDisplayPresenter::showClock()`。以後は `ShogiClock::timeUpdated` を直接受けて更新）

### 1-B: 人間 vs エンジン

**操作手順:**
1. アプリを起動する
2. メニュー「対局」→「新規対局」を選択する
3. 対局設定ダイアログで「人間 vs エンジン」を選択する
4. エンジンを選択し、持ち時間を設定する
5. 「開始」をクリックする

**期待される結果:**
- 盤面が平手初期配置で表示される
- 先手（人間）の対局者名が表示される
- 後手（エンジン）のエンジン名が表示される
- エンジンの思考出力が思考ドックに表示される
- 先手が人間の手番として操作可能になる
- 持ち時間が設定通り表示される

**重点確認ポイント:**
- `MatchPlayers` で後手だけがエンジン（`whiteIsEngine`）になっているか
- エンジンの初期化・接続（USI isready → readyok）
- `HumanTurnPolicy::isHumanTurnNow()` が先手の手番で `true` を返すか
- 時計への持ち時間の適用（`MatchCoordinator::start()` が `MatchStartOptions::time` を `ShogiClock` に設定し、`KifuDocument::beginMatch()` が `MatchTimeRecord::begin()` で対局情報用に記録する）
- 対局者名・エンジン名の表示（盤の対局者名は `PlayerInfoPresenter`、評価値グラフの系列名と思考タブの2つ目のエンジン欄は `KifuDocument::playersChanged` → `EngineDisplayPresenter`）

---

## シナリオ 2: 指し手反映と棋譜追記

**前提:** シナリオ1で対局が開始されている状態

**操作手順:**
1. 先手が `7六歩` を指す（7七の歩を7六にドラッグ）
2. 後手が `3四歩` を指す（3三の歩を3四にドラッグ、またはエンジンの応手を待つ）
3. 数手指し続ける

**期待される結果:**
- 各指し手が盤面に正しく反映される
- 駒のハイライト（直前の移動先）が表示される
- 棋譜欄に手数と指し手が追記される
- 棋譜欄の最新行が自動選択される
- 手番表示が交互に切り替わる
- 持ち時間がカウントダウンされる（設定時）

**重点確認ポイント:**
- 人間の手は `BoardMoveController` → `MatchCoordinator::onHumanMove()`、エンジンの手は Strategy → `MatchStrategyHost::applyEngineMove()` を通り、`MatchCoordinator::completeMove()` が対局の手順（`MatchCoordinator::record()`）に積み、`MatchRecordSink::appendRecord` で棋譜の受け手に渡すフロー
- `GameSession::appendRecord` → `LiveGameSession::appendRecord` による分岐ツリー（棋譜の正本）への指し手の記録（USI 形式の指し手はノードの手 `ShogiMove::move`（`UsiMove`）から作る）
- 表示中の手（`KifuNavigationState::currentNode()`）が最新手に移ること（`LiveGameSession::appendRecord` が移す）。棋譜欄・分岐ツリーは `KifuDisplayPresenter`、盤とハイライトは `BoardSyncPresenter` が `KifuNavigationState::changed` を受けて合わせる
- 手番表示の切り替え（盤の手番 `ShogiBoard::turnChanged` → `BoardChromeWidget::setActiveSide`）
- 評価値グラフへのプロット追加（エンジン対局時。`MatchCoordinator::engineMovePlayed` → `EvaluationGraphPresenter::plotEngineScore`）

---

## シナリオ 3: 棋譜行選択での盤面同期

**前提:** 数手指した状態、または棋譜ファイルが読み込まれた状態

**操作手順:**
1. 棋譜欄で任意の行をクリックする（例: 3手目）
2. 開局位置（0手目）をクリックする
3. 最終手をクリックする
4. 中間の手をクリックする

**期待される結果:**
- 選択した手数の局面が盤面に表示される
- 直前の指し手のハイライトが正しく表示される
- 手番表示が局面に合った状態になる
- コメントが該当手数のものに切り替わる
- 定跡ウィンドウが更新される（表示中の場合）

**重点確認ポイント:**
- `RecordPane::mainRowChanged` → `KifuNavigationController::selectRecordRow()` のフロー（未保存コメントの確認 `confirmLeave()` の後、`KifuNavigationState::setCurrentNode()` で表示中の手を移す）
- `BoardSyncPresenter::syncToCurrentNode()` による盤面とハイライトの更新（ノードの SFEN が盤と違うときだけ `ShogiBoard::setSfen()`）
- 手番表示が盤の手番（`ShogiBoard::turnChanged`）から決まること
- 盤面の局面が表示中の手順（分岐ツリーのノード）から取られていること
- 表示中の手数（`KifuNavigationState::currentPly()`）が選んだ行に合い、棋譜欄の選択が `KifuDisplayPresenter::refresh()` で同じ行に戻ること

---

## シナリオ 4: 分岐選択での盤面同期

**前提:** 分岐を含む棋譜が読み込まれた状態

**操作手順:**
1. 分岐ツリーで本譜以外の分岐ノードをクリックする
2. 分岐内で別の手数をクリックする
3. 本譜に戻る
4. 再度分岐を選択する

**期待される結果:**
- 分岐ノードの局面が盤面に表示される
- 分岐元からの差分ハイライトが正しく表示される
- 棋譜欄の表示が分岐ラインに切り替わる
- 手番表示が分岐局面に合った状態になる
- 本譜に戻ると棋譜欄が本譜表示に復帰する

**重点確認ポイント:**
- `BranchTreeView::nodeActivated(nodeId)` → `KifuNavigationController::selectBranchTreeNode(nodeId)` → `KifuNavigationState::setCurrentNode(node, lineEnd)` のフロー（表示中の手順にある手（分岐前の共有の手）はその手順のまま移り、ほかの手順の手はその手から先頭の子をたどった手順に移る）
- 分岐候補欄からの移動（`RecordPane::branchRowActivated` → `KifuNavigationController::selectBranchCandidateRow()`。「本譜へ戻る」は `goToMainLineAtCurrentPly()`）
- `BoardSyncPresenter::syncToCurrentNode()` による盤面 + ハイライト更新
- `KifuNavigationState::setCurrentNode(node, lineEnd)` が選んだ手順を覚え、以降の「進む」がその分岐をたどるか
- 分岐ナビ中に棋譜欄の選択の変化（`RecordPane::mainRowChanged`）で別の手に移らないか（`KifuDisplayPresenter` が選択を表示中の手に合わせる）

---

## シナリオ 5: 終局処理と連続対局

### 5-A: 通常終局

**操作手順:**
1. 対局を進め、詰みまたは投了に至る
   - または、メニュー「対局」→「投了」を選択する

**期待される結果:**
- 終局メッセージボックスが表示される
- 棋譜欄に終局手（投了/詰み等）が追記される
- ナビゲーションボタンが有効化される
- 盤面にゲームオーバースタイルが適用される
- 棋譜の全手を自由にナビゲーションできる
- 対局情報に終了日時が記録される

**重点確認ポイント:**
- `MatchCoordinator::endGame()` のフロー（時計の停止、エンジンへの通知、終局手の追記、動作状態を `Idle` に戻して `gameEnded`）
- `MatchCoordinator::appendTerminalRecord()` による終局手追記（`MatchRecordSink::appendRecord` 経由）
- 司令塔が終局で呼ぶ `GameSession::finishRecord()` で `LiveGameSession::commit()` が呼ばれ（中断も。中断して再開を待つ間も盤で変化を指せ、分岐を編集できるか）、終局日時が記録され（`KifuDocument::recordMatchEnd()`）、自動保存を設定していれば棋譜が保存されるか
- `UiStatePolicyManager` の状態遷移（`DuringGame` → `Idle`）
- `HumanTurnPolicy::isGameActivelyInProgress()` が `false` を返すか
- `MatchCoordinator::gameOverState().isOver` の設定

### 5-B: 連続対局（エンジン vs エンジン）

**操作手順:**
1. 「対局」→「連続対局設定」で回数を設定する（例: 3局）
2. エンジン vs エンジンで対局を開始する
3. 1局目の終了を待つ

**期待される結果:**
- 1局目終了後、自動的に2局目が開始される
- 先後交替設定が反映される（設定した場合）。先後で持ち時間を変えていれば、持ち時間もエンジンと一緒に入れ替わる
- 2局目以降も、盤の対局者名と対局情報（対局日・開始日時・対局者・持ち時間）が各局の内容になる
- 棋譜・盤面がリセットされ、新しい対局が始まる
- 設定した回数で連続対局が終了する

**重点確認ポイント:**
- `GameSession::onGameEndProcessed` → `ConsecutiveGameSeries::startNextGame()` → `nextGameReady` → `GameSession::startNextGame` のフロー
- `ConsecutiveGameSeries::shouldStartNextGame()` の判定
- `GameSession::startNextGame()` が `SessionResetCoordinator::resetToNewRecord()` で新しい棋譜にしてから `startGame()` を呼ぶか
- 1局目の計画（`GameStartPlan`。持ち時間を含む）を `ConsecutiveGameSeries::beginSeries()` で保持し、次局で使い直すか
- 先後交替時の `MatchPlayers`（エンジン名）・エンジンのパス・持ち時間の入れ替え（`ConsecutiveGameSeries::launchPreparedNextGame()`）

---

## シナリオ 6: リセット後の初期状態復帰

**操作手順:**
1. 対局を開始し、数手指す
2. メニュー「ファイル」→「新規」を選択する
3. 初期画面に戻ることを確認する
4. 再度対局を開始できることを確認する

**期待される結果:**
- 盤面が平手初期配置に戻る
- 棋譜欄がクリアされる
- 対局者名がクリアされる
- 手番表示が先手に戻る
- エンジンが停止する
- 評価値グラフがクリアされる
- コメント欄がクリアされる
- 分岐ツリーがクリアされる
- ナビゲーション状態がリセットされる
- 再度対局が正常に開始できる

**重点確認ポイント:**
- `GameSession::resetToInitialState()` の実行順序（「新規」は `MainWindowActions::resetToInitialState()` が未保存の確認をしてから呼ぶ）:
  1. 検討・詰み探索（`AnalysisSession::stop()`。自分のエンジンだけを手放す）と通信対局（`CsaGameCoordinator::stopGame()`）を止め、対局・棋譜解析のエンジンも `EngineSlots::destroyEngines(false)` で止めて（持ち主には `engineRevoked` で知らせる）、司令塔の終局状態・連続対局・中断した対局を消す
  2. `SessionResetCoordinator::resetToNewRecord()` — 読み込み途中の棋譜を捨て（`KifuLoader::cancelLoad()`）、`KifuDocument::startNew()` で平手の開始局面だけの棋譜にする。文書の置き換え（`KifuDocument::replaced`）を受けて、通信ログ・思考情報・検討・解析結果（`EngineDisplayPresenter::clearAll()`）と評価値グラフが消える。文書の置き換えで対局情報は既定の項目になり、対局者・持ち時間・終局日時・上書き保存先も消え、盤の対局者名は対局情報の初期値（「先手」「後手」）に戻る（`headerReset` → `PlayerInfoPresenter()`）
  3. 動作状態を `Idle` に戻す
- 動作状態（`AppActivityState::activity()`） が `Idle` に戻り、`KifuDocument::players()` が空・`KifuDocument::filePath()` が空になるか
- 分岐ツリーの開始局面（`KifuBranchTree::root()`）が平手 SFEN に戻り、表示中の手（`KifuNavigationState::currentSfen()`）も平手になるか
- 司令塔の終局状態が `MatchCoordinator::clearGameOverState()` で消えるか（対局の手順 `MatchCoordinator::record()` は次の対局の開始で置き換わる）
- `LiveGameSession` が破棄（`discard()`）されるか
- 再対局時に古い接続（signal/slot）が残っていないか

---

## シナリオ 7: 評価グラフ・コメント連携

### 7-A: 評価値グラフ

**操作手順:**
1. エンジン対局を開始する
2. 数手進め、評価値グラフにプロットが追加されることを確認する
3. 棋譜欄で過去の手数をクリックする
4. 評価値グラフのカーソルライン・手数ラベル・上部の評価値が移動することを確認する
5. 「表示設定」で手動固定の範囲を狭め、範囲外の点にホバーして元の評価値を確認する
6. 「自動」に戻し、ウィンドウの幅と文字サイズを変えて目盛りが重ならないことを確認する
7. 詰みが報告された局面で、端の三角マーカーと勝者・詰み手数を確認する
8. グラフを画像保存し、凡例・手数・評価値・マーカーが含まれることを確認する
9. 再起動して自動／手動固定、手動範囲、目盛り間隔、文字サイズが復元されることを確認する

**期待される結果:**
- 各手ごとにエンジンの評価値がプロットされる
- 棋譜行の選択に連動してカーソルラインが移動する
- グラフの表示範囲が適切（先手有利を上、後手有利を下）

**重点確認ポイント:**
- `EvaluationGraphPresenter::plotEngineScore` の呼び出し（`MatchCoordinator::engineMovePlayed` から）
- `EvaluationGraphPresenter::showCurrentPly` による縦線移動（`KifuNavigationState::changed` を受ける）
- 待った（`GameSession::undoTwoMoves`）時のプロット削除（`MatchCoordinator::movesUndone` → `EvaluationGraphPresenter::trimToPly`）

### 7-B: コメント

**操作手順:**
1. 棋譜を読み込む（コメント付き）
2. 棋譜欄で手数を切り替え、コメントが表示されることを確認する
3. コメントを編集する
4. 別の手数に移動し、編集した手数に戻る

**期待される結果:**
- 棋譜のコメントがコメント欄に表示される
- コメント編集が保存される
- 未保存コメントがある場合、行移動時に確認ダイアログが表示される

**重点確認ポイント:**
- `CommentCoordinator::showCurrentComment` によるコメント表示（`KifuNavigationState::changed`・`KifuDocument::commentChanged` を受ける）
- `CommentCoordinator::confirmLeaveEditingMove()` での未保存コメント確認ロジック（棋譜欄の行・分岐候補・分岐ツリー・ボタン・評価値グラフからの移動は、どれも `KifuNavigationController::setLeaveGuard()` に渡した確認を `confirmLeave()` で通る。移るのをやめたら `navigationRejected` で選択を表示中の手に戻す）
- `KifuDocument::commentChanged` シグナル（`KifuRecordListModel::refreshNode()` がその行を描き直す）
- 分岐ツリーを正本とするコメントと棋譜欄・コメント欄の表示の一致

---

## 重点回帰観点（全シナリオ共通）

以下の既知のリスクポイントを各フェーズ完了後に重点的に確認する:

1. **棋譜ナビゲーションの経路**
   - 棋譜欄・分岐ツリー・評価値グラフからの移動が、表示中の手（`KifuNavigationState`）を `KifuNavigationController` で動かすだけで盤面・棋譜欄・分岐ツリーに反映されるか（各部品が `KifuNavigationState::changed` を受けて合わせる）
   - 分岐ナビ中に棋譜欄の行選択の通知で盤面が古い局面に上書きされないか

2. **対局の手順の置き場**
   - 対局中の手順は司令塔（`MatchCoordinator`、起動時に一度だけ作る）の `record()`（`RecordLine`）だけが持ち、ほかの部品が局面列へのポインタを持っていないか（評価値グラフは `MatchCoordinator::engineMovePlayed` で指した側と手数を受け取り、評価値はエンジンの枠 `EngineSlots` から読む）
   - 盤面同期（`BoardSyncPresenter`）が対局の局面列ではなく、表示中の手（`KifuNavigationState::currentNode()`）の局面から盤面を描いているか

3. **手番の置き場**
   - 手番を盤（`ShogiBoard`）だけが持ち、`MatchCoordinator::sideToMove()`・手番表示・人間の手番の判定がどれも盤の手番（`ShogiBoard::currentPlayer()`）から決まるか
   - 棋譜ナビゲーション時は表示中の手の SFEN で盤が置き換わり、手番もそれに従うか

4. **`KifuLoader` の使い回し**
   - 起動時に作った1つのインスタンス（`MainWindowComposer::createSessions()` が作る）で、2回目以降の読み込みも正しく反映されるか
   - 読み込み中に別の棋譜を読み込んだとき、前の読み込みの結果が後から反映されないか（`KifuLoader::startLoad()` が前のジョブを捨てる）
   - 初期状態へのリセット（`GameSession::resetToInitialState()` → `SessionResetCoordinator::resetToNewRecord()`）で、読み込み途中の棋譜が `KifuLoader::cancelLoad()` で捨てられるか

5. **`UiStatePolicyManager` の状態遷移**
   - 各シナリオで期待される状態遷移が正しく発生しているか
   - 遷移漏れによるUI要素の無効化/有効化の不整合がないか
