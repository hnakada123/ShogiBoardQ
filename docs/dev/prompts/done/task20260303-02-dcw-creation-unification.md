# Task 20260303-02: DialogCoordinatorWiring 生成経路の一本化（P1）

## 概要

`DialogCoordinatorWiring` が `MainWindowSignalRouter::connectAllActions()` と `MainWindowCompositionRoot::ensureDialogCoordinator()` の2箇所で生成される。生成責務を CompositionRoot/Registry 経由に統一し、SignalRouter は取得済みインスタンスを使うだけにする。

## 背景

- `src/ui/wiring/mainwindowsignalrouter.cpp:56-58` で `new DialogCoordinatorWiring(m_deps.mainWindow)` を直接実行
- `src/app/mainwindowcompositionroot.cpp:59-61` でも `createDialogCoordinatorWiring(parent)` を実行
- 生成責務が分散し、依存更新漏れや設計逸脱の温床になっている

## 実装手順

### Step 1: SignalRouter から直接生成を削除

`src/ui/wiring/mainwindowsignalrouter.cpp` の `connectAllActions()` から以下のブロックを削除:

```cpp
// 削除対象（55-58行目付近）
// DialogCoordinatorWiring を先行生成（cancelKifuAnalysis 接続先として必要）
if (m_deps.dialogCoordinatorWiringPtr && !deref(m_deps.dialogCoordinatorWiringPtr)) {
    *m_deps.dialogCoordinatorWiringPtr = new DialogCoordinatorWiring(m_deps.mainWindow);
}
```

代わりに、`connectAllActions()` 冒頭で `ensureDialogCoordinator` コールバックを呼ぶようにする:

```cpp
// CompositionRoot/Registry 経由で DialogCoordinatorWiring を確保
if (m_deps.ensureDialogCoordinator) {
    m_deps.ensureDialogCoordinator();
}
```

### Step 2: SignalRouter の Deps に `ensureDialogCoordinator` コールバックを追加

`src/ui/wiring/mainwindowsignalrouter.h` の Deps 構造体に以下を追加:

```cpp
std::function<void()> ensureDialogCoordinator;
```

### Step 3: SignalRouter の Deps 設定箇所を更新

`MainWindowSignalRouter` の Deps を設定している箇所（`MainWindowLifecyclePipeline` 内の `connectSignals()` 等）で、新しいコールバックを設定する:

```cpp
deps.ensureDialogCoordinator = [this]() { m_mw.m_registry->ensureDialogCoordinator(); };
```

設定箇所を Grep で検索: `m_signalRouter->setDeps` または `SignalRouter::Deps` の代入箇所。

### Step 4: UiActionsWiring 生成時の dcw null 検証

`mainwindowsignalrouter.cpp:76` の `d.dcw = deref(m_deps.dialogCoordinatorWiringPtr);` の後に、null チェックを追加:

```cpp
if (Q_UNLIKELY(!d.dcw)) {
    qCWarning(lcApp, "connectAllActions: DialogCoordinatorWiring is null after ensure");
}
```

### Step 5: 生成ルールをドキュメントに追記

`docs/dev/ownership-guidelines.md` の適切なセクション（「生成パターン」等）に以下のルールを追記:

> **Wiring クラスの生成ルール**: Wiring クラス（`*Wiring`）の `new` は `MainWindowCompositionRoot` または `MainWindowServiceRegistry` の `ensure*` メソッド経由のみで行う。`SignalRouter` やその他のクラスで直接 `new` してはならない。

## 確認手順

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

以下を確認:
- `DialogCoordinatorWiring` の `new` が CompositionRoot 由来のみになっている（Grep で `new DialogCoordinatorWiring` を検索して1箇所のみであること）
- `tst_wiring_contracts` が pass
- `tst_wiring_slot_coverage` が pass
- 解析中止（`actionCancelAnalyzeKifu`）が配線されていること（GameActionsWiring で `dcw->cancelKifuAnalysis` に connect）

## 制約

- CLAUDE.md のコードスタイルに従うこと
- `connect()` でラムダ不使用
- Deps 構造体の `std::function` コールバックは許容（Deps struct pattern）
- 翻訳更新は不要（UI文字列変更なし）
