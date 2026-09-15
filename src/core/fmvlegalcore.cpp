/// @file fmvlegalcore.cpp
/// @brief 合法手判定コアの実装（合法性判定）

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

constexpr char kEmpty = ' ';

} // namespace

// ---- LegalCore public ----

LegalMoveStatus LegalCore::checkMove(EnginePosition& pos, Color side, const Move& candidate) const
{
    bool ok = isLegalAfterDoUndo(pos, side, candidate);
    return LegalMoveStatus(ok, false);
}

LegalMoveStatus LegalCore::checkMoveVariants(EnginePosition& pos, Color side, const Move& candidate) const
{
    if (candidate.to >= kSquareNb) {
        return {};
    }

    Move probe = candidate;
    const int toRank = squareRank(probe.to);
    // 強制成りでは成った手を、それ以外では不成の手を代表として検証する。
    probe.promote = probe.kind == MoveKind::Board && isMandatoryPromotion(side, probe.piece, toRank);
    if (!isLegalAfterDoUndo(pos, side, probe)) {
        return {};
    }

    // 成り・不成で占有マス、敵駒、自玉の位置は同じなので、自玉の安全確認は共有できる。
    // 駒打ちは成れず、打ち歩詰めも上の通常の合法性判定で確認済み。
    const bool canPromote = probe.kind == MoveKind::Board && isPromotable(probe.piece)
                            && (isPromotionZone(side, squareRank(probe.from))
                                || isPromotionZone(side, toRank));
    return LegalMoveStatus(!probe.promote, canPromote);
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
    if (ownKingInCheck(pos, side) || isPawnDropMate(pos, side, m)) {
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

    if (!pieceAttacksSquare(pos, side, m.piece, m.from, m.to)) {
        return false;
    }

    const int fromRank = squareRank(m.from);
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
    return isSquareAttacked(pos, king, opposite(side));
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
    // 歩の直接王手は合駒で防げないため、応手に駒打ちはなく、この判定は再帰しない。
    return !hasAnyLegalMove(pos, opponent);
}

} // namespace fmv
