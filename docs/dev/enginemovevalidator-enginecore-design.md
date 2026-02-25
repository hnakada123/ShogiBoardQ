# EngineMoveValidator Context方式設計（src/core 実装骨格）

## 1. 目的

- `FastMoveValidator` と同一シグネチャの互換APIを `EngineMoveValidator` として提供する。
- 高速経路は **Context方式を主経路** とし、局面の再構築を最小化する。
- 内部をエンジン型（bitboard + make/unmake + 増分更新）に置き換える。

## 2. Context方式とは

Context方式は「毎回 `QVector/QMap` から局面を再構築する」のではなく、以下を1つのコンテキストとして保持し、連続判定に再利用する方式である。

- 内部局面 (`EnginePosition`)
- 手番 (`Turn`)
- make/unmake 用の履歴 (`UndoState` スタック)
- 同期状態 (`synced` フラグ、任意で世代番号)

この方式では、局面が変わらない間は `syncContext()` を再実行せず、`isLegalMove()` / `generateLegalMoves()` / `checkIfKingInCheck()` を繰り返し呼べる。

## 3. API方針（Context主経路）

### 3.1 主経路API（新規）

```cpp
class EngineMoveValidator
{
public:
    enum Turn { BLACK, WHITE, TURN_SIZE };

    struct Context;

    // Context初期同期（QVector/QMap -> 内部局面）
    bool syncContext(Context& ctx,
                     const Turn& turn,
                     const QVector<Piece>& boardData,
                     const QMap<Piece, int>& pieceStand) const;

    // Contextに対する高速判定
    LegalMoveStatus isLegalMove(Context& ctx, ShogiMove& move) const;
    int generateLegalMoves(Context& ctx) const;
    int checkIfKingInCheck(Context& ctx) const;

    // 任意: GUI側で手を進める場合の増分適用API（高速化用）
    bool tryApplyMove(Context& ctx, ShogiMove& move) const;
    bool undoLastMove(Context& ctx) const;
};
```

### 3.2 互換API（ラッパ）

互換APIは残すが、内部では毎回 `Context` を一時生成して主経路APIに委譲する。

```cpp
LegalMoveStatus isLegalMove(const Turn& turn,
                            const QVector<Piece>& boardData,
                            const QMap<Piece, int>& pieceStand,
                            ShogiMove& currentMove) const;

int generateLegalMoves(const Turn& turn,
                       const QVector<Piece>& boardData,
                       const QMap<Piece, int>& pieceStand) const;

int checkIfKingInCheck(const Turn& turn,
                       const QVector<Piece>& boardData) const;
```

## 4. src/core ファイル分割

```text
src/core/
  enginemovevalidator.h                 # 公開API（Context主経路 + 互換ラッパ）
  enginemovevalidator.cpp               # ラッパ/委譲/Context操作

  fmvtypes.h                            # 内部enum/定数/Move/MoveList
  fmvbitboard81.h
  fmvbitboard81.cpp

  fmvposition.h                         # EnginePosition/UndoState
  fmvposition.cpp                       # do/undo・差分更新

  fmvattacks.h
  fmvattacks.cpp                        # attackersTo / step/ray利き

  fmvlegalcore.h                        # Context上で動く合法判定コア
  fmvlegalcore.cpp

  fmvconverter.h                        # Qt型 <-> 内部型 変換（境界専用）
  fmvconverter.cpp
```

## 5. 責務分割

| ファイル | 主要責務 | 備考 |
|---|---|---|
| `enginemovevalidator.*` | 公開API、Contextライフサイクル、互換ラッパ | 高速経路の入口 |
| `fmvtypes.*` | 内部型、盤座標、MoveList | 依存最小 |
| `fmvbitboard81.*` | 81bit演算 | 単体テスト対象 |
| `fmvposition.*` | 局面状態、do/undo、ハッシュ | Contextの中核 |
| `fmvattacks.*` | 利き計算、attackersTo | 王手判定基盤 |
| `fmvlegalcore.*` | 合法判定・合法手生成・打ち歩詰め | Contextを入力に動作 |
| `fmvconverter.*` | `QVector/QMap/ShogiMove` 境界変換 | 互換APIと初回同期のみ使用 |

