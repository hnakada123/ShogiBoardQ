/// @file sfenreplay.cpp
/// @brief 局面に USI の1手を指す処理の実装

#include "sfenreplay.h"

#include "usimove.h"

namespace SfenReplay {

QString positionAfter(const QString& sfen, const QString& usiMove)
{
    std::optional<Position> position = Position::fromSfen(sfen);
    const std::optional<UsiMove> move = UsiMove::parse(usiMove);
    if (!position || !move || !position->apply(*move, Position::DropRule::Lenient)) return {};
    return position->toSfen();
}

} // namespace SfenReplay
