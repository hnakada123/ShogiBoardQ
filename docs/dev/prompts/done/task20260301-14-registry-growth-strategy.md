# Task 14: レジストリAPI成長戦略の整備

## 概要

`MainWindowServiceRegistry` の `ensure*` が上限11に達している問題に対し、「生成」と「依存更新」の分離を標準化し、成長余地を確保する。

## 前提条件

- Task 01（GameSubRegistry分割）完了必須
- Task 08（RuntimeRefs分割）完了推奨

## 現状

- `MainWindowServiceRegistry` の公開 `ensure*`: 11（上限 11）
- サブレジストリ: `GameSubRegistry`（22メソッド）、`KifuSubRegistry`（10メソッド）
- 現在の `ensure*()` パターン:
  1. ガード: `if (m_mw.m_xxx) return;`
  2. 生成: `m_mw.m_xxx = new Xxx(...)` or `m_mw.m_xxx = make_unique<Xxx>(...)`
  3. 依存更新: `m_mw.m_xxx->updateDeps({...})`
  4. 配線: `connect(...)`
- 一部の `ensure*()` はガードなしで毎回 `updateDeps()` を呼ぶ（例: `ensureBranchNavigationWiring()`）

## 分析

### 問題

1. `ensure*()` が「生成」「依存更新」「配線」を1メソッドに混在させている
2. 初回呼び出しと2回目以降で異なる処理が必要だが、統一的に扱われている
3. 新規 `ensure*` を `ServiceRegistry` に追加できない（上限到達）

### 解決方針

1. `ensure*()` を「生成」（`createX()`）と「依存更新」（`refreshXDeps()`）に分離
2. 「生成」は1回のみ。「依存更新」は依存が変わるたびに呼べる
3. `ServiceRegistry` 直下の `ensure*` は増やさず、サブレジストリに委譲

## 実施内容

### Step 1: 現行パターンの分析

1. `MainWindowServiceRegistry` の11個の `ensure*` を読み込み
2. 各メソッドの「生成」「依存更新」「配線」を行単位で分離可能か判定
3. サブレジストリの `ensure*` についても同様に分析

### Step 2: 2段構造の標準化

ガードあり `ensure*()` を以下に変換:

```cpp
// Before
void ensureX() {
    if (m_mw.m_x) {
        m_mw.m_x->updateDeps({...});
        return;
    }
    m_mw.m_x = new X(...);
    m_mw.m_x->updateDeps({...});
    connect(...);
}

// After
void ensureX() {
    if (!m_mw.m_x) {
        createX();
    }
    refreshXDeps();
}

void createX() {  // private
    m_mw.m_x = new X(...);
    connect(...);
}

void refreshXDeps() {  // public or private
    if (!m_mw.m_x) return;
    m_mw.m_x->updateDeps({...});
}
```

### Step 3: サブレジストリの依存更新API明示化

1. `GameSubRegistry` に `refreshMatchWiringDeps()` 等の公開メソッドを追加
2. `KifuSubRegistry` に `refreshBranchNavWiringDeps()` 等を追加
3. 外部から「依存が変わった」ことを通知できるインタフェースを整備

### Step 4: 境界仕様のテスト化

1. `tst_structural_kpi.cpp` に以下のテストを追加:
   - `ServiceRegistry` 直下の `ensure*` が11以下であること（現行テストの維持）
   - 各サブレジストリの `ensure*` 数の上限設定
2. `docs/dev/registry-boundary.md` に「どのレイヤが何を ensure してよいか」を明記

### Step 5: ビルドとテスト

1. ビルド確認
2. 全テスト通過確認

## 完了条件

- `MainWindowServiceRegistry` の公開 `ensure*`: 11を維持（新規追加ゼロ）
- `GameSubRegistry`/`KifuSubRegistry` で `refreshXDeps()` パターンが標準化
- 境界仕様ドキュメントが作成されている
- テストが追加されている
- ビルド成功
- 全テスト通過

## 注意事項

- `ensure*()` の公開APIは変更しない（互換性維持）
- `createX()` は private。外部からは引き続き `ensure*()` を呼ぶ
- `refreshXDeps()` を public にするかは呼び出し元の要件次第
- 過度なリファクタリングを避け、パターンの標準化に集中する
