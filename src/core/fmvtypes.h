#ifndef FMVTYPES_H
#define FMVTYPES_H

/// @file fmvtypes.h
/// @brief EngineMoveValidator 内部型定義（header-only）

#include <array>
#include <cstdint>

#include "shogitypes.h"

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

// 駒の種類の並びはアプリの PieceKind と同じ（盤には Piece をそのまま置く）
static_assert(static_cast<int>(PieceType::Pawn) == static_cast<int>(PieceKind::Pawn));
static_assert(static_cast<int>(PieceType::Gold) == static_cast<int>(PieceKind::Gold));
static_assert(static_cast<int>(PieceType::King) == static_cast<int>(PieceKind::King));
static_assert(static_cast<int>(PieceType::ProPawn) == static_cast<int>(PieceKind::ProPawn));
static_assert(static_cast<int>(PieceType::Dragon) == static_cast<int>(PieceKind::Dragon));
static_assert(static_cast<int>(PieceType::PieceTypeNb) == kPieceKindCount);
static_assert(static_cast<int>(HandType::Rook) == static_cast<int>(PieceKind::Rook));

/// 盤の駒の種類（None に対しては呼ばない）
constexpr PieceType typeOf(Piece piece) noexcept { return static_cast<PieceType>(pieceKind(piece)); }
/// 盤の駒の色（None に対しては呼ばない）
constexpr Color colorOf(Piece piece) noexcept { return isWhitePiece(piece) ? Color::White : Color::Black; }
/// 色と種類の駒
constexpr Piece pieceOf(Color color, PieceType type) noexcept
{
    return makePiece(color == Color::White ? Turn::White : Turn::Black, static_cast<PieceKind>(type));
}
/// 成った駒の種類（成れない駒はそのまま）
constexpr PieceType promotedType(PieceType type) noexcept { return typeOf(promote(pieceOf(Color::Black, type))); }
/// 成る前の駒の種類（生駒はそのまま）
constexpr PieceType demotedType(PieceType type) noexcept { return typeOf(demote(pieceOf(Color::Black, type))); }
/// 持ち駒の種類（成駒は元の駒。玉は HandTypeNb）
constexpr HandType handTypeOf(PieceType type) noexcept
{
    const PieceType base = demotedType(type);
    return base == PieceType::King ? HandType::HandTypeNb : static_cast<HandType>(base);
}
/// 持ち駒の種類の駒（HandTypeNb は PieceTypeNb）
constexpr PieceType pieceTypeOf(HandType hand) noexcept
{
    return hand == HandType::HandTypeNb ? PieceType::PieceTypeNb : static_cast<PieceType>(hand);
}
/// 成れる駒の種類か
constexpr bool isPromotableType(PieceType type) noexcept { return isPromotable(pieceOf(Color::Black, type)); }

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

constexpr Color opposite(Color c) noexcept
{
    return (c == Color::Black) ? Color::White : Color::Black;
}

constexpr int squareFile(Square sq) noexcept
{
    return static_cast<int>(sq) % kBoardSize;
}

constexpr int squareRank(Square sq) noexcept
{
    return static_cast<int>(sq) / kBoardSize;
}

constexpr Square toSquare(int file, int rank) noexcept
{
    return static_cast<Square>(rank * kBoardSize + file);
}

} // namespace fmv

#endif // FMVTYPES_H
