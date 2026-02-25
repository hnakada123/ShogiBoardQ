/// @file enginemovevalidator.cpp
/// @brief EngineMoveValidator の実装（Context主経路 + 互換ラッパ）

#include "enginemovevalidator.h"

#include "fmvconverter.h"
#include "fmvlegalcore.h"

namespace {
fmv::LegalCore& legalCore()
{
    static fmv::LegalCore core;
    return core;
}
} // namespace

bool EngineMoveValidator::syncContext(Context& ctx,
                                      const Turn& turn,
                                      const QVector<Piece>& boardData,
                                      const QMap<Piece, int>& pieceStand) const
{
    ctx.turn = turn;
    ctx.undoSize = 0;
    ctx.synced = fmv::Converter::toEnginePosition(ctx.pos, boardData, pieceStand);
    return ctx.synced;
}

LegalMoveStatus EngineMoveValidator::isLegalMove(Context& ctx, ShogiMove& move) const
{
    if (!ctx.synced) {
        return LegalMoveStatus(false, false);
    }

    fmv::ConvertedMove cm;
    if (!fmv::Converter::toEngineMove(cm, ctx.turn, ctx.pos, move)) {
        return LegalMoveStatus(false, false);
    }

    fmv::EnginePosition work = ctx.pos;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);

    bool nonPromoting = legalCore().checkMove(work, side, cm.nonPromote).nonPromotingMoveExists;
    bool promoting = false;
    if (cm.hasPromoteVariant) {
        promoting = legalCore().checkMove(work, side, cm.promote).nonPromotingMoveExists;
    }
    return LegalMoveStatus(nonPromoting, promoting);
}

int EngineMoveValidator::generateLegalMoves(Context& ctx) const
{
    if (!ctx.synced) {
        return 0;
    }
    fmv::EnginePosition work = ctx.pos;
    return legalCore().countLegalMoves(work, fmv::Converter::toColor(ctx.turn));
}

int EngineMoveValidator::checkIfKingInCheck(Context& ctx) const
{
    if (!ctx.synced) {
        return 0;
    }
    return legalCore().countChecksToKing(ctx.pos, fmv::Converter::toColor(ctx.turn));
}

bool EngineMoveValidator::tryApplyMove(Context& ctx, ShogiMove& move) const
{
    if (!ctx.synced || ctx.undoSize >= Context::kUndoMax) {
        return false;
    }

    fmv::ConvertedMove cm;
    if (!fmv::Converter::toEngineMove(cm, ctx.turn, ctx.pos, move)) {
        return false;
    }

    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    fmv::UndoState undo;
    const fmv::Move selected = move.isPromotion && cm.hasPromoteVariant ? cm.promote : cm.nonPromote;
    if (!legalCore().tryApplyLegalMove(ctx.pos, side, selected, undo)) {
        return false;
    }

    ctx.undoStack[static_cast<std::size_t>(ctx.undoSize++)] = undo;
    ctx.turn = (ctx.turn == BLACK) ? WHITE : BLACK;
    return true;
}

bool EngineMoveValidator::undoLastMove(Context& ctx) const
{
    if (!ctx.synced || ctx.undoSize <= 0) {
        return false;
    }

    ctx.turn = (ctx.turn == BLACK) ? WHITE : BLACK;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    const fmv::UndoState undo = ctx.undoStack[static_cast<std::size_t>(--ctx.undoSize)];
    legalCore().undoAppliedMove(ctx.pos, side, undo);
    return true;
}

// ---- 互換ラッパ（毎回Contextを作る） ----

LegalMoveStatus EngineMoveValidator::isLegalMove(const Turn& turn,
                                                  const QVector<Piece>& boardData,
                                                  const QMap<Piece, int>& pieceStand,
                                                  ShogiMove& currentMove) const
{
    Context ctx;
    if (!syncContext(ctx, turn, boardData, pieceStand)) {
        return LegalMoveStatus(false, false);
    }
    return isLegalMove(ctx, currentMove);
}

int EngineMoveValidator::generateLegalMoves(const Turn& turn,
                                             const QVector<Piece>& boardData,
                                             const QMap<Piece, int>& pieceStand) const
{
    Context ctx;
    if (!syncContext(ctx, turn, boardData, pieceStand)) {
        return 0;
    }
    return generateLegalMoves(ctx);
}

int EngineMoveValidator::checkIfKingInCheck(const Turn& turn,
                                             const QVector<Piece>& boardData) const
{
    Context ctx;
    QMap<Piece, int> emptyStand;
    if (!syncContext(ctx, turn, boardData, emptyStand)) {
        return 0;
    }
    return checkIfKingInCheck(ctx);
}
