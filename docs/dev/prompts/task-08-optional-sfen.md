# Task 8: std::optional 導入（SFEN パース）

`ShogiBoard` の SFEN パース結果を `std::optional` で返すように変更してください。

## 前提

Task 5（Turn enum）が完了していること。

## 作業内容

### Step 1: SfenComponents 構造体の定義

`src/core/shogiboard.h`（または `src/core/shogitypes.h`）に追加:

```cpp
struct SfenComponents {
    QString board;     // 盤面部分
    QString stand;     // 駒台部分
    Turn turn;         // 手番
    int moveNumber;    // 手数
};
```

### Step 2: parseSfen() の新設

`src/core/shogiboard.h/.cpp` に追加:

```cpp
static std::optional<SfenComponents> parseSfen(const QString& sfenStr);
```

- 既存の `validateSfenString()` のバリデーションロジックを `parseSfen()` に移行
- パース失敗時は `std::nullopt` を返す
- qCWarning でエラーログ出力は維持

### Step 3: 既存 API の互換維持

- `validateSfenString()` は `parseSfen()` を呼び出すラッパーとして残す
- 既存の呼び出し元は段階的に新 API に移行（同一 Task 内で全て移行しなくてもよい）

### Step 4: setSfenStr() の改善

- `setSfenStr()` 内部で `parseSfen()` を使用するように変更
- パース失敗時の挙動は現状通り（ログ出力 + false 返却）

## 制約

- `#include <optional>` を追加
- CLAUDE.md のコードスタイルに準拠
- 全既存テストが通ること
