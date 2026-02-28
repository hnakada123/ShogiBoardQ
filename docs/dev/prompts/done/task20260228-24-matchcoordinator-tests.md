# Task 24: MatchCoordinator テスト追加（Phase 2: テスト基盤拡充）

## 目的

最重要オーケストレータである `MatchCoordinator`（789行）のユニットテストを新規作成し、主要フローの回帰を検知する。

## 背景

- `MatchCoordinator` はゲーム進行の中核であり、テストなし（カバレッジ 0%）
- 責務移譲で `GameEndHandler`, `GameStartOrchestrator`, `EngineLifecycleManager`, `MatchTimekeeper` に分割済みだが、統合的なテストがない
- Hooks/Refs/Deps パターンにより、スタブ注入によるテストが構造的に可能
- 包括的改善分析 §3.4 P1

## 対象テスト

新規作成:
- `tests/tst_matchcoordinator.cpp`

## 事前調査

### Step 1: MatchCoordinator の公開インターフェース確認

```bash
cat src/game/matchcoordinator.h
```

### Step 2: Hooks/Refs/Deps 構造の確認

```bash
# Hooks のサブ構造体
rg "struct.*Hooks" src/game/matchcoordinator.h -A 20

# Deps
rg "struct Deps" src/game/matchcoordinator.h -A 20
```

### Step 3: 依存クラスの確認

```bash
# MatchCoordinator が使用するクラス
rg "#include" src/game/matchcoordinator.cpp | head -20
```

### Step 4: 既存のスタブファイル確認

```bash
ls tests/stubs/
```

## 実装手順

### Step 5: テスト対象フローの選定

以下のフローを優先的にテスト:

1. **対局開始フロー**: `startGame()` → 初期状態設定 → シグナル発火
2. **対局終了フロー**: 投了/詰み検知 → `gameEnded` シグナル
3. **手番管理**: `doMove()` → 手番切替 → 時計更新
4. **中断/再開**: `breakOff()` → 状態リセット

### Step 6: 必要なスタブの作成

MatchCoordinator は多くの依存を持つため、最小限のスタブを作成:

```cpp
// tests/stubs/stub_shogigamecontroller.h
class StubShogiGameController : public ShogiGameController {
    Q_OBJECT
public:
    using ShogiGameController::ShogiGameController;
    // 必要に応じてメソッドをオーバーライド
};
```

Hooks にはテスト用のラムダ（Deps struct 内の callback パターン）を注入:

```cpp
MatchCoordinator::Hooks hooks;
hooks.ui.showGameOverMessage = [&](/* params */) { /* 記録 */ };
hooks.time.handleTimeUpdated = [&](/* params */) { /* 記録 */ };
// ...
```

### Step 7: テストケースの実装

```cpp
class TestMatchCoordinator : public QObject
{
    Q_OBJECT

private slots:
    void init();    // 各テストの前処理
    void cleanup(); // 各テストの後処理

    // 対局開始
    void startGame_emitsGameStarted();
    void startGame_initializesState();

    // 手の実行
    void doMove_switchesTurn();
    void doMove_updatesRecord();

    // 対局終了
    void gameEnd_emitsGameEnded();
    void breakOff_stopsGame();

    // エンジン関連
    void engineMove_isApplied();
};
```

### Step 8: CMakeLists.txt に追加

```bash
scripts/update-sources.sh
```

テストファイルを `tests/CMakeLists.txt` に追加。MatchCoordinator 関連のソースファイルをリンク対象に含める。

### Step 9: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R matchcoordinator --output-on-failure -V
```

## 完了条件

- [ ] `tests/tst_matchcoordinator.cpp` が作成されている
- [ ] 主要フロー（開始/終了/手番/中断）のテストケースが実装されている
- [ ] Hooks/Deps を使ったスタブ注入でテストが独立動作する
- [ ] 全テスト通過
- [ ] テストケース数: 6件以上

## KPI変化目標

- テスト数: +1（新規テストファイル）
- MatchCoordinator カバレッジ: 0% → 部分的

## 注意事項

- MatchCoordinator は多くの依存を持つため、全メソッドのテストは本タスクの範囲外
- 最小限のスタブで主要フローの動作確認を目指す
- `connect()` でラムダを使用しない（テストコードでも CLAUDE.md 準拠。ただし Hooks の callback は例外）
- 既存のテストスタブ（`tests/stubs/`）を可能な限り再利用する
- `QSignalSpy` を活用してシグナル発火を検証する
