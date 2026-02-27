# MatchCoordinator さらなる削減の分析

## 現状サマリ

| ファイル | 行数 |
|---|---|
| matchcoordinator.cpp | 1,442 |
| matchcoordinator.h | 605 |
| **合計** | **2,047** |

既に抽出済み:
- `GameEndHandler` (489行) - 終局処理
- `GameStartOrchestrator` (537行) - 対局開始フロー
- `AnalysisSessionHandler` (438行) - 検討/詰み探索セッション管理
- `StrategyContext` (93行) - Strategy→MC内部アクセス

## 全メソッド責務分類

### A: エンジンライフサイクル管理（~169行）

エンジンの生成・初期化・シグナル配線・破棄を担当。

| メソッド | 行数 | 概要 |
|---|---|---|
| `initializeAndStartEngineFor` | 28 | エンジン初期化・起動 |
| `wireResignToArbiter` | 17 | 投了シグナル配線 |
| `wireWinToArbiter` | 17 | 入玉宣言勝ちシグナル配線 |
| `onEngine1Resign` | 5 | エンジン1投了スロット |
| `onEngine2Resign` | 5 | エンジン2投了スロット |
| `onEngine1Win` | 5 | エンジン1入玉宣言勝ちスロット |
| `onEngine2Win` | 4 | エンジン2入玉宣言勝ちスロット |
| `destroyEngine` | 13 | エンジン破棄（単体） |
| `destroyEngines` | 19 | エンジン破棄（全） |
| `ensureEngineModels` | 16 | モデルフォールバック生成 |
| `initEnginesForEvE` | 40 | EvE用エンジン生成・配線 |

### B: エンジン指し手実行（~99行）

エンジンの思考結果を盤面に適用するロジック。

| メソッド | 行数 | 概要 |
|---|---|---|
| `engineThinkApplyMove` | 56 | エンジン思考結果適用 |
| `engineMoveOnce` | 43 | 1手進行 |

### C: USI送信（~39行）

USIプロトコルのコマンド送信。

| メソッド | 行数 | 概要 |
|---|---|---|
| `sendGoToEngine` | 17 | goコマンド送信 |
| `sendStopToEngine` | 5 | stopコマンド送信 |
| `sendRawToEngine` | 5 | 任意コマンド送信 |
| `sendRawTo` | 5 | 内部ヘルパ |
| `clampMsToInt` (anonymous) | 7 | 安全な型変換 |

### D: 検討・詰み探索オーケストレーション（~150行）

検討/詰み探索モードの開始・停止・エラー処理。

| メソッド | 行数 | 概要 |
|---|---|---|
| `startAnalysis` | 68 | 検討開始（エンジン生成含む） |
| `stopAnalysisEngine` | 16 | 検討エンジン停止 |
| `updateConsiderationMultiPV` | 5 | MultiPV変更 |
| `updateConsiderationPosition` | 8 | ポジション変更 |
| `onUsiError` | 34 | USIエラー処理 |
| `ensureAnalysisSession` | 19 | セッションハンドラ遅延生成 |

### E: 時計・時間管理（~169行）

時計制御、USI用残時間計算、ターンエポック管理。

| メソッド | 行数 | 概要 |
|---|---|---|
| `setTimeControlConfig` | 12 | 時間制御設定 |
| `timeControl` | 3 | 設定取得 |
| `computeGoTimes` | 54 | USI用残時間算出 |
| `computeGoTimesForUSI` | 5 | 外部API |
| `refreshGoTimes` | 5 | 再計算 |
| `setClock` | 7 | 時計差し替え |
| `onClockTick` | 6 | 時計tickスロット |
| `pokeTimeUpdateNow` | 4 | 即時timeUpdated発火 |
| `emitTimeUpdateFromClock` | 24 | timeUpdatedシグナル発行 |
| `wireClock` | 11 | 時計接続 |
| `unwireClock` | 4 | 時計切断 |
| `markTurnEpochNowFor` | 4 | ターンエポック記録 |
| `turnEpochFor` | 3 | ターンエポック取得 |
| `recomputeClockSnapshot` | 27 | 時計スナップショット |

