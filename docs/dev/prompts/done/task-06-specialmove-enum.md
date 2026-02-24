# Task 6: SpecialMove enum の導入

USI エンジンの特殊手（resign, win 等）のマジック文字列を enum に置き換えてください。

## 背景

現在、`UsiProtocolHandler` や `MatchCoordinator` で `"resign"`, `"win"` 等の特殊手を文字列比較で判定している。タイポしてもコンパイルエラーにならない。

## 作業内容

### Step 1: SpecialMove enum の定義

`src/core/shogitypes.h`（Task 5 で作成済み、または新規）に追加:

```cpp
enum class SpecialMove { None, Resign, Win, Draw };

inline SpecialMove parseSpecialMove(const QString& s) {
    if (s.compare(QStringLiteral("resign"), Qt::CaseInsensitive) == 0) return SpecialMove::Resign;
    if (s.compare(QStringLiteral("win"), Qt::CaseInsensitive) == 0) return SpecialMove::Win;
    // draw 等、必要に応じて追加
    return SpecialMove::None;
}
```

### Step 2: UsiProtocolHandler の変更

- `src/engine/usiprotocolhandler.cpp` の `handleBestMoveLine()` で、bestmove のトークンが特殊手かどうかを `parseSpecialMove()` で判定
- 文字列比較（`m_bestMove.compare(QStringLiteral("resign"), ...)` 等）を enum 比較に置換
- 必要に応じて、結果を `SpecialMove` 型で保持するメンバ変数を追加

### Step 3: MatchCoordinator の変更

- `src/game/matchcoordinator.cpp` で `"resign"` / `"win"` と比較している箇所を検索し、enum に置換

### Step 4: 他の使用箇所の更新

- コードベース全体で `"resign"`, `"win"`, `"draw"` の文字列比較を検索し、enum に統一

## 制約

- CLAUDE.md のコードスタイルに準拠
- 全既存テストが通ること
- 影響範囲が小さいため、翻訳更新は必要に応じて実施
