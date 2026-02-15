# 対局開始から投了までのフロー

本ドキュメントでは、対局が開始された後の指し手の流れ（人間の着手、エンジンの着手、時計管理）から、投了による終局までの一連のフローを説明する。

## 全体フロー図

```
対局開始（configureAndStart 完了）
    │
    ▼
┌────────────────────────────────────────┐
│          対局ループ                      │
│                                          │
│  ┌──────────────────────────────────┐  │
│  │ 人間の手番                        │  │
│  │  1. 盤面クリック（駒選択→移動先） │  │
│  │  2. validateAndMove()             │  │
│  │  3. 棋譜記録・時計更新            │  │
│  └───────────┬──────────────────────┘  │
│              │                           │
│              ▼                           │
│  ┌──────────────────────────────────┐  │
│  │ エンジンの手番                    │  │
│  │  1. position + go コマンド送信    │  │
│  │  2. bestmove 受信                 │  │
│  │  3. validateAndMove()             │  │
│  │  4. 棋譜記録・時計更新            │  │
│  └───────────┬──────────────────────┘  │
│              │                           │
│              ▼                           │
│  最大手数チェック → 持将棋               │
│  タイムアウトチェック → 時間切れ負け     │
│              │                           │
│              ▼                           │
│  手番を交代して対局ループに戻る          │
└────────────────────────────────────────┘
    │
    │ 投了 / 中断 / 時間切れ / 入玉宣言
    ▼
┌────────────────────────────────────────┐
│          終局処理                        │
│  1. エンジンへ gameover 通知            │
│  2. setGameOver() で終局状態確定        │
│  3. 棋譜に終局手（「投了」等）を追記    │
│  4. 結果ダイアログ表示                  │
│  5. 棋譜自動保存（設定時）              │
│  6. リプレイモードへ遷移                │
└────────────────────────────────────────┘
```

---

## 1. 人間の着手フロー

### 1.1 盤面クリック → 駒選択

**ファイル:** `src/board/boardinteractioncontroller.cpp`

```
ユーザーが盤面をクリック
    ↓
ShogiView::clicked シグナル
    ↓
BoardInteractionController::onLeftClick(pt)
```

`onLeftClick()` の処理:

1. **人間の手番チェック**: `m_isHumanTurnCb` コールバックで現在が人間の手番かを確認。エンジンの手番なら無視する。
2. **1回目のクリック（駒選択）**: クリックした座標の駒を選択し、オレンジ色のハイライトを表示。`m_waitingSecondClick = true` に設定。
3. **2回目のクリック（移動先指定）**: 移動先の座標を確定し、`moveRequested(from, to)` シグナルを発行。

### 1.2 指し手のルーティング

**ファイル:** `src/ui/controllers/boardsetupcontroller.cpp`

```
BoardInteractionController::moveRequested(from, to)
    ↓
BoardSetupController::onMoveRequested(from, to)
```

`onMoveRequested()` の処理:
- PlayMode（対局モード）を判定
- 通常の対局モードであることを確認（局面編集モードでないこと）
- `ShogiGameController::validateAndMove()` を呼び出す

### 1.3 指し手の検証と盤面更新

**ファイル:** `src/game/shogigamecontroller.cpp`

```cpp
bool ShogiGameController::validateAndMove(
    QPoint& outFrom, QPoint& outTo, QString& record,
    PlayMode& playMode, int nextMoveIndex,
    QStringList* sfenRecord, QVector<ShogiMove>& gameMoves)
```

#### 処理ステップ

| ステップ | 処理内容 |
|---|---|
| 入力検証 | 盤面の存在確認、移動元の駒の存在確認、座標の妥当性検証 |
| 成り判定 | `decidePromotion()` で成り/不成りを判定。人間の場合は成り確認ダイアログを表示 |
| 盤面更新 | `board()->updateBoardAndPieceStand()` で駒の移動・取得を反映 |
| SFEN記録 | `board()->addSfenRecord()` で新しい局面SFENをリストに追加 |
| 棋譜文字列生成 | 指し手を日本語棋譜表記に変換（例: `"▲７六歩(77)"`） |
| moveCommitted発行 | `emit moveCommitted(moverBefore, confirmedPly)` シグナルを発行（手番変更**前**） |
| 手番変更 | `setCurrentPlayer()` で相手に手番を移す |