### F: UNDO（~173行）

2手戻し処理。既にUndoRefs/UndoHooksパターンで独立性が高い。

| メソッド | 行数 | 概要 |
|---|---|---|
| `setUndoBindings` | 4 | Refs/Hooks設定 |
| `undoTwoPlies` | 101 | 2手UNDO実行 |
| `tryRemoveLastItems` | 16 | モデル末尾削除ヘルパ |
| `buildBasePositionUpToHands` (static) | 52 | position文字列構築 |

### G: 終局状態管理（~45行）

GameOverStateの保持・更新・シグナル発行。

| メソッド | 行数 | 概要 |
|---|---|---|
| `clearGameOverState` | 9 | 状態クリア |
| `setGameOver` | 27 | 終局確定 |
| `markGameOverMoveAppended` | 9 | 追記済みマーク |

### H: 対局開始（Strategy生成 + GSO委譲）（~136行）

Strategy生成はMC密結合のため残留。

| メソッド | 行数 | 概要 |
|---|---|---|
| `configureAndStart` | 5 | GSO委譲 |
| `createAndStartModeStrategy` | 59 | Strategy生成（MC密結合） |
| `buildStartOptions` | 8 | GSO委譲 |
| `ensureHumanAtBottomIfApplicable` | 5 | GSO委譲 |
| `prepareAndStartGame` | 8 | GSO委譲 |
| `startMatchTimingAndMaybeInitialGo` | 8 | タイマー起動+初手go |
| `ensureGameStartOrchestrator` | 43 | オーケストレータ遅延生成 |

### I: 終局処理委譲（~93行）

GameEndHandlerへの転送メソッド群。

| メソッド | 行数 | 概要 |
|---|---|---|
| `handleResign` | 4 | 投了処理委譲 |
| `handleEngineResign` | 4 | エンジン投了委譲 |
| `handleEngineWin` | 4 | エンジン入玉宣言勝ち委譲 |
| `handleNyugyokuDeclaration` | 4 | 入玉宣言処理委譲 |
| `handleBreakOff` | 9 | 中断処理 |
| `appendGameOverLineAndMark` | 4 | 棋譜追記委譲 |
| `appendBreakOffLineAndMark` | 4 | 中断追記委譲 |
| `handleMaxMovesJishogi` | 4 | 持将棋委譲 |
| `checkAndHandleSennichite` | 4 | 千日手チェック委譲 |
| `handleSennichite` | 4 | 千日手処理委譲 |
| `handleOuteSennichite` | 4 | 連続王手千日手委譲 |
| `ensureGameEndHandler` | 44 | ハンドラ遅延生成 |

### J: その他（小メソッド・アクセサ・転送）（~183行）

| メソッド | 行数 | 概要 |
|---|---|---|
| Constructor | 33 | 初期化 |
| Destructor | 1 | デフォルト |
| `strategyCtx` | 4 | StrategyContextアクセサ |
| `updateUsiPtrs` | 4 | エンジンポインタ差替 |
| `flipBoard` | 6 | 盤面反転 |
| `setGameInProgressActions` | 3 | UI状態更新 |
| `updateTurnDisplay` | 4 | 手番表示更新 |
| `setPlayMode` | 4 | モード設定 |
| `primaryEngine` / `secondaryEngine` | 8 | エンジンアクセサ |
| `armTurnTimerIfNeeded` | 3 | Strategy委譲 |
| `finishTurnTimerAndSetConsiderationFor` | 3 | Strategy委譲 |
| `disarmHumanTimerIfNeeded` | 3 | Strategy委譲 |
| `initializePositionStringsForStart` | 4 | 転送 |
| `initPositionStringsFromSfen` | 33 | position文字列初期化 |
| `startInitialEngineMoveIfNeeded` | 4 | Strategy委譲 |
| `handleTimeUpdated` | 3 | **空メソッド**（削除可能） |
| `handlePlayerTimeOut` | 7 | タイムアウト処理 |
| `handleGameEnded` | 5 | 対局終了処理 |
| `playMode` | 4 | モード取得 |
| `onHumanMove` | 4 | Strategy委譲 |
| `forceImmediateMove` | 25 | 即座手強制 |
| `clock` / `clock const` | 8 | 時計アクセサ |
| `isStandardStartposSfen` (static) | 5 | SFEN判定ユーティリティ |

