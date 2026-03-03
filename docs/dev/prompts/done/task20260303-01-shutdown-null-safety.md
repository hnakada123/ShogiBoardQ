# Task 20260303-01: シャットダウン後アクセスの安全化（P0）

## 概要

`runShutdown()` で `m_queryService.reset()` / `m_playModePolicy.reset()` した後に、遅延シグナルや非同期処理経由で `buildRuntimeRefs()` が再入するとクラッシュする可能性がある。終了中フラグとnullガードを導入して安全化する。

## 背景

- `src/app/mainwindowlifecyclepipeline.cpp:106-107` で `m_queryService.reset()` / `m_playModePolicy.reset()` を実行
- `src/app/mainwindow.cpp:194` で `m_queryService->sfenRecord()` を無条件参照（nullチェックなし）
- `closeEvent()` 後にキュー処理が残っていた場合のクラッシュ余地がある

## 実装手順

### Step 1: `MainWindow` に終了中フラグ追加

`src/app/mainwindow.h` のメンバ変数セクションに以下を追加:

```cpp
bool m_isShuttingDown = false;
```

`MainWindowLifecyclePipeline` から参照するため、private メンバとし friend アクセスで設定する（既に `MainWindowLifecyclePipeline` は friend class）。

### Step 2: `runShutdown()` 冒頭でフラグ設定

`src/app/mainwindowlifecyclepipeline.cpp` の `runShutdown()` で `m_shutdownDone = true;` の直後に:

```cpp
m_mw.m_isShuttingDown = true;
```

### Step 3: `buildRuntimeRefs()` に null ガード追加

`src/app/mainwindow.cpp:194` の行を以下に変更:

```cpp
refs.kifu.sfenRecord = m_queryService ? m_queryService->sfenRecord() : nullptr;
```

### Step 4: 終了中の `ensure*` 新規生成を抑制

`src/app/mainwindowuiregistry.cpp` の各 `ensure*` メソッド冒頭に終了中ガードを追加。代表的な `ensureDialogCoordinator()` の例:

```cpp
void MainWindowServiceRegistry::ensureDialogCoordinator()
{
    if (m_mw.m_isShuttingDown) return;  // 追加
    if (m_mw.m_dialogCoordinator) return;
    ...
}
```

同様のガードを `MainWindowServiceRegistry` の他の `ensure*` メソッド全体に追加する。ただし `ensureDockLayoutManager()` は `runShutdown()` から呼ばれるため除外。

### Step 5: `MainWindowRuntimeRefs` の sfenRecord 利用箇所でnullガード確認

`sfenRecord` を使う全箇所で `nullptr` を許容するコードになっているか確認し、必要に応じてガードを追加する。Grep で `refs.kifu.sfenRecord` / `sfenRecord` の利用箇所を確認すること。

## 確認手順

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

- 全 57 テスト pass
- `closeEvent()` 後のキュー処理でクラッシュしないことを論理的に確認（shutdown フラグ + null ガードにより ensure* が新規生成しない + buildRuntimeRefs が null を許容）

## 制約

- CLAUDE.md のコードスタイルに従うこと（`connect()` でラムダ不使用、`std::as_const()` 等）
- `m_isShuttingDown` は NSDMI で `false` 初期化（ヘッダー内）
- フラグは `MainWindowLifecyclePipeline` からのみ設定する
- 翻訳更新は不要（UI文字列変更なし）
