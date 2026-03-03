# Task 20260303-03: `buildRuntimeRefs()` の副作用分離（P1）

## 概要

`buildRuntimeRefs()` は参照構築関数だが、内部で `m_playModePolicy->updateDeps()` を呼んでおり、読み取り関数に副作用がある。参照構築と依存更新を分離して関数責務を明確化する。

## 背景

- `src/app/mainwindow.cpp:253-261` で `buildRuntimeRefs()` 内に `m_playModePolicy->updateDeps()` の呼び出しがある
- `buildRuntimeRefs()` は各 `ensure*` メソッドから頻繁に呼ばれるスナップショット構築関数
- 読み取り関数に副作用があると呼び出し順依存の理解負荷が高い

## 実装手順

### Step 1: `buildRuntimeRefs()` から依存更新コードを削除

`src/app/mainwindow.cpp` の `buildRuntimeRefs()` 末尾（253-261行目付近）から以下のブロックを削除:

```cpp
// 削除対象
// PlayModePolicyService の依存を最新状態に更新
if (m_playModePolicy) {
    PlayModePolicyService::Deps policyDeps;
    policyDeps.playMode = &m_state.playMode;
    policyDeps.gameController = m_gameController;
    policyDeps.match = m_match;
    policyDeps.csaGameCoordinator = m_csaGameCoordinator;
    m_playModePolicy->updateDeps(policyDeps);
}
```

### Step 2: 新規メソッド `refreshPlayModePolicyDeps()` を追加

`src/app/mainwindow.h` の private セクションに宣言を追加:

```cpp
void refreshPlayModePolicyDeps();
```

`src/app/mainwindow.cpp` に実装を追加:

```cpp
void MainWindow::refreshPlayModePolicyDeps()
{
    if (!m_playModePolicy) return;
    PlayModePolicyService::Deps policyDeps;
    policyDeps.playMode = &m_state.playMode;
    policyDeps.gameController = m_gameController;
    policyDeps.match = m_match;
    policyDeps.csaGameCoordinator = m_csaGameCoordinator;
    m_playModePolicy->updateDeps(policyDeps);
}
```

### Step 3: `refreshPlayModePolicyDeps()` の呼び出しポイントを設定

以下のタイミングで呼び出す:

1. **起動完了直後**: `MainWindowLifecyclePipeline::initializeEarlyServices()` の PlayModePolicyService 生成直後（現在 `updateDeps` を呼んでいる箇所を `refreshPlayModePolicyDeps()` に置き換え）
2. **対局開始直前 / Match 再生成直後**: `MatchCoordinatorWiring` の `initializeSession()` 等で Match が再生成された後。Grep で `m_match =` の代入箇所を確認し、Match ポインタ更新後に `refreshPlayModePolicyDeps()` が呼ばれるようにする。

具体的な呼び出し箇所は以下を検索して特定する:
- `m_match =` の代入（Match 再生成時）
- `m_gameController =` の代入（GameController 更新時）
- `m_csaGameCoordinator =` の代入（CSA 更新時）

### Step 4: 既存の初期化順序との整合確認

`initializeEarlyServices()` 内の `PlayModePolicyService` 初期化コード（`src/app/mainwindowlifecyclepipeline.cpp:174-178` 付近）を確認し、`refreshPlayModePolicyDeps()` 呼び出しに統一する。ここでは MainWindow のメソッドとして呼ぶ:

```cpp
m_mw.refreshPlayModePolicyDeps();
```

### Step 5: コメントで呼び出し契約を明示

`refreshPlayModePolicyDeps()` の実装コメントに、呼び出しが必要なタイミングを列挙する:

```cpp
// `refreshPlayModePolicyDeps`: PlayModePolicyService の依存を最新に更新する。
// 以下のタイミングで呼び出すこと:
//   - 起動完了直後（initializeEarlyServices）
//   - Match 再生成直後（MatchCoordinatorWiring::initializeSession）
//   - GameController 再生成時
```

## 確認手順

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

以下を確認:
- `buildRuntimeRefs()` が状態変更を一切行わない（`updateDeps` 等の副作用呼び出しがない）
- `tst_app_lifecycle_pipeline` が pass（存在する場合）
- `tst_lifecycle_scenario` が pass
- 全 57 テスト pass

## 制約

- CLAUDE.md のコードスタイルに従うこと
- `connect()` でラムダ不使用
- `refreshPlayModePolicyDeps()` は private メソッドとする
- 翻訳更新は不要（UI文字列変更なし）
