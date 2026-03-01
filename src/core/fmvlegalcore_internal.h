#ifndef FMVLEGALCORE_INTERNAL_H
#define FMVLEGALCORE_INTERNAL_H

/// @file fmvlegalcore_internal.h
/// @brief 合法手判定 内部ヘルパー（分割ファイル間で共有）

#include "fmvposition.h"
#include "fmvtypes.h"

#include <cctype>

namespace fmv {
namespace detail {

/// 成り可能な駒種か
inline bool isPromotable(PieceType pt) noexcept
{
    switch (pt) {
    case PieceType::Pawn:
    case PieceType::Lance:
    case PieceType::Knight:
    case PieceType::Silver:
    case PieceType::Bishop:
    case PieceType::Rook:
        return true;
    default:
        return false;
    }
}

/// 成り段か
inline bool isPromotionZone(Color side, int rank) noexcept
{
    if (side == Color::Black) {
        return rank <= 2;
    }
    return rank >= 6;
}

/// 強制成りか
inline bool isMandatoryPromotion(Color side, PieceType pt, int toRank) noexcept
{
    if (side == Color::Black) {
        if ((pt == PieceType::Pawn || pt == PieceType::Lance) && toRank == 0) {
            return true;
        }
        if (pt == PieceType::Knight && toRank <= 1) {
            return true;
        }
        return false;
    }
    if ((pt == PieceType::Pawn || pt == PieceType::Lance) && toRank == 8) {
        return true;
    }
    if (pt == PieceType::Knight && toRank >= 7) {
        return true;
    }
    return false;
}

/// 行き所のない駒の打ち判定
inline bool isDropDeadSquare(Color side, PieceType pt, int toRank) noexcept
{
    if (side == Color::Black) {
        if ((pt == PieceType::Pawn || pt == PieceType::Lance) && toRank == 0) {
            return true;
        }
        if (pt == PieceType::Knight && toRank <= 1) {
            return true;
        }
        return false;
    }
    if ((pt == PieceType::Pawn || pt == PieceType::Lance) && toRank == 8) {
        return true;
    }
    if (pt == PieceType::Knight && toRank >= 7) {
        return true;
    }
    return false;
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

/// char → PieceType
inline PieceType charToPieceType(char c) noexcept
{
    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (upper) {
    case 'P': return PieceType::Pawn;
    case 'L': return PieceType::Lance;
    case 'N': return PieceType::Knight;
    case 'S': return PieceType::Silver;
    case 'G': return PieceType::Gold;
    case 'B': return PieceType::Bishop;
    case 'R': return PieceType::Rook;
    case 'K': return PieceType::King;
    case 'Q': return PieceType::ProPawn;
    case 'M': return PieceType::ProLance;
    case 'O': return PieceType::ProKnight;
    case 'T': return PieceType::ProSilver;
    case 'C': return PieceType::Horse;
    case 'U': return PieceType::Dragon;
    default: return PieceType::PieceTypeNb;
    }
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
