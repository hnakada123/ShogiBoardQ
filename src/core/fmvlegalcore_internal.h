#ifndef FMVLEGALCORE_INTERNAL_H
#define FMVLEGALCORE_INTERNAL_H

/// @file fmvlegalcore_internal.h
/// @brief 合法手判定 内部ヘルパー（分割ファイル間で共有）

#include "fmvposition.h"
#include "fmvtypes.h"

#include "piecerules.h"

namespace fmv {
namespace detail {

/// 成り可能な駒種か
inline bool isPromotable(PieceType pt) noexcept
{
    return isPromotableType(pt);
}

/// 成り段か（段は0始まり）
inline bool isPromotionZone(Color side, int rank) noexcept
{
    return PieceRules::inPromotionZone(side == Color::Black ? Turn::Black : Turn::White, rank + 1);
}

/// 行き所のない段か（段は0始まり）。盤上の移動では強制成り、駒打ちでは打てない段
inline bool isDeadSquare(Color side, PieceType pt, int rank) noexcept
{
    return PieceRules::isDeadSquare(pieceOf(side, pt), rank + 1);
}

/// 指定色の指定筋に歩があるか
inline bool hasPawnOnFile(const EnginePosition& pos, Color side, int file) noexcept
{
    int ci = static_cast<int>(side);
    Bitboard81 pawnBB = pos.pieceOcc[ci][static_cast<int>(PieceType::Pawn)];
    while (pawnBB.any()) {
        Square sq = pawnBB.popFirst();
        if (squareFile(sq) == file) {
            return true;
        }
    }
    return false;
}

/// マスの駒の種類（空きマスは PieceTypeNb）
inline PieceType typeAt(const EnginePosition& pos, Square sq) noexcept
{
    const Piece piece = pos.board[sq];
    return piece == Piece::None ? PieceType::PieceTypeNb : typeOf(piece);
}

/// 歩打ちで直接王手になるか
inline bool givesDirectPawnCheck(const EnginePosition& pos, Color side, const Move& m) noexcept
{
    if (m.kind != MoveKind::Drop || m.piece != PieceType::Pawn) {
        return false;
    }

    Color opponent = opposite(side);
    Square opKing = pos.kingSq[static_cast<int>(opponent)];
    if (opKing == kInvalidSquare) {
        return false;
    }

    int forward = (side == Color::Black) ? -1 : 1;
    int pawnFile = squareFile(m.to);
    int pawnRank = squareRank(m.to);
    int expectedKingRank = pawnRank + forward;

    return squareFile(opKing) == pawnFile && squareRank(opKing) == expectedKingRank;
}

} // namespace detail
} // namespace fmv

#endif // FMVLEGALCORE_INTERNAL_H
