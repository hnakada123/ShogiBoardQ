# UI状態ポリシーマネージャ（UiStatePolicyManager）

本ドキュメントでは、アプリケーションの各モードにおけるUI要素の有効/無効/表示/非表示の制御ルールを説明する。

## 概要

`UiStatePolicyManager`（`src/ui/coordinators/uistatepolicymanager.h/.cpp`）は、アプリケーションの7つの状態に応じて、全UI要素のポリシーを一元管理する状態テーブル駆動型のクラスである。

従来は `disableNavigationForGame()` / `enableNavigationAfterGame()` / `applyEditMenuEditingState()` など個別メソッドで分散管理していたが、本クラスに統合した。

## アプリケーション状態（AppState）

| 状態 | 説明 | 遷移トリガー |
|---|---|---|
| **Idle** | 待機状態（起動直後・各モード終了後） | 各モード終了シグナル |
| **DuringGame** | 対局中 | `GameStartCoordinator::started` |
| **DuringAnalysis** | 棋譜解析中 | `DialogCoordinator::analysisModeStarted` |
| **DuringCsaGame** | CSA通信対局中 | `CsaGameWiring::disableNavigationRequested` |
| **DuringTsumeSearch** | 詰み探索中 | `DialogCoordinator::tsumeSearchModeStarted` |
| **DuringConsideration** | 検討中 | `DialogCoordinator::considerationModeStarted` |
| **DuringPositionEdit** | 局面編集中 | `PositionEditCoordinator` コールバック |

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
   matchGameEnded          │   │   │   │            │
   enableArrowButtons      │   │   │   │       コールバック
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
| **Shown (S)** | 表示＋有効（Hidden の対として使用） |

## 状態テーブル（有効/無効マトリクス）

### ファイルメニュー

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 新規 | E | D | D | D | D | D | D |
| 開く | E | D | D | D | D | D | D |
| 上書き保存 | E | D | D | D | E | E | D |
| 名前を付けて保存 | E | D | D | D | E | E | D |
| 盤面画像保存 | E | E | E | E | E | E | E |
| 終了 | E | E | E | E | E | E | E |

### 編集メニュー

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 棋譜コピー（KIF/KI2/CSA/JKF） | E | D | D | D | E | E | D |
| 局面コピー（USI/USEN/SFEN/BOD） | E | E | E | E | E | E | E |
| 棋譜貼り付け | E | D | D | D | D | D | D |
| SFEN集ビューア | E | D | D | D | D | D | D |
| 盤面画像コピー | E | E | E | E | E | E | E |
| 局面編集開始 | S | H | H | H | H | H | H |
| 局面編集終了 | H | H | H | H | H | H | S |
| 平手初期配置 | H | H | H | H | H | H | S |
| 詰将棋初期配置 | H | H | H | H | H | H | S |
| 全駒を駒台へ | H | H | H | H | H | H | S |
| 手番変更 | H | H | H | H | H | H | S |

### 対局メニュー

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| 対局 | E | D | D | D | D | D | D |
| CSA通信対局 | E | D | D | D | D | D | D |
| 投了 | D | E | D | E | D | D | D |
| 待った | D | E | D | D | D | D | D |
| すぐ指させる | D | E | D | E | D | D | D |
| 中断 | D | E | D | E | D | D | D |
| 棋譜解析 | E | D | D | D | D | D | D |
| 棋譜解析中止 | D | D | E | D | D | D | D |
| 詰み探索 | E | D | D | D | D | D | D |
| 詰み探索中止 | D | D | D | D | E | D | D |
| 持将棋点数 | E | E | E | E | E | E | E |
| 入玉宣言 | D | E | D | E | D | D | D |

### 設定メニュー

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| エンジン設定 | E | D | D | D | D | E | D |

### ウィジェット

| UI要素 | Idle | 対局中 | 解析中 | CSA中 | 詰み探索中 | 検討中 | 局面編集中 |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| ナビゲーションボタン | E | D | D | D | E | E | D |
| 棋譜欄クリック | E | D | D | D | E | E | D |
| 分岐ツリークリック | E | D | D | D | E | E | D |
| 盤面クリック（指し手入力） | E | E | D | E | D | D | D |

**補足**:
- 「ナビゲーションボタン」と「棋譜欄クリック」は `RecordPane::setNavigationEnabled()` で一括制御
- 「分岐ツリークリック」は `EngineAnalysisTab::setBranchTreeClickEnabled()` で制御
- 「盤面クリック」は `BoardInteractionController::setMoveInputEnabled()` で制御（対局中の手番チェックは `isHumanTurnCallback` で別途実施）
- 「表示メニュー」は全状態で常に有効のため省略

## シグナル配線

```
GameStartCoordinator::started ──────────► transitionToDuringGame()
GameStartCoordinator::matchGameEnded ───► transitionToIdle()
GameStateController callback ───────────► transitionToIdle()

DialogCoordinator::analysisModeStarted ─► transitionToDuringAnalysis()
DialogCoordinator::analysisModeEnded ───► transitionToIdle()  （ダイアログキャンセル時 or 解析完了/中止時）

DialogCoordinator::tsumeSearchModeStarted ► transitionToDuringTsumeSearch()
DialogCoordinator::tsumeSearchModeEnded ──► transitionToIdle()  （ダイアログキャンセル時）
MatchCoordinator::tsumeSearchModeEnded ──► transitionToIdle()  （探索完了/中止時）

DialogCoordinator::considerationModeStarted ► transitionToDuringConsideration()
MatchCoordinator::considerationModeEnded ──► transitionToIdle()

CsaGameWiring::disableNavigationRequested ► transitionToDuringCsaGame()
CsaGameWiring::enableNavigationRequested ─► transitionToIdle()

PositionEditCoordinator callback(true) ──► transitionToDuringPositionEdit()
PositionEditCoordinator callback(false) ─► transitionToIdle()
```

## エンジンエラー時の復旧パス

エンジンのクラッシュ（セグフォルト等）やプロセスエラー発生時、各モードは以下の経路で Idle に遷移する:

```
【対局中】
  Usi::errorOccurred → MatchCoordinator::onUsiError()
    → handleBreakOff() → gameEnded()
    → GameStartCoordinator::matchGameEnded → transitionToIdle()

【棋譜解析中】
  Usi::errorOccurred → AnalysisFlowController::onEngineError()
    → stop() → analysisStopped()
    → DialogCoordinator::analysisModeEnded → transitionToIdle()

【詰み探索中】
  Usi::errorOccurred → MatchCoordinator::onUsiError()
    → tsumeSearchModeEnded → transitionToIdle()

【検討中】
  Usi::errorOccurred → MatchCoordinator::onUsiError()
    → considerationModeEnded → transitionToIdle()
```

**注意**: CSA通信対局は `CsaGameCoordinator` / `CsaGameWiring` が独自にエラー管理するため、`MatchCoordinator::onUsiError()` では対象外。

## 設計上の注意点

- **テーブル駆動**: 新しい状態やUI要素の追加は `buildPolicyTable()` にエントリを追加するだけ
- **遅延生成対応**: `updateDeps()` で依存オブジェクトを後から注入可能（`ensure*()` パターンと親和性が高い）
- **表示メニュー**: 全状態で常に有効のため、テーブルには含めていない
- **対局中の盤面クリック**: `setMoveInputEnabled(false)` で無効化するが、対局中は `BoardInteractionController` の `isHumanTurnCallback` による手番チェックが別途機能する
