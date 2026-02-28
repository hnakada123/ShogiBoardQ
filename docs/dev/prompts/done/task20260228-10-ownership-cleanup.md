# Task 10: delete箇所の監査・分類・置換（テーマC: 所有権モデル最終統一）

## 目的

残存する `delete` 文5件を監査し、可能な箇所を `unique_ptr` / QObject parent ownershipに置換、例外箇所に理由コメントを追加する。

## 背景

- 現在 `delete` 文は5件（`tst_structural_kpi` 実測）
- テーマC（所有権モデル最終統一）の目標は `5 → 2`
- 各箇所の置換可否を判断し、安全に移行する

## 事前調査

### Step 1: 全delete箇所の一覧化

```bash
rg -n '\bdelete\b' src --type cpp
```

現在の5件:
1. `src/ui/wiring/matchcoordinatorwiring.cpp:214` — `delete m_match;`
2. `src/engine/engineprocessmanager.cpp:63` — `delete m_process;`
3. `src/engine/engineprocessmanager.cpp:91` — `delete m_process;`
4. `src/dialogs/engineregistrationdialog.cpp:792` — `delete selectedItem;`
5. `src/widgets/flowlayout.cpp:23` — `delete item;`

### Step 2: 各箇所の分類

各delete箇所について以下を調査:

```bash
# 対象オブジェクトの型と生成箇所
rg "m_match" src/ui/wiring/matchcoordinatorwiring.cpp -n
rg "m_process" src/engine/engineprocessmanager.cpp -n
rg "selectedItem" src/dialogs/engineregistrationdialog.cpp -n -C 5
rg "item" src/widgets/flowlayout.cpp -n -C 5
```

分類:
- **A: QObject parent で代替可能** — parentを設定すれば自動解放
- **B: std::unique_ptr で代替可能** — 所有権が明確で単一箇所で管理
- **C: 代替困難（例外扱い）** — 破棄順依存、外部API制約等

## 実装手順

### Step 3: カテゴリA（QObject parent代替）の置換

対象が `QObject` 派生で、適切なparentが設定可能な場合:
1. コンストラクタでparentを設定
2. `delete` 文を削除
3. 動作確認

### Step 4: カテゴリB（unique_ptr代替）の置換

対象が単一所有者パターンの場合:
1. メンバ変数を `std::unique_ptr<T>` に変更
2. `new` を `std::make_unique<T>()` に変更
3. `delete` 文を `m_ptr.reset()` または削除
4. ヘッダの前方宣言とデストラクタの整合性を確認

### Step 5: カテゴリC（例外）の理由コメント追加

代替困難な箇所には以下の形式でコメントを追加:

```cpp
// NOTE: Manual delete required because [具体的な理由]
// - 例: QLayoutItem is not a QObject, cannot use parent ownership
// - 例: Process must be destroyed before manager due to signal ordering
delete item;
```

### Step 6: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen build/tests/tst_structural_kpi
```

### Step 7: 構造KPI更新

`tests/tst_structural_kpi.cpp` の `deleteCount` 上限を実測値に合わせて更新。

## 完了条件

- [ ] `delete` 件数が5から減少（目標: 2）
- [ ] 残存する `delete` 全てに理由コメントがある
- [ ] 全テスト通過
- [ ] メモリリーク・ダングリングポインタがない（ビルド+テストで確認）
- [ ] 構造KPI例外値が最新化されている

## KPI変化目標

- `delete` 文: 5 → 2
- 例外 `delete` 箇所のコメント付与率: 100%

## 注意事項

- `QLayoutItem` は `QObject` ではないため、parent ownershipは使えない（`flowlayout.cpp`）
- `QProcess` の解放タイミングはシグナル接続に依存する場合がある
- `delete` → `unique_ptr` 変換時、ヘッダで前方宣言だけの場合はデストラクタを .cpp に定義する必要がある（unique_ptr forward-declared type問題）
