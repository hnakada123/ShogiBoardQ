# Task 09: DialogCoordinator DI パターン統一（P2 / §3.2）

## 概要

`DialogCoordinator`（387行）が個別 setter メソッド（10個以上）を使用しており、プロジェクト標準の `Deps` 構造体 + `updateDeps()` パターンに準拠していない。

## 現状

`src/ui/coordinators/dialogcoordinator.h` にある個別 setter:
- `setMatchCoordinator()`
- `setGameController()`
- `setUsiEngine()`
- 等10個以上

さらに3つのコンテキスト構造体（`ConsiderationContext`, `TsumeSearchContext`, `KifuAnalysisContext`）と3つのパラメータ構造体がヘッダ内に定義されている。

## 手順

### Step 1: 現状の調査

1. `dialogcoordinator.h` の全 setter メソッドをリストアップする
2. 各 setter の呼び出し元を特定する（主に `MainWindowCompositionRoot` や Wiring クラスから呼ばれているはず）
3. コンテキスト構造体の使用箇所を確認する

### Step 2: Deps 構造体の設計

1. 全 setter のパラメータを `Deps` 構造体にまとめる:
   ```cpp
   struct Deps {
       MatchCoordinator* matchCoordinator = nullptr;
       ShogiGameController* gameController = nullptr;
       Usi* usiEngine = nullptr;
       // ... 他の依存
   };
   ```
2. `updateDeps(const Deps& deps)` メソッドを追加する

### Step 3: setter の置換

1. 全 setter メソッドを削除する
2. 呼び出し元を `Deps` 構造体の構築 + `updateDeps()` 呼び出しに変更する
3. lazy-init パターン（`ensure*()` 内での setter 呼び出し）がある場合は、`updateDeps()` のタイミングを調整する

### Step 4: コンテキスト構造体の整理

1. `ConsiderationContext`, `TsumeSearchContext`, `KifuAnalysisContext` を独立ヘッダに分離するか検討する
2. ヘッダが387行と大きいため、構造体の外部化でヘッダサイズを削減できないか評価する

### Step 5: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認

## 注意事項

- `Deps` パターンでは、依存オブジェクトの生成タイミングが setter 呼び出しと異なる場合がある。lazy-init との整合性に注意
- `DialogCoordinator` は多くの Wiring クラスから参照されているため、影響範囲が広い
- MEMORY に記載のある「Lazy-init dependency gap」パターンに注意
