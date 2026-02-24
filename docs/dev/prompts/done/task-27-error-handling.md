# Task 27: エラーハンドリングの一貫性向上

## 目的

システム境界（外部入力のパース）でのエラーハンドリングを `std::optional` と `ErrorBus` で一貫させる。

## 背景

- `ErrorBus` が用意されているが全体的に使用されているわけではない。
- `std::optional` の活用が限定的で、パース結果を空文字列や `bool` で返す箇所がある。
- `Q_ASSERT` はリリースビルドで無効化されるため、安全網として不十分な箇所がある。
- 文字列→整数変換でバリデーションなしのパターンが存在する。

## 対象

### 優先度高: システム境界の入力パース

- `src/core/shogiboard.cpp` — SFEN パース関数
- `src/kifu/formats/usitosfenconverter.cpp` — 文字列→整数変換
- `src/engine/usiprotocolhandler.cpp` — USI エンジン出力パース

### 優先度中: 内部の防御的チェック

- `Q_ASSERT` で guard されている重要箇所

## 作業

### Step 1: std::optional 導入（SFEN パース）

```cpp
// Before: 出力パラメータで結果を返す
bool validateSfenString(const QString& sfen, QString& board, QString& stand);

// After: 失敗を型で表現
struct SfenComponents { QString board; QString stand; QString turn; int moveNumber; };
std::optional<SfenComponents> parseSfen(const QString& sfen);
```

対象関数を洗い出し、`std::optional` を返すようリファクタリングする。呼び出し元も合わせて修正する。

### Step 2: 文字列→整数変換の安全化

```cpp
// Before: バリデーションなし
int fromFile = usi.at(0).toLatin1() - '0';

// After: 安全な変換
auto fromFile = parseDigit(usi.at(0));
if (!fromFile) {
    return std::nullopt;  // or ErrorBus
}
```

外部入力を扱う変換箇所にバリデーションを追加する。

### Step 3: Q_ASSERT → ランタイムチェック

リリースビルドでも有効なチェックが必要な箇所を特定し、以下のパターンに置換する:

```cpp
// Before
Q_ASSERT(m_view);

// After
if (Q_UNLIKELY(!m_view)) {
    qCWarning(lcApp) << "Internal error: view is null";
    return;
}
```

全ての `Q_ASSERT` を置換するのではなく、外部入力やユーザー操作に起因しうる箇所のみ対象とする。純粋な内部ロジックの不変条件は `Q_ASSERT` のままで問題ない。

## 制約

- 既存の正常系動作が変わらないこと。
- 既存テストが全て通ること。
- ErrorBus の使用は既存パターンに合わせる。

## 受け入れ条件

- [ ] SFEN パース関数が `std::optional` を返すようになっている。
- [ ] 外部入力の文字列→整数変換にバリデーションが入っている。
- [ ] リリースビルドで有効なチェックが必要な `Q_ASSERT` が置換されている。
- [ ] ビルド成功、既存テストが通る。
