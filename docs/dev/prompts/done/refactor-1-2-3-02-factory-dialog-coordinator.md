# Step 02: ensureDialogCoordinator の factory 化

## 設計書

`docs/dev/mainwindow-refactor-1-2-3-plan.md` のフェーズ1・ステップ2に対応。

## 前提

Step 01 が完了済み（`MainWindowRuntimeRefs` と `MainWindowDepsFactory` が存在する）。

## 背景

`MainWindow::ensureDialogCoordinator()` 内で `DialogCoordinatorWiring::Deps` の約30フィールドを直接組み立てている。このDeps生成を `MainWindowDepsFactory` に移す。

## タスク

### 1. MainWindow に `MainWindowRuntimeRefs` 構築ヘルパーを追加

- `MainWindow` に `MainWindowRuntimeRefs buildRuntimeRefs()` メソッドを追加する（private）
- このメソッドは `MainWindow` の現在のメンバ変数から `MainWindowRuntimeRefs` を構築して返す
- メンバ変数は `ensure*()` 呼び出しのタイミングで異なる値を持つため、呼び出し時点のスナップショットを返す設計にする

### 2. ensureDialogCoordinator() の Deps 組み立てを factory に委譲

`src/app/mainwindow.cpp` の `ensureDialogCoordinator()` を変更:

**Before（現在のコード、約40行のDeps組み立て）:**
```cpp
DialogCoordinatorWiring::Deps deps;
deps.parentWidget = this;
deps.match = m_match;
// ... 30行以上のフィールド設定 ...
deps.navigateKifuViewToRow = std::bind(...);
```

**After:**
```cpp
auto refs = buildRuntimeRefs();
auto deps = MainWindowDepsFactory::createDialogCoordinatorDeps(refs, /* callbacks */);
```

- `std::function` コールバック（`getBoardFlipped`, `getConsiderationWiring`, `getUiStatePolicyManager`, `navigateKifuViewToRow`）はファクトリの引数として渡す
- コールバックの中身（`[this]() { ensureConsiderationWiring(); return m_considerationWiring; }` 等）は `MainWindow` 側に残す（`this` キャプチャが必要なため）

### 3. MainWindowDepsFactory の実装を完成

- `createDialogCoordinatorDeps` の実装を完成させる
- `MainWindowRuntimeRefs` の各フィールドから `DialogCoordinatorWiring::Deps` の対応フィールドへマッピング
- コールバック引数はそのまま `Deps` にセットする

## 制約

- `ensureDialogCoordinator()` の外部から見た挙動（生成タイミング、生成される Deps の値）は一切変更しない
- `DialogCoordinatorWiring::Deps` 構造体自体は変更しない
- 新規 lambda `connect()` は追加しない

## 回帰確認

ビルド成功後、以下を手動確認:
- アプリ起動直後のメニュー操作
- 棋譜解析ダイアログの起動
- 検討モードの開始・終了

## 完了条件

- `ensureDialogCoordinator()` 内の `Deps d; d.xxx = ...` の直接構築コードが消え、factory 呼び出しに置き換わっている
- `cmake -B build -S . && cmake --build build` が成功する
- コミットメッセージ: 「ensureDialogCoordinator の Deps 組み立てを MainWindowDepsFactory に移動」
