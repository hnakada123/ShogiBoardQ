#ifndef FMVCONVERTER_H
#define FMVCONVERTER_H

/// @file fmvconverter.h
/// @brief Qt型 ←→ エンジン内部型の境界変換


#include "shogitypes.h"
#include "fmvposition.h"
#include "fmvtypes.h"
#include "usimove.h"

class Position;

namespace fmv {

struct ConvertedMove {
    Move nonPromote{};
    Move promote{};
    bool hasPromoteVariant = false;
};

class Converter
{
public:
    static bool toEnginePosition(EnginePosition& out, const Position& position);

    /// USI の指し手を内部の指し手にする（成りの指定は見ず、成れる手は両方を入れる）。
    /// 移動元が手番の駒でない、打つ駒を持っていない指し手は false
    static bool toEngineMove(ConvertedMove& out,
                             Turn turn,
                             const EnginePosition& pos,
                             const UsiMove& move);

    static Color toColor(Turn t) noexcept;

    /// 内部の指し手を USI の指し手にする
    static UsiMove toUsiMove(const Move& m);
};

} // namespace fmv

#endif // FMVCONVERTER_H