---

## 抽出候補の分析

### 候補1: MatchUndoHandler（カテゴリF、~173行）

**抽出の容易さ: ★★★★★（最も容易）**

既に `UndoRefs` / `UndoHooks` パターンが導入されており、独立性が極めて高い。

**抽出対象:**
- `setUndoBindings()`, `undoTwoPlies()`, `tryRemoveLastItems()`
- `buildBasePositionUpToHands()` (static helper)
- `isStandardStartposSfen()` (static helper、共用のためヘッダへ移動)
- メンバ: `m_isUndoInProgress`, `u_`, `h_`

**依存関係:**
- `m_gc` → Refs経由で注入
- `m_sfenHistory` → Refs経由で注入
- `m_positionStr1`, `m_positionPonder1` → Refs経由（ポインタ）
- `m_positionStrHistory` → Refs経由（ポインタ）
- `gameMovesRef()` → Refs経由（ポインタ）
- `m_view` → Refs経由で注入

**MC側に残る転送メソッド:** ~10行（setUndoBindings + undoTwoPlies の1行転送）

**見積もりネット削減: ~160行**

---

### 候補2: EngineLifecycleManager（カテゴリA+B+C、~307行）

**抽出の容易さ: ★★★☆☆（中程度）**

エンジンの生成・破棄・シグナル配線・USI送信を一括管理するクラス。

**抽出対象:**
- `initializeAndStartEngineFor()`, `wireResignToArbiter()`, `wireWinToArbiter()`
- `onEngine1Resign()`, `onEngine2Resign()`, `onEngine1Win()`, `onEngine2Win()`
- `destroyEngine()`, `destroyEngines()`, `ensureEngineModels()`
- `initEnginesForEvE()`
- `engineThinkApplyMove()`, `engineMoveOnce()`
- `sendGoToEngine()`, `sendStopToEngine()`, `sendRawToEngine()`, `sendRawTo()`
- メンバ: `m_usi1`, `m_usi2`, `m_comm1`, `m_think1`, `m_comm2`, `m_think2`, `m_engineShutdownInProgress`

**依存関係:**
- `m_gc` → Refs経由
- `m_hooks` (renderBoardFromGc, appendEvalP1/P2, log, sendGoToEngine, sendStopToEngine, sendRawToEngine) → Hooks経由
- `m_playMode` → Refs（ポインタ）
- `m_clock` → byoyomi判定で使用（computeGoTimesから間接参照）
- `handleEngineResign()`, `handleEngineWin()` → Hooks（MCへのコールバック）

**課題:**
- `StrategyContext` がエンジン関連メソッド（`engineThinkApplyMove`, `wireResignToArbiter`, `wireWinToArbiter`, `destroyEngines`, `initializeAndStartEngineFor`, `primaryEngine`, `computeGoTimesForUSI`）を公開している
- Strategy → StrategyContext → EngineLifecycleManager のチェーンが必要
- `startAnalysis()` がエンジン生成を直接行うため、EngineLifecycleManager のAPIを使う形に変更が必要

**MC側に残る転送メソッド:** ~40行（アクセサ + StrategyContext用の委譲）

**見積もりネット削減: ~260行**

---

### 候補3: MatchTimekeeper（カテゴリE、~169行）

**抽出の容易さ: ★★★☆☆（中程度）**

時計管理、USI用残時間計算、ターンエポック管理を一括。

