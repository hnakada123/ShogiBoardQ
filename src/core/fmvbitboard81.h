#ifndef FMVBITBOARD81_H
#define FMVBITBOARD81_H

/// @file fmvbitboard81.h
/// @brief 81ビットのビットボード（将棋盤81マス用）

#include <cstdint>

#include "fmvtypes.h"

namespace fmv {

/// 81ビットビットボード（lo: bits 0-63, hi: bits 64-80）
class Bitboard81
{
public:
    std::uint64_t lo = 0ULL;
    std::uint64_t hi = 0ULL;

    Bitboard81() noexcept = default;
    Bitboard81(std::uint64_t loVal, std::uint64_t hiVal) noexcept
        : lo(loVal), hi(hiVal) {}

    void set(Square sq) noexcept;
    void clear(Square sq) noexcept;
    bool test(Square sq) const noexcept;

    int count() const noexcept;
    Square popFirst() noexcept;

    bool any() const noexcept { return lo != 0ULL || hi != 0ULL; }
    bool none() const noexcept { return !any(); }

    Bitboard81 operator&(const Bitboard81& rhs) const noexcept;
    Bitboard81 operator|(const Bitboard81& rhs) const noexcept;
    Bitboard81 operator~() const noexcept;

    Bitboard81& operator|=(const Bitboard81& rhs) noexcept;
    Bitboard81& operator&=(const Bitboard81& rhs) noexcept;

    bool operator==(const Bitboard81& rhs) const noexcept;
    bool operator!=(const Bitboard81& rhs) const noexcept;

    static Bitboard81 squareBit(Square sq) noexcept;
};

} // namespace fmv

#endif // FMVBITBOARD81_H
