# MainWindow friend class 削減計画

## 現状

`mainwindow.h` に **18件** の `friend class` 宣言がある（2026-02-26時点）。

## 全 friend class 一覧と分析結果

### カテゴリ A: 即時削除可能（アクセスなし）

| # | クラス | ファイル | 理由 |
|---|--------|----------|------|
| 1 | `AnalysisTabWiring` | `src/ui/wiring/analysistabwiring.cpp` | `.cpp` で MainWindow の private メンバへのアクセスが **0件**。完全に Deps 構造体経由で動作。friend 宣言は不要 |
| 2 | `CsaGameWiring` | `src/ui/wiring/csagamewiring.cpp` | `.cpp` で MainWindow の private メンバへのアクセスが **0件**。Dependencies 構造体のコンストラクタインジェクションで完全分離済み |

**削除方法**: `mainwindow.h` から `friend class` 行を削除するだけ。コード変更不要。

---

### カテゴリ B: 容易に削除可能（少数アクセス、パラメータ化で対応）

| # | クラス | ファイル | アクセス数 | 概要 |
|---|--------|----------|-----------|------|
| 3 | `KifuExportDepsAssembler` | `src/app/kifuexportdepsassembler.cpp` | ~24フィールド読み取り | `ensure*()` 呼び出しなし、書き込みなし。純粋な読み取りスナップショット |
| 4 | `MatchCoordinatorWiring` | `src/ui/wiring/matchcoordinatorwiring.cpp` | 3フィールド + 3 ensure呼び出し | `wireForwardingSignals()` 1メソッドのみでアクセス |

**KifuExportDepsAssembler の移行方法**:
- `assemble()` が `MainWindow&` の代わりに `RuntimeRefs` を受け取るように変更
- または `MainWindow::updateKifuExportDependencies()` が `buildRuntimeRefs()` 経由で RuntimeRefs → KifuExportController::Dependencies に変換
- `RuntimeRefs` は既に必要なデータの大部分を含んでいる

**MatchCoordinatorWiring の移行方法**:
- `wireForwardingSignals(MainWindow* mw, ...)` のパラメータを以下に変更：
  ```cpp
  struct ForwardingTargets {
      MainWindowAppearanceController* appearance;
      PlayerInfoWiring* playerInfo;
      KifuNavigationCoordinator* kifuNav;
      BranchNavigationWiring* branchNav;
  };
  void wireForwardingSignals(const ForwardingTargets& targets, ...);
  ```
- `ensure*()` は呼び出し元（MainWindowGameRegistry）で事前実行

---

### カテゴリ C: 中程度の難易度（ensure呼び出し + 複数フィールド）

| # | クラス | ファイル | アクセス数 | 概要 |
|---|--------|----------|-----------|------|
| 5 | `RecordNavigationWiring` | `src/ui/wiring/recordnavigationwiring.cpp` | ~5フィールド + 3 ensure | 配線メソッドで ensure + connect |
| 6 | `KifuLoadCoordinatorFactory` | `src/app/kifuloadcoordinatorfactory.cpp` | ~16フィールド + 3 ensure + 1書き込み | ファクトリ生成 + シグナル配線 |
| 7 | `MainWindowSignalRouter` | `src/ui/wiring/mainwindowsignalrouter.cpp` | ~16フィールド + 3 ensure | Deps構造体組み立て + シグナル接続 |

**RecordNavigationWiring の移行方法**:
- `wireSignals()` の引数に配線ターゲットを明示的に渡す
- `ensure*()` 呼び出しは呼び出し元で事前実行し、生成済みポインタを渡す
- `setCurrentTurn()` は private slot → public slot に変更が必要

**KifuLoadCoordinatorFactory の移行方法**:
- ファクトリメソッドが入力構造体を受け取り、生成した `KifuLoadCoordinator*` を戻り値で返す
- `m_kifuLoadCoordinator` への書き込みは呼び出し元で行う
- `ensure*()` は呼び出し元で事前実行

**MainWindowSignalRouter の移行方法**:
- `connectAllActions()` に Deps 構造体を渡す（UiActionsWiring::Deps と同様）
- `ensure*()` は `std::function` コールバックで注入

---

### カテゴリ D: 削除困難（大量アクセス・密結合）

| # | クラス | ファイル | アクセス数 | 概要 |
|---|--------|----------|-----------|------|
| 8 | `MainWindowServiceRegistry` | `src/app/mainwindowserviceregistry.cpp` | 0（直接）| サブレジストリへの転送ハブ。自身はアクセスしないが、5つのサブレジストリに `MainWindow&` を渡す |
| 9 | `MainWindowAnalysisRegistry` | `src/app/mainwindowanalysisregistry.cpp` | ~27 | ensure + Deps組み立て + buildRuntimeRefs |
| 10 | `MainWindowBoardRegistry` | `src/app/mainwindowboardregistry.cpp` | ~41 | ensure + Deps + ui フォーム + 書き込み |
| 11 | `MainWindowGameRegistry` | `src/app/mainwindowgameregistry.cpp` | ~70+（最大） | 全集約構造体への読み書き、16フィールドの直接クリア、ダブルポインタ渡し |
| 12 | `MainWindowKifuRegistry` | `src/app/mainwindowkifuregistry.cpp` | ~48 | 全集約構造体 + ui + buildRuntimeRefs |
| 13 | `MainWindowUiRegistry` | `src/app/mainwindowuiregistry.cpp` | ~47 | 全集約構造体 + ui + DockWidgets全フィールド |

