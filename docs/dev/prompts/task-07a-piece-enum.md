# Task 7a: Piece enum と変換ユーティリティの作成

駒を表す `Piece` enum と `QChar` との相互変換ユーティリティを作成してください。
この段階では既存コードの変更は行わず、新しい型と変換関数のみを追加する。

## 背景

現在、駒は `QChar` で表現されている（SFEN 表記: `P`=歩, `L`=香, `N`=桂, `S`=銀, `G`=金, `B`=角, `R`=飛, `K`=玉）。小文字は後手の駒。成り駒は `+P`, `+L` 等だが内部では別文字（例: `T`=と金）で管理されている場合がある。

## 作業内容

### Step 1: Piece enum の定義

`src/core/shogitypes.h` に追加:

```cpp
enum class Piece : char {
    None = 0,
    // 先手の駒（大文字）
    Pawn = 'P', Lance = 'L', Knight = 'N', Silver = 'S',
    Gold = 'G', Bishop = 'B', Rook = 'R', King = 'K',
    // 先手の成り駒
    PromotedPawn = 'T', PromotedLance = 'U', PromotedKnight = 'M',
    PromotedSilver = 'A', Horse = 'H', Dragon = 'D',
    // 後手の駒（小文字）
    // ... 必要に応じて
};
```

**重要**: まず `src/core/shogiboard.cpp` と `src/core/shogimove.h` を読み、実際に使われている駒文字のマッピングを確認した上で enum を定義すること。内部表現が SFEN 標準と異なる場合は、それに合わせる。

### Step 2: 変換ユーティリティの作成

`src/core/shogitypes.h`（または `src/core/pieceutils.h`）に以下を追加:

```cpp
// QChar → Piece 変換（無効な文字は Piece::None）
Piece charToPiece(QChar ch);

// Piece → QChar 変換
QChar pieceToChar(Piece p);

// 成り判定・成り変換
bool isPromoted(Piece p);
Piece promote(Piece p);
Piece demote(Piece p);

// 先手/後手判定
bool isBlack(Piece p);  // 大文字 = 先手
Piece toBlack(Piece p); // 先手の駒に変換
Piece toWhite(Piece p); // 後手の駒に変換
```

### Step 3: テストの追加

`tests/tst_coredatastructures.cpp` に Piece 変換のテストを追加:

- 全駒種の QChar ↔ Piece ラウンドトリップ
- 無効な文字 → `Piece::None`
- 成り/成り解除のテスト
- 先手/後手変換のテスト

## 制約

- **既存コードは変更しない**（この Task は追加のみ）
- 全既存テストが通ること
- 後続の Task 7b〜7d で段階的に既存コードを移行する
