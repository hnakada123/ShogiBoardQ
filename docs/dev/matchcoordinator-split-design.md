# MatchCoordinator 分割設計書（ISSUE-010）

## 1. 現状サマリ

| ファイル | 行数 | 備考 |
|---------|------|------|
| `matchcoordinator.cpp` | 797 | 以前 1,444行から削減済み |
| `matchcoordinator.h` | 628 | Hooks/Deps/StartOptions 等の構造体が大部分 |
| 合計 | 1,425 | |

### 既に抽出済みの子ハンドラ（4クラス）

| クラス | 責務 | パターン |
|--------|------|----------|
| `GameEndHandler` | 投了・中断・千日手・持将棋・入玉宣言 | QObject, Refs/Hooks |
| `GameStartOrchestrator` | 対局開始フロー（オプション構築・履歴探索） | 非QObject, Refs/Hooks |
| `MatchTimekeeper` | 時計管理・GoTimes計算・ターンエポック | QObject, Refs/Hooks |
| `EngineLifecycleManager` | エンジン生成・破棄・USI送受・指し手実行 | QObject, Refs/Hooks |
| `AnalysisSessionHandler` | 検討・詰み探索セッション管理 | QObject, Hooks |
| `MatchUndoHandler` | 2手UNDO処理 | 非QObject, Refs/UndoHooks |

---

## 2. 全メソッド責務分類

### 2.1 オーケストレーション（子ハンドラへの委譲のみ）— 39メソッド

MC は既に大部分が「薄いファサード」化している。以下は全て1〜5行の委譲メソッド。

**→ GameEndHandler 委譲（8メソッド）:**
- `handleResign()`, `handleEngineResign()`, `handleEngineWin()`
- `handleNyugyokuDeclaration()`, `handleBreakOff()`
- `appendGameOverLineAndMark()`, `appendBreakOffLineAndMark()`
- `handleMaxMovesJishogi()`, `checkAndHandleSennichite()`
- `handleSennichite()`, `handleOuteSennichite()`

**→ GameStartOrchestrator 委譲（4メソッド）:**
- `configureAndStart()`, `buildStartOptions()` (static)
- `ensureHumanAtBottomIfApplicable()`, `prepareAndStartGame()`

**→ AnalysisSessionHandler 委譲（4メソッド）:**
- `startAnalysis()`, `stopAnalysisEngine()`
- `updateConsiderationMultiPV()`, `updateConsiderationPosition()`

**→ MatchUndoHandler 委譲（2メソッド）:**
- `setUndoBindings()`, `undoTwoPlies()`

**→ EngineLifecycleManager 委譲（12メソッド）:**
- `updateUsiPtrs()`, `initializeAndStartEngineFor()`
- `destroyEngine()`, `destroyEngines()`, `initEnginesForEvE()`
- `engineThinkApplyMove()`, `engineMoveOnce()`
- `primaryEngine()`, `secondaryEngine()`
- `sendGoToEngine()`, `sendStopToEngine()`, `sendRawToEngine()`
- `engineManager()`, `isEngineShutdownInProgress()`

**→ MatchTimekeeper 委譲（12メソッド）:**
- `setTimeControlConfig()`, `timeControl()`
- `markTurnEpochNowFor()`, `turnEpochFor()`
- `computeGoTimes()`, `computeGoTimesForUSI()`, `refreshGoTimes()`
- `setClock()`, `clock()` (2 overloads)
- `pokeTimeUpdateNow()`, `recomputeClockSnapshot()`

**→ Strategy 委譲（5メソッド）:**
- `startInitialEngineMoveIfNeeded()`, `armTurnTimerIfNeeded()`
- `finishTurnTimerAndSetConsiderationFor()`, `disarmHumanTimerIfNeeded()`
- `onHumanMove()`

### 2.2 対局状態管理 — 6メソッド + 5 inline アクセサ

MC が直接管理する状態とそのアクセサ:

| メソッド | 行数 | 操作対象 |
|---------|------|---------|
| `setPlayMode()` | 4 | m_playMode |
| `clearGameOverState()` | 8 | m_gameOver |
| `setGameOver()` | 25 | m_gameOver + emit |
| `markGameOverMoveAppended()` | 8 | m_gameOver.moveAppended + emit |
| `updateTurnDisplay()` | 4 | m_cur + m_hooks |
| `setGameInProgressActions()` | 2 | m_hooks.setGameActions |

