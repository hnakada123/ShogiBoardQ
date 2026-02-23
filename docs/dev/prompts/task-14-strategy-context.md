# Task 14: Strategy の friend 依存解消

`MatchCoordinator` の `friend` 宣言を `StrategyContext` インターフェース経由に置き換えてください。

## 背景

現在、`MatchCoordinator` は `HumanVsHumanStrategy`, `HumanVsEngineStrategy`, `EngineVsEngineStrategy` に `friend` を付与しており、Strategy クラスが MatchCoordinator の全 private メンバにアクセスできる。これは内部状態への強結合を生む。

## 作業内容

### Step 1: 現状の依存分析

各 Strategy クラスが `MatchCoordinator` のどの private メンバ/メソッドにアクセスしているか一覧化する:
- `src/game/humanvshumanstrategy.cpp`
- `src/game/humanvsenginestrategy.cpp`
- `src/game/enginevsenginestrategy.cpp`

### Step 2: StrategyContext の定義

`src/game/strategycontext.h`（新規）に、Strategy が必要とする最小限の API を定義:

```cpp
class StrategyContext {
public:
    virtual ~StrategyContext() = default;

    // 手番管理
    virtual Turn currentTurn() const = 0;

    // エンジン操作
    virtual void requestEngineGo(Turn side) = 0;
    virtual void stopEngine(Turn side) = 0;

    // 対局状態
    virtual void applyMove(const ShogiMove& move) = 0;
    virtual void setGameOver(GameResult result) = 0;

    // 時計
    virtual int remainingTimeMs(Turn side) const = 0;

    // ... Strategy が実際に使っているメソッドのみ追加
};
```

**重要**: Step 1 の分析結果に基づき、実際に使われている操作のみ定義すること。

### Step 3: MatchCoordinator を StrategyContext に適合

- `MatchCoordinator` が `StrategyContext` を継承（または内部クラスとして実装）
- 各 virtual メソッドを既存の private メソッドへ委譲

### Step 4: Strategy クラスの修正

- コンストラクタ引数を `MatchCoordinator&` → `StrategyContext&` に変更
- MatchCoordinator の private メンバへの直接アクセスを StrategyContext のメソッド呼び出しに置換

### Step 5: friend 宣言の削除

- `matchcoordinator.h` から3つの `friend class` 宣言を削除

## 制約

- CLAUDE.md のコードスタイルに準拠
- `connect()` にラムダを使わない
- 全既存テストが通ること
- Strategy の動作は一切変更しない（インターフェースの差し替えのみ）
