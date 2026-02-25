#ifndef FMVTYPES_H
#define FMVTYPES_H

/// @file fmvtypes.h
/// @brief EngineMoveValidator 内部型定義（header-only）

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

enum class HandType : std::uint8_t {
    Pawn = 0, Lance, Knight, Silver, Gold, Bishop, Rook, HandTypeNb
};

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

    bool push(const Move& m) noexcept
    {
        if (size >= kMaxMoves) {
            return false;
        }
        moves[static_cast<std::size_t>(size)] = m;
        ++size;
        return true;
    }
};

inline Color opposite(Color c) noexcept
{
    return (c == Color::Black) ? Color::White : Color::Black;
}

inline int squareFile(Square sq) noexcept
{
    return static_cast<int>(sq) % kBoardSize;
}

inline int squareRank(Square sq) noexcept
{
    return static_cast<int>(sq) / kBoardSize;
}

inline Square toSquare(int file, int rank) noexcept
{
    return static_cast<Square>(rank * kBoardSize + file);
}

} // namespace fmv

#endif // FMVTYPES_H