**抽出対象:**
- `setTimeControlConfig()`, `timeControl()`
- `computeGoTimes()`, `computeGoTimesForUSI()`, `refreshGoTimes()`
- `setClock()`, `onClockTick()`, `pokeTimeUpdateNow()`, `emitTimeUpdateFromClock()`
- `wireClock()`, `unwireClock()`
- `markTurnEpochNowFor()`, `turnEpochFor()`
- `recomputeClockSnapshot()`
- メンバ: `m_tc`, `m_clock`, `m_clockConn`, `m_turnEpochP1Ms`, `m_turnEpochP2Ms`

**依存関係:**
- `m_gc` → Refs経由（currentPlayer判定）
- `m_hooks` (remainingMsFor, incrementMsFor, byoyomiMs) → Hooks経由
- `timeUpdated` シグナル → QObject化が必要
- `m_clock` の byoyomi1Applied/byoyomi2Applied アクセス

**課題:**
- `timeUpdated` シグナルは MC の public シグナルとして外部で connect されている。MatchTimekeeper を QObject にして直接シグナルを発行するか、MC がシグナルを転送する形にするか
- `StrategyContext` が `computeGoTimes()`, `computeGoTimesForUSI()`, `pokeTimeUpdateNow()` を公開している
- `computeGoTimes()` は `engineThinkApplyMove()` からも呼ばれるため、EngineLifecycleManager との連携が必要

**MC側に残る転送メソッド:** ~30行

**見積もりネット削減: ~135行**

---

### 候補4: 検討オーケストレーション拡張（カテゴリD、~150行）

**抽出の容易さ: ★★☆☆☆（やや困難）**

`startAnalysis()` の68行がエンジン生成・配線・PlayMode設定など MC 内部に深く結合。

**現状の構造:**
- `AnalysisSessionHandler` は既に検討セッションの状態管理を担当
- しかし `startAnalysis()` 本体は MC に残っており、エンジン生成（`destroyEngines`, `new Usi`, `wireResignToArbiter` 等）を直接行う

**抽出方針:**
- EngineLifecycleManager を先に抽出すれば、`startAnalysis()` はエンジン関連の呼び出しを EngineLifecycleManager 経由にでき、 `AnalysisSessionHandler` への完全委譲が現実的になる
- `onUsiError()` も対局中のエラー処理（`handleBreakOff` 呼び出し）を含むため、分離には条件分岐のリファクタリングが必要

**前提: EngineLifecycleManager の抽出が完了していること**

**MC側に残る転送メソッド:** ~25行

**見積もりネット削減: ~120行**

---

## 分割計画

### 目標

| 指標 | 現状 | 目標 |
|---|---|---|
| matchcoordinator.cpp | 1,442行 | ≤ 800行 |
| matchcoordinator.h | 605行 | ≤ 450行 |

### Phase 1: MatchUndoHandler 抽出（難易度: 低）

**新規クラス:** `src/game/matchundohandler.h/.cpp`

**設計:**
- 非QObject（`std::unique_ptr` 所有）
- Refs/Hooks パターン（既存の UndoRefs/UndoHooks をそのまま移行）

**Refs（外部参照）:**
```cpp
struct Refs {
    ShogiGameController* gc = nullptr;
    ShogiView* view = nullptr;
    QStringList* sfenHistory = nullptr;
    QString* positionStr1 = nullptr;
    QString* positionPonder1 = nullptr;
    QStringList* positionStrHistory = nullptr;
    QVector<ShogiMove>* gameMoves = nullptr;
};
```

**移行メンバ:** `m_isUndoInProgress`, `u_` (UndoRefs), `h_` (UndoHooks)

**MC 側の変更:**
- `undoTwoPlies()` → 1行転送
- `setUndoBindings()` → 1行転送
- ヘッダから UndoRefs/UndoHooks/UNDO関連メンバを削除

**見積もり:**
- MC.cpp: 1,442 → ~1,282行（-160行）
- MC.h: 605 → ~565行（-40行）