#### 成り判定の詳細

```
decidePromotion()
    ├─ 強制成り: 行き場のない駒（例: 1段目の歩）→ 自動で成り
    ├─ 人間の選択: 成りゾーン内の場合 → showPromotionDialog() を発行
    │              → ユーザーの回答を待つ
    └─ エンジン: setForcedPromotion() で自動適用
```

### 1.4 着手後のハイライト更新

**ファイル:** `src/board/boardinteractioncontroller.cpp`

```
BoardInteractionController::onMoveApplied(from, to, success)
```

- 成功時: 移動元に赤色ハイライト、移動先に黄色ハイライトを表示
- オレンジ色の選択ハイライトをクリア

---

## 2. 対局モード別の着手後処理

### 2.1 人間 vs エンジン（HvE）: onHumanMove_HvE()

**ファイル:** `src/game/matchcoordinator.cpp:2450-2594`

人間が指した後、エンジンに思考を依頼して1手返しを行う中核メソッド。

#### フロー詳細

```
人間の着手が確定（validateAndMove成功）
    ↓
onHumanMove_HvE(humanFrom, humanTo)
    │
    ├─ (1) 人間側タイマー停止・考慮時間確定
    │      finishHumanTimerAndSetConsideration()
    │
    ├─ (2) エンジンに前回の指し手座標を通知
    │      eng->setPreviousFileTo() / setPreviousRankTo()
    │
    ├─ (3) USI時間パラメータの計算
    │      computeGoTimesForUSI(bMs, wMs)
    │
    ├─ (4) エンジンの手番かチェック
    │      engineTurnNow = (gc->currentPlayer() == engineSeat)
    │      └─ エンジンの手番でなければ return（人間タイマー再開）
    │
    ├─ (5) エンジンに思考を依頼（ブロッキング）
    │      eng->handleHumanVsEngineCommunication(
    │          positionStr, positionPonder,
    │          eFrom, eTo,             // 出力: エンジンの指し手
    │          byoyomiMs, bTime, wTime,
    │          positionStrHistory,
    │          incMs1, incMs2, useByoyomi
    │      )
    │
    ├─ (6) エンジンの指し手を盤面に適用
    │      gc->validateAndMove(eFrom, eTo, rec, ...)
    │
    ├─ (7) エンジンの考慮時間を時計に設定
    │      thinkMs = eng->lastBestmoveElapsedMs()
    │      clock->setPlayer*ConsiderationTime(thinkMs)
    │      clock->applyByoyomiAndResetConsideration*()
    │
    ├─ (8) 棋譜にエンジンの指し手を追記
    │      hooks.appendKifuLine(rec, elapsed)
    │
    ├─ (9) 盤面描画・ハイライト・手番表示を更新
    │      hooks.renderBoardFromGc()
    │      hooks.showMoveHighlights(eFrom, eTo)
    │      updateTurnDisplay(m_cur)
    │
    ├─ (10) 評価値グラフに追記
    │       hooks.appendEvalP1() or hooks.appendEvalP2()
    │
    ├─ (11) 最大手数チェック
    │       m_maxMoves > 0 && m_currentMoveIndex >= m_maxMoves
    │       └─ 到達: handleMaxMovesJishogi() → return
    │
    └─ (12) 人間タイマー再開
           armHumanTimerIfNeeded()
```

#### USI通信の詳細

`handleHumanVsEngineCommunication()` の内部処理:

1. **position コマンド送信**: `position startpos moves 7g7f 3c3d ...`
2. **go コマンド送信**: `go btime <bMs> wtime <wMs> [byoyomi <ms>] [binc <ms> winc <ms>]`
3. **bestmove 応答待ち**: エンジンが `bestmove <move> [ponder <move>]` を返すまでブロック
4. **指し手の解析**: USI座標（例: `"7g7f"`）をQPoint座標に変換
5. **ponder処理**: 予測手がある場合はponder文字列を保存

### 2.2 人間 vs 人間（HvH）: onHumanMove_HvH()

**ファイル:** `src/game/matchcoordinator.cpp:3073-3095`