## 6. ヘッダ骨格

### 6.1 `src/core/enginemovevalidator.h`

```cpp
#ifndef ENGINEMOVEVALIDATOR_H
#define ENGINEMOVEVALIDATOR_H

#include <QMap>
#include <QVector>

#include "legalmovestatus.h"
#include "shogimove.h"
#include "shogitypes.h"
#include "fmvposition.h"

class EngineMoveValidator
{
public:
    enum Turn {
        BLACK,
        WHITE,
        TURN_SIZE
    };

    struct Context {
        Turn turn = BLACK;
        bool synced = false;
        fmv::EnginePosition pos{};
        static constexpr int kUndoMax = 512;
        std::array<fmv::UndoState, kUndoMax> undoStack{};
        int undoSize = 0;
    };

    // ---- Context主経路 ----
    bool syncContext(Context& ctx,
                     const Turn& turn,
                     const QVector<Piece>& boardData,
                     const QMap<Piece, int>& pieceStand) const;

    LegalMoveStatus isLegalMove(Context& ctx, ShogiMove& move) const;
    int generateLegalMoves(Context& ctx) const;
    int checkIfKingInCheck(Context& ctx) const;

    bool tryApplyMove(Context& ctx, ShogiMove& move) const;
    bool undoLastMove(Context& ctx) const;

    // ---- 互換ラッパ ----
    LegalMoveStatus isLegalMove(const Turn& turn,
                                const QVector<Piece>& boardData,
                                const QMap<Piece, int>& pieceStand,
                                ShogiMove& currentMove) const;

    int generateLegalMoves(const Turn& turn,
                           const QVector<Piece>& boardData,
                           const QMap<Piece, int>& pieceStand) const;

    int checkIfKingInCheck(const Turn& turn,
                           const QVector<Piece>& boardData) const;
};

#endif // ENGINEMOVEVALIDATOR_H
```

### 6.2 `src/core/fmvtypes.h`

```cpp
#ifndef FMVTYPES_H
#define FMVTYPES_H

#include <array>
#include <cstdint>

namespace fmv {

enum class Color : std::uint8_t { Black = 0, White = 1, ColorNb = 2 };
enum class MoveKind : std::uint8_t { Board = 0, Drop = 1 };
enum class PieceType : std::uint8_t {
    Pawn = 0, Lance, Knight, Silver, Gold, Bishop, Rook, King,
    ProPawn, ProLance, ProKnight, ProSilver, Horse, Dragon,
    PieceTypeNb
};
enum class HandType : std::uint8_t { Pawn = 0, Lance, Knight, Silver, Gold, Bishop, Rook, HandTypeNb };

using Square = std::uint8_t;
constexpr Square kInvalidSquare = 0xFF;
constexpr int kBoardSize = 9;
constexpr int kSquareNb = 81;

struct Move {
    Square from = kInvalidSquare;
    Square to = kInvalidSquare;
    PieceType piece = PieceType::Pawn;
    MoveKind kind = MoveKind::Board;
    bool promote = false;
};

struct MoveList {
    static constexpr int kMaxMoves = 600;
    std::array<Move, kMaxMoves> moves{};
    int size = 0;
    void clear() noexcept { size = 0; }
    bool push(const Move& m) noexcept;
};

Color opposite(Color c) noexcept;
int squareFile(Square sq) noexcept;
int squareRank(Square sq) noexcept;
Square toSquare(int file, int rank) noexcept;

} // namespace fmv

#endif // FMVTYPES_H
```

### 6.3 `src/core/fmvposition.h`

