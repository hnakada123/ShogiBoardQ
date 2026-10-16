/// @file sfenreplay.cpp
/// @brief 局面に USI の1手を指す処理の実装

#include "sfenreplay.h"

#include "usimove.h"

namespace SfenReplay {

bool applyLenient(Position& position, const QString& usiMove)
{
    const auto move = UsiMove::parse(usiMove);
    return move && position.apply(*move, Position::DropRule::Lenient);
}

QString positionAfter(const QString& sfen, const QString& usiMove)
{
    std::optional<Position> position = Position::fromSfen(sfen);
    if (!position || !applyLenient(*position, usiMove)) return {};
    return position->toSfen();
}

} // namespace SfenReplay
