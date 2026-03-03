# Task 20260303-01: callback 中継メソッド化（生ポインタ bind 撲滅）

## 概要
`std::bind(..., m_queryService.get(), ...)` で生ポインタを callback に固定している箇所を、安全な中継メソッド経由に変更する。shutdown 後の Use-after-free リスクを恒久的に排除する。

## 優先度
High

## 背景
- `runShutdown()` で `m_queryService` の Deps を無効化する暫定対策は実施済み
- しかし将来 `reset()` 方針へ戻る変更や再初期化が入ると再び事故化しやすい
- `std::bind` のターゲットを `this`（Registry 自身）に統一することで安全化する

## 対象ファイル

### 修正対象
1. `src/app/gamewiringsubregistry.h` - 中継メソッド宣言追加
2. `src/app/gamewiringsubregistry.cpp` - 中継メソッド実装追加 + bind 先変更
3. `src/app/mainwindowfoundationregistry.h` - 中継メソッド宣言追加
4. `src/app/mainwindowfoundationregistry.cpp` - 中継メソッド実装追加 + bind 先変更

### 参照（変更不要だが理解が必要）
- `src/app/matchruntimequeryservice.h` - クエリサービスのインターフェース確認用
- `src/app/mainwindowlifecyclepipeline.cpp:106-114` - 暫定対策の現状確認用

## 実装手順

### Step 1: GameWiringSubRegistry に中継メソッドを追加

`src/app/gamewiringsubregistry.h` の private セクションに以下を追加:
```cpp
// MatchRuntimeQueryService 中継メソッド（shutdown 安全性確保）
int queryRemainingMsFor(MatchCoordinator::Player player);
int queryIncrementMsFor(MatchCoordinator::Player player);
int queryByoyomiMs();
bool queryIsHumanSide(ShogiGameController::Player player);
```

`src/app/gamewiringsubregistry.cpp` に実装を追加:
```cpp
int GameWiringSubRegistry::queryRemainingMsFor(MatchCoordinator::Player player)
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->getRemainingMsFor(player);
    return 0;
}

int GameWiringSubRegistry::queryIncrementMsFor(MatchCoordinator::Player player)
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->getIncrementMsFor(player);
    return 0;
}

int GameWiringSubRegistry::queryByoyomiMs()
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->getByoyomiMs();
    return 0;
}

bool GameWiringSubRegistry::queryIsHumanSide(ShogiGameController::Player player)
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->isHumanSide(player);
    return false;
}
```

### Step 2: GameWiringSubRegistry の bind 先を変更

`src/app/gamewiringsubregistry.cpp` の `buildMatchWiringDeps()` 内で以下を置換:

**Before (150-152行目付近):**
```cpp
in.remainingMsFor = std::bind(&MatchRuntimeQueryService::getRemainingMsFor, m_mw.m_queryService.get(), _1);
in.incrementMsFor = std::bind(&MatchRuntimeQueryService::getIncrementMsFor, m_mw.m_queryService.get(), _1);
in.byoyomiMs = std::bind(&MatchRuntimeQueryService::getByoyomiMs, m_mw.m_queryService.get());
```

**After:**
```cpp
in.remainingMsFor = std::bind(&GameWiringSubRegistry::queryRemainingMsFor, this, _1);
in.incrementMsFor = std::bind(&GameWiringSubRegistry::queryIncrementMsFor, this, _1);
in.byoyomiMs = std::bind(&GameWiringSubRegistry::queryByoyomiMs, this);
```

**Before (161行目付近):**
```cpp
in.isHumanSide = std::bind(&MatchRuntimeQueryService::isHumanSide, m_mw.m_queryService.get(), _1);
```

**After:**
```cpp
in.isHumanSide = std::bind(&GameWiringSubRegistry::queryIsHumanSide, this, _1);
```

### Step 3: MainWindowFoundationRegistry に中継メソッドを追加

`src/app/mainwindowfoundationregistry.h` の private セクションに以下を追加:
```cpp
// MatchRuntimeQueryService 中継メソッド（shutdown 安全性確保）
bool queryIsGameActivelyInProgress();
```

`src/app/mainwindowfoundationregistry.cpp` に実装を追加:
```cpp
bool MainWindowFoundationRegistry::queryIsGameActivelyInProgress()
{
    if (m_mw.m_queryService)
        return m_mw.m_queryService->isGameActivelyInProgress();
    return false;
}
```

### Step 4: MainWindowFoundationRegistry の bind 先を変更

`src/app/mainwindowfoundationregistry.cpp` の `refreshKifuNavigationCoordinatorDeps()` 内で以下を置換:

**Before (139行目):**
```cpp
callbacks.isGameActivelyInProgress = std::bind(&MatchRuntimeQueryService::isGameActivelyInProgress, m_mw.m_queryService.get());
```

**After:**
```cpp
callbacks.isGameActivelyInProgress = std::bind(&MainWindowFoundationRegistry::queryIsGameActivelyInProgress, this);
```

### Step 5: ビルド確認
```bash
cmake --build build -j4
```

## 受け入れ基準
- `m_queryService.get()` を直接 `std::bind` のターゲットにしている箇所がゼロ
- ビルド成功（warning なし）
- `ctest --test-dir build --output-on-failure` 全件成功

## 検証コマンド
```bash
# 生ポインタ bind が残っていないことを確認
grep -rn "m_queryService.get()" src/app/gamewiringsubregistry.cpp src/app/mainwindowfoundationregistry.cpp
# 上記コマンドの出力が空であること
```