```cpp
void MatchCoordinator::onHumanMove_HvH(ShogiGameController::Player moverBefore)
{
    const Player moverP = (moverBefore == ShogiGameController::Player1) ? P1 : P2;

    // 直前手の消費時間（consideration）を確定
    finishTurnTimerAndSetConsiderationFor(moverP);

    // 秒読み/インクリメントを適用し、総考慮へ加算して表示値を確定
    if (m_clock) {
        if (moverP == P1) {
            m_clock->applyByoyomiAndResetConsideration1();
        } else {
            m_clock->applyByoyomiAndResetConsideration2();
        }
    }

    // 表示更新（時計ラベル等）
    if (m_clock) handleTimeUpdated();

    // 次手番の計測と UI 準備
    armTurnTimerIfNeeded();
}
```

HvHはエンジン通信がないため、処理はシンプル:
1. 着手者の考慮時間を確定
2. 秒読み/フィッシャー加算を適用
3. 時計表示を更新
4. 次の手番のタイマーを開始

### 2.3 エンジン vs エンジン（EvE）: kickNextEvETurn()

**ファイル:** `src/game/matchcoordinator.cpp:1435-1504`

EvEの対局は `QTimer::singleShot(0, ...)` を使ったイベントループ駆動の自動ループで進行する。

```
kickNextEvETurn()
    │
    ├─ (1) モードチェック・エンジン存在確認
    │
    ├─ (2) 現在の手番を判定
    │      p1ToMove = (gc->currentPlayer() == Player1)
    │      mover    = p1ToMove ? m_usi1 : m_usi2
    │      receiver = p1ToMove ? m_usi2 : m_usi1
    │
    ├─ (3) エンジンに思考を依頼
    │      engineThinkApplyMove(mover, pos, ponder, &from, &to)
    │          ├─ computeGoTimes() で時間パラメータ計算
    │          ├─ handleEngineVsHumanOrEngineMatchCommunication() で通信
    │          └─ from, to に指し手座標を格納
    │
    ├─ (4) 指し手を盤面に適用
    │      gc->validateAndMove(from, to, rec, ...)
    │      m_eveMoveIndex++
    │
    ├─ (5) 相手側のposition文字列を同期
    │      p1ToMove → m_positionStr2 = m_positionStr1
    │      p2ToMove → m_positionStr1 = m_positionStr2
    │
    ├─ (6) 考慮時間を時計に設定
    │      thinkMs = mover->lastBestmoveElapsedMs()
    │      clock->setPlayer*ConsiderationTime(thinkMs)
    │      clock->applyByoyomiAndResetConsideration*()
    │
    ├─ (7) 棋譜追記・盤面描画・ハイライト
    │      hooks.appendKifuLine(rec, elapsed)
    │      hooks.renderBoardFromGc()
    │      hooks.showMoveHighlights(from, to)
    │
    ├─ (8) 相手エンジンに前回の指し手座標を通知
    │      receiver->setPreviousFileTo() / setPreviousRankTo()
    │
    ├─ (9) 最大手数チェック
    │      m_maxMoves > 0 && m_eveMoveIndex >= m_maxMoves
    │      └─ 到達: handleMaxMovesJishogi() → return
    │
    └─ (10) 次のターンをスケジュール
           QTimer::singleShot(0, this, &MatchCoordinator::kickNextEvETurn)
```

**重要**: `QTimer::singleShot(0, ...)` により、Qtのイベントループに制御を返してからら次のターンを実行する。これによりUIの更新が途切れず、アプリケーションがフリーズしない。

---

## 3. 時計管理

### 3.1 時間計測の仕組み

対局モードごとに異なる方法で考慮時間を計測する。

| モード | 計測方法 | 計測開始 | 計測終了 |
|---|---|---|---|
| HvH | `QElapsedTimer` (m_turnTimer) | `armTurnTimerIfNeeded()` | `finishTurnTimerAndSetConsiderationFor()` |
| HvE（人間） | `QElapsedTimer` (m_humanTurnTimer) | `armHumanTimerIfNeeded()` | `finishHumanTimerAndSetConsideration()` |
| HvE（エンジン） | `Usi::lastBestmoveElapsedMs()` | go送信時 | bestmove受信時 |
| EvE | `Usi::lastBestmoveElapsedMs()` | go送信時 | bestmove受信時 |

### 3.2 タイマー管理メソッド

#### HvH用

