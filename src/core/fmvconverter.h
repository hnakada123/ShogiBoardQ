#ifndef FMVCONVERTER_H
#define FMVCONVERTER_H

/// @file fmvconverter.h
/// @brief Qt型 ←→ エンジン内部型の境界変換

#include <QMap>
#include <QList>

#include "enginemovevalidator.h"
#include "fmvposition.h"
#include "fmvtypes.h"
#include "shogimove.h"
#include "usimove.h"

namespace fmv {

struct ConvertedMove {
    Move nonPromote{};
    Move promote{};
    bool hasPromoteVariant = false;
};

class Converter
{
public:
    static bool toEnginePosition(EnginePosition& out,
                                 const QList<Piece>& boardData,
                                 const QMap<Piece, int>& pieceStand);

    static bool toEngineMove(ConvertedMove& out,
                             Turn turn,
                             const EnginePosition& pos,
                             const ShogiMove& in);

    static Color toColor(Turn t) noexcept;

    /// 内部の指し手を USI の指し手にする
    static UsiMove toUsiMove(const Move& m);
};

} // namespace fmv

#endif // FMVCONVERTER_H
