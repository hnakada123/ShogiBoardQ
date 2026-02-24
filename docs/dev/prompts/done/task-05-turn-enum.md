# Task 5: Turn enum の導入

手番を表す `QString`（`"b"` / `"w"`）を `enum class Turn` に置き換えてください。

## 背景

現在、`ShogiBoard::m_currentPlayer` は `QString` 型で `"b"`（先手）/ `"w"`（後手）を格納している。これはタイポしてもコンパイルエラーにならず、型安全性に欠ける。

## 作業内容

### Step 1: Turn enum の定義

`src/core/shogitypes.h`（新規）に以下を定義:

```cpp
#pragma once
#include <QString>

enum class Turn { Black, White };

// SFEN 文字列との相互変換
inline QString turnToSfen(Turn t) { return t == Turn::Black ? QStringLiteral("b") : QStringLiteral("w"); }
inline Turn sfenToTurn(const QString& s) { return s == QStringLiteral("b") ? Turn::Black : Turn::White; }

// 反転
inline Turn oppositeTurn(Turn t) { return t == Turn::Black ? Turn::White : Turn::Black; }
```

### Step 2: ShogiBoard の変更

- `src/core/shogiboard.h`: `QString m_currentPlayer` → `Turn m_currentPlayer = Turn::Black`
- `currentPlayer()` の戻り値を `Turn` に変更
- `setCurrentPlayer()` の引数を `Turn` に変更
- SFEN パース/生成箇所で `turnToSfen()` / `sfenToTurn()` を使用

### Step 3: TurnManager の変更

- `src/game/turnmanager.h/.cpp` を確認し、`Turn` enum を使用するように変更
- 既存の `Side`（`ShogiGameController::Player`）との変換メソッドを用意

### Step 4: 呼び出し元の更新

- `ShogiBoard::currentPlayer()` を使っている全箇所を検索し、`Turn` 型に対応
- `"b"` / `"w"` と比較している箇所を `Turn::Black` / `Turn::White` に置換
- 文字列としての手番が必要な箇所（SFEN 生成、UI 表示）では `turnToSfen()` を使用

## 制約

- CLAUDE.md のコードスタイルに準拠
- `connect()` にラムダを使わない
- signals/slots の引数型は完全修飾する（clazy-fully-qualified-moc-types）
- 全既存テストが通ること
- `lupdate src -ts resources/translations/ShogiBoardQ_ja_JP.ts resources/translations/ShogiBoardQ_en.ts` で翻訳更新
