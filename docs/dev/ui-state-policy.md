# UI状態ポリシーマネージャ（UiStatePolicyManager）

本ドキュメントでは、アプリケーションの各モードにおけるUI要素の有効/無効/表示/非表示の制御ルールを説明する。

## 概要

`UiStatePolicyManager`（`src/ui/coordinators/uistatepolicymanager.h/.cpp`）は、アプリケーションの7つの状態に応じて、全UI要素のポリシーを一元管理する状態テーブル駆動型のクラスである。

## アプリケーション状態（AppState）

状態は `AppActivityState::Activity`（`src/common/appactivitystate.h`）で、値は `AppState::activity` に1つだけ置く
（`UiStatePolicyManager::AppState` はこの列挙の別名）。状態は各部品が `AppActivityState::setActivity()` で直接切り替える。
`UiStatePolicyManager` は `AppActivityState::changed` を受けて全UI要素にポリシーを適用し直すので、
状態を変える側は `UiStatePolicyManager` を知らなくてよい。

| 状態 | 説明 | この状態にする部品 |
|---|---|---|
| **Idle** | 待機状態（起動直後・各モード終了後） | 各モードを終える部品・「新規」（`GameSession::resetToInitialState()`） |
| **DuringGame** | 対局中 | `MatchCoordinator::start()`・`resumeInterruptedGame()` |
| **DuringAnalysis** | 棋譜解析中 | `KifuAnalysisController::start()` |
| **DuringCsaGame** | CSA通信対局中 | `CsaGameSession::onGameStarted()`（`CsaGameCoordinator::gameStarted` を受ける） |
| **DuringTsumeSearch** | 詰み探索中 | `AnalysisSession::start()`（詰み探索のとき） |
| **DuringConsideration** | 検討中 | `AnalysisSession::start()`（検討のとき） |
| **DuringPositionEdit** | 局面編集中 | `PositionEditSession::begin()` |

## 状態遷移図

```
Idle ──MatchCoordinator::start() / resumeInterruptedGame()────────► DuringGame
     ◄─MatchCoordinator::endGame()（終局・中断・エンジンエラー）──

Idle ──KifuAnalysisController::start()────────────────────────────► DuringAnalysis
     ◄─KifuAnalysisController::finishRun()（完了・中止・エンジンエラー・エンジンの枠をほかの処理が取った（engineRevoked））

Idle ──CsaGameSession::onGameStarted()────────────────────────────► DuringCsaGame
     ◄─CsaGameSession::onGameEnded()

Idle ──AnalysisSession::start()（詰み探索）────────────────────────► DuringTsumeSearch
     ◄─AnalysisSession::stop()（探索完了・中止・エンジンエラー・エンジンの枠をほかの処理が取った（engineRevoked））

Idle ──AnalysisSession::start()（検討）────────────────────────────► DuringConsideration
     ◄─AnalysisSession::stop()（検討中止・エンジンエラー・エンジンの枠をほかの処理が取った（engineRevoked））

Idle ──PositionEditSession::begin()──────────────────────────────► DuringPositionEdit
     ◄─PositionEditSession::finish()
```

CSA通信対局は、待機ダイアログのキャンセル（`CsaGameSession::onWaitingCancelled()`）や対局が始まらなかったときも
`CsaGameSession` が Idle に戻す。

## ポリシー（Policy）

| ポリシー | 動作 |
|---|---|
| **Enabled (E)** | 表示＋有効（クリック可能） |
| **Disabled (D)** | 表示＋無効（グレーアウト） |
| **Hidden (H)** | 非表示 |

## 状態テーブル（有効/無効マトリクス）

表は `uistatepolicymanager.cpp` のコンパイル時の表 `kRules` で、UI要素ごとに「有効にする状態の集合」と
「それ以外の状態で無効にする（Disable）か隠す（Hide）か」を宣言している。メニュー項目との対応は `kActions`。

### ファイルメニュー

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 新規 | E | D | D | D | D | D | D |
| 開く | E | D | D | D | D | D | D |
| 上書き保存 | E | D | D | D | E | E | D |
| 名前を付けて保存 | E | D | D | D | E | E | D |
| 盤面画像保存 | E | E | E | E | E | E | E |
| 評価値グラフ画像保存 | E | E | E | E | E | E | E |
| 終了 | E | E | E | E | E | E | E |

### 編集メニュー

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 棋譜コピー（KIF/KI2/CSA/JKF/USI全て/USEN） | E | D | D | D | E | E | D |
| 局面コピー（USI現在の指し手まで/SFEN/BOD） | E | E | E | E | E | E | E |
| 棋譜貼り付け | E | D | D | D | D | D | D |
| SFEN集ビューア | E | D | D | D | D | D | D |
| 盤面画像コピー | E | E | E | E | E | E | E |
| 評価値グラフ画像コピー | E | E | E | E | E | E | E |
| 局面編集開始 | E | H | H | H | H | H | H |
| 局面編集終了 | H | H | H | H | H | H | E |
| 平手初期配置 | H | H | H | H | H | H | E |
| 詰将棋初期配置 | H | H | H | H | H | H | E |
| 全駒を駒台へ | H | H | H | H | H | H | E |
| 手番変更 | H | H | H | H | H | H | E |

