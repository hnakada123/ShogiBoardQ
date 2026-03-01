# Task 14: 所有権ルール文書化と QPointer 拡大

## 概要

QObject parent 所有と `std::unique_ptr` の境界ルールを文書化し、再生成されるオブジェクトに寿命コメントを明記する。
`QPointer` を「非所有参照」の標準として拡大する。

## 前提条件

- Task 01, 07, 09 のいずれかが完了していること（所有権構造が安定してから文書化する）

## 現状

- `docs/dev/ownership-guidelines.md` が存在（CLAUDE.md から参照されている）
- `delete` 件数は 1（`src/widgets/flowlayout.cpp`）と健全
- `new` が registry/compose 層に多い
- 再生成されるオブジェクト（orchestrator/wiring）に寿命コメントがないものがある
- `QPointer` の使用は限定的

## 実施内容

### Step 1: 既存文書の確認と更新

1. `docs/dev/ownership-guidelines.md` を読み、現行ルールを把握
2. 以下を追記・更新:
   - QObject parent 所有の使用基準（いつ parent を設定するか）
   - `std::unique_ptr` の使用基準（非QObject、またはparent不要の場合）
   - 裸の `new` が許容される場合（`QObject` + parent 即時設定のみ）
   - 再生成オブジェクトの寿命管理パターン

### Step 2: 再生成オブジェクトの特定と寿命コメント追加

registry/compose 層で再生成されるオブジェクトを洗い出す:

```bash
grep -rn "new " src/app/mainwindow*.cpp src/app/*registry*.cpp | grep -v "//"
```

各 `new` 呼び出しに以下の寿命コメントを追記:

```cpp
// Lifetime: owned by MainWindow (QObject parent)
// Recreated: on each game session start
m_foo = new Foo(this);
```

### Step 3: QPointer 拡大の検討

1. 非所有参照（RuntimeRefs の raw pointer フィールド等）で `QPointer` に置換可能なものを特定
2. 以下の基準で QPointer に変更:
   - 参照先の寿命が参照元より短い可能性がある場合
   - 参照先が再生成される場合
   - `nullptr` チェックが既にある場合
3. 段階的に変更（一度に全置換しない）

### Step 4: ダングリングポインタリスクの検証

```bash
# QPointer 使用箇所の確認
grep -rn "QPointer" src/ | wc -l
# 再生成パターンの確認
grep -rn "delete\|reset\|\.reset(" src/app/ | grep -v "//"
```

## 完了条件

- `docs/dev/ownership-guidelines.md` が更新されていること
- 再生成オブジェクトに寿命コメントが 100% 付与されていること
- `QPointer` が最低5箇所で導入されていること
- `delete` 件数が維持されていること（<= 1）
- ビルド成功
- 全テスト通過

## 注意事項

- `QPointer` は `QObject` 派生型にのみ使用可能
- `QPointer` の導入はパフォーマンスへの影響が軽微（QObject の destroyed シグナルを利用）
- `std::unique_ptr` と `QPointer` は役割が異なる:
  - `unique_ptr` = 所有権を持つ
  - `QPointer` = 所有権を持たないが、寿命を自動追跡する
- `RuntimeRefs` のフィールドは raw pointer のままでもよい（値オブジェクトのため生存期間が短い）