**inline アクセサ:** `playMode()`, `maxMoves()`, `gameMoves()`, `sfenRecordPtr()` (2), `gameOverState()`

**関連シグナル:** `gameOverStateChanged`, `requestAppendGameOverMove`, `gameEnded`, `boardFlipped`, `timeUpdated`, `considerationModeEnded`, `tsumeSearchModeEnded`, `considerationWaitingStarted`

### 2.3 Strategy 生成 — 1メソッド

| メソッド | 行数 | 内容 |
|---------|------|------|
| `createAndStartModeStrategy()` | 42 | PlayMode に応じた Strategy 生成・起動 |

### 2.4 UI 通知（Hooks 経由） — 2メソッド

| メソッド | 行数 | 内容 |
|---------|------|------|
| `flipBoard()` | 4 | renderBoardFromGc + emit boardFlipped |
| `forceImmediateMove()` | 15 | 手番エンジンに stop 送信 |

### 2.5 エラー復旧 — 1メソッド

| メソッド | 行数 | 内容 |
|---------|------|------|
| `onUsiError()` | 34 | Analysis エラー復旧 → 対局中なら handleBreakOff |

### 2.6 初期化・内部ヘルパ — 8メソッド

| メソッド | 行数 | 内容 |
|---------|------|------|
| `constructor` | 30 | Deps 読み取り、ELM/Timekeeper 初期化 |
| `ensureEngineManager()` | 24 | ELM 遅延生成 + Hooks 配線 |
| `ensureTimekeeper()` | 25 | Timekeeper 遅延生成 + Hooks 配線 |
| `ensureAnalysisSession()` | 48 | ASH 遅延生成 + Hooks 配線 |
| `ensureGameEndHandler()` | 43 | GEH 遅延生成 + Hooks 配線 |
| `ensureGameStartOrchestrator()` | 43 | GSO 遅延生成 + Hooks 配線 |
| `ensureUndoHandler()` | 15 | UndoHandler 遅延生成 + Refs 設定 |
| `initPositionStringsFromSfen()` | 20 | SFEN → position 文字列変換 |
| `startMatchTimingAndMaybeInitialGo()` | 3 | 時計開始 + 初手 go |
| `handlePlayerTimeOut()` | 4 | GC に時間切れ通知 |

---

## 3. Hooks の現状分析

### 3.1 現在の21コールバック一覧

```
Hooks (matchcoordinator.h:105-195):
  UI/描画系:
    1. updateTurnDisplay(Player)         → HooksFactory lambda
    2. setPlayersNames(p1, p2)           → PlayerInfoWiring
    3. setEngineNames(e1, e2)            → PlayerInfoWiring
    4. setGameActions(bool)              → 未配線（UiStatePolicyManager代替）
    5. renderBoardFromGc()               → MainWindowMatchAdapter
    6. showGameOverDialog(title, msg)    → MainWindowMatchAdapter
    7. showMoveHighlights(from, to)      → MainWindowMatchAdapter
    8. log(msg)                          → 未配線

  時計読み出し:
    9. remainingMsFor(Player)            → TimekeepingService
   10. incrementMsFor(Player)            → TimekeepingService
   11. byoyomiMs()                       → TimekeepingService
   12. humanPlayerSide()                 → 未配線

  USI送受:
   13. sendGoToEngine(Usi*, GoTimes)     → UsiCommandController
   14. sendStopToEngine(Usi*)            → UsiCommandController
   15. sendRawToEngine(Usi*, cmd)        → UsiCommandController

  ゲーム初期化/棋譜:
   16. initializeNewGame(sfenStart)      → MainWindowMatchAdapter
   17. appendKifuLine(text, elapsed)     → GameRecordUpdateService
   18. appendEvalP1()                    → EvaluationGraphController
   19. appendEvalP2()                    → EvaluationGraphController
   20. autoSaveKifu(dir, mode, ...)      → KifuFileController

  UndoHooks (matchundohandler.h:59-71, 別構造体):
   21. updateHighlightsForPly(ply)       → MainWindowMatchAdapter
   22. updateTurnAndTimekeepingDisplay() → MainWindowMatchAdapter
   23. isHumanSide(Player)               → PlayModePolicyService
```

