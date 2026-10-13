/// @file enginemovevalidator.cpp
/// @brief EngineMoveValidator の実装（Context主経路 + 互換ラッパ）

#include "enginemovevalidator.h"

#include "fmvattacks.h"
#include "fmvconverter.h"
#include "fmvlegalcore.h"
#include "fmvlegalcore_internal.h"

namespace {
fmv::LegalCore& legalCore()
{
    static fmv::LegalCore core;
    return core;
}
} // namespace

bool EngineMoveValidator::syncContext(Context& ctx,
                                      Turn turn,
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

QList<UsiMove> EngineMoveValidator::legalMoves(Context& ctx) const
{
    QList<UsiMove> out;
    if (!ctx.synced) {
        return out;
    }
    fmv::EnginePosition work = ctx.pos;
    fmv::MoveList list;
    legalCore().generateLegalMoves(work, fmv::Converter::toColor(ctx.turn), list);
    out.reserve(list.size);
    for (int i = 0; i < list.size; ++i) {
        out.append(fmv::Converter::toUsiMove(list.moves[static_cast<std::size_t>(i)]));
    }
    return out;
}

QList<UsiMove> EngineMoveValidator::pseudoLegalMoves(Context& ctx) const
{
    QList<UsiMove> out;
    if (!ctx.synced) {
        return out;
    }
    const fmv::EnginePosition& pos = ctx.pos;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    const auto ci = static_cast<std::size_t>(side);
    for (int from = 0; from < fmv::kSquareNb; ++from) {
        const auto fromSq = static_cast<fmv::Square>(from);
        if (!pos.colorOcc[ci].test(fromSq)) {
            continue;
        }
        const fmv::PieceType pt = fmv::detail::charToPieceType(pos.board[fromSq]);
        if (pt == fmv::PieceType::PieceTypeNb) {
            continue;
        }
        for (int to = 0; to < fmv::kSquareNb; ++to) {
            const auto toSq = static_cast<fmv::Square>(to);
            if (pos.colorOcc[ci].test(toSq) || !fmv::pieceAttacksSquare(pos, side, pt, fromSq, toSq)) {
                continue;
            }
            fmv::Move m;
            m.from = fromSq;
            m.to = toSq;
            m.piece = pt;
            out.append(fmv::Converter::toUsiMove(m));
            if (fmv::detail::isPromotable(pt)
                && (fmv::detail::isPromotionZone(side, fmv::squareRank(fromSq))
                    || fmv::detail::isPromotionZone(side, fmv::squareRank(toSq)))) {
                m.promote = true;
                out.append(fmv::Converter::toUsiMove(m));
            }
        }
    }
    for (int hand = 0; hand < static_cast<int>(fmv::HandType::HandTypeNb); ++hand) {
        if (pos.hand[ci][static_cast<std::size_t>(hand)] <= 0) {
            continue;
        }
        fmv::Move m;
        m.kind = fmv::MoveKind::Drop;
        // HandType と PieceType は歩〜飛の並びが同じ
        m.piece = static_cast<fmv::PieceType>(hand);
        for (int to = 0; to < fmv::kSquareNb; ++to) {
            m.to = static_cast<fmv::Square>(to);
            if (!pos.occupied.test(m.to)) {
                out.append(fmv::Converter::toUsiMove(m));
            }
        }
    }
    return out;
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
                                      Turn turn,
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
    ctx.turn = (ctx.turn == Turn::Black) ? Turn::White : Turn::Black;
    return true;
}

bool EngineMoveValidator::undoLastMove(SimulationContext& simulation) const
{
    Context& ctx = simulation.context;
    if (!ctx.synced || simulation.undoSize <= 0) {
        return false;
    }

    ctx.turn = (ctx.turn == Turn::Black) ? Turn::White : Turn::Black;
    const fmv::Color side = fmv::Converter::toColor(ctx.turn);
    const fmv::UndoState undo = simulation.undoStack[static_cast<std::size_t>(--simulation.undoSize)];
    legalCore().undoAppliedMove(ctx.pos, side, undo);
    return true;
}
#endif

// ---- 互換ラッパ（毎回Contextを作る） ----

LegalMoveStatus EngineMoveValidator::isLegalMove(Turn turn,
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

int EngineMoveValidator::generateLegalMoves(Turn turn,
                                             const QList<Piece>& boardData,
                                             const QMap<Piece, int>& pieceStand) const
{
    Context ctx;
    if (!syncContext(ctx, turn, boardData, pieceStand)) {
        return 0;
    }
    return generateLegalMoves(ctx);
}

QList<UsiMove> EngineMoveValidator::legalMoves(Turn turn,
                                               const QList<Piece>& boardData,
                                               const QMap<Piece, int>& pieceStand) const
{
    Context ctx;
    if (!syncContext(ctx, turn, boardData, pieceStand)) {
        return {};
    }
    return legalMoves(ctx);
}

QList<UsiMove> EngineMoveValidator::pseudoLegalMoves(Turn turn,
                                                     const QList<Piece>& boardData,
                                                     const QMap<Piece, int>& pieceStand) const
{
    Context ctx;
    if (!syncContext(ctx, turn, boardData, pieceStand)) {
        return {};
    }
    return pseudoLegalMoves(ctx);
}

int EngineMoveValidator::checkIfKingInCheck(Turn turn,
                                             const QList<Piece>& boardData) const
{
    Context ctx;
    QMap<Piece, int> emptyStand;
    if (!syncContext(ctx, turn, boardData, emptyStand)) {
        return 0;
    }
    return checkIfKingInCheck(ctx);
}
