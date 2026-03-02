# Task 02: `<iostream>` のコアヘッダ汚染除去（P1 / §2.2）

## 概要

`src/core/shogimove.h` が `<iostream>` をインクルードしており、プロジェクト全体のビルド時間に悪影響。実際の使用箇所はゼロのため削除する。

## 現状

```cpp
// src/core/shogimove.h:8-10
#include <iostream>  // ← 不要
#include <QVector>   // ← Qt6では QList に統一すべき（Task 06で対応）
#include <QDebug>
```

`friend std::ostream& operator<<(std::ostream& os, const ShogiMove& move);` が宣言されているが、使用箇所ゼロ（QDebug のみ使用）。

## 手順

### Step 1: 使用状況の最終確認

1. `std::ostream` と `operator<<` の `ShogiMove` に関する使用箇所をプロジェクト全体で検索する
2. `std::cout` や `std::cerr` 等で `ShogiMove` を出力している箇所がないことを確認する

### Step 2: 削除

1. `src/core/shogimove.h` から以下を削除:
   - `#include <iostream>`
   - `friend std::ostream& operator<<(std::ostream& os, const ShogiMove& move);`
2. `src/core/shogimove.cpp` から `std::ostream& operator<<` の実装を削除（存在する場合）

### Step 3: ビルド確認

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認

## 注意事項

- `shogimove.h` はプロジェクト全体から広く参照されるため、この変更だけでビルド時間短縮効果がある
- `#include <QVector>` の `<QList>` への移行は Task 06 で対応するため、ここでは触れない