```cpp
#ifndef FMVPOSITION_H
#define FMVPOSITION_H

#include <array>
#include <cstdint>

#include "fmvbitboard81.h"
#include "fmvtypes.h"

namespace fmv {

struct UndoState {
    Move move{};
    char movedPieceBefore = ' ';
    char capturedPiece = ' ';
    Square kingSquareBefore[2]{kInvalidSquare, kInvalidSquare};
    std::uint64_t zobristBefore = 0ULL;
};

class EnginePosition
{
public:
    std::array<char, kSquareNb> board{};
    Bitboard81 occupied{};
    Bitboard81 colorOcc[2]{};
    Bitboard81 pieceOcc[2][static_cast<int>(PieceType::PieceTypeNb)]{};
    Square kingSq[2]{kInvalidSquare, kInvalidSquare};
    std::array<int, static_cast<int>(HandType::HandTypeNb)> hand[2]{};
    std::uint64_t zobristKey = 0ULL;

    void clear() noexcept;
    void rebuildBitboards() noexcept;

    bool doMove(const Move& move, Color side, UndoState& undo) noexcept;
    void undoMove(const UndoState& undo, Color side) noexcept;
};

} // namespace fmv

#endif // FMVPOSITION_H
```

### 6.4 `src/core/fmvlegalcore.h`

```cpp
#ifndef FMVLEGALCORE_H
#define FMVLEGALCORE_H

#include "legalmovestatus.h"
#include "fmvposition.h"
#include "fmvtypes.h"

namespace fmv {

class LegalCore
{
public:
    LegalCore() = default;

    // Context主経路
    LegalMoveStatus checkMove(EnginePosition& pos, Color side, const Move& candidate) const;
    int countLegalMoves(EnginePosition& pos, Color side) const;
    int countChecksToKing(const EnginePosition& pos, Color side) const;

    bool tryApplyLegalMove(EnginePosition& pos, Color side, const Move& m, UndoState& undo) const;
    void undoAppliedMove(EnginePosition& pos, Color side, const UndoState& undo) const;

private:
    bool isPseudoLegal(const EnginePosition& pos, Color side, const Move& m) const;
    bool ownKingInCheck(const EnginePosition& pos, Color side) const;
    bool isLegalAfterDoUndo(EnginePosition& pos, Color side, const Move& m) const;

    int generateEvasionMoves(const EnginePosition& pos, Color side, MoveList& out) const;
    int generateNonEvasionMoves(const EnginePosition& pos, Color side, MoveList& out) const;
    int generateDropMoves(const EnginePosition& pos, Color side, MoveList& out) const;

    bool isPawnDropMate(EnginePosition& pos, Color side, const Move& m) const;
    bool hasAnyLegalMove(EnginePosition& pos, Color side) const;
};

} // namespace fmv

#endif // FMVLEGALCORE_H
```

### 6.5 `src/core/fmvconverter.h`

```cpp
#ifndef FMVCONVERTER_H
#define FMVCONVERTER_H

#include <QMap>
#include <QVector>

#include "enginemovevalidator.h"
#include "fmvposition.h"
#include "fmvtypes.h"
#include "shogimove.h"

namespace fmv {

struct ConvertedMove {
    Move nonPromote{};
    Move promote{};
    bool hasPromoteVariant = false;
};

class Converter
{
public:
    // 境界変換（初回同期/再同期時のみ）
    static bool toEnginePosition(EnginePosition& out,
                                 const QVector<Piece>& boardData,
                                 const QMap<Piece, int>& pieceStand);

    // ShogiMove -> 内部Move
    static bool toEngineMove(ConvertedMove& out,
                             const EngineMoveValidator::Turn& turn,
                             const EnginePosition& pos,
                             const ShogiMove& in);

    static Color toColor(const EngineMoveValidator::Turn& t) noexcept;
};

} // namespace fmv

#endif // FMVCONVERTER_H
```

## 7. CPP骨格

### 7.1 `src/core/enginemovevalidator.cpp`