### 対局メニュー

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 対局 | E | D | D | D | D | D | D |
| CSA通信対局 | E | D | D | D | D | D | D |
| 投了 | D | E | D | E | D | D | D |
| 待った | D | E | D | D | D | D | D |
| すぐ指させる | D | E | D | E | D | D | D |
| 中断 | D | E | D | E | D | D | D |
| 再開 | E | D | D | D | D | D | D |
| 棋譜解析 | E | D | D | D | D | D | D |
| 棋譜解析中止 | D | D | E | D | D | D | D |
| 詰み探索 | E | D | D | D | D | D | D |
| 詰み探索中止 | D | D | D | D | E | D | D |
| 詰将棋局面生成 | E | D | D | D | D | D | D |
| 持将棋点数 | E | E | E | E | E | E | E |
| 入玉宣言 | D | E | D | E | D | D | D |

### 設定メニュー

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| エンジン設定 | E | D | D | D | D | E | D |
| 設定を初期値に戻す | E | D | D | D | D | D | D |

### ウィジェット

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| ナビゲーションボタン | E | D | D | D | E | E | D |
| 棋譜欄クリック | E | D | D | D | E | E | D |
| 分岐ツリークリック | E | D | D | D | E | E | D |
| 検討ドック「検討開始」ボタン | E | D | E | D | E | E | E |

**補足**:
- 表のほかに条件が付く項目がある（`effectivePolicy()`）。待った・投了・入玉宣言は、ローカル対局では人間が参加しているときだけ有効
  （`Deps::localGameHasHuman`。エンジン同士の対局では無効。CSA通信対局の投了・入玉宣言はこの条件を見ない）。
  再開は中断した対局を再開できるときだけ有効（`Deps::canResumeGame`。合成ルートが `MatchCoordinator::canResumeInterruptedGame()` を渡す。中断した対局の状態は司令塔の `InterruptedGameState` だけが持つ）
- 「ナビゲーションボタン」と「棋譜欄クリック」は同じ UI 要素（`WidgetNavigation`）で、局面送りの QAction を `KifuNavigationController::setStepActionsEnabled()`（`Deps::navigation`）、棋譜欄の行のクリックを `RecordPane::setKifuViewEnabled()`（`Deps::recordPane`）で切り替える。
  棋譜欄と評価値グラフのボタンは `KifuNavigationController::bindButtons()` で同じ QAction に結ばれ、有効・無効も QAction に従う
- 「分岐ツリークリック」は `BranchTreeManager::setBranchTreeClickEnabled()` で制御
- 盤面クリック（指し手入力）は表に含めない。駒をつまめるかは盤で指した手の行き先（`BoardInputRoute`。主画面は `BoardMoveController`）が決める（局面編集中は常に可、それ以外は `HumanTurnPolicy::isHumanTurnNow()`。解析中・詰み探索中・検討中は受け付けない）
- 「検討開始」ボタンは `ConsiderationTabManager::setStartConsiderationEnabled()` で制御（検討中は同ボタンが「検討中止」として動作するため有効のまま）
- 「表示メニュー」は全状態で常に有効のため省略

## 状態変化の伝わり方

```
各部品 ──setActivity()──► AppActivityState（AppState::activity）
                              │ changed
                              ▼
                 UiStatePolicyManager::refreshActivity()
                   → 全UI要素に effectivePolicy() を適用 → stateChanged

MatchCoordinator::resumeAvailabilityChanged ──► refreshResumeAvailability()（canResumeGame に問い合わせて「再開」だけ適用し直す）
KifuLoadCoordinator::loadFinished ──► enableNavigationIfAllowed()（ナビゲーションが許可されていれば局面送りの QAction を有効化）
```

## エンジンエラー時の復旧パス

エンジンのクラッシュ（セグフォルト等）やプロセスエラー発生時、各モードは以下の経路で Idle に遷移する:

```
【対局中】
  Usi::errorOccurred → EngineLifecycleManager::engineError
    → MatchCoordinator::handleEngineError() → handleBreakOff() → endGame()
    → setActivity(Idle) → gameEnded（GameSession::onGameEnded()）

【棋譜解析中】
  Usi::errorOccurred → AnalysisJob::onError() → stop() → finished(false)
    → KifuAnalysisController::onFinished() → finishRun() → setActivity(Idle)
  （AnalysisJob::errorOccurred → KifuAnalysisController::onEngineError() がエラーを表示する）

【詰み探索中】
  Usi::errorOccurred → AnalysisSession::onEngineError()
    → stop() → setActivity(Idle) → tsumeSearchModeEnded

【検討中】
  Usi::errorOccurred → AnalysisSession::onEngineError()
    → stop() → setActivity(Idle) → considerationModeEnded
```

**注意**: CSA通信対局は `CsaGameCoordinator`（エンジンのエラーは `CsaEngineController::engineError` → `onEngineControllerError()`）と `CsaGameSession` が独自にエラー管理するため、`MatchCoordinator::handleEngineError()` では対象外。

## 設計上の注意点

- **テーブル駆動**: UI要素を足すときは `UiElement` に値を足し、`kRules` に同じ順で規則を、メニュー項目なら `kActions` に対応する `QAction` を足す。`kRules` が全要素を宣言順に並べていることは `static_assert` で確かめる
- **依存の注入**: `MainWindowComposer::createSessions()` がコンストラクタの `Deps` で依存オブジェクトを渡す（[composition-root.md](composition-root.md)）。`Deps::localGameHasHuman` は司令塔の対局者（`MatchCoordinator::isHumanSide()`）から決める。`Deps::activity` を渡さないと自分だけの動作状態を使う（テスト用）
- **表示メニュー**: 全状態で常に有効のため、テーブルには含めていない
- **盤面クリック**: `UiStatePolicyManager` は盤を触らない。`BoardInteractionController` はクリックのたびに `BoardInputRoute::acceptsInput()` に問い合わせる。動作状態による判定は `BoardMoveController`（`HumanTurnPolicy` を使う）が行う
