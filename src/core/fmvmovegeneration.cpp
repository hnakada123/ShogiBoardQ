/// @file fmvmovegeneration.cpp
/// @brief 合法手生成の実装

#include "fmvlegalcore.h"

#include "fmvattacks.h"
#include "fmvlegalcore_internal.h"

namespace fmv {

using detail::charToPieceType;
using detail::givesDirectPawnCheck;
using detail::isDropDeadSquare;
using detail::isMandatoryPromotion;
using detail::isPromotable;
using detail::isPromotionZone;

namespace {

bool inBoard(int file, int rank) noexcept
{
    return file >= 0 && file < kBoardSize && rank >= 0 && rank < kBoardSize;
}

/// HandType → 打ち駒のPieceType
PieceType handTypeToPieceType(HandType ht) noexcept
{
    switch (ht) {
    case HandType::Pawn:   return PieceType::Pawn;
    case HandType::Lance:  return PieceType::Lance;
    case HandType::Knight: return PieceType::Knight;
    case HandType::Silver: return PieceType::Silver;
    case HandType::Gold:   return PieceType::Gold;
    case HandType::Bishop: return PieceType::Bishop;
    case HandType::Rook:   return PieceType::Rook;
    default: return PieceType::PieceTypeNb;
    }
}

/// 駒の移動先候補をMoveListに追加（成り/不成の両方）
void addBoardMoves(const EnginePosition& pos, Color side,
                   Square from, PieceType pt,
                   int df, int dr, bool isRay,
                   MoveList& out) noexcept
{
    int ci = static_cast<int>(side);
    int file = squareFile(from) + df;
    int rank = squareRank(from) + dr;

    auto addOne = [&](int f, int r) {
        if (!inBoard(f, r)) {
            return false;
        }
        Square to = toSquare(f, r);
        // 味方駒がいれば不可
        if (pos.colorOcc[ci].test(to)) {
            return false;
        }

        int fromRank = squareRank(from);
        int toRank = r;
        bool canPromote = isPromotable(pt)
                          && (isPromotionZone(side, fromRank) || isPromotionZone(side, toRank));
        bool mandatory = isMandatoryPromotion(side, pt, toRank);

        if (!mandatory) {
            Move m;
            m.kind = MoveKind::Board;
            m.from = from;
            m.to = to;
            m.piece = pt;
            m.promote = false;
            out.push(m);
        }
        if (canPromote) {
            Move m;
            m.kind = MoveKind::Board;
            m.from = from;
            m.to = to;
            m.piece = pt;
            m.promote = true;
            out.push(m);
        }

        // レイ走査で相手駒に当たったら停止
        return pos.occupied.test(to);
    };

    if (!isRay) {
        addOne(file, rank);
    } else {
        while (inBoard(file, rank)) {
            if (addOne(file, rank)) {
                break;
            }
            file += df;
            rank += dr;
        }
    }
}

/// 駒種ごとの移動先生成
void generatePieceMoves(const EnginePosition& pos, Color side,
                        Square from, PieceType pt,
                        MoveList& out) noexcept
{
    int forward = (side == Color::Black) ? -1 : 1;

    switch (pt) {
    case PieceType::Pawn:
        addBoardMoves(pos, side, from, pt, 0, forward, false, out);
        break;
    case PieceType::Lance:
        addBoardMoves(pos, side, from, pt, 0, forward, true, out);
        break;
    case PieceType::Knight:
        addBoardMoves(pos, side, from, pt, -1, 2 * forward, false, out);
        addBoardMoves(pos, side, from, pt,  1, 2 * forward, false, out);
        break;
    case PieceType::Silver:
        addBoardMoves(pos, side, from, pt, -1, forward, false, out);
        addBoardMoves(pos, side, from, pt,  0, forward, false, out);
        addBoardMoves(pos, side, from, pt,  1, forward, false, out);
        addBoardMoves(pos, side, from, pt, -1, -forward, false, out);
        addBoardMoves(pos, side, from, pt,  1, -forward, false, out);
        break;
    case PieceType::Gold:
    case PieceType::ProPawn:
    case PieceType::ProLance:
    case PieceType::ProKnight:
    case PieceType::ProSilver:
        addBoardMoves(pos, side, from, pt, -1, forward, false, out);
        addBoardMoves(pos, side, from, pt,  0, forward, false, out);
        addBoardMoves(pos, side, from, pt,  1, forward, false, out);
        addBoardMoves(pos, side, from, pt, -1, 0, false, out);
        addBoardMoves(pos, side, from, pt,  1, 0, false, out);
        addBoardMoves(pos, side, from, pt,  0, -forward, false, out);
        break;
    case PieceType::King:
        for (int dr = -1; dr <= 1; ++dr) {
            for (int df = -1; df <= 1; ++df) {
                if (df == 0 && dr == 0) {
                    continue;
                }
                addBoardMoves(pos, side, from, pt, df, dr, false, out);
            }
        }
        break;
    case PieceType::Bishop:
        addBoardMoves(pos, side, from, pt, -1, -1, true, out);
        addBoardMoves(pos, side, from, pt,  1, -1, true, out);
        addBoardMoves(pos, side, from, pt, -1,  1, true, out);
        addBoardMoves(pos, side, from, pt,  1,  1, true, out);
        break;
    case PieceType::Rook:
        addBoardMoves(pos, side, from, pt, 0, -1, true, out);
        addBoardMoves(pos, side, from, pt, 0,  1, true, out);
        addBoardMoves(pos, side, from, pt, -1, 0, true, out);
        addBoardMoves(pos, side, from, pt,  1, 0, true, out);
        break;
    case PieceType::Horse:
        // 斜め走り
        addBoardMoves(pos, side, from, pt, -1, -1, true, out);
        addBoardMoves(pos, side, from, pt,  1, -1, true, out);
        addBoardMoves(pos, side, from, pt, -1,  1, true, out);
        addBoardMoves(pos, side, from, pt,  1,  1, true, out);
        // 十字ステップ
        addBoardMoves(pos, side, from, pt,  0, -1, false, out);
        addBoardMoves(pos, side, from, pt,  0,  1, false, out);
        addBoardMoves(pos, side, from, pt, -1,  0, false, out);
        addBoardMoves(pos, side, from, pt,  1,  0, false, out);
        break;
    case PieceType::Dragon:
        // 縦横走り
        addBoardMoves(pos, side, from, pt, 0, -1, true, out);
        addBoardMoves(pos, side, from, pt, 0,  1, true, out);
        addBoardMoves(pos, side, from, pt, -1, 0, true, out);
        addBoardMoves(pos, side, from, pt,  1, 0, true, out);
        // 斜めステップ
        addBoardMoves(pos, side, from, pt, -1, -1, false, out);
        addBoardMoves(pos, side, from, pt,  1, -1, false, out);
        addBoardMoves(pos, side, from, pt, -1,  1, false, out);
        addBoardMoves(pos, side, from, pt,  1,  1, false, out);
        break;
    default:
        break;
    }
}

} // namespace

// ---- LegalCore 合法手生成 ----

void LegalCore::generateLegalMoves(EnginePosition& pos, Color side, MoveList& out) const
{
    MoveList pseudo;
    pseudo.clear();

    if (ownKingInCheck(pos, side)) {
        generateEvasionMoves(pos, side, pseudo);
    } else {
        generateNonEvasionMoves(pos, side, pseudo);
        generateDropMoves(pos, side, pseudo);
    }

    for (int i = 0; i < pseudo.size; ++i) {
        const Move& m = pseudo.moves[static_cast<std::size_t>(i)];

        // 打ち歩詰め判定
        if (m.kind == MoveKind::Drop && m.piece == PieceType::Pawn) {
            UndoState undo;
            if (!pos.doMove(m, side, undo)) {
                continue;
            }
            bool selfCheck = ownKingInCheck(pos, side);
            bool pawnDropMate = false;
            if (!selfCheck && givesDirectPawnCheck(pos, side, m)) {
                Color opponent = opposite(side);
                if (!hasAnyLegalMove(pos, opponent)) {
                    pawnDropMate = true;
                }
            }
            pos.undoMove(undo, side);
            if (!selfCheck && !pawnDropMate) {
                out.push(m);
            }
        } else {
            if (isLegalAfterDoUndo(pos, side, m)) {
                out.push(m);
            }
        }
    }
}

int LegalCore::countLegalMoves(EnginePosition& pos, Color side) const
{
    MoveList pseudo;
    pseudo.clear();

    if (ownKingInCheck(pos, side)) {
        generateEvasionMoves(pos, side, pseudo);
    } else {
        generateNonEvasionMoves(pos, side, pseudo);
        generateDropMoves(pos, side, pseudo);
    }

    int legalCount = 0;
    for (int i = 0; i < pseudo.size; ++i) {
        const Move& m = pseudo.moves[static_cast<std::size_t>(i)];

        // 打ち歩詰め判定
        if (m.kind == MoveKind::Drop && m.piece == PieceType::Pawn) {
            UndoState undo;
            if (!pos.doMove(m, side, undo)) {
                continue;
            }
            bool selfCheck = ownKingInCheck(pos, side);
            bool pawnDropMate = false;
            if (!selfCheck && givesDirectPawnCheck(pos, side, m)) {
                Color opponent = opposite(side);
                if (!hasAnyLegalMove(pos, opponent)) {
                    pawnDropMate = true;
                }
            }
            pos.undoMove(undo, side);
            if (!selfCheck && !pawnDropMate) {
                ++legalCount;
            }
        } else {
            if (isLegalAfterDoUndo(pos, side, m)) {
                ++legalCount;
            }
        }
    }
    return legalCount;
}

void LegalCore::generateEvasionMoves(const EnginePosition& pos, Color side, MoveList& out) const
{
    int ci = static_cast<int>(side);
    Square king = pos.kingSq[ci];
    if (king == kInvalidSquare) {
        return;
    }

    // 1. 玉の移動
    generatePieceMoves(pos, side, king, PieceType::King, out);

    // 2. 王手している駒が1つなら、取る・合駒もあり得る
    Bitboard81 checkers = attackersTo(pos, king, opposite(side));
    if (checkers.count() != 1) {
        return; // 両王手は玉逃げのみ
    }

    Square checkerSq = checkers.popFirst();

    // 2a. 王手駒を取る手（玉以外で）
    Bitboard81 myPieces = pos.colorOcc[ci];
    Bitboard81 tmp = myPieces;
    while (tmp.any()) {
        Square from = tmp.popFirst();
        if (from == king) {
            continue;
        }
        char c = pos.board[from];
        PieceType pt = charToPieceType(c);
        if (pt == PieceType::PieceTypeNb) {
            continue;
        }

        int fromRank = squareRank(from);
        int toRank = squareRank(checkerSq);
        bool canPromote = isPromotable(pt)
                          && (isPromotionZone(side, fromRank) || isPromotionZone(side, toRank));
        bool mandatory = isMandatoryPromotion(side, pt, toRank);

        // 手動で利きチェック（isPseudoLegalの一部）
        Move capMove;
        capMove.kind = MoveKind::Board;
        capMove.from = from;
        capMove.to = checkerSq;
        capMove.piece = pt;

        if (!mandatory) {
            capMove.promote = false;
            out.push(capMove);
        }
        if (canPromote) {
            capMove.promote = true;
            out.push(capMove);
        }
    }

    // 2b. 走り駒の王手なら合駒（間に打つ/移動する）
    PieceType checkerType = charToPieceType(pos.board[checkerSq]);
    bool isSlider = (checkerType == PieceType::Lance || checkerType == PieceType::Bishop
                     || checkerType == PieceType::Rook || checkerType == PieceType::Horse
                     || checkerType == PieceType::Dragon);
    if (!isSlider) {
        return;
    }

    // checkerSq → king の間のマスを列挙
    int kf = squareFile(king);
    int kr = squareRank(king);
    int cf = squareFile(checkerSq);
    int cr = squareRank(checkerSq);
    int stepF = 0;
    int stepR = 0;
    if (cf < kf) stepF = 1;
    else if (cf > kf) stepF = -1;
    if (cr < kr) stepR = 1;
    else if (cr > kr) stepR = -1;

    int f = cf + stepF;
    int r = cr + stepR;
    while (f != kf || r != kr) {
        Square between = toSquare(f, r);

        // 盤上駒で合駒
        Bitboard81 tmp2 = myPieces;
        while (tmp2.any()) {
            Square from = tmp2.popFirst();
            if (from == king) {
                continue;
            }
            char c = pos.board[from];
            PieceType pt = charToPieceType(c);
            if (pt == PieceType::PieceTypeNb) {
                continue;
            }

            int fromRank = squareRank(from);
            int toRank = squareRank(between);
            bool canPromote = isPromotable(pt)
                              && (isPromotionZone(side, fromRank) || isPromotionZone(side, toRank));
            bool mandatory = isMandatoryPromotion(side, pt, toRank);

            Move interpose;
            interpose.kind = MoveKind::Board;
            interpose.from = from;
            interpose.to = between;
            interpose.piece = pt;

            if (!mandatory) {
                interpose.promote = false;
                out.push(interpose);
            }
            if (canPromote) {
                interpose.promote = true;
                out.push(interpose);
            }
        }

        // 打ちで合駒
        for (int hti = 0; hti < static_cast<int>(HandType::HandTypeNb); ++hti) {
            if (pos.hand[ci][static_cast<std::size_t>(hti)] <= 0) {
                continue;
            }
            PieceType pt = handTypeToPieceType(static_cast<HandType>(hti));
            if (pt == PieceType::PieceTypeNb) {
                continue;
            }

            int toRank = squareRank(between);
            int toFile = squareFile(between);
            if (isDropDeadSquare(side, pt, toRank)) {
                continue;
            }
            if (pt == PieceType::Pawn && detail::hasPawnOnFile(pos, side, toFile)) {
                continue;
            }

            Move drop;
            drop.kind = MoveKind::Drop;
            drop.from = kInvalidSquare;
            drop.to = between;
            drop.piece = pt;
            drop.promote = false;
            out.push(drop);
        }

        f += stepF;
        r += stepR;
    }
}

void LegalCore::generateNonEvasionMoves(const EnginePosition& pos, Color side, MoveList& out) const
{
    int ci = static_cast<int>(side);
    Bitboard81 myPieces = pos.colorOcc[ci];
    while (myPieces.any()) {
        Square from = myPieces.popFirst();
        char c = pos.board[from];
        PieceType pt = charToPieceType(c);
        if (pt == PieceType::PieceTypeNb) {
            continue;
        }
        generatePieceMoves(pos, side, from, pt, out);
    }
}

void LegalCore::generateDropMoves(const EnginePosition& pos, Color side, MoveList& out) const
{
    int ci = static_cast<int>(side);

    for (int hti = 0; hti < static_cast<int>(HandType::HandTypeNb); ++hti) {
        if (pos.hand[ci][static_cast<std::size_t>(hti)] <= 0) {
            continue;
        }
        PieceType pt = handTypeToPieceType(static_cast<HandType>(hti));
        if (pt == PieceType::PieceTypeNb) {
            continue;
        }

        for (int sq = 0; sq < kSquareNb; ++sq) {
            auto s = static_cast<Square>(sq);
            if (pos.occupied.test(s)) {
                continue;
            }

            int toRank = squareRank(s);
            int toFile = squareFile(s);
            if (isDropDeadSquare(side, pt, toRank)) {
                continue;
            }
            if (pt == PieceType::Pawn && detail::hasPawnOnFile(pos, side, toFile)) {
                continue;
            }

            Move m;
            m.kind = MoveKind::Drop;
            m.from = kInvalidSquare;
            m.to = s;
            m.piece = pt;
            m.promote = false;
            out.push(m);
        }
    }
}

bool LegalCore::hasAnyLegalMove(EnginePosition& pos, Color side) const
{
    MoveList pseudo;
    pseudo.clear();

    if (ownKingInCheck(pos, side)) {
        generateEvasionMoves(pos, side, pseudo);
    } else {
        generateNonEvasionMoves(pos, side, pseudo);
        generateDropMoves(pos, side, pseudo);
    }

    for (int i = 0; i < pseudo.size; ++i) {
        const Move& m = pseudo.moves[static_cast<std::size_t>(i)];
        UndoState undo;
        if (!pos.doMove(m, side, undo)) {
            continue;
        }
        bool selfCheck = ownKingInCheck(pos, side);
        pos.undoMove(undo, side);
        if (!selfCheck) {
            return true;
        }
    }
    return false;
}

} // namespace fmv
