# Task 07: `std::as_const()` 欠落修正（P2 / §2.4）

## 概要

メンバコンテナに対する range-based for ループで `std::as_const()` が欠落している箇所を修正し、clazy `range-loop-detach` 警告を解消する。

## 現状

分析で指摘された2箇所:

| ファイル | 行 | コード |
|---|---|---|
| `src/engine/usiprotocolhandler.cpp` | 83 | `for (const QString& cmd : m_setOptionCommands)` |
| `src/models/kifubranchlistmodel.cpp` | 284 | `for (const auto& n : m_nodes)` |

## 手順

### Step 1: 全体検索

1. メンバ変数（`m_` プレフィックス）に対する range-for ループを全てリストアップする
2. `std::as_const()` が欠落している箇所を洗い出す（上記2箇所以外にも存在する可能性）
3. `this->` 経由でアクセスしている場合も対象に含める

### Step 2: 修正

1. 各該当箇所を `std::as_const()` でラップする:
   ```cpp
   // Before
   for (const QString& cmd : m_setOptionCommands)
   // After
   for (const QString& cmd : std::as_const(m_setOptionCommands))
   ```
2. ローカル変数やconst参照には不要なので、誤って追加しないこと

### Step 3: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認

## 注意事項

- `std::as_const()` は `<utility>` ヘッダに含まれる。既にインクルードされていない場合は追加する
- Qt の `qAsConst()` は Qt6.6 で非推奨。`std::as_const()` を使用する
- コンテナがローカル変数で const でない場合でも、ループ内で変更しないなら `std::as_const()` を使うべき
