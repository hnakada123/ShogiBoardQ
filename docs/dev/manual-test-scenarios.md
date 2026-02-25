# 手動テストシナリオ

作成日: 2026-02-25
目的: MainWindow 責務移譲の各フェーズ完了後に、既存挙動の回帰がないことを確認する。

---

## ベースライン数値

| 指標 | 値 |
|---|---|
| `mainwindow.cpp` 行数 | 3,444 |
| `mainwindow.h` 行数 | 798 |
| `MainWindow::` メソッド数 | 171 |

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
- `PlayMode::HumanVsHuman` が正しく設定されているか
- `m_state.playMode` の同期（`onGameStarted` 経由）
- `UiStatePolicyManager` の状態遷移（`Idle` → `DuringGame`）
- 時計表示の初期化（持ち時間設定がある場合）

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
- エンジンの思考出力がエンジン解析タブに表示される
- 先手が人間の手番として操作可能になる
- 持ち時間が設定通り表示される

**重点確認ポイント:**
- `PlayMode::EvenHumanVsEngine` が正しく設定されているか
- エンジンの初期化・接続（USI isready → readyok）
- `isHumanTurnNow()` が先手の手番で `true` を返すか
- `TimeControlController` への時間設定適用（`onApplyTimeControlRequested`）
- エンジン名の表示（`setEngineNamesBasedOnMode`）

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
- `onMoveCommitted` → `updateGameRecord` → `appendMoveLine` のフロー
- `m_kifu.gameUsiMoves` への USI 形式指し手の記録
- `m_state.currentSfenStr` の更新（`sfenRecord()->last()`）
- `LiveGameSessionUpdater::appendMove` によるライブセッション更新
- `onTurnManagerChanged` による手番切替の正確さ
- 評価値グラフへのプロット追加（エンジン対局時）

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
- `onRecordPaneMainRowChanged` → `RecordNavigationHandler::onMainRowChanged` のフロー
- `syncBoardAndHighlightsAtRow` → `KifuNavigationCoordinator` の委譲
- `BoardSyncPresenter::loadBoardWithHighlights` による盤面更新
- `setCurrentTurn()` による手番同期（非ライブモード: 盤面SFENから取得）
- `m_state.skipBoardSyncForBranchNav` ガードが不当に立っていないか
- `m_kifu.currentSelectedPly` の更新
- `m_state.currentMoveIndex` の更新

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
- `onBranchNodeActivated` → `BranchNavigationWiring` のフロー
- `onBranchNodeHandled` → `KifuNavigationCoordinator::handleBranchNodeHandled`
- `loadBoardFromSfen` による盤面更新
- `loadBoardWithHighlights` による盤面 + ハイライト更新
- `m_state.skipBoardSyncForBranchNav` ガードの設定と解除タイミング
- `syncNavStateToPly` による NavState の同期（現在ライン上のノード優先）
- 分岐ナビ中に `onRecordPaneMainRowChanged` が不当な盤面上書きをしないか

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
- `onMatchGameEnded` のフロー（GameStateController委譲 + TimeController記録）
- `onRequestAppendGameOverMove` による終局手追記
- `LiveGameSession::commit()` が呼ばれるか
- `UiStatePolicyManager` の状態遷移（`DuringGame` → `Idle`）
- `isGameActivelyInProgress()` が `false` を返すか
- `GameOverState.isOver` の設定

### 5-B: 連続対局（エンジン vs エンジン）

**操作手順:**
1. 「対局」→「連続対局設定」で回数を設定する（例: 3局）
2. エンジン vs エンジンで対局を開始する
3. 1局目の終了を待つ

**期待される結果:**
- 1局目終了後、自動的に2局目が開始される
- 先後交替設定が反映される（設定した場合）
- 棋譜・盤面がリセットされ、新しい対局が始まる
- 設定した回数で連続対局が終了する

**重点確認ポイント:**
- `onMatchGameEnded` → `startNextConsecutiveGame` のフロー
- `ConsecutiveGamesController::shouldStartNextGame()` の判定
- `onPreStartCleanupRequested` によるクリーンアップ
- `m_lastTimeControl` の保持と再適用
- 先後交替時の `PlayMode` 変更

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
- `resetToInitialState()` の実行順序:
  1. `resetEngineState()` — エンジン/CSA/連続対局停止
  2. `onPreStartCleanupRequested()` — クリーンアップ + UI クリア
  3. `resetGameState()` — 全状態変数クリア
  4. `resetModels()` — モデルリセット（ResetService委譲）
  5. `resetUiState()` — UI状態リセット
- `m_state.playMode` が `PlayMode::NotStarted` に戻るか
- `m_state.startSfenStr` が平手 SFEN に戻るか
- `m_state.currentSfenStr` が `"startpos"` に戻るか
- `sfenRecord()` が正しくクリアされるか（MC経由）
- `ReplayController` の状態がリセットされるか
- `LiveGameSession` がリセットされるか
- 再対局時に古い接続（signal/slot）が残っていないか

---

## シナリオ 7: 評価グラフ・コメント連携

### 7-A: 評価値グラフ

**操作手順:**
1. エンジン対局を開始する
2. 数手進め、評価値グラフにプロットが追加されることを確認する
3. 棋譜欄で過去の手数をクリックする
4. 評価値グラフのカーソルラインが移動することを確認する

**期待される結果:**
- 各手ごとにエンジンの評価値がプロットされる
- 棋譜行の選択に連動してカーソルラインが移動する
- グラフの表示範囲が適切（先手有利を上、後手有利を下）

**重点確認ポイント:**
- `EvaluationGraphController::redrawEngine1Graph` / `redrawEngine2Graph` の呼び出し
- `EvaluationGraphController::setCurrentPly` による縦線移動
- `undoLastTwoMoves` 時のプロット削除（`removeLastP1Score` / `removeLastP2Score`）

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
- `CommentCoordinator::broadcastComment` によるコメント表示
- `onRecordRowChangedByPresenter` での未保存コメント確認ロジック
- `GameRecordModel::commentChanged` シグナル
- `m_kifu.commentsByRow` の同期

---

## 重点回帰観点（全シナリオ共通）

以下の既知のリスクポイントを各フェーズ完了後に重点的に確認する:

1. **`skipBoardSyncForBranchNav` の競合**
   - 分岐ナビ中のガード設定/解除タイミングが正しいか
   - `QTimer::singleShot(0)` による解除が確実に機能しているか

2. **`sfenRecord()` 参照先のライフタイム**
   - `MatchCoordinator` 再生成時にポインタが無効化されないか
   - `ensureBoardSyncPresenter` で sfenRecord ポインタが更新されるか

3. **`TurnManager` と `GameController` の同期順序**
   - ライブ対局中は GC を単一ソースとしているか
   - 棋譜ナビゲーション時は盤面SFEN を優先しているか

4. **`KifuLoadCoordinator` 再生成時の古い接続残り**
   - `deleteLater()` 後に新しいインスタンスで接続が正しく張り直されるか
   - 旧インスタンスの保留シグナルによる dangling pointer がないか

5. **`UiStatePolicyManager` の状態遷移**
   - 各シナリオで期待される状態遷移が正しく発生しているか
   - 遷移漏れによるUI要素の無効化/有効化の不整合がないか
