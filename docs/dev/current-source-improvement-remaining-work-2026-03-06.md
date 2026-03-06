# Current Source Improvement Remaining Work (2026-03-06)

## 目的

`app/` 層の再整理のうち、Phase 3 以降の残作業をそのまま再開できる形で残す。

この文書は 2026-03-06 時点の実装状態を前提にしている。

## 現在の到達点

- Phase 1 完了
  - `GameSessionSubRegistry` と `GameWiringSubRegistry` を `GameSubRegistry` に統合済み
  - その後 `GameSubRegistry` 自体も `MainWindowServiceRegistry` に吸収済み
- Phase 3 変更単位 1 完了
  - `SessionLifecycleCoordinator` に開始系の責務を追加
  - `GameSessionOrchestrator::startNewShogiGame()` と `GameSessionOrchestrator::invokeStartGame()` は削除済み
  - `MainWindowServiceRegistry::startGameSession()` を追加済み
- Phase 3 変更単位 2 完了
  - `MatchCoordinatorWiring` の転送先を責任者へ直結済み
  - `requestAppendGameOverMove` / `gameOverStateChanged` は `GameStateController`
  - `matchGameEnded` / `requestPreStartCleanup` / `requestApplyTimeControl` / `gameStarted` は `SessionLifecycleCoordinator`
  - `consecutiveGamesConfigured` は `ConsecutiveGamesController`
  - `resignationTriggered` だけは `GameSessionOrchestrator` に残している

## 直近の検証結果