```cpp
// タイマー開始
void MatchCoordinator::armTurnTimerIfNeeded()  // line 1544
{
    if (!m_turnTimerArmed) {
        m_turnTimer.start();
        m_turnTimerArmed = true;
    }
}

// タイマー停止・考慮時間確定
void MatchCoordinator::finishTurnTimerAndSetConsiderationFor(Player mover)  // line 1551
{
    if (!m_turnTimerArmed) return;
    const qint64 ms = m_turnTimer.isValid() ? m_turnTimer.elapsed() : 0;
    if (m_clock) {
        if (mover == P1) m_clock->setPlayer1ConsiderationTime(static_cast<int>(ms));
        else             m_clock->setPlayer2ConsiderationTime(static_cast<int>(ms));
    }
    m_turnTimer.invalidate();
    m_turnTimerArmed = false;
}
```

#### HvE用

```cpp
// 人間タイマー開始
void MatchCoordinator::armHumanTimerIfNeeded()  // line 1566

// 人間タイマー停止・考慮時間確定
void MatchCoordinator::finishHumanTimerAndSetConsideration()  // line 1573

// 人間タイマー無効化（終局時等）
void MatchCoordinator::disarmHumanTimerIfNeeded()  // line 1589
```

### 3.3 秒読み/フィッシャー加算の適用

各着手後に呼ばれるメソッド:

```cpp
// 先手の秒読み/加算を適用し、考慮時間カウンタをリセット
m_clock->applyByoyomiAndResetConsideration1();

// 後手の秒読み/加算を適用し、考慮時間カウンタをリセット
m_clock->applyByoyomiAndResetConsideration2();
```

### 3.4 USI goコマンドの時間パラメータ計算

**ファイル:** `src/game/matchcoordinator.cpp` - `computeGoTimes()`

```
GoTimes computeGoTimes() の返り値:
    btime   = 先手の残り時間（ms）
    wtime   = 後手の残り時間（ms）
    byoyomi = 秒読み（ms）、0なら秒読みなし
    binc    = 先手フィッシャー加算（ms）
    winc    = 後手フィッシャー加算（ms）
```

**秒読みモード**の場合:
- 秒読み期間に入っている場合は `btime`/`wtime` を0にする
- `byoyomi` に固定秒読み時間を設定

**フィッシャーモード**の場合:
- `btime`/`wtime` に残り時間を設定
- `binc`/`winc` に加算時間を設定
- 残り時間から加算分を引いてからエンジンに送信

### 3.5 時計のUI更新

**ファイル:** `src/game/matchcoordinator.cpp:2935-2943`

```cpp
void MatchCoordinator::handleTimeUpdated()
{
    emit timeTick();  // UI側でリフレッシュ

    QString turn, p1, p2;
    recomputeClockSnapshot(turn, p1, p2);
    emit uiUpdateTurnAndClock(turn, p1, p2);
}
```

`recomputeClockSnapshot()` は以下を生成:
- `turnText`: `"先手番"` or `"後手番"`
- `p1`: 先手の残り時間 `"MM:SS"` 形式
- `p2`: 後手の残り時間 `"MM:SS"` 形式

---

## 4. 棋譜記録

### 4.1 SFEN記録の更新

各着手時に `validateAndMove()` 内で更新される:

```cpp
board()->addSfenRecord(nextPlayerColorSfen, moveNumber, m_sfenHistory);
```

SFENリストには各手番後の局面が順に追加される:
```
[0] "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"  (初期局面)
[1] "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 2"  (1手目後)
[2] ...
```

### 4.2 棋譜行の追記

MatchCoordinatorのHooks経由で棋譜欄に1行追記される:

```cpp
m_hooks.appendKifuLine(rec, elapsed);
```

- `rec`: 指し手の日本語表記（例: `"▲７六歩(77)"`）
- `elapsed`: 考慮時間と累計時間（例: `"00:03/00:00:06"`）

### 4.3 指し手リストの管理

```cpp
QVector<ShogiMove> m_gameMoves;  // 対局中の全指し手
int m_currentMoveIndex;           // 現在の手数インデックス
```

---

## 5. 投了フロー

### 5.1 投了の発動

**ファイル:** `src/ui/wiring/uiactionswiring.cpp:34`

```cpp
QObject::connect(ui->actionResign, &QAction::triggered,
                 mw, &MainWindow::handleResignation, Qt::UniqueConnection);
```

