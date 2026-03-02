# Task 17: matchcoordinator.h ヘッダ分割（P3 / §3.5）

## 概要

`matchcoordinator.h`（546行）に6つのネスト構造体と57以上の public API が含まれており、インクルードコストが高い。独立可能な構造体を別ヘッダに分離する。

## 現状

ネスト構造体:
- `Hooks`（4つのサブ構造体: `UI`, `Time`, `Engine`, `Game`）
- `Deps`
- `StartOptions`
- `TimeControl`
- `AnalysisOptions`
- `GameOverState`

## 手順

### Step 1: 依存関係の調査

1. 各ネスト構造体の使用箇所を洗い出す
2. `Hooks` のサブ構造体がどこからアクセスされるか確認する
3. `StartOptions`, `TimeControl`, `AnalysisOptions` が外部からどう参照されるか確認する

### Step 2: 分離対象の選定

分離候補（独立性が高い順）:
1. `TimeControl` → `src/game/timecontrol.h`（データ構造、他クラスでも使用）
2. `StartOptions` → `src/game/startoptions.h`（データ構造、GameStartOrchestrator でも使用）
3. `AnalysisOptions` → `src/game/analysisoptions.h`（データ構造）
4. `GameOverState` → `src/game/gameoverstate.h`（データ構造）
5. `Hooks` → `src/game/matchcoordinatorhooks.h`（コールバック集約）

`Deps` は `MatchCoordinator` に密結合のため分離の優先度が低い。

### Step 3: ヘッダ分離の実施

1. 各構造体を独立ヘッダに移動する
2. `matchcoordinator.h` から分離した構造体の `#include` を追加する
3. 型エイリアスまたは forward declaration で既存コードの互換性を維持する:
   ```cpp
   // matchcoordinator.h に残す（互換性のため）
   // using StartOptions = MatchStartOptions; // 必要に応じて
   ```
4. `CMakeLists.txt` に新しいヘッダファイルを追加する

### Step 4: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認
3. `matchcoordinator.h` の行数が大幅に減少していることを確認する

## 注意事項

- MEMORY に「MC Hooks split (Task 05)」の記載あり。既に4つのサブ構造体に分割済み
- ネスト構造体を外部に出す場合、`MatchCoordinator::StartOptions` のような既存の型参照を全て更新する必要がある
- 影響範囲が広いため、段階的に実施することを推奨