### 3.2 コールバックの消費先マッピング

各子ハンドラが MC::Hooks から受け取るコールバック:

| 子ハンドラ | 消費するコールバック |
|-----------|-------------------|
| EngineLifecycleManager | log, renderBoardFromGc, appendEvalP1, appendEvalP2 |
| MatchTimekeeper | remainingMsFor, incrementMsFor, byoyomiMs |
| GameEndHandler | appendKifuLine, showGameOverDialog, log, autoSaveKifu (間接) |
| GameStartOrchestrator | initializeNewGame, setPlayersNames, setEngineNames, setGameActions, renderBoardFromGc |
| AnalysisSessionHandler | showGameOverDialog, setEngineNames |
| MC 自身 | updateTurnDisplay, setGameActions, renderBoardFromGc, sendStopToEngine, log, showGameOverDialog |

### 3.3 未配線（デッドコールバック）

| コールバック | 状態 | 対策 |
|------------|------|------|
| `setGameActions` | 未配線（UiStatePolicyManager で代替済み） | 削除候補 |
| `log` | 未配線 | qDebug/qCDebug への置換を検討 |
| `humanPlayerSide` | 未配線（null チェック付きで使用） | 要調査 |

---

## 4. UseCase 候補の設計

### 4.1 分析結果: MC の現状

MC は既に6つの子ハンドラへの委譲が完了しており、.cpp の797行の内訳は:

| カテゴリ | 行数（概算） | 割合 |
|---------|------------|------|
| ensure* セットアップ（6メソッド） | ~230 | 29% |
| 薄い委譲メソッド（~50メソッド） | ~200 | 25% |
| Strategy 生成 | ~42 | 5% |
| 状態管理（GameOver/PlayMode） | ~50 | 6% |
| エラー復旧 | ~34 | 4% |
| コンストラクタ | ~30 | 4% |
| その他（flipBoard, forceImmediateMove等） | ~40 | 5% |
| コメント・空行 | ~170 | 21% |

**結論**: MC は既に十分に薄いファサードになっている。これ以上の分割は「薄い委譲をさらに薄い委譲で包む」結果になり、コードの可読性を下げるリスクがある。

### 4.2 推奨: 「分割」ではなく「整理」アプローチ

大規模な UseCase 分割よりも、以下の整理を推奨する:

#### Phase A: デッドコード・未配線コールバックの削除

| 対象 | アクション | 影響行数 |
|------|----------|---------|
| `Hooks::setGameActions` | 削除（UiStatePolicyManager で代替済み） | -10行 |
| `Hooks::log` | 削除（qCDebug で代替） | -15行 |
| `Hooks::humanPlayerSide` | 使用箇所を調査し、不要なら削除 | -3行 |
| `handleTimeUpdated()` | 空メソッド、削除 | -1行 |

#### Phase B: MatchStateHolder の抽出（optional）

**目的**: MC の状態変数とアクセサを分離し、StrategyContext の依存先を明確にする

```cpp
/// @brief 対局状態の保持・遷移を管理するクラス（非QObject）
class MatchStateHolder {
public:
    // --- PlayMode ---
    PlayMode playMode() const;
    void setPlayMode(PlayMode m);

    // --- 手番 ---
    Player currentTurn() const;
    void setCurrentTurn(Player p);

    // --- 終局状態 ---
    const GameOverState& gameOverState() const;
    void clearGameOverState();
    void setGameOver(const GameEndInfo& info, bool loserIsP1);
    void markGameOverMoveAppended();
    bool isGameOver() const;

    // --- 棋譜自動保存設定 ---
    bool autoSaveKifu() const;
    const QString& kifuSaveDir() const;

    // --- 対局者名 ---
    const QString& humanName1() const;
    // ... (他の名前アクセサ)

    // --- 最大手数 ---
    int maxMoves() const;

signals: // QObject にする場合
    void gameOverStateChanged(const GameOverState& st);
    void playModeChanged(PlayMode mode);

private:
    PlayMode m_playMode = PlayMode::NotStarted;
    Player m_cur = P1;
    GameOverState m_gameOver;
    int m_maxMoves = 0;
    bool m_autoSaveKifu = false;
    QString m_kifuSaveDir;
    QString m_humanName1, m_humanName2;
    QString m_engineNameForSave1, m_engineNameForSave2;
};
```

