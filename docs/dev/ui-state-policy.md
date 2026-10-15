# UI状態ポリシーマネージャ（UiStatePolicyManager）

本ドキュメントでは、アプリケーションの各モードにおけるUI要素の有効/無効/表示/非表示の制御ルールを説明する。

## 概要

`UiStatePolicyManager`（`src/ui/coordinators/uistatepolicymanager.h/.cpp`）は、アプリケーションの7つの状態に応じて、全UI要素のポリシーを一元管理する状態テーブル駆動型のクラスである。

従来は `disableNavigationForGame()` / `enableNavigationAfterGame()` / `applyEditMenuEditingState()` など個別メソッドで分散管理していたが、本クラスに統合した。

## アプリケーション状態（AppState）

状態は `AppActivityState::Activity`（`src/common/appactivitystate.h`）で、値は `AppState::activity` に1つだけ置く
（`UiStatePolicyManager::AppState` はこの列挙の別名）。`UiStatePolicyManager` は `AppActivityState::changed` を受けて
全UI要素にポリシーを適用し直すので、`MatchCoordinator`・`AnalysisSession`・`CsaGameWiring` などが
`setActivity()` で状態を変えたときにも表示が切り替わる。

| 状態 | 説明 | 遷移トリガー |
|---|---|---|
| **Idle** | 待機状態（起動直後・各モード終了後） | 各モード終了シグナル |
| **DuringGame** | 対局中 | `GameStartCoordinator::started` |
| **DuringAnalysis** | 棋譜解析中 | `DialogCoordinator::analysisModeStarted` |
| **DuringCsaGame** | CSA通信対局中 | `CsaGameCoordinator::gameStarted`（`CsaGameWiring::onGameStarted()`） |
| **DuringTsumeSearch** | 詰み探索中 | `DialogCoordinator::tsumeSearchModeStarted` |
| **DuringConsideration** | 検討中 | `DialogCoordinator::considerationModeStarted` |
| **DuringPositionEdit** | 局面編集中 | `PositionEditCoordinator` のコールバック（`onEnterEditMenu`） |

## 状態遷移図

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
                    │                 Idle                     │
                    │           （待機状態）                    │
                    │                                         │
                    └──┬───┬───┬───┬───┬───┬──────────────────┘
                       │   │   │   │   │   │
          started      │   │   │   │   │   │  コールバック
              ┌────────┘   │   │   │   │   └────────┐
              ▼            │   │   │   │            ▼
        ┌───────────┐      │   │   │   │    ┌───────────────┐
        │ DuringGame │      │   │   │   │    │DuringPosition │
        │ （対局中） │      │   │   │   │    │    Edit       │
        └─────┬─────┘      │   │   │   │    │（局面編集中） │
              │            │   │   │   │    └───────┬───────┘
   gameEnded               │   │   │   │            │
   gameOverStateChanged    │   │   │   │       コールバック
              │            │   │   │   │            │
              └─────►Idle◄─┘   │   │   └──◄─────────┘
                     ▲  ▲      │   │
    analysisModeEnded│  │      │   │considerationModeEnded
                     │  │      │   │tsumeSearchModeEnded
              ┌──────┘  └──┐   │   │
              │            │   │   │
        ┌───────────┐  ┌───────────┐  ┌───────────────┐
        │  During   │  │  During   │  │    During     │
        │ Analysis  │  │ CsaGame   │  │ TsumeSearch   │
        │（解析中） │  │（CSA中）  │  │（詰み探索中） │
        └───────────┘  └───────────┘  └───────────────┘
                                           │
                                      ┌────┘
                                      ▼
                               ┌───────────────┐
                               │    During     │
                               │Consideration  │
                               │  （検討中）   │
                               └───────────────┘