---

### Phase 2: EngineLifecycleManager 抽出（難易度: 中）

**新規クラス:** `src/game/enginelifecyclemanager.h/.cpp`

**設計:**
- QObject（シグナル/スロット: onEngine1Resign 等）
- Refs/Hooks パターン

**Refs:**
```cpp
struct Refs {
    ShogiGameController* gc = nullptr;
    PlayMode* playMode = nullptr;
    // エンジンポインタはManager内部で所有管理
};
```

**Hooks:**
```cpp
struct Hooks {
    std::function<void(const QString&)> log;
    std::function<void()> renderBoardFromGc;
    std::function<void()> appendEvalP1;
    std::function<void()> appendEvalP2;
    std::function<void(int)> onEngineResign;   // → MC.handleEngineResign
    std::function<void(int)> onEngineWin;      // → MC.handleEngineWin
};
```

**公開API:**
```cpp
// ライフサイクル
void initializeAndStartEngineFor(Player side, const QString& path, const QString& name);
void destroyEngine(int idx, bool clearThinking = true);
void destroyEngines(bool clearModels = true);
void initEnginesForEvE(const QString& name1, const QString& name2);

// エンジンポインタ
Usi* usi1() const;
Usi* usi2() const;
void setUsi1(Usi* u);
void setUsi2(Usi* u);
void updateUsiPtrs(Usi* e1, Usi* e2);
bool isShutdownInProgress() const;

// モデル
EngineModelPair ensureEngineModels(int engineIndex);

// 指し手実行
bool engineThinkApplyMove(Usi* engine, QString& positionStr, QString& ponderStr,
                          QPoint* outFrom, QPoint* outTo);
bool engineMoveOnce(Usi* eng, QString& positionStr, QString& ponderStr,
                    bool useSelectedField2, int engineIndex, QPoint* outTo);

// USI送信
void sendGoToEngine(Usi* which, const GoTimes& t);
void sendStopToEngine(Usi* which);
void sendRawToEngine(Usi* which, const QString& cmd);
```

**StrategyContext への影響:**
- StrategyContext のエンジン関連メソッドを EngineLifecycleManager への委譲に変更
- MC が `m_engineManager` メンバを持ち、StrategyContext から参照

**MC 側の変更:**
- エンジン関連のprivateメンバ（`m_usi1/m_usi2`, `m_comm1/m_think1`, `m_comm2/m_think2`, `m_engineShutdownInProgress`）を削除
- エンジン関連メソッドを1行転送に変更
- `startAnalysis()` のエンジン生成部分を EngineLifecycleManager API呼び出しに変更

**見積もり:**
- MC.cpp: ~1,282 → ~1,022行（-260行）
- MC.h: ~565 → ~510行（-55行）

---

### Phase 3: MatchTimekeeper 抽出（難易度: 中）

**新規クラス:** `src/game/matchtimekeeper.h/.cpp`

**設計:**
- QObject（`timeUpdated` シグナル発行のため）
- Refs/Hooks パターン

**Refs:**
```cpp
struct Refs {
    ShogiGameController* gc = nullptr;
    ShogiClock* clock = nullptr;      // ポインタのポインタ（setClock対応）
};
```

**Hooks:**
```cpp
struct Hooks {
    std::function<qint64(Player)> remainingMsFor;
    std::function<qint64(Player)> incrementMsFor;
    std::function<qint64()> byoyomiMs;
};
```

**MC 側の変更:**
- 時計関連メンバ（`m_clock`, `m_clockConn`, `m_tc`, `m_turnEpochP1Ms/P2Ms`）を削除
- `timeUpdated` シグナルは MC → MatchTimekeeper のシグナル-シグナル転送で互換性維持
- `pokeTimeUpdateNow` は MC のpublic slotとして残す（1行転送）

**見積もり:**
- MC.cpp: ~1,022 → ~887行（-135行）
- MC.h: ~510 → ~465行（-45行）

---

