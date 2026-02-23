# Task 9: フォーマット変換の入力バリデーション強化

`src/kifu/formats/usitosfenconverter.cpp` を中心に、文字→数値変換のバリデーションを追加してください。

## 背景

現在、USI 変換器で `usi.at(0).toLatin1() - '0'` のような変換を文字種チェックなしで行っている箇所がある。不正な入力に対して未定義の値が生成される可能性がある。

## 作業内容

### Step 1: バリデーションヘルパーの作成

`src/kifu/formats/` 内の適切な場所（既存ファイルの無名名前空間、または `notationutils.h`）に追加:

```cpp
// 将棋盤の列文字（'1'〜'9'）を数値に変換
std::optional<int> parseFileChar(QChar ch);

// 将棋盤の段文字（'a'〜'i'）を数値に変換
std::optional<int> parseRankChar(QChar ch);

// 数字文字を数値に変換
std::optional<int> parseDigit(QChar ch);
```

### Step 2: usitosfenconverter.cpp の修正

- `int fromFile = usi.at(0).toLatin1() - '0'` → `parseFileChar()` を使用
- 変換失敗時はエラーログ出力 + 空文字列返却（既存のエラーパターンに合わせる）
- 全ての文字→数値変換箇所に適用

### Step 3: 他の変換器の確認

- `csatosfenconverter.cpp` にも同様のバリデーション不足がないか確認
- 発見した場合は同様に修正

### Step 4: テストの確認

- Task 3 で追加したテスト（不正入力テスト）が通ること
- 既存テストが通ること

## 制約

- CLAUDE.md のコードスタイルに準拠
- `#include <optional>` を追加
- エラー時の挙動は既存のパターン（qCWarning + 空値返却）に合わせる