```

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
| 盤面クリック（指し手入力） | E | E | D | E | D | D | D |
| 検討ドック「検討開始」ボタン | E | D | E | D | E | E | E |

**補足**:
- 表のほかに条件が付く項目がある（`effectivePolicy()`）。待った・投了・入玉宣言は、ローカル対局では人間が参加しているときだけ有効
  （`Deps::localGameHasHuman`。エンジン同士の対局では無効。CSA通信対局の投了・入玉宣言はこの条件を見ない）。
  再開は中断した対局があるときだけ有効（`setResumeAvailable()`）
- 「ナビゲーションボタン」と「棋譜欄クリック」は `RecordPane::setNavigationEnabled()` で一括制御
- 「分岐ツリークリック」は `EngineAnalysisTab::setBranchTreeClickEnabled()` で制御
- 「盤面クリック」は `BoardInteractionController::setMoveInputEnabled()` で制御（対局中の手番チェックは `isHumanTurnCallback` で別途実施）
- 「検討開始」ボタンは `EngineAnalysisTab::setConsiderationStartEnabled()` で制御（検討中は同ボタンが「検討中止」として動作するため有効のまま）
- 「表示メニュー」は全状態で常に有効のため省略

## シグナル配線

```
GameStartCoordinator::started ──────────► transitionToDuringGame()
MatchCoordinator::gameOverStateChanged ─► GameSession::onGameOverStateChanged() → transitionToIdle()  （終局の行を棋譜に書いた後）
MatchCoordinator::gameEnded ────────────► GameSession::onGameEnded() → transitionToIdle()

DialogCoordinator::analysisModeStarted ─► transitionToDuringAnalysis()
DialogCoordinator::analysisModeEnded ───► transitionToIdle()  （ダイアログキャンセル時 or 解析完了/中止時）

DialogCoordinator::tsumeSearchModeStarted ► transitionToDuringTsumeSearch()
DialogCoordinator::tsumeSearchModeEnded ──► transitionToIdle()  （ダイアログキャンセル時）
AnalysisSession::tsumeSearchModeEnded ───► transitionToIdle()  （探索完了/中止時）

DialogCoordinator::considerationModeStarted ► transitionToDuringConsideration()
AnalysisSession::considerationModeEnded ───► transitionToIdle()

CsaGameCoordinator::gameStarted ─► CsaGameWiring::onGameStarted() → transitionToDuringCsaGame()
CsaGameCoordinator::gameEnded ───► CsaGameWiring::onGameEnded() → transitionToIdle()

PositionEditCoordinator onEnterEditMenu ─► transitionToDuringPositionEdit()
PositionEditCoordinator onLeaveEditMenu ─► transitionToIdle()
```

## エンジンエラー時の復旧パス

エンジンのクラッシュ（セグフォルト等）やプロセスエラー発生時、各モードは以下の経路で Idle に遷移する:

```
【対局中】
  Usi::errorOccurred → EngineLifecycleManager::engineError
    → MatchCoordinator::handleEngineError() → handleBreakOff() → gameEnded
    → GameSession::onGameEnded() → transitionToIdle()

【棋譜解析中】
  Usi::errorOccurred → AnalysisJob::errorOccurred → AnalysisFlowController::onEngineError()
    → stop() → analysisStopped()
    → DialogCoordinator::analysisModeEnded → transitionToIdle()

【詰み探索中】
  Usi::errorOccurred → AnalysisSession::onEngineError()
    → stop() → tsumeSearchModeEnded → transitionToIdle()

【検討中】
  Usi::errorOccurred → AnalysisSession::onEngineError()
    → stop() → considerationModeEnded → transitionToIdle()
```

**注意**: CSA通信対局は `CsaGameCoordinator` / `CsaGameWiring` が独自にエラー管理するため、`MatchCoordinator::handleEngineError()` では対象外。

## 設計上の注意点

- **テーブル駆動**: UI要素を足すときは `UiElement` に値を足し、`kRules` に同じ順で規則を、メニュー項目なら `kActions` に対応する `QAction` を足す。`kRules` が全要素を宣言順に並べていることは `static_assert` で確かめる
- **依存の注入**: 生成（`MainWindowComposer::createServices()`）の後、`MainWindowComposer::wireFoundation()` が `updateDeps()` で依存オブジェクトを渡す（[composition-root.md](composition-root.md)）。`Deps::localGameHasHuman` は司令塔の対局者（`MatchCoordinator::isHumanSide()`）から決める
- **表示メニュー**: 全状態で常に有効のため、テーブルには含めていない
- **対局中の盤面クリック**: 対局中も `setMoveInputEnabled(true)` のままにし、人間の手番かどうかは `BoardInteractionController` の `isHumanTurnCallback` が別途判定する