**抽出対象（MC から移動）:**
- メンバ変数: `m_playMode`, `m_cur`, `m_gameOver`, `m_maxMoves`, `m_autoSaveKifu`, `m_kifuSaveDir`, `m_humanName1`, `m_humanName2`, `m_engineNameForSave1`, `m_engineNameForSave2`
- メソッド: `setPlayMode()`, `clearGameOverState()`, `setGameOver()`, `markGameOverMoveAppended()`
- 推定行数: .h ~80行, .cpp ~60行

**メリット:**
- StrategyContext が MatchStateHolder を直接参照でき、MC への循環依存が軽減
- GameStartOrchestrator の Refs が MatchStateHolder のポインタ1本で済む（現在15個のメンバポインタ）
- テスト可能性の向上（状態遷移ロジックの単体テスト）

**リスク:**
- `setGameOver()` 内の `emit gameEnded()` / `emit gameOverStateChanged()` は QObject が必要。非QObject にすると通知パターンが変わる
- GameStartOrchestrator::Refs の15個のポインタが MatchStateHolder に集約されるが、Refs パターンとの整合性
- StrategyContext の書き換えが必要（MC private メンバアクセスの移行）

**判断**: Phase B は**メリットは認めるが、費用対効果は限定的**。MC が既に薄いファサードなので、状態管理の分離で得られる行数削減は ~60行程度。StrategyContext の書き換えコストを考えると、他の改善が優先。

#### Phase C: Hooks 構造体の分割（推奨）

現在の `Hooks` は21個のコールバックを一つの構造体に詰め込んでいる。これは ensure* メソッドの可読性を下げている。

**分割案:**

```cpp
/// UI更新系コールバック
struct UIHooks {
    std::function<void(Player)> updateTurnDisplay;
    std::function<void(const QString&, const QString&)> setPlayersNames;
    std::function<void(const QString&, const QString&)> setEngineNames;
    std::function<void()> renderBoardFromGc;
    std::function<void(const QString&, const QString&)> showGameOverDialog;
    std::function<void(const QPoint&, const QPoint&)> showMoveHighlights;
};

/// 時計読み出し系コールバック
struct TimeHooks {
    std::function<qint64(Player)> remainingMsFor;
    std::function<qint64(Player)> incrementMsFor;
    std::function<qint64()> byoyomiMs;
};

/// USI送受系コールバック
struct EngineHooks {
    std::function<void(Usi*, const GoTimes&)> sendGoToEngine;
    std::function<void(Usi*)> sendStopToEngine;
    std::function<void(Usi*, const QString&)> sendRawToEngine;
};

/// ゲーム初期化・棋譜系コールバック
struct GameHooks {
    std::function<void(const QString&)> initializeNewGame;
    std::function<void(const QString&, const QString&)> appendKifuLine;
    std::function<void()> appendEvalP1;
    std::function<void()> appendEvalP2;
    std::function<void(const QString&, PlayMode,
                       const QString&, const QString&,
                       const QString&, const QString&)> autoSaveKifu;
};

/// 全コールバック集約（後方互換用）
struct Hooks {
    UIHooks ui;
    TimeHooks time;
    EngineHooks engine;
    GameHooks game;
};
```

**メリット:**
- ensure* メソッドでのコールバック転送が「カテゴリ単位」で見やすくなる
- 子ハンドラに渡す際に必要なサブセットが明確になる
- 未配線コールバックの発見が容易になる

**リスク:**
- `MatchCoordinatorHooksFactory::buildHooks()` の書き換え
- StrategyContext の `hooks()` 戻り値型の変更
- 子ハンドラ内の `m_hooks.XXX` アクセスパスが `m_hooks.ui.XXX` に変わる
- 影響ファイル数が多い（後述 §5 参照）