**削除困難な理由**:
- 5つの Registry クラスは MainWindow の **全 private 集約構造体** (`GameState`, `PlayerState`, `KifuState`, `BranchNavigation`, `DataModels`, `DockWidgets`) にアクセスしている
- `ensure*()` によるガード付き遅延生成パターン（null チェック → new → MainWindow フィールドに代入）が支配的
- `GameRegistry::clearGameStateFields()` は16個の個別フィールドに直接書き込む
- `GameRegistry::ensureGameSessionOrchestrator()` はダブルポインタ (`&m_mw.m_xxx`) を渡し、オーケストレータがポインタ更新を追跡する

**将来的な削減方向**:
- MainWindow が集約構造体への **可変参照アクセサ** を公開する（`GameState& gameState()`）
- 全 `ensure*()` メソッドを MainWindowServiceRegistry に集約（現在は MainWindow に残っているものがある）
- `buildRuntimeRefs()` を公開メソッドにする
- ただし、これらは実質的に private を public にするのと同等であり、カプセル化の観点からは friend パターンの方が望ましい

---

### カテゴリ E: 保留（アーキテクチャ上 friend が妥当）

| # | クラス | ファイル | アクセス数 | 概要 |
|---|--------|----------|-----------|------|
| 14 | `MainWindowUiBootstrapper` | `src/app/mainwindowuibootstrapper.cpp` | ~6フィールド + ui + 1 ensure | UI起動シーケンス全体をオーケストレーション |
| 15 | `MainWindowRuntimeRefsFactory` | `src/app/mainwindowruntimerefsfactory.cpp` | ~43（最多読み取り） | 全メンバのスナップショット取得 |
| 16 | `MainWindowWiringAssembler` | `src/app/mainwindowwiringassembler.cpp` | ~50+ | 大量コールバック構築 + ensure |
| 17 | `MainWindowDockBootstrapper` | `src/app/mainwindowdockbootstrapper.cpp` | ~30+書き込み含む | ドック生成・モデル初期化・ウィジェット代入 |
| 18 | `MainWindowLifecyclePipeline` | `src/app/mainwindowlifecyclepipeline.cpp` | ~25 | 起動/終了パイプライン、フィールド書き込み多数 |

**保留の理由**:
- これらは MainWindow の **構築・初期化・ライフサイクル管理** を担うクラスであり、MainWindow 内部への広範なアクセスは設計上必然
- `RuntimeRefsFactory` は全メンバの読み取りスナップショットで、MainWindow の `buildRuntimeRefs()` に統合可能だが、ファクトリクラスとして分離する方が保守性が高い
- `WiringAssembler` と `DockBootstrapper` は初期化時のみ動作し、実行時のアクセスはない
- friend で限定するのが最もバランスの良いカプセル化戦略

---

## 削減計画

### 目標

18件 → **11件以下**（7件削減）

### Phase 1: 即時削除（リスクゼロ）

**対象**: AnalysisTabWiring, CsaGameWiring（カテゴリ A の2件）

- 作業: `mainwindow.h` から `friend class` 宣言を削除
- 確認: ビルドが通ることを確認
- 工数: 極小

### Phase 2: パラメータ化による削除（低リスク）

**対象**: KifuExportDepsAssembler, MatchCoordinatorWiring（カテゴリ B の2件）

**順序**:
1. `KifuExportDepsAssembler` — `RuntimeRefs` 受け取りに変更（依存少、ensure なし）
2. `MatchCoordinatorWiring` — `wireForwardingSignals()` のパラメータ化（1メソッドのみ対象）

### Phase 3: 中規模リファクタ（中リスク）

**対象**: RecordNavigationWiring, KifuLoadCoordinatorFactory, MainWindowSignalRouter（カテゴリ C の3件）

**順序**:
1. `RecordNavigationWiring` — 依存が最も少ない（5フィールド）
2. `KifuLoadCoordinatorFactory` — 入力構造体 + 戻り値パターンで置換
3. `MainWindowSignalRouter` — Deps 構造体 + コールバック注入（最も複雑）

### Phase 4: 長期検討（必要に応じて）

**対象**: カテゴリ D, E の11件

- 5つの Registry クラス + ServiceRegistry（6件）は現状維持が妥当
- 5つの初期化/ライフサイクルクラス（5件）も現状維持が妥当
- 将来的に状態を独立オブジェクトに抽出する大規模リファクタの際に再検討

---

## 削減後の状態

| Phase | 削減数 | 残り friend 数 |
|-------|--------|---------------|
| 現状 | — | 18 |
| Phase 1 完了 | -2 | 16 |
| Phase 2 完了 | -2 | 14 |
| Phase 3 完了 | -3 | 11 |
| （Phase 4 は将来検討） | — | 11 |

---

## 共通アクセスパターンの分析

### パターン 1: Null ガード + 生成 + 代入
```cpp
if (m_mw.m_xxx) return;
m_mw.m_xxx = new Xxx(&m_mw);
```
全 Registry クラスで支配的。friend が必要な最大の理由。

### パターン 2: Deps 構造体組み立て
```cpp
Deps deps;
deps.field1 = m_mw.m_field1;
deps.field2 = m_mw.m_field2;
controller->updateDeps(deps);
```
5-25個の private フィールドを読み取り。RuntimeRefs で代替可能な場合あり。

### パターン 3: ダブルポインタ渡し
```cpp
deps.xxx = &m_mw.m_xxx;
```
GameRegistry の `ensureGameSessionOrchestrator()` で使用。ポインタ更新追跡のため。

### パターン 4: ensure コールバック
```cpp
deps.ensureXxx = std::bind(&MainWindow::ensureXxx, &m_mw);
```
WiringAssembler 等で使用。遅延初期化のトリガー。

### パターン 5: buildRuntimeRefs + CompositionRoot
```cpp
auto refs = m_mw.buildRuntimeRefs();
m_mw.m_compositionRoot->createXxx(refs);
```
ほぼ全 Registry で使用。RuntimeRefs を公開すれば代替可能。
