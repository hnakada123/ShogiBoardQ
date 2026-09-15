#ifndef FMVATTACKS_H
#define FMVATTACKS_H

/// @file fmvattacks.h
/// @brief 表とビット演算による利き計算

#include "fmvbitboard81.h"
#include "fmvposition.h"
#include "fmvtypes.h"

namespace fmv {

/// 指定マスsqを攻撃しているattacker色の駒のビットボードを返す
Bitboard81 attackersTo(const EnginePosition& pos, Square sq, Color attacker) noexcept;

/// 攻撃駒を1枚見つけた時点で終了する、利きの有無の判定
bool isSquareAttacked(const EnginePosition& pos, Square sq, Color attacker) noexcept;

/// 駒種・方向・障害物からfrom→toの利きを判定（成りや自玉の安全確認は含まない）
bool pieceAttacksSquare(const EnginePosition& pos, Color side, PieceType piece,
                        Square from, Square to) noexcept;

} // namespace fmv

#endif // FMVATTACKS_H
