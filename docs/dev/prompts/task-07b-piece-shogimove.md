# Task 7b: ShogiMove の Piece 化

`ShogiMove` 構造体の駒表現を `QChar` から `Piece` enum に変更してください。

## 前提

Task 7a で `Piece` enum と変換ユーティリティが作成済みであること。

## 作業内容

### Step 1: ShogiMove の変更

`src/core/shogimove.h`:
- `QChar movingPiece` → `Piece movingPiece = Piece::None`
- `QChar capturedPiece` → `Piece capturedPiece = Piece::None`

### Step 2: ShogiMove を使っている箇所の更新

1. `ShogiMove` を生成している全箇所を検索
2. `QChar` で駒を設定している箇所を `Piece` enum に変更
3. `QChar` として駒を読み取っている箇所を `Piece` に変更
4. SFEN 文字列との変換が必要な箇所では `pieceToChar()` / `charToPiece()` を使用

### Step 3: operator== と出力の更新

- `operator==` が `Piece` 同士の比較になるように確認
- `QDebug` 出力と `ostream` 出力を `Piece` に対応

## 制約

- CLAUDE.md のコードスタイルに準拠
- 全既存テストが通ること
- 段階的移行: この Task では `ShogiMove` のみ変更し、`ShogiBoard` は次の Task で対応
