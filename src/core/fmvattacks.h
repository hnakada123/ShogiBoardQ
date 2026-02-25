#ifndef FMVATTACKS_H
#define FMVATTACKS_H

/// @file fmvattacks.h
/// @brief 利き計算（attackersTo）

#include "fmvbitboard81.h"
#include "fmvposition.h"
#include "fmvtypes.h"

namespace fmv {

/// 指定マスsqを攻撃しているattacker色の駒のビットボードを返す
Bitboard81 attackersTo(const EnginePosition& pos, Square sq, Color attacker);

} // namespace fmv

#endif // FMVATTACKS_H