ユーザーがメニュー「対局」→「投了」をクリックすると発動する。**確認ダイアログは表示されない**（即座に投了処理が実行される）。

### 5.2 投了のルーティング

```
MainWindow::handleResignation()
    ↓
GameStateController::handleResignation()  (src/game/gamestatecontroller.cpp:102-107)
    ↓
MatchCoordinator::handleResign()  (src/game/matchcoordinator.cpp:147-214)
```

### 5.3 MatchCoordinator::handleResign()

**ファイル:** `src/game/matchcoordinator.cpp:147-214`

投了処理の中核メソッド。

#### 処理ステップ

**1. 終局済みチェック（行149-152）**

```cpp
if (m_gameOver.isOver) {
    return;  // 中断後のタイムアウト等で呼ばれるのを防ぐ
}
```

**2. GameEndInfoの構築（行154-160）**

```cpp
GameEndInfo info;
info.cause = Cause::Resignation;
// 投了は「現在手番側」が行う
info.loser = (m_gc->currentPlayer() == ShogiGameController::Player1) ? P1 : P2;
const Player winner = (info.loser == P1) ? P2 : P1;
```

投了の敗者は「現在の手番のプレイヤー」。自分の手番中に投了するため。

**3. エンジンへの終了通知（行162-206）**

対局モードに応じてエンジンに通知を送る:

| モード | 通知内容 |
|---|---|
| HvE | 勝ったエンジンに `"gameover win"` + `"quit"` |
| EvE | 負けたエンジンに `"gameover lose"`、勝ったエンジンに `"gameover win"` + `"quit"` |
| HvH | エンジン通知なし |

**4. 終局状態の確定（行210）**

```cpp
setGameOver(info, /*loserIsP1=*/(info.loser==P1), /*appendMoveOnce=*/true);
```

**5. 結果ダイアログの表示（行213）**

```cpp
displayResultsAndUpdateGui(info);
```

### 5.4 エンジン投了: handleEngineResign()

**ファイル:** `src/game/matchcoordinator.cpp:216-243`

エンジンが `bestmove resign` を返した場合の処理。

```
USIエンジンが "bestmove resign" を送信
    ↓
UsiProtocolHandler が bestMoveResignReceived シグナルを発行
    ↓
wireResignToArbiter() で接続されたスロットが呼ばれる
    ├─ エンジン1の場合: onEngine1Resign()
    └─ エンジン2の場合: onEngine2Resign()
    ↓
handleEngineResign(idx)
    ├─ 時計を停止（stopコマンドは送らない）
    ├─ 負けエンジンに "gameover lose" + "quit"
    ├─ 勝ちエンジンに "gameover win" + "quit"
    ├─ setGameOver(info, loserIsP1, appendMoveOnce=true)
    └─ displayResultsAndUpdateGui(info)
```

---

## 6. 終局状態管理

### 6.1 setGameOver()

**ファイル:** `src/game/matchcoordinator.cpp:1742-1768`

終局を確定させる唯一の入口。

```cpp
void MatchCoordinator::setGameOver(const GameEndInfo& info, bool loserIsP1, bool appendMoveOnce)
{
    if (m_gameOver.isOver) return;  // 二重呼び出し防止

    m_gameOver.isOver        = true;
    m_gameOver.hasLast       = true;
    m_gameOver.lastLoserIsP1 = loserIsP1;
    m_gameOver.lastInfo      = info;
    m_gameOver.when          = QDateTime::currentDateTime();

    emit gameOverStateChanged(m_gameOver);
    emit gameEnded(info);

    if (appendMoveOnce && !m_gameOver.moveAppended) {
        emit requestAppendGameOverMove(info);  // 棋譜への終局手追記を要求
    }
}
```

#### 発行されるシグナル

| シグナル | 受信先 | 処理内容 |
|---|---|---|
| `gameOverStateChanged` | GameStateController | UI状態の遷移（リプレイモードへ） |
| `gameEnded` | GameStateController | ボードロック、タイマー停止、棋譜追記 |
| `requestAppendGameOverMove` | GameStateController | 終局手の棋譜追記（1回限り） |

### 6.2 displayResultsAndUpdateGui()

**ファイル:** `src/game/matchcoordinator.cpp:356-411`

結果表示とUI更新。

