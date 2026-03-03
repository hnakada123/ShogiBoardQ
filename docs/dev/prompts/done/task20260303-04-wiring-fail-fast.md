# Task 20260303-04: Wiring の null 依存前提を明文化し fail-fast 化（P2）

## 概要

`GameActionsWiring::wire()` と `UiActionsWiring::wire()` が必須依存（`dcw`/`gso`/`dlw` 等）の null チェックなしで `connect()` を呼んでいる。必須依存の妥当性を検証し、未充足時は早期 return + 明確ログを出す fail-fast 化を行う。

## 背景

- `src/ui/wiring/gameactionswiring.cpp:11-37` で `gso`、`dcw`、`dlw` を null チェックなしで使用
- `src/ui/wiring/uiactionswiring.cpp:22-27` で `m_d.dcw`、`m_d.gso` 等を null 可能のまま子 Wiring に渡す
- 現状は前段の初期化順に依存して成立しているが、将来の変更で壊れやすい

## 実装手順

### Step 1: `GameActionsWiring::wire()` に必須依存チェックを追加

`src/ui/wiring/gameactionswiring.cpp` の `wire()` 冒頭に以下を追加:

```cpp
void GameActionsWiring::wire()
{
    auto* ui  = m_d.ui;
    auto* mw  = m_d.mw;
    auto* dlw = m_d.dlw;
    auto* dcw = m_d.dcw;
    auto* gso = m_d.gso;

    if (Q_UNLIKELY(!ui || !mw)) {
        qCWarning(lcUi, "GameActionsWiring::wire(): ui or mw is null");
        return;
    }
    if (Q_UNLIKELY(!dlw || !dcw || !gso)) {
        qCWarning(lcUi, "GameActionsWiring::wire(): required wiring dependency is null "
                  "(dlw=%p, dcw=%p, gso=%p)",
                  static_cast<void*>(dlw), static_cast<void*>(dcw), static_cast<void*>(gso));
        return;
    }
    // ... 既存の connect() 呼び出し
```

`logcategories.h` の include が必要な場合は追加する。

### Step 2: 他の `*ActionsWiring::wire()` にも同様のチェックを追加

以下のファイルも確認し、同様の null チェック + `qCWarning` + 早期 return を追加:

- `src/ui/wiring/fileactionswiring.cpp` - `wire()` の `dlw`, `kfc`, `gso` チェック
- `src/ui/wiring/editactionswiring.cpp` - `wire()` の必須依存チェック
- `src/ui/wiring/viewactionswiring.cpp` - `wire()` の必須依存チェック

各ファイルの `wire()` メソッドを読み、`connect()` のレシーバとして使われるポインタを必須依存として検証する。

### Step 3: `UiActionsWiring::wire()` の子 Wiring 生成前にチェック強化

`src/ui/wiring/uiactionswiring.cpp` の子 Wiring 生成ブロック（22-28行目）の前に、主要依存の null 検証を追加:

```cpp
if (Q_UNLIKELY(!m_d.dlw || !m_d.dcw || !m_d.gso)) {
    qCWarning(lcUi, "UiActionsWiring::wire(): critical dependency is null "
              "(dlw=%p, dcw=%p, gso=%p)",
              static_cast<void*>(m_d.dlw), static_cast<void*>(m_d.dcw),
              static_cast<void*>(m_d.gso));
    return;
}
```

### Step 4: テストに null 依存時の防御確認を追加

`tests/tst_wiring_contracts.cpp` に以下の趣旨のテストを追加（既存のテストパターンに合わせる）:

- `GameActionsWiring` の `new` で検索して生成パターンを確認
- `wire()` メソッドが呼ばれる前に全必須依存が非 null であることを検証するテスト
- もしくは、ソースコード上で `Q_UNLIKELY(!` パターンの存在を確認する構造テスト

テストの追加方法は既存の `tst_wiring_contracts.cpp` のパターンに従う。

## 確認手順

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

以下を確認:
- 全テスト pass
- `tst_wiring_contracts` pass
- `tst_wiring_slot_coverage` pass
- 必須依存不足時に `qCWarning` で原因がログに出力されること（コードレビューで確認）

## 制約

- CLAUDE.md のコードスタイルに従うこと
- `qCWarning(lcUi, ...)` を使用（`qWarning()` ではなく）
- `logcategories.h` の include が必要な場合は追加
- 早期 return はするが、`Q_ASSERT` は使わない（リリースビルドで無効化されるため `qCWarning` + return が適切）
- 翻訳更新は不要（UI文字列変更なし）
