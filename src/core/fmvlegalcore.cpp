/// @file fmvlegalcore.cpp
/// @brief 合法手判定コアの実装

#include "fmvlegalcore.h"

#include "fmvattacks.h"

#include <cctype>
#include <cstdlib>

namespace fmv {

namespace {

constexpr char kEmpty = ' ';

bool inBoard(int file, int rank) noexcept
{
    return file >= 0 && file < kBoardSize && rank >= 0 && rank < kBoardSize;
}

/// 成り可能な駒種か
bool isPromotable(PieceType pt) noexcept
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
bool isPromotionZone(Color side, int rank) noexcept
{
    if (side == Color::Black) {
        return rank <= 2;
    }
    return rank >= 6;
}

/// 強制成りか
bool isMandatoryPromotion(Color side, PieceType pt, int toRank) noexcept
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
bool isDropDeadSquare(Color side, PieceType pt, int toRank) noexcept
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
bool hasPawnOnFile(const EnginePosition& pos, Color side, int file) noexcept
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
PieceType charToPieceType(char c) noexcept
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

/// 歩打ちで直接王手になるか
bool givesDirectPawnCheck(const EnginePosition& pos, Color side, const Move& m) noexcept
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

} // namespace

// ---- LegalCore public ----

LegalMoveStatus LegalCore::checkMove(EnginePosition& pos, Color side, const Move& candidate) const
{
    bool ok = isLegalAfterDoUndo(pos, side, candidate);
    return LegalMoveStatus(ok, false);
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
        if (m.piece == PieceType::Pawn && hasPawnOnFile(pos, side, toFile)) {
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
            if (pt == PieceType::Pawn && hasPawnOnFile(pos, side, toFile)) {
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
            if (pt == PieceType::Pawn && hasPawnOnFile(pos, side, toFile)) {
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