```cpp
#include "enginemovevalidator.h"

#include "fmvconverter.h"
#include "fmvlegalcore.h"

namespace {
fmv::LegalCore& legalCore()
{
    static fmv::LegalCore core;
    return core;
}
}

bool EngineMoveValidator::syncContext(Context& ctx,
                                      const Turn& turn,
                                      const QVector<Piece>& boardData,
                                      const QMap<Piece, int>& pieceStand) const
{
    ctx.turn = turn;
    ctx.undoSize = 0;
    ctx.synced = fmv::Converter::toEnginePosition(ctx.pos, boardData, pieceStand);
    return ctx.synced;
}

LegalMoveStatus EngineMoveValidator::isLegalMove(Context& ctx, ShogiMove& move) const
{
    if (!ctx.synced) {
        return LegalMoveStatus(false, false);
    }

    fmv::ConvertedMove cm;
    if (!fmv::Converter::toEngineMove(cm, ctx.turn, ctx.pos, move)) {
        return LegalMoveStatus(false, false);
    }

    fmv::EnginePosition work = ctx.pos;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);

    bool nonPromoting = legalCore().checkMove(work, side, cm.nonPromote).nonPromotingMoveExists;
    bool promoting = false;
    if (cm.hasPromoteVariant) {
        promoting = legalCore().checkMove(work, side, cm.promote).nonPromotingMoveExists;
    }
    return LegalMoveStatus(nonPromoting, promoting);
}

int EngineMoveValidator::generateLegalMoves(Context& ctx) const
{
    if (!ctx.synced) {
        return 0;
    }
    fmv::EnginePosition work = ctx.pos;
    return legalCore().countLegalMoves(work, fmv::Converter::toColor(ctx.turn));
}

int EngineMoveValidator::checkIfKingInCheck(Context& ctx) const
{
    if (!ctx.synced) {
        return 0;
    }
    return legalCore().countChecksToKing(ctx.pos, fmv::Converter::toColor(ctx.turn));
}

bool EngineMoveValidator::tryApplyMove(Context& ctx, ShogiMove& move) const
{
    if (!ctx.synced || ctx.undoSize >= Context::kUndoMax) {
        return false;
    }

    fmv::ConvertedMove cm;
    if (!fmv::Converter::toEngineMove(cm, ctx.turn, ctx.pos, move)) {
        return false;
    }

    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    fmv::UndoState undo;
    const fmv::Move selected = move.isPromotion && cm.hasPromoteVariant ? cm.promote : cm.nonPromote;
    if (!legalCore().tryApplyLegalMove(ctx.pos, side, selected, undo)) {
        return false;
    }

    ctx.undoStack[static_cast<std::size_t>(ctx.undoSize++)] = undo;
    ctx.turn = (ctx.turn == BLACK) ? WHITE : BLACK;
    return true;
}

bool EngineMoveValidator::undoLastMove(Context& ctx) const
{
    if (!ctx.synced || ctx.undoSize <= 0) {
        return false;
    }

    ctx.turn = (ctx.turn == BLACK) ? WHITE : BLACK;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    const fmv::UndoState undo = ctx.undoStack[static_cast<std::size_t>(--ctx.undoSize)];
    legalCore().undoAppliedMove(ctx.pos, side, undo);
    return true;
}

// ---- 互換ラッパ（毎回Contextを作る。低速だが互換維持）----
LegalMoveStatus EngineMoveValidator::isLegalMove(const Turn& turn,
                                                 const QVector<Piece>& boardData,
                                                 const QMap<Piece, int>& pieceStand,
                                                 ShogiMove& currentMove) const
{
    Context ctx;
    if (!syncContext(ctx, turn, boardData, pieceStand)) {
        return LegalMoveStatus(false, false);
    }
    return isLegalMove(ctx, currentMove);
}

int EngineMoveValidator::generateLegalMoves(const Turn& turn,
                                            const QVector<Piece>& boardData,
                                            const QMap<Piece, int>& pieceStand) const
{
    Context ctx;
    if (!syncContext(ctx, turn, boardData, pieceStand)) {
        return 0;
    }
    return generateLegalMoves(ctx);
}

int EngineMoveValidator::checkIfKingInCheck(const Turn& turn,
                                            const QVector<Piece>& boardData) const
{
    Context ctx;
    QMap<Piece, int> emptyStand;
    if (!syncContext(ctx, turn, boardData, emptyStand)) {
        return 0;
    }
    return checkIfKingInCheck(ctx);
}
```

### 7.2 `src/core/fmvlegalcore.cpp`

