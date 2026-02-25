/// @file fmvbitboard81.cpp
/// @brief 81ビットビットボードの実装

#include "fmvbitboard81.h"

namespace fmv {

void Bitboard81::set(Square sq) noexcept
{
    if (sq < 64) {
        lo |= (1ULL << sq);
    } else {
        hi |= (1ULL << (sq - 64));
    }
}

void Bitboard81::clear(Square sq) noexcept
{
    if (sq < 64) {
        lo &= ~(1ULL << sq);
    } else {
        hi &= ~(1ULL << (sq - 64));
    }
}

bool Bitboard81::test(Square sq) const noexcept
{
    if (sq < 64) {
        return (lo & (1ULL << sq)) != 0ULL;
    }
    return (hi & (1ULL << (sq - 64))) != 0ULL;
}

int Bitboard81::count() const noexcept
{
    return __builtin_popcountll(lo) + __builtin_popcountll(hi);
}

Square Bitboard81::popFirst() noexcept
{
    if (lo != 0ULL) {
        int bit = __builtin_ctzll(lo);
        lo &= lo - 1;
        return static_cast<Square>(bit);
    }
    if (hi != 0ULL) {
        int bit = __builtin_ctzll(hi);
        hi &= hi - 1;
        return static_cast<Square>(bit + 64);
    }
    return kInvalidSquare;
}

Bitboard81 Bitboard81::operator&(const Bitboard81& rhs) const noexcept
{
    return {lo & rhs.lo, hi & rhs.hi};
}

Bitboard81 Bitboard81::operator|(const Bitboard81& rhs) const noexcept
{
    return {lo | rhs.lo, hi | rhs.hi};
}

Bitboard81 Bitboard81::operator~() const noexcept
{
    // hi の上位ビット（17ビットのみ使用）をマスク
    constexpr std::uint64_t kHiMask = (1ULL << 17) - 1;
    return {~lo, (~hi) & kHiMask};
}

Bitboard81& Bitboard81::operator|=(const Bitboard81& rhs) noexcept
{
    lo |= rhs.lo;
    hi |= rhs.hi;
    return *this;
}

Bitboard81& Bitboard81::operator&=(const Bitboard81& rhs) noexcept
{
    lo &= rhs.lo;
    hi &= rhs.hi;
    return *this;
}

bool Bitboard81::operator==(const Bitboard81& rhs) const noexcept
{
    return lo == rhs.lo && hi == rhs.hi;
}

bool Bitboard81::operator!=(const Bitboard81& rhs) const noexcept
{
    return !(*this == rhs);
}

Bitboard81 Bitboard81::squareBit(Square sq) noexcept
{
    Bitboard81 bb;
    bb.set(sq);
    return bb;
}

} // namespace fmv
