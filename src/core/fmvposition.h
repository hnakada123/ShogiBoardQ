#ifndef FMVPOSITION_H
#define FMVPOSITION_H

/// @file fmvposition.h
/// @brief エンジン内部局面データ構造

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
    /// 盤面配列: board[rank*9+file] = 駒char（' '=空）
    std::array<char, kSquareNb> board{};

    /// 全駒の占有ビットボード
    Bitboard81 occupied{};

    /// 色別占有ビットボード
    Bitboard81 colorOcc[2]{};

    /// 色×駒種別の占有ビットボード
    Bitboard81 pieceOcc[2][static_cast<int>(PieceType::PieceTypeNb)]{};

    /// 玉の位置
    Square kingSq[2]{kInvalidSquare, kInvalidSquare};

    /// 持ち駒配列 [Color][HandType]
    std::array<int, static_cast<int>(HandType::HandTypeNb)> hand[2]{};

    /// Zobristハッシュキー（将来用、現在は0固定）
    std::uint64_t zobristKey = 0ULL;

    void clear() noexcept;
    void rebuildBitboards() noexcept;

    bool doMove(const Move& move, Color side, UndoState& undo) noexcept;
    void undoMove(const UndoState& undo, Color side) noexcept;
};

} // namespace fmv

#endif // FMVPOSITION_H
