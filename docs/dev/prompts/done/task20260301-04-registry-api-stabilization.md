# Task 20260301-04: Registry API 余白確保（P1）

## 背景

`ensure*` 系メソッド数が全サブレジストリで上限値と同一に張り付いている。
今後の機能追加時に設計を押し広げる余地が少なく、APIが再肥大化するリスクがある。

### 現状の数値

| Registry | ensure* 数 | 上限 |
|---|---:|---:|
| MainWindowServiceRegistry | 11 | 11 |
| MainWindowFoundationRegistry | 15 | 15 |
| GameSubRegistry | 6 | 6 |
| GameSessionSubRegistry | 6 | 6 |
| GameWiringSubRegistry | 4 | 4 |
| KifuSubRegistry | 8 | 8 |

## 作業内容

### Step 1: ensure* と refreshDeps の分離標準化

現在の `ensure*` メソッドは「生成 + 依存注入」を一体で行っている。
これを以下の2段階に分離する標準パターンを確立する:

```cpp
// Before: ensure*() = 生成 + 依存注入
void ensureFoo()
{
    if (!m_mw.m_foo) {
        m_mw.m_foo = new Foo(/* deps */);
        // wire signals...
    }
    // refresh deps every time
    m_mw.m_foo->updateDeps(/* current deps */);
}

// After: ensure*() = 生成のみ、refreshFooDeps() = 依存注入
void ensureFoo()
{
    if (m_mw.m_foo) return;
    m_mw.m_foo = new Foo(/* minimal deps */);
    // wire signals (one-time)...
}

void refreshFooDeps()
{
    if (!m_mw.m_foo) return;
    m_mw.m_foo->updateDeps(/* current deps */);
}
```

#### 対象の選定

全 `ensure*` メソッドを調査し、以下に分類する:

1. **生成のみ（分離不要）**: `updateDeps()` 呼び出しがない
2. **生成 + 依存更新（分離推奨）**: 毎回 `updateDeps()` を呼ぶ必要がある
3. **複合（要検討）**: 生成と依存注入が密結合

#### 分離の実施

カテゴリ2のメソッドについて、`refresh*Deps()` メソッドを新設し、
`ensure*()` から依存注入ロジックを分離する。

**注意**: `refresh*Deps()` は `ensure*` のカウントに含めない（KPIテストのパターンは `ensure` のみ検索）。

### Step 2: SubRegistry 追加規約の明文化

`docs/dev/registry-boundary.md` を更新し、以下のルールを追記する:

```markdown
## ensure* 追加ルール

1. 新規 ensure* を追加する場合、同時に既存 ensure* の統合・削除を検討する
2. 統合不可能な場合、PR 説明に「削減セット不提出の理由」を記載する
3. refresh*Deps() は ensure* とは別カウントとし、自由に追加できる
4. ensure* の新規追加は KPI テストの上限値更新を伴う（レビュー対象）
```

### Step 3: KPI テストの refresh* 除外確認

`tst_structural_kpi.cpp` の SubRegistry ensure* カウントが `refresh*` を含まないことを確認する。
現在の検索パターンが `ensure` のみであれば変更不要。

```cpp
// 確認: countMatchingLines の pattern が "ensure" のみであること
```

### Step 4: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件

- [x] ensure* と refreshDeps の分離パターンが少なくとも2箇所で適用される
- [x] `docs/dev/registry-boundary.md` に追加規約が記載される
- [x] KPI テストの ensure* カウントが refresh* を除外していることが確認される
- [x] 全テスト pass