**判断**: **推奨**。コールバックのカテゴリ化は保守性向上に直結する。Phase A（デッドコールバック削除）の後に実施すれば、移行対象が減る。

#### Phase D: ensure* セットアップの外部化（optional）

ensure* メソッド群（~230行）は MC 内部の配線ロジック。これを `MatchCoordinatorWiringHelper` のようなクラスに移動する案。

**メリット:**
- MC の .cpp が ~560行に削減
- 配線ロジックと実行ロジックの分離

**リスク:**
- ensure* は MC の private メンバに直接アクセスしており、外部化すると friend 宣言 or Refs パターンが必要
- 現時点の ensure* は「遅延初期化」パターンで MC のライフサイクルと密結合
- コードの物理的な移動に過ぎず、論理的な改善は限定的

**判断**: **非推奨**。MC の ensure* は MC の責務として自然であり、移動のメリットが薄い。

---

## 5. 影響ファイル一覧

### 5.1 MatchCoordinator::Hooks を直接参照するファイル

| ファイル | 参照方法 |
|---------|---------|
| `src/ui/wiring/matchcoordinatorwiring.h` | Deps に `Hooks hooks` メンバ |
| `src/ui/wiring/matchcoordinatorwiring.cpp` | MC 生成時に Hooks を渡す |
| `src/ui/wiring/matchcoordinatorhooksfactory.h` | `buildHooks()` の戻り値型 |
| `src/ui/wiring/matchcoordinatorhooksfactory.cpp` | Hooks の組み立て |
| `src/game/matchcoordinator.cpp` | m_hooks を子ハンドラに転送 |
| `src/game/strategycontext.h` | `hooks()` で Hooks& を返す |

### 5.2 MatchCoordinator の型を参照するファイル（105ファイル）

主要なカテゴリ別:

**子ハンドラ（型エイリアス・Hooks パススルー）:**
- `src/game/gameendhandler.h/.cpp`
- `src/game/gamestartorchestrator.h/.cpp`
- `src/game/matchtimekeeper.h/.cpp`
- `src/game/enginelifecyclemanager.h/.cpp` (Player/GoTimes 互換型あり)
- `src/game/analysissessionhandler.h/.cpp`
- `src/game/matchundohandler.h/.cpp`

**Strategy クラス:**
- `src/game/humanvshumanstrategy.h/.cpp`
- `src/game/humanvsenginestrategy.h/.cpp`
- `src/game/enginevsenginestrategy.h/.cpp`
- `src/game/gamemodestrategy.h`
- `src/game/strategycontext.h`

**Wiring/配線層:**
- `src/ui/wiring/matchcoordinatorwiring.h/.cpp`
- `src/ui/wiring/matchcoordinatorhooksfactory.h/.cpp`
- `src/ui/wiring/considerationwiring.h/.cpp`
- `src/ui/wiring/dialogcoordinatorwiring.h/.cpp`
- `src/ui/wiring/dialoglaunchwiring.h/.cpp`

**App 層:**
- `src/app/mainwindow.h`
- `src/app/gamesessionorchestrator.h/.cpp`
- `src/app/mainwindowmatchadapter.h/.cpp`
- `src/app/mainwindowmatchwiringdepsservice.h/.cpp`
- `src/app/sessionlifecyclecoordinator.h/.cpp`

**Controller/Service 層:**
- `src/game/gamestartcoordinator.h/.cpp`
- `src/game/gamestatecontroller.h/.cpp`
- `src/game/consecutivegamescontroller.h/.cpp`
- `src/game/gamesessionfacade.h/.cpp`
- `src/app/playmodepolicyservice.h`
- `src/app/matchruntimequeryservice.h/.cpp`

**テスト:**
- `tests/tst_game_start_flow.cpp`
- `tests/test_stubs_game_start_flow.cpp`
- `tests/test_stubs_cleanup.cpp`

### 5.3 `m_hooks.` を使用する .cpp ファイル（game/ 内）

| ファイル | 使用コールバック数 |
|---------|------------------|
| `matchcoordinator.cpp` | 15種 |
| `gameendhandler.cpp` | 10種 |
| `gamestartorchestrator.cpp` | 9種 |
| `enginelifecyclemanager.cpp` | 6種 |
| `analysissessionhandler.cpp` | 8種 |
| `matchtimekeeper.cpp` | 3種 |

