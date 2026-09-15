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

    constexpr Bitboard81() noexcept = default;
    constexpr Bitboard81(std::uint64_t loVal, std::uint64_t hiVal) noexcept
        : lo(loVal), hi(hiVal) {}

    constexpr void set(Square sq) noexcept
    {
        if (sq < 64) {
            lo |= (1ULL << sq);
        } else {
            hi |= (1ULL << (sq - 64));
        }
    }

    constexpr void clear(Square sq) noexcept
    {
        if (sq < 64) {
            lo &= ~(1ULL << sq);
        } else {
            hi &= ~(1ULL << (sq - 64));
        }
    }

    constexpr bool test(Square sq) const noexcept
    {
        return sq < 64 ? (lo & (1ULL << sq)) != 0ULL
                       : (hi & (1ULL << (sq - 64))) != 0ULL;
    }

    int count() const noexcept;
    Square popFirst() noexcept;

    constexpr bool any() const noexcept { return lo != 0ULL || hi != 0ULL; }
    constexpr bool none() const noexcept { return !any(); }

    constexpr Bitboard81 operator&(const Bitboard81& rhs) const noexcept
    {
        return {lo & rhs.lo, hi & rhs.hi};
    }

    constexpr Bitboard81 operator|(const Bitboard81& rhs) const noexcept
    {
        return {lo | rhs.lo, hi | rhs.hi};
    }

    constexpr Bitboard81 operator~() const noexcept
    {
        constexpr std::uint64_t kHiMask = (1ULL << 17) - 1;
        return {~lo, (~hi) & kHiMask};
    }

    constexpr Bitboard81& operator|=(const Bitboard81& rhs) noexcept
    {
        lo |= rhs.lo;
        hi |= rhs.hi;
        return *this;
    }

    constexpr Bitboard81& operator&=(const Bitboard81& rhs) noexcept
    {
        lo &= rhs.lo;
        hi &= rhs.hi;
        return *this;
    }

    constexpr bool operator==(const Bitboard81& rhs) const noexcept
    {
        return lo == rhs.lo && hi == rhs.hi;
    }

    constexpr bool operator!=(const Bitboard81& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    static constexpr Bitboard81 squareBit(Square sq) noexcept
    {
        Bitboard81 bb;
        bb.set(sq);
        return bb;
    }
};

} // namespace fmv

#endif // FMVBITBOARD81_H