### Phase 4: 検討オーケストレーション統合（難易度: 中〜高）

Phase 2（EngineLifecycleManager）完了後に実施。

**変更対象:** `AnalysisSessionHandler` を拡張

`startAnalysis()` のエンジン生成部分が EngineLifecycleManager へ移動済みのため、残りの
オーケストレーションロジックを `AnalysisSessionHandler` に吸収する。

**移行メソッド:**
- `startAnalysis()` の本体（AnalysisSessionHandler に `startFullAnalysis()` として移行）
- `stopAnalysisEngine()` のエンジン停止指示部分
- `onUsiError()` の検討モード関連部分

**MC側に残る転送メソッド:**
- `startAnalysis()` → 1行転送
- `stopAnalysisEngine()` → 2〜3行（engineManager.destroyEngines + フラグ管理）
- `onUsiError()` → 検討部分はASH委譲、対局部分はMC残留

**見積もり:**
- MC.cpp: ~887 → ~787行（-100行）
- MC.h: ~465 → ~445行（-20行）

---

## 最終見積もり

| Phase | MC.cpp | MC.h | 抽出クラス |
|---|---|---|---|
| 現状 | 1,442 | 605 | - |
| Phase 1 | ~1,282 | ~565 | MatchUndoHandler (~190行) |
| Phase 2 | ~1,022 | ~510 | EngineLifecycleManager (~340行) |
| Phase 3 | ~887 | ~465 | MatchTimekeeper (~200行) |
| Phase 4 | ~787 | ~445 | ASH拡張 (~100行追加) |

**最終目標: MC.cpp ≤ 800行、MC.h ≤ 450行**

---

## ヘッダ整理計画

Phase 1〜4 に伴い、以下のヘッダ整理を実施:

### 削除可能な型定義（MC.h → 各クラスのヘッダへ）
- `UndoRefs`, `UndoHooks` → MatchUndoHandler.h
- `EngineModelPair` → EngineLifecycleManager.h
- `TimeControl` → MatchTimekeeper.h

### 削除可能な前方宣言
- `UsiCommLogModel`, `ShogiEngineThinkingModel` → EngineLifecycleManager.h
- `KifuRecordListModel`, `BoardInteractionController` → MatchUndoHandler.h
- `ShogiClock` → MatchTimekeeper.h（MC.h にも残る場合あり）

### シグナル互換性
- `timeUpdated(qint64, qint64, bool, qint64)` → MC がシグナル-シグナル転送で維持
- `gameEnded`, `boardFlipped`, `gameOverStateChanged`, `requestAppendGameOverMove` → MC に残留
- `considerationModeEnded`, `tsumeSearchModeEnded`, `considerationWaitingStarted` → MC に残留（ASHから転送）

---

## 注意事項・リスク

1. **StrategyContext の更新**: Phase 2 で EngineLifecycleManager を抽出する際、StrategyContext のエンジン関連メソッドすべてを新クラスへの委譲に変更する必要がある。Strategy クラス（HumanVsEngineStrategy, EngineVsEngineStrategy 等）は StrategyContext 経由でアクセスするため、API互換性を保つ必要がある。

2. **循環依存の回避**: EngineLifecycleManager は MC 内部の状態（playMode, gameOverState 等）に依存するが、MC は EngineLifecycleManager のエンジンポインタに依存する。Refs パターンでポインタ経由のアクセスにすることで循環を回避する。

3. **スレッド安全性**: エンジン破棄中フラグ（`m_engineShutdownInProgress`）の管理が EngineLifecycleManager に移動するため、`startAnalysis()` からの参照方法を確認する。

4. **既存テストへの影響**: MatchCoordinator の public API は転送メソッドで維持するため、外部からの呼び出しパターンは変わらない。ただし StrategyContext 経由のアクセスパスは変更される。

5. **Phase の独立性**: 各 Phase は独立して実施可能（Phase 4 のみ Phase 2 に依存）。ビルド・テスト可能な状態を各Phase完了時に保証する。
