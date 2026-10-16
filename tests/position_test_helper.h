#ifndef POSITION_TEST_HELPER_H
#define POSITION_TEST_HELPER_H

/// @file position_test_helper.h
/// @brief テスト用: 81マスの駒の並びと持ち駒の表から局面（Position）を作る

#include <QList>
#include <QMap>

#include "shogiposition.h"

namespace PositionTestHelper {

/// squares[(段 - 1) * 9 + (筋 - 1)] の駒と、駒 → 枚数の持ち駒から局面を作る
inline Position fromSquares(const QList<Piece>& squares, const QMap<Piece, int>& hands = {})
{
    Position position;
    for (qsizetype i = 0; i < squares.size() && i < Position::kSquareCount; ++i)
        position.setPieceAt({static_cast<int>(i % 9) + 1, static_cast<int>(i / 9) + 1}, squares.at(i));
    for (auto it = hands.cbegin(); it != hands.cend(); ++it) position.setHandCount(it.key(), it.value());
    return position;
}

} // namespace PositionTestHelper

#endif // POSITION_TEST_HELPER_H