```cpp
void MatchCoordinator::displayResultsAndUpdateGui(const GameEndInfo& info)
{
    // 対局中メニューを無効化
    setGameInProgressActions(false);

    // 結果メッセージを構築
    QString msg;
    switch (info.cause) {
    case Cause::Resignation:
        msg = tr("%1の投了。%2の勝ちです。").arg(loserJP, winnerJP);
        break;
    case Cause::Timeout:
        msg = tr("%1の時間切れ。%2の勝ちです。").arg(loserJP, winnerJP);
        break;
    case Cause::Jishogi:
        msg = tr("最大手数に達しました。持将棋です。");
        break;
    case Cause::NyugyokuWin:
        msg = tr("%1の入玉宣言。%2の勝ちです。").arg(winnerJP, winnerJP);
        break;
    case Cause::IllegalMove:
        msg = tr("%1の反則負け。%2の勝ちです。").arg(loserJP, winnerJP);
        break;
    case Cause::BreakOff:
    default:
        msg = tr("対局が終了しました。");
        break;
    }

    // ダイアログ表示
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(tr("対局終了"), msg);
    }

    // 棋譜自動保存
    if (m_autoSaveKifu && !m_kifuSaveDir.isEmpty() && m_hooks.autoSaveKifu) {
        m_hooks.autoSaveKifu(m_kifuSaveDir, m_playMode,
                             m_humanName1, m_humanName2,
                             m_engineNameForSave1, m_engineNameForSave2);
    }

    emit gameEnded(info);
}
```

### 6.3 棋譜への終局手追記: appendGameOverLineAndMark()

**ファイル:** `src/game/matchcoordinator.cpp:2995-3071`

棋譜欄に終局手（「投了」「時間切れ」等）を1行だけ追記する。

#### 終局手の棋譜表記

| 終局原因 | 棋譜表記例 |
|---|---|
| 投了 (Resignation) | `"▲投了"` / `"△投了"` |
| 時間切れ (Timeout) | `"▲時間切れ"` / `"△時間切れ"` |
| 持将棋 (Jishogi) | `"▲持将棋"` / `"△持将棋"` |
| 入玉勝ち (NyugyokuWin) | `"▲入玉勝ち"` / `"△入玉勝ち"` |
| 反則負け (IllegalMove) | `"▲反則負け"` / `"△反則負け"` |
| 中断 (BreakOff) | `"▲中断"` / `"△中断"` |

`▲` = 先手、`△` = 後手（敗者側のマーク）

#### 二重追記防止

```cpp
void MatchCoordinator::appendGameOverLineAndMark(Cause cause, Player loser)
{
    if (!m_gameOver.isOver) return;        // 終局していなければ無視
    if (m_gameOver.moveAppended) return;   // 既に追記済みなら無視

    // ... 棋譜行の生成と追記 ...

    markGameOverMoveAppended();            // moveAppended = true にして二重追記を防止
}
```

`markGameOverMoveAppended()` が呼ばれると `m_gameOver.moveAppended = true` になり、`gameOverStateChanged` シグナルが再発行される。これにより `GameStateController::onGameOverStateChanged()` が呼ばれ、リプレイモードへの遷移が行われる。

---

## 7. 終局後のUI状態遷移

### 7.1 GameStateController::onMatchGameEnded()

**ファイル:** `src/game/gamestatecontroller.cpp:181-223`

```
gameEnded シグナル受信
    ↓
onMatchGameEnded(info)
    ├─ (1) ボードのゲームオーバースタイルロックを有効化
    │      shogiView->setGameOverStyleLock(true)
    │
    ├─ (2) タイマー停止・マウスクリック無効化
    │      match->disarmHumanTimerIfNeeded()
    │      clock->stopClock()
    │      shogiView->setMouseClickMode(false)
    │
    ├─ (3) 棋譜追記と時間確定
    │      setGameOverMove(info.cause, loserIsP1)
    │          ├─ appendGameOverLineAndMark() で棋譜に終局手を追記
    │          ├─ shogiView->update()
    │          ├─ 棋譜ビューを単一選択モードに設定
    │          ├─ リプレイモードを有効化
    │          └─ ライブ追記モードを終了
    │
    └─ (4) UIの後処理
           ナビゲーション矢印ボタンを有効化
           棋譜ビューを単一選択モードに設定
```