---

## 6. 推奨移行手順（段階的）

### Step 1: デッドコールバック削除（Phase A）
**難易度**: 低  **影響**: 小  **推定工数**: 0.5h

1. `Hooks::setGameActions` を削除
   - `matchcoordinator.h` から宣言削除
   - `matchcoordinator.cpp` の `setGameInProgressActions()` を削除
   - `matchcoordinatorhooksfactory.cpp` の該当行削除
   - `gamestartorchestrator.cpp` の `m_hooks.setGameActions` 呼び出し削除
   - `gameendhandler.cpp` の `m_hooks.setGameInProgressActions` 呼び出し削除

2. `Hooks::log` を削除
   - 全 `m_hooks.log` 呼び出しを `qCDebug(lcGame)` / `qCWarning(lcGame)` に置換
   - `enginelifecyclemanager.cpp`, `gameendhandler.cpp`, `matchcoordinator.cpp` が対象

3. `handleTimeUpdated()` の空メソッド削除
   - `matchcoordinator.h` から削除
   - `strategycontext.h` の委譲メソッドも削除

### Step 2: Hooks 構造体の分割（Phase C）
**難易度**: 中  **影響**: 中  **推定工数**: 2h

1. `matchcoordinator.h` に `UIHooks`, `TimeHooks`, `EngineHooks`, `GameHooks` を定義
2. `Hooks` を4つのサブ構造体を含む形に変更
3. `MatchCoordinatorHooksFactory::buildHooks()` を更新
4. `matchcoordinator.cpp` の ensure* メソッドを更新（アクセスパス変更）
5. `strategycontext.h` の `hooks()` 利用箇所を更新
6. 各 Strategy クラスの `m_hooks.XXX` → `m_hooks.ui.XXX` 等に変更

**後方互換対策**: 一括置換で対応可能。`m_hooks.renderBoardFromGc` → `m_hooks.ui.renderBoardFromGc` のようなパターン。

### Step 3: MatchStateHolder 抽出（Phase B, optional）
**難易度**: 高  **影響**: 大  **推定工数**: 4h

1. `MatchStateHolder` クラスを `src/game/matchstateholder.h/.cpp` に作成
2. MC から状態変数を移動
3. MC が `MatchStateHolder` を所有し、アクセサを提供
4. `StrategyContext` を `MatchStateHolder` への参照に変更
5. `GameStartOrchestrator::Refs` を `MatchStateHolder*` に集約
6. `GameEndHandler::Refs` の `playMode`, `gameOver` を `MatchStateHolder*` に集約
7. テスト更新

---

## 7. リスクと対策

| リスク | 影響度 | 対策 |
|-------|-------|------|
| Hooks 分割で Strategy クラスのアクセスパスが壊れる | 中 | grep で全箇所を特定し一括置換 |
| StrategyContext の変更で Strategy クラスのビルドが壊れる | 中 | Phase B は StrategyContext を段階的に移行 |
| 未配線コールバック削除で実は使われている箇所がある | 低 | grep で全参照を確認済み |
| テストコードの更新漏れ | 低 | CI でビルドエラーとして検出 |
| MatchStateHolder 導入時の QObject シグナル要件 | 中 | QObject として定義するか、Observer パターンで代替 |

---

## 8. 結論

MatchCoordinator は既に6つの子ハンドラへの委譲が完了しており、797行の .cpp のうち約半数が委譲コードと ensure* セットアップコードである。**これ以上の大規模な責務分割は費用対効果が低い**。

推奨アクションは:

1. **Step 1（デッドコード削除）**: 即時実行可能。MC の Hooks を 21→18 に削減
2. **Step 2（Hooks 分割）**: 保守性向上に有効。影響は中程度だが一括置換で対応可能
3. **Step 3（MatchStateHolder）**: メリットはあるが優先度は低い。他の改善（MC 以外のクラスの分割）が先

最終的な目標行数: Step 1+2 実施後に .cpp ~750行、.h ~580行（Hooks 再構成分）。
