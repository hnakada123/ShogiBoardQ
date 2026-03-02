# Task 07: CompositionRoot の create/refresh 分離（ISSUE-011 / P1）

## 概要

`MainWindowCompositionRoot` の `ensure*` メソッドが生成責務と依存更新責務を混在させている。`createX()` と `refreshXDeps()` に分離し、見通しを改善する。

## 現状

`src/app/mainwindowcompositionroot.h/.cpp` に11個の `ensure*` メソッド:

```cpp
// 現在のパターン（例）
void ensureDialogCoordinator(refs, callbacks, parent, T*& out) {
    if (!out) return;  // ガード（既存なら何もしない ← 誤り？）
    // 実際は: if (out) return;
    out = new T(parent);
    auto deps = MainWindowDepsFactory::createDialogCoordinatorDeps(refs, callbacks);
    out->updateDeps(deps);
}
```

問題点:
- `ensure` が「生成 + 依存注入」の両方を行う
- 依存更新だけ再実行したいケースに対応できない
- テスタビリティが低い

## 手順

### Step 1: 現状のパターンを分析

1. 全11個の `ensure*` メソッドの実装を読む
2. 各メソッドで「生成」と「依存更新」のコードを分離可能か確認する
3. 依存更新のみが必要なケース（lazy-init 後の再注入）を洗い出す

### Step 2: create/refresh パターンの導入

1. 各 `ensure*` を3つのメソッドに分離する:
   ```cpp
   // 生成のみ
   T* createDialogCoordinator(QWidget* parent);

   // 依存更新のみ
   void refreshDialogCoordinatorDeps(T* coordinator, refs, callbacks);

   // オーケストレーション（既存 ensure の薄いラッパー）
   void ensureDialogCoordinator(refs, callbacks, parent, T*& out) {
       if (out) return;
       out = createDialogCoordinator(parent);
       refreshDialogCoordinatorDeps(out, refs, callbacks);
   }
   ```
2. 主要な ensure 系から段階的に適用する（全11個を一度に変更しない）

### Step 3: ServiceRegistry との連携

1. `MainWindowServiceRegistry` の ensure 系も同様のパターンを検討する
2. ただし ServiceRegistry はスコープが異なるため、必要に応じて段階的に対応

### Step 4: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_structural_kpi|tst_app_lifecycle_pipeline|tst_wiring_contracts" --output-on-failure
   ```
3. KPI 退行がないことを確認

## 完了条件

- 主要な ensure 系で責務分離が適用される
- 挙動互換 + KPI退行なし

## 注意事項

- MEMORY に「Lazy-init dependency gap」の記載あり。`refreshXDeps` を適切なタイミングで呼べるようにすることが重要
- 全メソッドを一度に分離せず、最も複雑な2-3個から始める
- ISSUE-010 完了後に着手する