以下は通過済み。

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R "tst_app_lifecycle_pipeline|tst_app_game_session|tst_lifecycle_runtime|tst_lifecycle_scenario|tst_wiring_contracts|tst_wiring_slot_coverage|tst_app_error_handling|tst_structural_kpi"
```

## 次に着手する作業

### 1. 変更単位 3: 連続対局の開始経路を短縮する

#### 目的

`ConsecutiveGamesController` から次局開始する経路を `GameSessionOrchestrator` 経由ではなく直接 `GameStartCoordinator` に寄せる。

#### 現在の残留経路

- `ConsecutiveGamesController::requestPreStartCleanup`
  - まだ `GameSessionOrchestrator::onPreStartCleanupRequested()` に接続されている
- `ConsecutiveGamesController::requestStartNextGame`
  - まだ `GameSessionOrchestrator::onConsecutiveStartRequested()` に接続されている

#### 変更対象

- `src/game/consecutivegamescontroller.h`
- `src/game/consecutivegamescontroller.cpp`
- `src/app/gamesubregistry_wiring.cpp`
- `src/app/gamesessionorchestrator.h`
- `src/app/gamesessionorchestrator.cpp`
- `tests/tst_app_game_session.cpp`
- `tests/tst_wiring_contracts.cpp`
- `tests/test_stubs_wiring_contracts.cpp`

#### 実施内容

1. `ConsecutiveGamesController::startNextGame()` の遅延後処理で `m_gameStart->start(params)` を直接呼ぶ
2. `requestStartNextGame` シグナルを削除する
3. `requestPreStartCleanup` も削除し、代わりに `ConsecutiveGamesController` に `std::function<void()> performPreStartCleanup` を持たせるか、`SessionLifecycleCoordinator*` を注入して直接呼ぶ
4. `GameSessionOrchestrator::onConsecutiveStartRequested()` を削除する
5. `gamesubregistry_wiring.cpp` の `ensureConsecutiveGamesController()` から GSO への接続を削除する

#### 完了条件

- `rg -n "requestStartNextGame|onConsecutiveStartRequested" src tests` が 0 件
- `rg -n "requestPreStartCleanup" src/game/consecutivegamescontroller.* src/app/gamesubregistry_wiring.cpp tests` が 0 件
- `ConsecutiveGamesController` が次局開始を単独で完結する

## その次の作業

### 2. 変更単位 4: GameSessionOrchestrator の Deps を縮小する

#### 目的

`GameSessionOrchestrator` を UI 操作専用の薄いオーケストレータに寄せる。

#### 現在まだ残っている余分な依存

- `SessionLifecycleCoordinator** sessionLifecycle`
- `ConsecutiveGamesController** consecutiveGamesController`
- `PreStartCleanupHandler** preStartCleanupHandler`
- `TimeControlController** timeController`
- `LiveGameSession** liveGameSession`
- `GameStartCoordinator::TimeControl* lastTimeControl`
- `ensureSessionLifecycleCoordinator`
- `ensureConsecutiveGamesController`
- `ensurePreStartCleanupHandler`
- `ensureReplayController`
- `clearSessionDependentUi`
- `updateJosekiWindow`
- `sfenRecord`

#### 変更対象

- `src/app/gamesessionorchestrator.h`
- `src/app/gamesessionorchestrator.cpp`
- `src/app/gamesubregistry_session.cpp`
- `tests/tst_app_game_session.cpp`
- `tests/tst_app_error_handling.cpp`
- `tests/tst_lifecycle_scenario.cpp`

#### 実施内容

1. `GameSessionOrchestrator` からライフサイクル系スロットを削除する
   - `onMatchGameEnded`
   - `onGameOverStateChanged`
   - `onRequestAppendGameOverMove`
   - `onPreStartCleanupRequested`
   - `onApplyTimeControlRequested`
   - `onGameStarted`
   - `onConsecutiveGamesConfigured`
2. `Deps` を UI 操作に必要なものだけに縮める
3. コメントを「開始ダイアログ / 投了 / 中断 / 詰み探索停止 / 外部ブラウザ」に責務限定で更新する

#### 完了条件

- `GameSessionOrchestrator` の public slots が UI 操作系のみ
- `rg -n "onMatchGameEnded|onGameOverStateChanged|onRequestAppendGameOverMove|onPreStartCleanupRequested|onApplyTimeControlRequested|onGameStarted|onConsecutiveGamesConfigured" src/app/gamesessionorchestrator.* tests` が 0 件

## Phase 4 以降の候補

### 3. 所有権と再生成のルール統一

優先度は Phase 3 より下。

特に以下を確認する。

- `MainWindowServiceRegistry::ensureGameStateController()` が refresh 可能になったため、他の `ensure*` でも再生成・再配線が必要なオブジェクトがないかを点検する
- `ReplayController` / `GameStateController` / `SessionLifecycleCoordinator` / `ConsecutiveGamesController` の再初期化契約を文書化する

対象候補:

- `src/app/gamesubregistry.cpp`
- `src/app/gamesubregistry_session.cpp`
- `src/app/gamesubregistry_wiring.cpp`
- `src/app/mainwindowcompositionroot.cpp`
- `docs/dev/ownership-guidelines.md`

## 実装時の注意

- `GameStateController` は `MatchCoordinator` 再生成後に依存更新が必要
  - 2026-03-06 時点では `ensureGameStateController()` を refresh 対応済み
- `requestAppendGameOverMove` は接続順が重要
  - 先に `GameStateController::onRequestAppendGameOverMove`
  - 後に `SessionLifecycleCoordinator::commitLiveGameSessionIfActive`
- `ConsecutiveGamesController` は `GameStartCoordinator` が後から生成される可能性がある
  - `ensureConsecutiveGamesController()` 側で既存インスタンスにも `setGameStartCoordinator()` を再適用すること

## 推奨コミット単位

1. `連続対局の開始経路を短縮`
2. `GameSessionOrchestratorをUI操作専用へ縮小`
3. `ライフサイクル依存の再生成規約を整理`

## 推奨検証コマンド

変更単位 3 の途中:

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R "tst_game_start_flow|tst_game_start_orchestrator|tst_app_game_session|tst_lifecycle_scenario|tst_wiring_contracts|tst_wiring_slot_coverage|tst_structural_kpi"
```

変更単位 4 の途中:

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R "tst_app_lifecycle_pipeline|tst_app_game_session|tst_lifecycle_runtime|tst_lifecycle_scenario|tst_wiring_contracts|tst_wiring_slot_coverage|tst_app_error_handling|tst_structural_kpi"
```

最終確認:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## 補足

現在のワークツリーには Phase 1 / Phase 2 / Phase 3 の途中変更が未コミットで残っている。
新しい作業に入る前に、不要な差分を戻すのではなく、現在の差分を前提に積み上げること。