### 7.2 GameStateController::onGameOverStateChanged()

**ファイル:** `src/game/gamestatecontroller.cpp:225-280`

`moveAppended = true` になった後に呼ばれ、リプレイモードへの最終遷移を行う。

```
gameOverStateChanged シグナル受信（moveAppended=true）
    ↓
onGameOverStateChanged(st)
    ├─ (1) ライブ追記モード終了
    │      replayController->exitLiveAppendMode()
    │
    ├─ (2) 分岐コンテキストのリセット
    │      kifuLoadCoordinator->resetBranchContext()
    │
    ├─ (3) 分岐ツリーの再構築
    │      refreshBranchTree()
    │
    ├─ (4) 現在手数の設定
    │      updatePlyState(lastRow, lastRow, lastRow)
    │
    ├─ (5) UI遷移
    │      enableArrowButtons()  → ナビゲーション矢印を有効化
    │      setReplayMode(true)   → リプレイモードに設定
    │
    └─ (6) ハイライト消去
           shogiView->removeHighlightAllData()
```

---

## 8. その他の終局条件

### 8.1 時間切れ: handlePlayerTimeOut()

**ファイル:** `src/game/matchcoordinator.cpp:2945-2952`

```cpp
void MatchCoordinator::handlePlayerTimeOut(int player)
{
    if (!m_gc) return;
    m_gc->applyTimeoutLossFor(player);  // 時間切れ負けを適用
    emit uiNotifyTimeout(player);
    handleGameEnded();
}
```

時計の残り時間が0になったときに呼ばれる。`applyTimeoutLossFor()` は `ShogiGameController` にて対戦結果を設定する。

### 8.2 中断: handleBreakOff()

**ファイル:** `src/game/matchcoordinator.cpp:1781-1856`

ユーザーがメニュー「対局」→「中断」をクリックした場合の処理。

```
handleBreakOff()
    ├─ (1) エンジン投了シグナルを切断（レースコンディション防止）
    │      disconnect(m_usi1, &Usi::bestMoveResignReceived, ...)
    │      disconnect(m_usi2, &Usi::bestMoveResignReceived, ...)
    │
    ├─ (2) 終局済みチェック
    │      if (m_gameOver.isOver) return
    │
    ├─ (3) 終局状態の設定
    │      m_gameOver.isOver = true
    │      m_gameOver.lastInfo.cause = Cause::BreakOff
    │
    ├─ (4) 時計停止・タイマー停止
    │      m_clock->markGameOver()
    │      m_clock->stopClock()
    │      disarmHumanTimerIfNeeded()
    │
    ├─ (5) エンジンへの通知
    │      EvE: 両エンジンに stop → "gameover draw" → quit
    │      HvE: 主エンジンに stop → "gameover draw" → quit
    │
    ├─ (6) UI終局通知
    │      emit gameEnded(info)
    │
    ├─ (7) 棋譜追記
    │      appendBreakOffLineAndMark() → "▲中断" or "△中断"
    │
    └─ (8) エンジンにquit送信
           m_usi1->sendQuitCommand()
           m_usi2->sendQuitCommand()
```

### 8.3 最大手数到達（持将棋）: handleMaxMovesJishogi()

**ファイル:** `src/game/matchcoordinator.cpp:3234-3285`

```
handleMaxMovesJishogi()
    ├─ 終局済みチェック
    ├─ タイマー停止
    ├─ エンジンへ "gameover draw" + quit 通知
    ├─ GameEndInfo構築（cause = Jishogi）
    ├─ 終局状態を直接設定
    ├─ appendGameOverLineAndMark(Jishogi, P1) → "▲持将棋"
    └─ displayResultsAndUpdateGui(info) → 「最大手数に達しました。持将棋です。」
```

### 8.4 入玉宣言: handleNyugyokuDeclaration()

**ファイル:** `src/game/matchcoordinator.cpp:251-335`

入玉宣言には3つの結果がある:

| 結果 | Cause | 敗者 |
|---|---|---|
| 引き分け（持将棋） | `Jishogi` | 宣言者（表示用） |
| 宣言成功（勝ち） | `NyugyokuWin` | 宣言者の相手 |
| 宣言失敗（反則負け） | `IllegalMove` | 宣言者 |

処理はいずれも:
1. タイマー停止・時計停止
2. GameEndInfo構築
3. エンジンへ適切なgameover通知
4. `setGameOver()` で終局確定

