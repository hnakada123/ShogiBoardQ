/// @file fmvbitboard81.cpp
/// @brief 81ビットビットボードの実装

#include "fmvbitboard81.h"

#include <QtCore/qalgorithms.h>

namespace fmv {

int Bitboard81::count() const noexcept
{
    return static_cast<int>(qPopulationCount(static_cast<quint64>(lo))
                            + qPopulationCount(static_cast<quint64>(hi)));
}

Square Bitboard81::popFirst() noexcept
{
    if (lo != 0ULL) {
        const int bit = static_cast<int>(qCountTrailingZeroBits(static_cast<quint64>(lo)));
        lo &= lo - 1;
        return static_cast<Square>(bit);
    }
    if (hi != 0ULL) {
        const int bit = static_cast<int>(qCountTrailingZeroBits(static_cast<quint64>(hi)));
        hi &= hi - 1;
        return static_cast<Square>(bit + 64);
    }
    return kInvalidSquare;
}

} // namespace fmv
