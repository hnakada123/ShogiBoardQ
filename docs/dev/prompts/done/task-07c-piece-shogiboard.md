# Task 7c: ShogiBoard の Piece 化

`ShogiBoard` の盤面データと駒台データを `QChar` から `Piece` enum に変更してください。

## 前提

Task 7a（Piece enum 定義）と Task 7b（ShogiMove の Piece 化）が完了していること。

## 作業内容

### Step 1: ShogiBoard のメンバ変数変更

`src/core/shogiboard.h`:
- `QVector<QChar> m_boardData` → `QVector<Piece> m_boardData`
- `QMap<QChar, int> m_pieceStand` → `QMap<Piece, int> m_pieceStand`

### Step 2: 公開 API の変更

- `QChar pieceAt(int file, int rank)` → `Piece pieceAt(int file, int rank)`
- `void setPieceAt(int file, int rank, QChar piece)` → `void setPieceAt(int file, int rank, Piece piece)`
- 駒台関連メソッドの引数・戻り値を `Piece` に変更

### Step 3: SFEN パース/生成の変更

- `setSfenStr()` 内の文字→駒変換を `charToPiece()` に置換
- `toSfenStr()` 内の駒→文字変換を `pieceToChar()` に置換
- `validateAndConvertSfenBoardStr()` の更新

### Step 4: ShogiBoard を使っている箇所の更新

- `ShogiBoard::pieceAt()` の戻り値を `Piece` として処理するように更新
- `QChar(' ')` との比較を `Piece::None` に変更
- MoveValidator 等、盤面データを読む全クラスを更新

## 制約

- CLAUDE.md のコードスタイルに準拠
- 全既存テストが通ること
- 影響範囲が広いため、コンパイルエラーを一つずつ潰す方式で進行
