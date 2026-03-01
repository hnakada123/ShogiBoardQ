/// @file fmvlegalcore.cpp
/// @brief 合法手判定コアの実装（合法性判定）

#include "fmvlegalcore.h"

#include "fmvattacks.h"
#include "fmvlegalcore_internal.h"

#include <cstdlib>

namespace fmv {

using detail::charToPieceType;
using detail::givesDirectPawnCheck;
using detail::isDropDeadSquare;
using detail::isMandatoryPromotion;
using detail::isPromotable;
using detail::isPromotionZone;

namespace {

constexpr char kEmpty = ' ';

} // namespace

// ---- LegalCore public ----

LegalMoveStatus LegalCore::checkMove(EnginePosition& pos, Color side, const Move& candidate) const
{
    bool ok = isLegalAfterDoUndo(pos, side, candidate);
    return LegalMoveStatus(ok, false);
}

int LegalCore::countChecksToKing(const EnginePosition& pos, Color side) const
{
    Square king = pos.kingSq[static_cast<int>(side)];
    if (king == kInvalidSquare) {
        return 0;
    }
    return attackersTo(pos, king, opposite(side)).count();
}

bool LegalCore::tryApplyLegalMove(EnginePosition& pos, Color side, const Move& m, UndoState& undo) const
{
    if (!isPseudoLegal(pos, side, m)) {
        return false;
    }
    if (!pos.doMove(m, side, undo)) {
        return false;
    }
    if (ownKingInCheck(pos, side)) {
        pos.undoMove(undo, side);
        return false;
    }
    return true;
}

void LegalCore::undoAppliedMove(EnginePosition& pos, Color side, const UndoState& undo) const
{
    pos.undoMove(undo, side);
}

// ---- LegalCore private ----

bool LegalCore::isPseudoLegal(const EnginePosition& pos, Color side, const Move& m) const
{
    int ci = static_cast<int>(side);

    if (m.to >= kSquareNb) {
        return false;
    }

    // 移動先に味方駒があれば不可
    if (pos.colorOcc[ci].test(m.to)) {
        return false;
    }

    int toRank = squareRank(m.to);
    int toFile = squareFile(m.to);

    if (m.kind == MoveKind::Drop) {
        if (m.promote) {
            return false;
        }
        // 空きマスのみ
        if (pos.occupied.test(m.to)) {
            return false;
        }

        // 持ち駒があるか
        HandType ht = HandType::HandTypeNb;
        switch (m.piece) {
        case PieceType::Pawn:   ht = HandType::Pawn;   break;
        case PieceType::Lance:  ht = HandType::Lance;  break;
        case PieceType::Knight: ht = HandType::Knight; break;
        case PieceType::Silver: ht = HandType::Silver; break;
        case PieceType::Gold:   ht = HandType::Gold;   break;
        case PieceType::Bishop: ht = HandType::Bishop; break;
        case PieceType::Rook:   ht = HandType::Rook;   break;
        default: return false;
        }
        if (pos.hand[ci][static_cast<std::size_t>(ht)] <= 0) {
            return false;
        }

        // 二歩
        if (m.piece == PieceType::Pawn && detail::hasPawnOnFile(pos, side, toFile)) {
            return false;
        }

        // 行き所なし
        if (isDropDeadSquare(side, m.piece, toRank)) {
            return false;
        }

        return true;
    }

    // 盤上移動
    if (m.from >= kSquareNb) {
        return false;
    }

    // from に自分の駒があるか
    char fromChar = pos.board[m.from];
    if (fromChar == kEmpty) {
        return false;
    }
    if (!pos.colorOcc[ci].test(m.from)) {
        return false;
    }

    // 駒種チェック
    PieceType fromType = charToPieceType(fromChar);
    if (fromType != m.piece) {
        return false;
    }

    // 実際に利きがあるかチェック（attacksSquare相当）
    // 簡易版: from→toの利きチェック
    int fromFile = squareFile(m.from);
    int fromRank = squareRank(m.from);
    int df = toFile - fromFile;
    int dr = toRank - fromRank;
    int forward = (side == Color::Black) ? -1 : 1;

    bool reachable = false;

    switch (m.piece) {
    case PieceType::Pawn:
        reachable = (df == 0 && dr == forward);
        break;
    case PieceType::Lance: {
        if (df != 0) break;
        if ((side == Color::Black && dr >= 0) || (side == Color::White && dr <= 0)) break;
        // パスチェック
        reachable = true;
        int step = (dr > 0) ? 1 : -1;
        for (int r = fromRank + step; r != toRank; r += step) {
            if (pos.board[static_cast<std::size_t>(toSquare(fromFile, r))] != kEmpty) {
                reachable = false;
                break;
            }
        }
        break;
    }
    case PieceType::Knight:
        reachable = (dr == 2 * forward && (df == -1 || df == 1));
        break;
    case PieceType::Silver:
        reachable = (dr == forward && df >= -1 && df <= 1)
                    || (dr == -forward && (df == -1 || df == 1));
        break;
    case PieceType::Gold:
    case PieceType::ProPawn:
    case PieceType::ProLance:
    case PieceType::ProKnight:
    case PieceType::ProSilver:
        reachable = (dr == forward && df >= -1 && df <= 1)
                    || (dr == 0 && (df == -1 || df == 1))
                    || (dr == -forward && df == 0);
        break;
    case PieceType::King:
        reachable = (df >= -1 && df <= 1 && dr >= -1 && dr <= 1 && !(df == 0 && dr == 0));
        break;
    case PieceType::Bishop: {
        if (std::abs(df) != std::abs(dr) || df == 0) break;
        reachable = true;
        int stepF = (df > 0) ? 1 : -1;
        int stepR = (dr > 0) ? 1 : -1;
        int f = fromFile + stepF;
        int r = fromRank + stepR;
        while (f != toFile || r != toRank) {
            if (pos.board[static_cast<std::size_t>(toSquare(f, r))] != kEmpty) {
                reachable = false;
                break;
            }
            f += stepF;
            r += stepR;
        }
        break;
    }
    case PieceType::Rook: {
        if (df == 0 && dr != 0) {
            reachable = true;
            int step = (dr > 0) ? 1 : -1;
            for (int r = fromRank + step; r != toRank; r += step) {
                if (pos.board[static_cast<std::size_t>(toSquare(fromFile, r))] != kEmpty) {
                    reachable = false;
                    break;
                }
            }
        } else if (dr == 0 && df != 0) {
            reachable = true;
            int step = (df > 0) ? 1 : -1;
            for (int f = fromFile + step; f != toFile; f += step) {
                if (pos.board[static_cast<std::size_t>(toSquare(f, fromRank))] != kEmpty) {
                    reachable = false;
                    break;
                }
            }
        }
        break;
    }
    case PieceType::Horse: {
        // 斜め走り + 十字ステップ
        if (std::abs(df) == std::abs(dr) && df != 0) {
            reachable = true;
            int stepF = (df > 0) ? 1 : -1;
            int stepR = (dr > 0) ? 1 : -1;
            int f = fromFile + stepF;
            int r = fromRank + stepR;
            while (f != toFile || r != toRank) {
                if (pos.board[static_cast<std::size_t>(toSquare(f, r))] != kEmpty) {
                    reachable = false;
                    break;
                }
                f += stepF;
                r += stepR;
            }
        } else {
            reachable = (std::abs(df) + std::abs(dr) == 1);
        }
        break;
    }
    case PieceType::Dragon: {
        // 縦横走り + 斜めステップ
        if (df == 0 && dr != 0) {
            reachable = true;
            int step = (dr > 0) ? 1 : -1;
            for (int r = fromRank + step; r != toRank; r += step) {
                if (pos.board[static_cast<std::size_t>(toSquare(fromFile, r))] != kEmpty) {
                    reachable = false;
                    break;
                }
            }
        } else if (dr == 0 && df != 0) {
            reachable = true;
            int step = (df > 0) ? 1 : -1;
            for (int f = fromFile + step; f != toFile; f += step) {
                if (pos.board[static_cast<std::size_t>(toSquare(f, fromRank))] != kEmpty) {
                    reachable = false;
                    break;
                }
            }
        } else {
            reachable = (std::abs(df) == 1 && std::abs(dr) == 1);
        }
        break;
    }
    default:
        break;
    }

    if (!reachable) {
        return false;
    }

    // 成り可否チェック
    if (m.promote) {
        return isPromotable(m.piece)
               && (isPromotionZone(side, fromRank) || isPromotionZone(side, toRank));
    }

    // 強制成り
    if (isMandatoryPromotion(side, m.piece, toRank)) {
        return false;
    }

    return true;
}

bool LegalCore::ownKingInCheck(const EnginePosition& pos, Color side) const
{
    Square king = pos.kingSq[static_cast<int>(side)];
    if (king == kInvalidSquare) {
        return false;
    }
    return attackersTo(pos, king, opposite(side)).any();
}

bool LegalCore::isLegalAfterDoUndo(EnginePosition& pos, Color side, const Move& m) const
{
    UndoState undo;
    if (!tryApplyLegalMove(pos, side, m, undo)) {
        return false;
    }
    pos.undoMove(undo, side);
    return true;
}

bool LegalCore::isPawnDropMate(EnginePosition& pos, Color side, const Move& m) const
{
    if (m.kind != MoveKind::Drop || m.piece != PieceType::Pawn) {
        return false;
    }

    // doMoveした後の状態で呼ばれる想定
    if (!givesDirectPawnCheck(pos, side, m)) {
        return false;
    }

    Color opponent = opposite(side);
    return !hasAnyLegalMove(pos, opponent);
}

} // namespace fmv