---

## 9. 終局原因一覧

| 原因 | Cause列挙値 | 発動契機 | 棋譜表記 |
|---|---|---|---|
| 投了 | `Resignation` | ユーザーの投了操作 / エンジンの `bestmove resign` | `"▲投了"` |
| 時間切れ | `Timeout` | 時計の残り時間が0 | `"▲時間切れ"` |
| 中断 | `BreakOff` | ユーザーの中断操作 | `"▲中断"` |
| 持将棋 | `Jishogi` | 最大手数到達 / 入玉宣言の引き分け | `"▲持将棋"` |
| 入玉宣言勝ち | `NyugyokuWin` | エンジンの入玉宣言成功 | `"▲入玉勝ち"` |
| 反則負け | `IllegalMove` | 入玉宣言失敗 | `"▲反則負け"` |

---

## 10. シグナルチェーンまとめ

### 投了時の完全なシグナルフロー

```
ユーザー: メニュー「投了」クリック
    ↓
actionResign::triggered
    ↓
MainWindow::handleResignation()
    ↓
GameStateController::handleResignation()
    ↓
MatchCoordinator::handleResign()
    ├─ エンジンへ "gameover win/lose" + "quit"
    ├─ setGameOver(info, loserIsP1, true)
    │   ├─ emit gameOverStateChanged(m_gameOver)  ──→ (A)
    │   ├─ emit gameEnded(info)                    ──→ (B)
    │   └─ emit requestAppendGameOverMove(info)    ──→ (C)
    └─ displayResultsAndUpdateGui(info)
        ├─ setGameInProgressActions(false)
        ├─ hooks.showGameOverDialog("対局終了", "先手の投了。後手の勝ちです。")
        ├─ hooks.autoSaveKifu(...)  // 設定時
        └─ emit gameEnded(info)                    ──→ (B')

(B) GameStateController::onMatchGameEnded(info)
    ├─ shogiView->setGameOverStyleLock(true)
    ├─ match->disarmHumanTimerIfNeeded()
    ├─ clock->stopClock()
    ├─ shogiView->setMouseClickMode(false)
    └─ setGameOverMove(cause, loserIsP1)
        └─ appendGameOverLineAndMark(Resignation, loser)
            ├─ "▲投了" を棋譜に追記
            └─ markGameOverMoveAppended()
                └─ emit gameOverStateChanged(moveAppended=true)  ──→ (A')

(A') GameStateController::onGameOverStateChanged(st)  // moveAppended=true
    ├─ replayController->exitLiveAppendMode()
    ├─ kifuLoadCoordinator->resetBranchContext()
    ├─ refreshBranchTree()
    ├─ updatePlyState(lastRow)
    ├─ enableArrowButtons()
    ├─ setReplayMode(true)
    └─ shogiView->removeHighlightAllData()

対局終了 → リプレイ（棋譜閲覧）モードへ
```

---

## 11. 関連ファイル一覧

| コンポーネント | ヘッダ | 実装 |
|---|---|---|
| 盤面操作コントローラ | `src/board/boardinteractioncontroller.h` | `src/board/boardinteractioncontroller.cpp` |
| ボードセットアップコントローラ | `src/ui/controllers/boardsetupcontroller.h` | `src/ui/controllers/boardsetupcontroller.cpp` |
| ゲームコントローラ | `src/game/shogigamecontroller.h` | `src/game/shogigamecontroller.cpp` |
| 対局司令塔 | `src/game/matchcoordinator.h` | `src/game/matchcoordinator.cpp` |
| ゲーム状態コントローラ | `src/game/gamestatecontroller.h` | `src/game/gamestatecontroller.cpp` |
| ターンマネージャ | `src/game/turnmanager.h` | `src/game/turnmanager.cpp` |
| USIエンジン | `src/engine/usi.h` | `src/engine/usi.cpp` |
| USIプロトコルハンドラ | `src/engine/usiprotocolhandler.h` | `src/engine/usiprotocolhandler.cpp` |
| 将棋時計 | `src/services/shogiclock.h` | `src/services/shogiclock.cpp` |
| UIアクション配線 | `src/ui/wiring/uiactionswiring.h` | `src/ui/wiring/uiactionswiring.cpp` |
