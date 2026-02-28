# Task 09: RuntimeRefs分割・生ポインタ削減（テーマB: MainWindow依存密度低減）

## 目的

`MainWindowRuntimeRefs` を用途別に分割し、`MainWindow` の生ポインタメンバを47→35に削減する。

## 背景

- `MainWindowRuntimeRefs` に多数のメンバが集中しており、参照セットが大きい
- `MainWindow` の生ポインタ47件を削減し、依存の見通しを改善する
- テーマB（MainWindow周辺の依存密度低減）の後半作業

## 前提

- Task 08（Registry再分割）が完了していること

## 事前調査

### Step 1: MainWindowRuntimeRefsの現状分析

```bash
# 現在のメンバ一覧
cat src/app/mainwindowruntimerefs.h

# 使用箇所
rg "m_refs\." src/app/ --type cpp -n | head -50
rg "RuntimeRefs" src/app/ -l
```

### Step 2: MainWindowの生ポインタ一覧

```bash
# mainwindow.h の生ポインタメンバ
rg "^\s+\w+\s*\*\s*m_" src/app/mainwindow.h
rg "^\s+\w+\s*\*\s*m_" src/app/mainwindow.h | wc -l
```

## 実装手順

### Step 3: RuntimeRefsの用途別分割設計

RuntimeRefsのメンバを以下のカテゴリに分類:

1. **GameRuntimeRefs**: 対局関連の参照（GameController, TurnManager等）
2. **KifuRuntimeRefs**: 棋譜関連の参照（GameRecordModel, NavigationContext等）
3. **UiRuntimeRefs**: UI関連の参照（View, Widget等）

### Step 4: 分割実装

1. 新しいRefs構造体を `src/app/` に作成
2. `MainWindowRuntimeRefs` を子構造体への委譲に置換
3. 各利用箇所のアクセスパスを更新

### Step 5: 生ポインタの unique_ptr 化

`MainWindow` で直接保持している非UIメンバのうち、以下の条件を満たすものを `std::unique_ptr` に変換:

1. `MainWindow` がライフタイムオーナーである
2. QObject parent で管理されていない
3. 他のクラスから借用参照されるだけ

### Step 6: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
```

## 完了条件

- [ ] `MainWindowRuntimeRefs` が用途別に分割されている
- [ ] `MainWindow` の生ポインタ件数が47から減少（目標: 35）
- [ ] 全テスト通過
- [ ] 構造KPI例外値が最新化されている

## KPI変化目標

- `MainWindow` 生ポインタメンバ: 47 → 35
- `MainWindowRuntimeRefs` が3以上の子構造体に分割

## 注意事項

- Qt Widgetの生ポインタはQt parent ownershipのため、unique_ptr化しない
- `updateDeps()` で渡すRefs構造体のサイズを小さくすることが目的
- 破棄順序に注意（特にQObject間の依存）