```cpp
#include "fmvlegalcore.h"

#include "fmvattacks.h"

namespace fmv {

LegalMoveStatus LegalCore::checkMove(EnginePosition& pos, Color side, const Move& candidate) const
{
    const bool ok = isLegalAfterDoUndo(pos, side, candidate);
    return LegalMoveStatus(ok, false);
}

int LegalCore::countLegalMoves(EnginePosition& pos, Color side) const
{
    MoveList pseudo;
    pseudo.clear();

    if (ownKingInCheck(pos, side)) {
        generateEvasionMoves(pos, side, pseudo);
    } else {
        generateNonEvasionMoves(pos, side, pseudo);
        generateDropMoves(pos, side, pseudo);
    }

    int legalCount = 0;
    for (int i = 0; i < pseudo.size; ++i) {
        if (isLegalAfterDoUndo(pos, side, pseudo.moves[static_cast<std::size_t>(i)])) {
            ++legalCount;
        }
    }
    return legalCount;
}

int LegalCore::countChecksToKing(const EnginePosition& pos, Color side) const
{
    const Square king = pos.kingSq[static_cast<int>(side)];
    return attackersTo(pos, king, opposite(side)).count();
}

bool LegalCore::tryApplyLegalMove(EnginePosition& pos, Color side, const Move& m, UndoState& undo) const
{
    if (!isPseudoLegal(pos, side, m)) {
        return false;
    }
    if (!pos.doMove(m, side, undo)) {
        return false;
    }
    if (ownKingInCheck(pos, side)) {
        pos.undoMove(undo, side);
        return false;
    }
    return true;
}

void LegalCore::undoAppliedMove(EnginePosition& pos, Color side, const UndoState& undo) const
{
    pos.undoMove(undo, side);
}

bool LegalCore::isLegalAfterDoUndo(EnginePosition& pos, Color side, const Move& m) const
{
    UndoState undo;
    if (!tryApplyLegalMove(pos, side, m, undo)) {
        return false;
    }
    pos.undoMove(undo, side);
    return true;
}

bool LegalCore::ownKingInCheck(const EnginePosition& pos, Color side) const
{
    const Square king = pos.kingSq[static_cast<int>(side)];
    return attackersTo(pos, king, opposite(side)).any();
}

} // namespace fmv
```

## 8. 呼び出し側の移行パターン

### 8.1 旧（互換ラッパ）

```cpp
EngineMoveValidator v;
auto st = v.isLegalMove(turn, boardData, pieceStand, move);
```

### 8.2 新（Context主経路）

```cpp
EngineMoveValidator v;
EngineMoveValidator::Context ctx;

v.syncContext(ctx, turn, boardData, pieceStand); // 局面が変わった時だけ

auto st1 = v.isLegalMove(ctx, move1);
auto st2 = v.isLegalMove(ctx, move2);
int n = v.generateLegalMoves(ctx);
int c = v.checkIfKingInCheck(ctx);
```

### 8.3 進行をContext内で管理する場合

```cpp
v.syncContext(ctx, turn, boardData, pieceStand);
if (v.tryApplyMove(ctx, move)) {
    // ctx は次局面へ進んでいる
}
v.undoLastMove(ctx);
```

## 9. 実装順序（Context方式優先）

1. `enginemovevalidator.h/.cpp` に Context主経路APIを先に追加。  
2. `fmvposition` の do/undo を完成。  
3. `fmvlegalcore` を Context前提で実装。  
4. 互換ラッパAPIを最後に接続。  
5. 高頻度箇所から順に呼び出し側を Context方式へ移行。  

## 10. テスト方針

- 既存回帰:
  - `tst_fastmovevalidator`
  - `tst_integration`
- 追加:
  - `tst_enginemovevalidator_compat`（互換ラッパ）
  - `tst_enginemovevalidator_context`（sync/連続判定/undo）
  - `tst_fmvlegalcore`
  - `tst_fmv_perft`

## 11. 注意点

- `QVector/QMap` 変換は `syncContext()` と互換ラッパAPI内だけで行う。
- Contextは局面単位で使い回し、局面変更時のみ再同期する。
- スレッド間で同一Contextを共有しない。
- UI層向けに例外は投げず、失敗時は `false/0` を返す。
