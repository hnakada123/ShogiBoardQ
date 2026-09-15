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
                                      const QList<Piece>& boardData,
                                      const QMap<Piece, int>& pieceStand) const
{
    ctx.turn = turn;
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

    if (cm.hasPromoteVariant) {
        return legalCore().checkMoveVariants(work, side, cm.nonPromote);
    }
    return legalCore().checkMove(work, side, cm.nonPromote);
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

#ifdef SHOGIBOARDQ_TESTING
bool EngineMoveValidator::syncContext(SimulationContext& simulation,
                                      const Turn& turn,
                                      const QList<Piece>& boardData,
                                      const QMap<Piece, int>& pieceStand) const
{
    simulation.undoSize = 0;
    return syncContext(simulation.context, turn, boardData, pieceStand);
}

bool EngineMoveValidator::tryApplyMove(SimulationContext& simulation, ShogiMove& move) const
{
    Context& ctx = simulation.context;
    if (!ctx.synced || simulation.undoSize >= SimulationContext::kUndoMax) {
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

    simulation.undoStack[static_cast<std::size_t>(simulation.undoSize++)] = undo;
    ctx.turn = (ctx.turn == BLACK) ? WHITE : BLACK;
    return true;
}

bool EngineMoveValidator::undoLastMove(SimulationContext& simulation) const
{
    Context& ctx = simulation.context;
    if (!ctx.synced || simulation.undoSize <= 0) {
        return false;
    }

    ctx.turn = (ctx.turn == BLACK) ? WHITE : BLACK;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    const fmv::UndoState undo = simulation.undoStack[static_cast<std::size_t>(--simulation.undoSize)];
    legalCore().undoAppliedMove(ctx.pos, side, undo);
    return true;
}
#endif

// ---- 互換ラッパ（毎回Contextを作る） ----

LegalMoveStatus EngineMoveValidator::isLegalMove(const Turn& turn,
                                                  const QList<Piece>& boardData,
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
                                             const QList<Piece>& boardData,
                                             const QMap<Piece, int>& pieceStand) const
{
    Context ctx;
    if (!syncContext(ctx, turn, boardData, pieceStand)) {
        return 0;
    }
    return generateLegalMoves(ctx);
}

int EngineMoveValidator::checkIfKingInCheck(const Turn& turn,
                                             const QList<Piece>& boardData) const
{
    Context ctx;
    QMap<Piece, int> emptyStand;
    if (!syncContext(ctx, turn, boardData, emptyStand)) {
        return 0;
    }
    return checkIfKingInCheck(ctx);
}
