# Task 11: Q_ASSERT → ランタイムチェック

リリースビルドでも有効であるべき `Q_ASSERT` をランタイムチェックに置き換えてください。

## 背景

`Q_ASSERT` はリリースビルド（`QT_NO_DEBUG`）で無効化される。null ポインタ参照の直前にある `Q_ASSERT` は、リリースビルドでクラッシュの原因になりうる。

## 作業内容

### Step 1: 全 Q_ASSERT の列挙

コードベース全体で `Q_ASSERT` を検索し、一覧を作成する。

### Step 2: 分類と判断

各 Q_ASSERT を以下の3カテゴリに分類:

**A. ランタイムチェックに変更すべきもの** — null ポインタ参照の直前、外部入力に依存する不変条件:
```cpp
// Before
Q_ASSERT(m_view);
m_view->update();

// After
if (Q_UNLIKELY(!m_view)) {
    qCWarning(lcXxx, "Unexpected null pointer: m_view");
    return;
}
m_view->update();
```

**B. Q_ASSERT のまま維持するもの** — 内部ロジックの不変条件（プログラミングエラーの検出用）:
```cpp
Q_ASSERT(index >= 0 && index < m_items.size());  // 内部ロジック保証
```

**C. 削除してよいもの** — 自明な条件、または既に他のチェックで保証されている:
```cpp
Q_ASSERT(result);  // 直前で result の生成が保証されている
```

### Step 3: カテゴリ A の変換

- `Q_ASSERT(ptr)` → `if (Q_UNLIKELY(!ptr)) { qCWarning(...); return; }`
- `Q_ASSERT(condition)` → `if (Q_UNLIKELY(!condition)) { qCWarning(...); return; }`
- ErrorBus を使うかどうかは文脈に応じて判断（ユーザーに見せるべきエラーは ErrorBus、内部ログのみは qCWarning）

### Step 4: 結果の記録

変更した箇所と判断理由を簡潔にコメントで記録（必須ではないが推奨）。

## 制約

- CLAUDE.md のコードスタイルに準拠
- 変更は保守的に — 確実にリリースで問題になるもののみ変換
- 全既存テストが通ること
