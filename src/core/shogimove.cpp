/// @file shogimove.cpp
/// @brief 指した手の記録のデバッグ出力

#include "shogimove.h"

QDebug operator<<(QDebug dbg, const ShogiMove& move)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "ShogiMove(" << move.move.toString()
                  << " moving=" << move.movingPiece
                  << " captured=" << move.capturedPiece << ")";
    return dbg;
}
